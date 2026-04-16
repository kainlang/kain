use std::{mem::size_of, sync::mpsc};

use bytemuck::{Pod, Zeroable};
use kain_core::ShaderArtifactBundle;
use wgpu::util::DeviceExt;

use crate::renderer::{
    FrameCameraSource, FrameDiagnostics, RenderBackend, RenderError, RenderFrame, RenderResolution,
    RenderStats, RenderViewSettings,
};
use crate::shader_bundle::{
    default_viewport_shader_bundle, wgsl_module_source, VIEWPORT_SHADER_MODULE_NAME,
};
use crate::{
    CameraPose, CpuPickingService, ManipulatorMode, Mat4, Mesh, ParticleEmitter, PickTargetId,
    PickingQuery, PickingRay, SceneCatalog, SceneDescription, Vec3,
};

const MAX_DIRECTIONAL_LIGHTS: usize = 2;
const MAX_POINT_LIGHTS: usize = 8;
const COLOR_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8UnormSrgb;
const DEPTH_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Depth32Float;
const PICK_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::R32Uint;

#[derive(Debug)]
pub struct WgpuRendererInitError {
    message: String,
}

impl WgpuRendererInitError {
    fn new(message: impl Into<String>) -> Self {
        Self {
            message: message.into(),
        }
    }
}

impl std::fmt::Display for WgpuRendererInitError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.message)
    }
}

impl std::error::Error for WgpuRendererInitError {}

pub struct WgpuRenderer {
    device: wgpu::Device,
    queue: wgpu::Queue,
    shader_bundle: ShaderArtifactBundle,
    uniform_buffer: wgpu::Buffer,
    uniform_bind_group: wgpu::BindGroup,
    background_pipeline: wgpu::RenderPipeline,
    scene_pipeline: wgpu::RenderPipeline,
    pick_pipeline: wgpu::RenderPipeline,
    particle_depth_pipeline: wgpu::RenderPipeline,
    particle_overlay_pipeline: wgpu::RenderPipeline,
    gizmo_pipeline: wgpu::RenderPipeline,
    target: Option<WgpuFrameTarget>,
}

struct WgpuFrameTarget {
    resolution: RenderResolution,
    color_texture: wgpu::Texture,
    color_view: wgpu::TextureView,
    pick_texture: wgpu::Texture,
    pick_view: wgpu::TextureView,
    _depth_texture: wgpu::Texture,
    depth_view: wgpu::TextureView,
    readback_buffer: wgpu::Buffer,
    pick_readback_buffer: wgpu::Buffer,
    padded_bytes_per_row: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct GpuVertex {
    position_and_ambient: [f32; 4],
    normal_and_diffuse: [f32; 4],
    base_color_and_specular_strength: [f32; 4],
    specular_color_and_shininess: [f32; 4],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
struct PickVertex {
    world_position: [f32; 4],
    instance_id: u32,
    _padding: [u32; 3],
}

impl PickVertex {
    const ATTRIBUTES: [wgpu::VertexAttribute; 2] = [
        wgpu::VertexAttribute {
            offset: 0,
            shader_location: 0,
            format: wgpu::VertexFormat::Float32x4,
        },
        wgpu::VertexAttribute {
            offset: size_of::<[f32; 4]>() as u64,
            shader_location: 1,
            format: wgpu::VertexFormat::Uint32,
        },
    ];

    pub fn layout() -> wgpu::VertexBufferLayout<'static> {
        wgpu::VertexBufferLayout {
            array_stride: size_of::<Self>() as wgpu::BufferAddress,
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: &Self::ATTRIBUTES,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct GizmoVertex {
    world_position: [f32; 4],
    color: [f32; 4],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct ParticleVertex {
    world_position: [f32; 4],
    color: [f32; 4],
    quad_uv: [f32; 2],
    _padding: [f32; 2],
}

impl GizmoVertex {
    const ATTRIBUTES: [wgpu::VertexAttribute; 2] = wgpu::vertex_attr_array![
        0 => Float32x4,
        1 => Float32x4
    ];

    pub fn layout() -> wgpu::VertexBufferLayout<'static> {
        wgpu::VertexBufferLayout {
            array_stride: size_of::<Self>() as wgpu::BufferAddress,
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: &Self::ATTRIBUTES,
        }
    }
}

impl ParticleVertex {
    const ATTRIBUTES: [wgpu::VertexAttribute; 3] = [
        wgpu::VertexAttribute {
            offset: 0,
            shader_location: 0,
            format: wgpu::VertexFormat::Float32x4,
        },
        wgpu::VertexAttribute {
            offset: size_of::<[f32; 4]>() as u64,
            shader_location: 1,
            format: wgpu::VertexFormat::Float32x4,
        },
        wgpu::VertexAttribute {
            offset: (size_of::<[f32; 4]>() * 2) as u64,
            shader_location: 2,
            format: wgpu::VertexFormat::Float32x2,
        },
    ];

    pub fn layout() -> wgpu::VertexBufferLayout<'static> {
        wgpu::VertexBufferLayout {
            array_stride: size_of::<Self>() as wgpu::BufferAddress,
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: &Self::ATTRIBUTES,
        }
    }
}

impl GpuVertex {
    const ATTRIBUTES: [wgpu::VertexAttribute; 4] = wgpu::vertex_attr_array![
        0 => Float32x4,
        1 => Float32x4,
        2 => Float32x4,
        3 => Float32x4
    ];

    pub fn layout() -> wgpu::VertexBufferLayout<'static> {
        wgpu::VertexBufferLayout {
            array_stride: size_of::<Self>() as wgpu::BufferAddress,
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: &Self::ATTRIBUTES,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct SceneUniforms {
    view_proj: [[f32; 4]; 4],
    camera_position: [f32; 4],
    ambient_color_intensity: [f32; 4],
    directional_directions: [[f32; 4]; MAX_DIRECTIONAL_LIGHTS],
    directional_colors: [[f32; 4]; MAX_DIRECTIONAL_LIGHTS],
    point_positions_intensity: [[f32; 4]; MAX_POINT_LIGHTS],
    point_colors_range: [[f32; 4]; MAX_POINT_LIGHTS],
    counts: [u32; 4],
    background_top: [f32; 4],
    background_bottom: [f32; 4],
    fog_color_density: [f32; 4],
}

impl WgpuRenderer {
    pub fn new() -> Result<Self, WgpuRendererInitError> {
        Self::new_with_shader_bundle(default_viewport_shader_bundle())
    }

    pub fn new_with_shader_bundle(
        shader_bundle: ShaderArtifactBundle,
    ) -> Result<Self, WgpuRendererInitError> {
        pollster::block_on(Self::new_async(shader_bundle))
    }

    async fn new_async(shader_bundle: ShaderArtifactBundle) -> Result<Self, WgpuRendererInitError> {
        let instance = wgpu::Instance::default();
        let adapter = instance
            .request_adapter(&wgpu::RequestAdapterOptions {
                power_preference: wgpu::PowerPreference::HighPerformance,
                compatible_surface: None,
                force_fallback_adapter: false,
            })
            .await
            .ok_or_else(|| WgpuRendererInitError::new("no compatible WGPU adapter was found"))?;

        let (device, queue) = adapter
            .request_device(
                &wgpu::DeviceDescriptor {
                    label: Some("kain-3d-wgpu-device"),
                    required_features: wgpu::Features::empty(),
                    required_limits: wgpu::Limits::default(),
                    memory_hints: wgpu::MemoryHints::Performance,
                },
                None,
            )
            .await
            .map_err(|err| {
                WgpuRendererInitError::new(format!("failed to create WGPU device: {err}"))
            })?;

        let shader_source = wgsl_module_source(&shader_bundle, VIEWPORT_SHADER_MODULE_NAME)
            .map_err(WgpuRendererInitError::new)?;
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("kain-3d-wgpu-shader"),
            source: wgpu::ShaderSource::Wgsl(shader_source),
        });

        let bind_group_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("kain-3d-scene-bind-group-layout"),
            entries: &[wgpu::BindGroupLayoutEntry {
                binding: 0,
                visibility: wgpu::ShaderStages::VERTEX_FRAGMENT,
                ty: wgpu::BindingType::Buffer {
                    ty: wgpu::BufferBindingType::Uniform,
                    has_dynamic_offset: false,
                    min_binding_size: None,
                },
                count: None,
            }],
        });

        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("kain-3d-wgpu-pipeline-layout"),
            bind_group_layouts: &[&bind_group_layout],
            push_constant_ranges: &[],
        });

        let uniform_buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("kain-3d-scene-uniform-buffer"),
            size: size_of::<SceneUniforms>() as u64,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        let uniform_bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("kain-3d-scene-bind-group"),
            layout: &bind_group_layout,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: uniform_buffer.as_entire_binding(),
            }],
        });

        let background_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("kain-3d-background-pipeline"),
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: Some("background_vs_main"),
                compilation_options: Default::default(),
                buffers: &[],
            },
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: wgpu::FrontFace::Ccw,
                cull_mode: None,
                polygon_mode: wgpu::PolygonMode::Fill,
                unclipped_depth: false,
                conservative: false,
            },
            depth_stencil: Some(wgpu::DepthStencilState {
                format: DEPTH_FORMAT,
                depth_write_enabled: false,
                depth_compare: wgpu::CompareFunction::Always,
                stencil: wgpu::StencilState::default(),
                bias: wgpu::DepthBiasState::default(),
            }),
            multisample: wgpu::MultisampleState::default(),
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: Some("background_fs_main"),
                compilation_options: Default::default(),
                targets: &[Some(wgpu::ColorTargetState {
                    format: COLOR_FORMAT,
                    blend: None,
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            multiview: None,
            cache: None,
        });

        let scene_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("kain-3d-scene-pipeline"),
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: Some("scene_vs_main"),
                compilation_options: Default::default(),
                buffers: &[GpuVertex::layout()],
            },
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: wgpu::FrontFace::Ccw,
                cull_mode: None,
                polygon_mode: wgpu::PolygonMode::Fill,
                unclipped_depth: false,
                conservative: false,
            },
            depth_stencil: Some(wgpu::DepthStencilState {
                format: DEPTH_FORMAT,
                depth_write_enabled: true,
                depth_compare: wgpu::CompareFunction::LessEqual,
                stencil: wgpu::StencilState::default(),
                bias: wgpu::DepthBiasState::default(),
            }),
            multisample: wgpu::MultisampleState::default(),
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: Some("scene_fs_main"),
                compilation_options: Default::default(),
                targets: &[Some(wgpu::ColorTargetState {
                    format: COLOR_FORMAT,
                    blend: None,
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            multiview: None,
            cache: None,
        });

        let pick_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("kain-3d-pick-pipeline"),
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: Some("pick_vs_main"),
                compilation_options: Default::default(),
                buffers: &[PickVertex::layout()],
            },
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: wgpu::FrontFace::Ccw,
                cull_mode: None,
                polygon_mode: wgpu::PolygonMode::Fill,
                unclipped_depth: false,
                conservative: false,
            },
            depth_stencil: Some(wgpu::DepthStencilState {
                format: DEPTH_FORMAT,
                depth_write_enabled: true,
                depth_compare: wgpu::CompareFunction::LessEqual,
                stencil: wgpu::StencilState::default(),
                bias: wgpu::DepthBiasState::default(),
            }),
            multisample: wgpu::MultisampleState::default(),
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: Some("pick_fs_main"),
                compilation_options: Default::default(),
                targets: &[Some(wgpu::ColorTargetState {
                    format: PICK_FORMAT,
                    blend: None,
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            multiview: None,
            cache: None,
        });

        let particle_blend = wgpu::BlendState {
            color: wgpu::BlendComponent {
                src_factor: wgpu::BlendFactor::One,
                dst_factor: wgpu::BlendFactor::One,
                operation: wgpu::BlendOperation::Add,
            },
            alpha: wgpu::BlendComponent {
                src_factor: wgpu::BlendFactor::One,
                dst_factor: wgpu::BlendFactor::One,
                operation: wgpu::BlendOperation::Add,
            },
        };
        let particle_pipeline_descriptor =
            |label: &'static str, depth_compare: wgpu::CompareFunction| {
                device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
                    label: Some(label),
                    layout: Some(&pipeline_layout),
                    vertex: wgpu::VertexState {
                        module: &shader,
                        entry_point: Some("particle_vs_main"),
                        compilation_options: Default::default(),
                        buffers: &[ParticleVertex::layout()],
                    },
                    primitive: wgpu::PrimitiveState {
                        topology: wgpu::PrimitiveTopology::TriangleList,
                        strip_index_format: None,
                        front_face: wgpu::FrontFace::Ccw,
                        cull_mode: None,
                        polygon_mode: wgpu::PolygonMode::Fill,
                        unclipped_depth: false,
                        conservative: false,
                    },
                    depth_stencil: Some(wgpu::DepthStencilState {
                        format: DEPTH_FORMAT,
                        depth_write_enabled: false,
                        depth_compare,
                        stencil: wgpu::StencilState::default(),
                        bias: wgpu::DepthBiasState::default(),
                    }),
                    multisample: wgpu::MultisampleState::default(),
                    fragment: Some(wgpu::FragmentState {
                        module: &shader,
                        entry_point: Some("particle_fs_main"),
                        compilation_options: Default::default(),
                        targets: &[Some(wgpu::ColorTargetState {
                            format: COLOR_FORMAT,
                            blend: Some(particle_blend),
                            write_mask: wgpu::ColorWrites::ALL,
                        })],
                    }),
                    multiview: None,
                    cache: None,
                })
            };
        let particle_depth_pipeline = particle_pipeline_descriptor(
            "kain-3d-particle-depth-pipeline",
            wgpu::CompareFunction::LessEqual,
        );
        let particle_overlay_pipeline = particle_pipeline_descriptor(
            "kain-3d-particle-overlay-pipeline",
            wgpu::CompareFunction::Always,
        );

        let gizmo_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("kain-3d-gizmo-pipeline"),
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: Some("gizmo_vs_main"),
                compilation_options: Default::default(),
                buffers: &[GizmoVertex::layout()],
            },
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::LineList,
                strip_index_format: None,
                front_face: wgpu::FrontFace::Ccw,
                cull_mode: None,
                polygon_mode: wgpu::PolygonMode::Fill,
                unclipped_depth: false,
                conservative: false,
            },
            depth_stencil: Some(wgpu::DepthStencilState {
                format: DEPTH_FORMAT,
                depth_write_enabled: false,
                depth_compare: wgpu::CompareFunction::Always,
                stencil: wgpu::StencilState::default(),
                bias: wgpu::DepthBiasState::default(),
            }),
            multisample: wgpu::MultisampleState::default(),
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: Some("gizmo_fs_main"),
                compilation_options: Default::default(),
                targets: &[Some(wgpu::ColorTargetState {
                    format: COLOR_FORMAT,
                    blend: Some(wgpu::BlendState::ALPHA_BLENDING),
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            multiview: None,
            cache: None,
        });

        Ok(Self {
            device,
            queue,
            shader_bundle,
            uniform_buffer,
            uniform_bind_group,
            background_pipeline,
            scene_pipeline,
            pick_pipeline,
            particle_depth_pipeline,
            particle_overlay_pipeline,
            gizmo_pipeline,
            target: None,
        })
    }

    pub fn shader_bundle(&self) -> &ShaderArtifactBundle {
        &self.shader_bundle
    }

    fn render_catalog_scene_internal(
        &mut self,
        catalog: &SceneCatalog,
        scene_name: &str,
        time_seconds: f32,
        resolution: RenderResolution,
        view: &RenderViewSettings,
    ) -> Result<RenderFrame, RenderError> {
        let resolved_scene = catalog
            .resolve_scene(scene_name)
            .ok_or_else(|| RenderError::MissingScene(scene_name.to_string()))?;
        let mut frame =
            self.render_scene_internal(resolved_scene.scene, time_seconds, resolution, view)?;
        frame.diagnostics.scene_resolution = Some(resolved_scene.resolution);
        Ok(frame)
    }

    fn render_scene_internal(
        &mut self,
        scene: &SceneDescription,
        time_seconds: f32,
        resolution: RenderResolution,
        view: &RenderViewSettings,
    ) -> Result<RenderFrame, RenderError> {
        self.ensure_target(resolution);
        let target = self.target.as_ref().expect("target was just initialized");
        let color_texture = target.color_texture.clone();
        let color_view = target.color_view.clone();
        let depth_view = target.depth_view.clone();
        let readback_buffer = target.readback_buffer.clone();
        let padded_bytes_per_row = target.padded_bytes_per_row;
        let prepared = prepare_wgpu_frame(scene, time_seconds, resolution, view)?;
        self.queue.write_buffer(
            &self.uniform_buffer,
            0,
            bytemuck::bytes_of(&prepared.uniforms),
        );

        let vertex_buffer = if prepared.scene_vertices.is_empty() {
            None
        } else {
            Some(
                self.device
                    .create_buffer_init(&wgpu::util::BufferInitDescriptor {
                        label: Some("kain-3d-scene-vertex-buffer"),
                        contents: bytemuck::cast_slice(&prepared.scene_vertices),
                        usage: wgpu::BufferUsages::VERTEX,
                    }),
            )
        };
        let depth_particle_buffer = if prepared.depth_particles.is_empty() {
            None
        } else {
            Some(
                self.device
                    .create_buffer_init(&wgpu::util::BufferInitDescriptor {
                        label: Some("kain-3d-particle-depth-buffer"),
                        contents: bytemuck::cast_slice(&prepared.depth_particles),
                        usage: wgpu::BufferUsages::VERTEX,
                    }),
            )
        };
        let overlay_particle_buffer = if prepared.overlay_particles.is_empty() {
            None
        } else {
            Some(
                self.device
                    .create_buffer_init(&wgpu::util::BufferInitDescriptor {
                        label: Some("kain-3d-particle-overlay-buffer"),
                        contents: bytemuck::cast_slice(&prepared.overlay_particles),
                        usage: wgpu::BufferUsages::VERTEX,
                    }),
            )
        };
        let gizmo_buffer = if prepared.gizmo_vertices.is_empty() {
            None
        } else {
            Some(
                self.device
                    .create_buffer_init(&wgpu::util::BufferInitDescriptor {
                        label: Some("kain-3d-gizmo-vertex-buffer"),
                        contents: bytemuck::cast_slice(&prepared.gizmo_vertices),
                        usage: wgpu::BufferUsages::VERTEX,
                    }),
            )
        };

        let mut encoder = self
            .device
            .create_command_encoder(&wgpu::CommandEncoderDescriptor {
                label: Some("kain-3d-scene-encoder"),
            });

        {
            let mut render_pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("kain-3d-scene-pass"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &color_view,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Clear(wgpu::Color::BLACK),
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                    view: &depth_view,
                    depth_ops: Some(wgpu::Operations {
                        load: wgpu::LoadOp::Clear(1.0),
                        store: wgpu::StoreOp::Store,
                    }),
                    stencil_ops: None,
                }),
                timestamp_writes: None,
                occlusion_query_set: None,
            });

            render_pass.set_bind_group(0, &self.uniform_bind_group, &[]);
            render_pass.set_pipeline(&self.background_pipeline);
            render_pass.draw(0..3, 0..1);

            if let Some(vertex_buffer) = vertex_buffer.as_ref() {
                render_pass.set_pipeline(&self.scene_pipeline);
                render_pass.set_vertex_buffer(0, vertex_buffer.slice(..));
                render_pass.draw(0..prepared.scene_vertices.len() as u32, 0..1);
            }

            if let Some(depth_particle_buffer) = depth_particle_buffer.as_ref() {
                render_pass.set_pipeline(&self.particle_depth_pipeline);
                render_pass.set_vertex_buffer(0, depth_particle_buffer.slice(..));
                render_pass.draw(0..prepared.depth_particles.len() as u32, 0..1);
            }

            if let Some(overlay_particle_buffer) = overlay_particle_buffer.as_ref() {
                render_pass.set_pipeline(&self.particle_overlay_pipeline);
                render_pass.set_vertex_buffer(0, overlay_particle_buffer.slice(..));
                render_pass.draw(0..prepared.overlay_particles.len() as u32, 0..1);
            }

            if let Some(gizmo_buffer) = gizmo_buffer.as_ref() {
                render_pass.set_pipeline(&self.gizmo_pipeline);
                render_pass.set_vertex_buffer(0, gizmo_buffer.slice(..));
                render_pass.draw(0..prepared.gizmo_vertices.len() as u32, 0..1);
            }
        }

        encoder.copy_texture_to_buffer(
            wgpu::TexelCopyTextureInfo {
                texture: &color_texture,
                mip_level: 0,
                origin: wgpu::Origin3d::ZERO,
                aspect: wgpu::TextureAspect::All,
            },
            wgpu::TexelCopyBufferInfo {
                buffer: &readback_buffer,
                layout: wgpu::TexelCopyBufferLayout {
                    offset: 0,
                    bytes_per_row: Some(padded_bytes_per_row),
                    rows_per_image: Some(resolution.height as u32),
                },
            },
            wgpu::Extent3d {
                width: resolution.width as u32,
                height: resolution.height as u32,
                depth_or_array_layers: 1,
            },
        );

        self.queue.submit(Some(encoder.finish()));

        let rgba = download_target_rgba(
            &self.device,
            &readback_buffer,
            resolution,
            padded_bytes_per_row,
        )
        .map_err(|err| RenderError::BackendFailure(err.to_string()))?;
        let mut stats = prepared.stats;
        stats.pixels_shaded = resolution.width * resolution.height;
        let mut diagnostics = FrameDiagnostics::default();
        diagnostics.scene_name = Some(scene.name.clone());
        diagnostics.viewport_summary = Some(scene.viewport_summary.clone());
        let composition_summary = scene.composition_summary_with_overrides_and_aspect_ratio(
            time_seconds,
            &view.instance_transform_overrides,
            resolution.width as f32 / resolution.height as f32,
        );
        diagnostics.composition_summary = Some(composition_summary.brief_label());
        diagnostics.scene_role = Some(composition_summary.scene_role_label().to_string());
        diagnostics.scene_scale = Some(
            crate::SceneCompositionSummary::scene_scale_label(composition_summary.bounds)
                .to_string(),
        );
        diagnostics.scene_profile = Some(
            composition_summary
                .bounds
                .map(|bounds| bounds.composition_profile_label().to_string())
                .unwrap_or_else(|| "unbounded".to_string()),
        );
        diagnostics.scene_density = Some(composition_summary.density_label().to_string());
        diagnostics.composition_stage = Some(
            composition_summary
                .bounds
                .map(|bounds| bounds.composition_stage_label().to_string())
                .unwrap_or_else(|| "unbounded".to_string()),
        );
        diagnostics.framing_hint = composition_summary.framing_hint_label().map(str::to_string);
        diagnostics.camera_fit_ratio = composition_summary
            .bounds
            .zip(composition_summary.framed_camera_distance)
            .map(|(bounds, distance)| format!("{:.2}", distance / bounds.radius().max(0.001)));
        diagnostics.camera_source = Some(if view.camera.is_some() {
            FrameCameraSource::ExplicitView
        } else {
            FrameCameraSource::AutoFramed
        });

        Ok(RenderFrame {
            width: resolution.width,
            height: resolution.height,
            rgba,
            stats,
            diagnostics,
        })
    }

    fn ensure_target(&mut self, resolution: RenderResolution) {
        let recreate = self
            .target
            .as_ref()
            .map(|target| target.resolution != resolution)
            .unwrap_or(true);

        if recreate {
            self.target = Some(create_target(&self.device, resolution));
        }
    }
}

impl RenderBackend for WgpuRenderer {
    fn backend_name(&self) -> &'static str {
        "wgpu"
    }

    fn render_catalog_scene_with_view(
        &mut self,
        catalog: &SceneCatalog,
        scene_name: &str,
        time_seconds: f32,
        resolution: RenderResolution,
        view: &RenderViewSettings,
    ) -> Result<RenderFrame, RenderError> {
        self.render_catalog_scene_internal(catalog, scene_name, time_seconds, resolution, view)
    }

    fn render_scene_with_view(
        &mut self,
        scene: &SceneDescription,
        time_seconds: f32,
        resolution: RenderResolution,
        view: &RenderViewSettings,
    ) -> Result<RenderFrame, RenderError> {
        self.render_scene_internal(scene, time_seconds, resolution, view)
    }

    fn pick_catalog_scene_at(
        &mut self,
        catalog: &SceneCatalog,
        scene_name: &str,
        time_seconds: f32,
        resolution: RenderResolution,
        view: &RenderViewSettings,
        pixel_x: f32,
        pixel_y: f32,
    ) -> Result<Option<crate::PickingHit>, RenderError> {
        let scene = catalog
            .scene(scene_name)
            .ok_or_else(|| RenderError::MissingScene(scene_name.to_string()))?;
        self.pick_scene_at(scene, time_seconds, resolution, view, pixel_x, pixel_y)
    }

    fn pick_scene_at(
        &mut self,
        scene: &SceneDescription,
        time_seconds: f32,
        resolution: RenderResolution,
        view: &RenderViewSettings,
        pixel_x: f32,
        pixel_y: f32,
    ) -> Result<Option<crate::PickingHit>, RenderError> {
        self.ensure_target(resolution);
        let target = self.target.as_ref().expect("target was just initialized");
        let pick_texture = target.pick_texture.clone();
        let pick_view = target.pick_view.clone();
        let depth_view = target.depth_view.clone();
        let pick_readback_buffer = target.pick_readback_buffer.clone();

        let uniforms = build_scene_uniforms(scene, time_seconds, resolution, view);
        self.queue
            .write_buffer(&self.uniform_buffer, 0, bytemuck::bytes_of(&uniforms));

        let (pick_vertices, pick_targets) = build_pick_scene(scene, time_seconds, view)?;
        if pick_vertices.is_empty() || pick_targets.is_empty() {
            return Ok(None);
        }

        let pick_buffer = self
            .device
            .create_buffer_init(&wgpu::util::BufferInitDescriptor {
                label: Some("kain-3d-pick-vertex-buffer"),
                contents: bytemuck::cast_slice(&pick_vertices),
                usage: wgpu::BufferUsages::VERTEX,
            });

        let mut encoder = self
            .device
            .create_command_encoder(&wgpu::CommandEncoderDescriptor {
                label: Some("kain-3d-pick-encoder"),
            });

        {
            let mut render_pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("kain-3d-pick-pass"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &pick_view,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Clear(wgpu::Color::TRANSPARENT),
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                    view: &depth_view,
                    depth_ops: Some(wgpu::Operations {
                        load: wgpu::LoadOp::Clear(1.0),
                        store: wgpu::StoreOp::Store,
                    }),
                    stencil_ops: None,
                }),
                timestamp_writes: None,
                occlusion_query_set: None,
            });
            render_pass.set_bind_group(0, &self.uniform_bind_group, &[]);
            render_pass.set_pipeline(&self.pick_pipeline);
            render_pass.set_vertex_buffer(0, pick_buffer.slice(..));
            render_pass.draw(0..pick_vertices.len() as u32, 0..1);
        }

        let pick_origin = wgpu::Origin3d {
            x: pixel_x.clamp(0.0, (resolution.width.saturating_sub(1)) as f32) as u32,
            y: pixel_y.clamp(0.0, (resolution.height.saturating_sub(1)) as f32) as u32,
            z: 0,
        };

        encoder.copy_texture_to_buffer(
            wgpu::TexelCopyTextureInfo {
                texture: &pick_texture,
                mip_level: 0,
                origin: pick_origin,
                aspect: wgpu::TextureAspect::All,
            },
            wgpu::TexelCopyBufferInfo {
                buffer: &pick_readback_buffer,
                layout: wgpu::TexelCopyBufferLayout {
                    offset: 0,
                    bytes_per_row: Some(wgpu::COPY_BYTES_PER_ROW_ALIGNMENT),
                    rows_per_image: Some(1),
                },
            },
            wgpu::Extent3d {
                width: 1,
                height: 1,
                depth_or_array_layers: 1,
            },
        );

        self.queue.submit(Some(encoder.finish()));

        let mapped = download_pick_id(&self.device, &pick_readback_buffer)
            .map_err(|err| RenderError::BackendFailure(err.to_string()))?;
        if mapped == 0 {
            return Ok(None);
        }

        let target = pick_targets.get((mapped - 1) as usize).cloned();
        let Some(target) = target else {
            return Ok(None);
        };

        let default_camera_pose;
        let camera = if let Some(camera) = view.camera.as_ref() {
            camera
        } else {
            let aspect_ratio = (resolution.width as f32 / resolution.height as f32).max(0.1);
            default_camera_pose = scene.framed_camera_pose(time_seconds, aspect_ratio);
            &default_camera_pose
        };
        let ray = PickingRay::from_viewport_pixel(pixel_x, pixel_y, resolution, camera);
        Ok(CpuPickingService.pick_scene_instance_with_overrides(
            scene,
            &PickingQuery::new(ray, time_seconds),
            &target.instance_id,
            &view.instance_transform_overrides,
        ))
    }
}

#[derive(Clone, Debug)]
pub struct PreparedWgpuFrame {
    pub uniforms: SceneUniforms,
    pub scene_vertices: Vec<GpuVertex>,
    pub depth_particles: Vec<ParticleVertex>,
    pub overlay_particles: Vec<ParticleVertex>,
    pub gizmo_vertices: Vec<GizmoVertex>,
    pub stats: RenderStats,
}

pub fn prepare_wgpu_frame(
    scene: &SceneDescription,
    time_seconds: f32,
    resolution: RenderResolution,
    view: &RenderViewSettings,
) -> Result<PreparedWgpuFrame, RenderError> {
    let active_camera = resolve_camera_pose(scene, time_seconds, resolution, view);
    let uniforms = build_scene_uniforms(scene, time_seconds, resolution, view);
    let (scene_vertices, mut stats) = build_gpu_scene(scene, time_seconds, view)?;
    let (depth_particles, overlay_particles, particle_count) =
        build_particle_vertices(scene, time_seconds, &active_camera);
    stats.particles_submitted = particle_count;
    stats.particles_shaded = particle_count;
    let gizmo_vertices = build_gizmo_vertices(scene, time_seconds, view);
    Ok(PreparedWgpuFrame {
        uniforms,
        scene_vertices,
        depth_particles,
        overlay_particles,
        gizmo_vertices,
        stats,
    })
}

fn build_scene_uniforms(
    scene: &SceneDescription,
    time_seconds: f32,
    resolution: RenderResolution,
    view: &RenderViewSettings,
) -> SceneUniforms {
    let camera = resolve_camera_pose(scene, time_seconds, resolution, view);

    let aspect_ratio = resolution.width as f32 / resolution.height as f32;
    let view_matrix = Mat4::look_at(camera.position, camera.target, camera.up);
    let projection = Mat4::perspective(
        camera.fov_y_degrees.to_radians(),
        aspect_ratio,
        camera.near_plane,
        camera.far_plane,
    );
    let view_projection = projection.mul_mat4(view_matrix);

    let mut directional_directions = [[0.0; 4]; MAX_DIRECTIONAL_LIGHTS];
    let mut directional_colors = [[0.0; 4]; MAX_DIRECTIONAL_LIGHTS];
    for (index, light) in scene
        .lighting
        .directional_lights
        .iter()
        .take(MAX_DIRECTIONAL_LIGHTS)
        .enumerate()
    {
        directional_directions[index] = [
            light.direction.x,
            light.direction.y,
            light.direction.z,
            light.intensity,
        ];
        directional_colors[index] = [light.color.r, light.color.g, light.color.b, 0.0];
    }

    let mut point_positions_intensity = [[0.0; 4]; MAX_POINT_LIGHTS];
    let mut point_colors_range = [[0.0; 4]; MAX_POINT_LIGHTS];
    for (index, light) in scene
        .lighting
        .point_lights
        .iter()
        .take(MAX_POINT_LIGHTS)
        .enumerate()
    {
        point_positions_intensity[index] = [
            light.position.x,
            light.position.y,
            light.position.z,
            light.intensity,
        ];
        point_colors_range[index] = [light.color.r, light.color.g, light.color.b, light.range];
    }

    SceneUniforms {
        view_proj: transpose_mat4(view_projection),
        camera_position: [camera.position.x, camera.position.y, camera.position.z, 1.0],
        ambient_color_intensity: [
            scene.lighting.ambient_color.r,
            scene.lighting.ambient_color.g,
            scene.lighting.ambient_color.b,
            scene.lighting.ambient_intensity,
        ],
        directional_directions,
        directional_colors,
        point_positions_intensity,
        point_colors_range,
        counts: [
            scene
                .lighting
                .directional_lights
                .len()
                .min(MAX_DIRECTIONAL_LIGHTS) as u32,
            scene.lighting.point_lights.len().min(MAX_POINT_LIGHTS) as u32,
            0,
            0,
        ],
        background_top: [
            scene.background.top.r,
            scene.background.top.g,
            scene.background.top.b,
            1.0,
        ],
        background_bottom: [
            scene.background.bottom.r,
            scene.background.bottom.g,
            scene.background.bottom.b,
            1.0,
        ],
        fog_color_density: [
            scene.background.bottom.r,
            scene.background.bottom.g,
            scene.background.bottom.b,
            view.fog_density.unwrap_or(0.0).max(0.0),
        ],
    }
}

fn build_gpu_scene(
    scene: &SceneDescription,
    time_seconds: f32,
    view: &RenderViewSettings,
) -> Result<(Vec<GpuVertex>, RenderStats), RenderError> {
    let animated_instances =
        scene.animated_instances_with_overrides(time_seconds, &view.instance_transform_overrides);
    let mut vertices = Vec::new();
    let mut triangles_submitted = 0usize;

    for instance in animated_instances {
        let mesh = scene
            .resolved_mesh(&instance.mesh, time_seconds)
            .ok_or_else(|| RenderError::MissingMesh(instance.mesh.clone()))?;
        let mesh = mesh.as_ref();
        let material = scene
            .materials
            .get(&instance.material)
            .ok_or_else(|| RenderError::MissingMaterial(instance.material.clone()))?;

        triangles_submitted += mesh.triangles.len();
        append_mesh_vertices(&mut vertices, mesh, &instance.transform, material);
    }

    Ok((
        vertices,
        RenderStats {
            triangles_submitted,
            triangles_rasterized: triangles_submitted,
            pixels_shaded: 0,
            particles_submitted: 0,
            particles_shaded: 0,
        },
    ))
}

fn append_mesh_vertices(
    vertices: &mut Vec<GpuVertex>,
    mesh: &Mesh,
    transform: &crate::Transform,
    material: &crate::Material,
) {
    for triangle in &mesh.triangles {
        for index in triangle {
            let vertex = &mesh.vertices[*index];
            let world_position = transform.transform_point(vertex.position);
            let world_normal = transform.transform_vector(vertex.normal).normalize();
            vertices.push(GpuVertex {
                position_and_ambient: [
                    world_position.x,
                    world_position.y,
                    world_position.z,
                    material.ambient_strength,
                ],
                normal_and_diffuse: [
                    world_normal.x,
                    world_normal.y,
                    world_normal.z,
                    material.diffuse_strength,
                ],
                base_color_and_specular_strength: [
                    material.base_color.r,
                    material.base_color.g,
                    material.base_color.b,
                    material.specular_strength,
                ],
                specular_color_and_shininess: [
                    material.specular_color.r,
                    material.specular_color.g,
                    material.specular_color.b,
                    material.shininess,
                ],
            });
        }
    }
}

fn build_pick_scene(
    scene: &SceneDescription,
    time_seconds: f32,
    view: &RenderViewSettings,
) -> Result<(Vec<PickVertex>, Vec<PickTargetId>), RenderError> {
    let animated_instances =
        scene.animated_instances_with_overrides(time_seconds, &view.instance_transform_overrides);
    let mut vertices = Vec::new();
    let mut targets = Vec::new();

    for (instance_index, instance) in animated_instances.iter().enumerate() {
        let mesh = scene
            .resolved_mesh(&instance.mesh, time_seconds)
            .ok_or_else(|| RenderError::MissingMesh(instance.mesh.clone()))?;
        let mesh = mesh.as_ref();
        let object_id = (instance_index + 1) as u32;
        targets.push(PickTargetId {
            instance_id: instance.id.clone(),
            mesh_id: instance.mesh.clone(),
        });

        for triangle in &mesh.triangles {
            for index in triangle {
                let vertex = &mesh.vertices[*index];
                let world_position = instance.transform.transform_point(vertex.position);
                vertices.push(PickVertex {
                    world_position: [world_position.x, world_position.y, world_position.z, 1.0],
                    instance_id: object_id,
                    _padding: [0; 3],
                });
            }
        }
    }

    Ok((vertices, targets))
}

fn resolve_camera_pose(
    scene: &SceneDescription,
    time_seconds: f32,
    resolution: RenderResolution,
    view: &RenderViewSettings,
) -> CameraPose {
    if let Some(camera) = view.camera.as_ref() {
        camera.clone()
    } else {
        let aspect_ratio = (resolution.width as f32 / resolution.height as f32).max(0.1);
        scene.framed_camera_pose(time_seconds, aspect_ratio)
    }
}

fn build_particle_vertices(
    scene: &SceneDescription,
    time_seconds: f32,
    camera: &CameraPose,
) -> (Vec<ParticleVertex>, Vec<ParticleVertex>, usize) {
    let mut depth_vertices = Vec::new();
    let mut overlay_vertices = Vec::new();
    let mut particle_count = 0usize;
    let camera_forward = camera.forward();
    let camera_right = camera.right();
    let camera_up = camera_right.cross(camera_forward).normalize();

    for emitter in &scene.particle_emitters {
        let axis = emitter.axis_or_up();
        let (basis_u, basis_v) = orthonormal_basis(axis);
        for index in 0..emitter.particle_count {
            particle_count += 1;
            let sample = sample_particle(emitter, index, time_seconds, axis, basis_u, basis_v);
            let to_camera = (camera.position - sample.world_position)
                .length()
                .max(0.001);
            let world_radius = (sample.radius * (0.8 + sample.emissive_strength * 0.6))
                .clamp(0.04, 1.6)
                * (1.0 + 0.08 / to_camera);
            let target_vertices = if sample.depth_test {
                &mut depth_vertices
            } else {
                &mut overlay_vertices
            };
            append_particle_billboard(
                target_vertices,
                sample.world_position,
                camera_right,
                camera_up,
                world_radius,
                [
                    sample.color[0],
                    sample.color[1],
                    sample.color[2],
                    sample.emissive_strength,
                ],
            );
        }
    }

    (depth_vertices, overlay_vertices, particle_count)
}

fn append_particle_billboard(
    vertices: &mut Vec<ParticleVertex>,
    center: Vec3,
    camera_right: Vec3,
    camera_up: Vec3,
    radius: f32,
    color: [f32; 4],
) {
    let right = camera_right * radius;
    let up = camera_up * radius;
    let corners = [
        (center - right - up, [-1.0, -1.0]),
        (center + right - up, [1.0, -1.0]),
        (center + right + up, [1.0, 1.0]),
        (center - right + up, [-1.0, 1.0]),
    ];
    let indices = [0usize, 1, 2, 0, 2, 3];
    for index in indices {
        let (position, quad_uv) = corners[index];
        vertices.push(ParticleVertex {
            world_position: [position.x, position.y, position.z, 1.0],
            color,
            quad_uv,
            _padding: [0.0; 2],
        });
    }
}

fn sample_particle(
    emitter: &ParticleEmitter,
    index: usize,
    time_seconds: f32,
    axis: Vec3,
    basis_u: Vec3,
    basis_v: Vec3,
) -> ParticleSample {
    let seed = index as f32 + 1.0;
    let phase = hash01(seed * 0.73) * std::f32::consts::TAU;
    let radius = lerp(
        emitter.radial_range[0],
        emitter.radial_range[1],
        hash01(seed * 1.31),
    );
    let vertical_extent = lerp(
        emitter.vertical_range[0],
        emitter.vertical_range[1],
        hash01(seed * 2.17),
    );
    let size = lerp(
        emitter.particle_size_range[0],
        emitter.particle_size_range[1],
        hash01(seed * 2.81),
    );
    let color_mix = hash01(seed * 3.61);
    let angular_velocity = emitter.orbit_radians_per_second
        * (1.0 + emitter.swirl * (hash01(seed * 4.73) * 2.0 - 1.0));
    let angle = phase + time_seconds * angular_velocity;
    let vertical_wave =
        vertical_extent * (time_seconds * (0.55 + hash01(seed * 5.11)) + phase).sin();
    let drift_cycle = hash01(seed * 6.07)
        + time_seconds * (0.08 + emitter.orbit_radians_per_second.abs() * 0.025);
    let drift_t = fract01(drift_cycle) * 2.0 - 1.0;

    ParticleSample {
        world_position: emitter.center
            + basis_u * angle.cos() * radius
            + basis_v * angle.sin() * radius
            + axis * vertical_wave
            + emitter.drift * drift_t,
        color: [
            emitter.color_start.r * (1.0 - color_mix) + emitter.color_end.r * color_mix,
            emitter.color_start.g * (1.0 - color_mix) + emitter.color_end.g * color_mix,
            emitter.color_start.b * (1.0 - color_mix) + emitter.color_end.b * color_mix,
        ],
        radius: size,
        emissive_strength: emitter.emissive_strength.max(0.05),
        depth_test: emitter.depth_test,
    }
}

fn orthonormal_basis(axis: Vec3) -> (Vec3, Vec3) {
    let helper = if axis.y.abs() > 0.92 {
        Vec3::new(1.0, 0.0, 0.0)
    } else {
        Vec3::UP
    };
    let basis_u = axis.cross(helper).normalized_or(Vec3::new(1.0, 0.0, 0.0));
    let basis_v = axis.cross(basis_u).normalized_or(Vec3::UP);
    (basis_u, basis_v)
}

#[derive(Clone, Copy, Debug)]
struct ParticleSample {
    world_position: Vec3,
    color: [f32; 3],
    radius: f32,
    emissive_strength: f32,
    depth_test: bool,
}

fn hash01(seed: f32) -> f32 {
    fract01((seed * 12.9898).sin() * 43_758.547)
}

fn fract01(value: f32) -> f32 {
    value - value.floor()
}

fn lerp(start: f32, end: f32, t: f32) -> f32 {
    start + (end - start) * t
}

fn build_gizmo_vertices(
    scene: &SceneDescription,
    time_seconds: f32,
    view: &RenderViewSettings,
) -> Vec<GizmoVertex> {
    let Some(selected_instance_id) = view.selected_instance_id.as_deref() else {
        return Vec::new();
    };

    let animated_instances =
        scene.animated_instances_with_overrides(time_seconds, &view.instance_transform_overrides);
    let Some(instance) = animated_instances
        .iter()
        .find(|candidate| candidate.id == selected_instance_id)
    else {
        return Vec::new();
    };

    let origin = instance.transform.translation;
    let max_scale = instance
        .transform
        .scale
        .x
        .max(instance.transform.scale.y)
        .max(instance.transform.scale.z)
        .max(1.0);
    let axis_length = match view.manipulator_mode.unwrap_or(ManipulatorMode::Translate) {
        ManipulatorMode::Translate => 1.25 * max_scale,
        ManipulatorMode::Rotate => 1.55 * max_scale,
        ManipulatorMode::Scale => 1.05 * max_scale,
    };

    let mut vertices = Vec::new();
    append_gizmo_axis(
        &mut vertices,
        origin,
        origin + Vec3::new(axis_length, 0.0, 0.0),
        [1.0, 0.34, 0.34, 1.0],
    );
    append_gizmo_axis(
        &mut vertices,
        origin,
        origin + Vec3::new(0.0, axis_length, 0.0),
        [0.38, 0.95, 0.56, 1.0],
    );
    append_gizmo_axis(
        &mut vertices,
        origin,
        origin + Vec3::new(0.0, 0.0, axis_length),
        [0.32, 0.72, 1.0, 1.0],
    );

    vertices
}

fn append_gizmo_axis(vertices: &mut Vec<GizmoVertex>, start: Vec3, end: Vec3, color: [f32; 4]) {
    vertices.push(GizmoVertex {
        world_position: [start.x, start.y, start.z, 1.0],
        color,
    });
    vertices.push(GizmoVertex {
        world_position: [end.x, end.y, end.z, 1.0],
        color,
    });
}

fn create_target(device: &wgpu::Device, resolution: RenderResolution) -> WgpuFrameTarget {
    let size = wgpu::Extent3d {
        width: resolution.width as u32,
        height: resolution.height as u32,
        depth_or_array_layers: 1,
    };

    let color_texture = device.create_texture(&wgpu::TextureDescriptor {
        label: Some("kain-3d-color-target"),
        size,
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format: COLOR_FORMAT,
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC,
        view_formats: &[],
    });
    let color_view = color_texture.create_view(&wgpu::TextureViewDescriptor::default());

    let pick_texture = device.create_texture(&wgpu::TextureDescriptor {
        label: Some("kain-3d-pick-target"),
        size,
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format: PICK_FORMAT,
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC,
        view_formats: &[],
    });
    let pick_view = pick_texture.create_view(&wgpu::TextureViewDescriptor::default());

    let depth_texture = device.create_texture(&wgpu::TextureDescriptor {
        label: Some("kain-3d-depth-target"),
        size,
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format: DEPTH_FORMAT,
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
        view_formats: &[],
    });
    let depth_view = depth_texture.create_view(&wgpu::TextureViewDescriptor::default());

    let padded_bytes_per_row = aligned_bytes_per_row(resolution.width as u32 * 4);
    let readback_buffer = device.create_buffer(&wgpu::BufferDescriptor {
        label: Some("kain-3d-readback-buffer"),
        size: padded_bytes_per_row as u64 * resolution.height as u64,
        usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ,
        mapped_at_creation: false,
    });
    let pick_readback_buffer = device.create_buffer(&wgpu::BufferDescriptor {
        label: Some("kain-3d-pick-readback-buffer"),
        size: wgpu::COPY_BYTES_PER_ROW_ALIGNMENT as u64,
        usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ,
        mapped_at_creation: false,
    });

    WgpuFrameTarget {
        resolution,
        color_texture,
        color_view,
        pick_texture,
        pick_view,
        _depth_texture: depth_texture,
        depth_view,
        readback_buffer,
        pick_readback_buffer,
        padded_bytes_per_row,
    }
}

fn download_target_rgba(
    device: &wgpu::Device,
    buffer: &wgpu::Buffer,
    resolution: RenderResolution,
    padded_bytes_per_row: u32,
) -> Result<Vec<u8>, WgpuRendererInitError> {
    let slice = buffer.slice(..);
    let (tx, rx) = mpsc::channel();
    slice.map_async(wgpu::MapMode::Read, move |result| {
        let _ = tx.send(result);
    });
    let _ = device.poll(wgpu::Maintain::Wait);

    rx.recv()
        .map_err(|_| WgpuRendererInitError::new("failed to receive WGPU readback completion"))?
        .map_err(|err| {
            WgpuRendererInitError::new(format!("failed to map WGPU readback buffer: {err}"))
        })?;

    let mapped = slice.get_mapped_range();
    let unpadded_bytes_per_row = resolution.width * 4;
    let mut rgba = vec![0; resolution.width * resolution.height * 4];
    for row in 0..resolution.height {
        let source_start = row * padded_bytes_per_row as usize;
        let source_end = source_start + unpadded_bytes_per_row;
        let target_start = row * unpadded_bytes_per_row;
        let target_end = target_start + unpadded_bytes_per_row;
        rgba[target_start..target_end].copy_from_slice(&mapped[source_start..source_end]);
    }
    drop(mapped);
    buffer.unmap();

    Ok(rgba)
}

fn download_pick_id(
    device: &wgpu::Device,
    buffer: &wgpu::Buffer,
) -> Result<u32, WgpuRendererInitError> {
    let slice = buffer.slice(..);
    let (tx, rx) = mpsc::channel();
    slice.map_async(wgpu::MapMode::Read, move |result| {
        let _ = tx.send(result);
    });
    let _ = device.poll(wgpu::Maintain::Wait);

    rx.recv()
        .map_err(|_| WgpuRendererInitError::new("failed to receive WGPU pick readback completion"))?
        .map_err(|err| {
            WgpuRendererInitError::new(format!("failed to map WGPU pick buffer: {err}"))
        })?;

    let mapped = slice.get_mapped_range();
    let bytes = [mapped[0], mapped[1], mapped[2], mapped[3]];
    drop(mapped);
    buffer.unmap();
    Ok(u32::from_le_bytes(bytes))
}

fn aligned_bytes_per_row(unpadded_bytes_per_row: u32) -> u32 {
    let alignment = wgpu::COPY_BYTES_PER_ROW_ALIGNMENT;
    let remainder = unpadded_bytes_per_row % alignment;
    if remainder == 0 {
        unpadded_bytes_per_row
    } else {
        unpadded_bytes_per_row + (alignment - remainder)
    }
}

fn transpose_mat4(matrix: Mat4) -> [[f32; 4]; 4] {
    let mut output = [[0.0; 4]; 4];
    for row in 0..4 {
        for col in 0..4 {
            output[row][col] = matrix.m[col][row];
        }
    }
    output
}

#[cfg(test)]
mod tests {
    use super::aligned_bytes_per_row;

    #[test]
    fn aligns_readback_rows_to_wgpu_requirement() {
        assert_eq!(aligned_bytes_per_row(256), 256);
        assert_eq!(aligned_bytes_per_row(260), 512);
        assert_eq!(aligned_bytes_per_row(1020), 1024);
    }
}

fn build_visible_instance_ids(
    scene: &SceneDescription,
    time_seconds: f32,
    view: &RenderViewSettings,
) -> Vec<String> {
    scene
        .animated_instances_with_overrides(time_seconds, &view.instance_transform_overrides)
        .into_iter()
        .map(|instance| instance.id)
        .collect()
}
