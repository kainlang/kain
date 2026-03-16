use std::{borrow::Cow, mem::size_of, sync::mpsc};

use bytemuck::{Pod, Zeroable};
use wgpu::util::DeviceExt;

use crate::renderer::{
    RenderBackend, RenderError, RenderFrame, RenderResolution, RenderStats, RenderViewSettings,
};
use crate::{Mat4, Mesh, SceneCatalog, SceneDescription};

const MAX_DIRECTIONAL_LIGHTS: usize = 2;
const MAX_POINT_LIGHTS: usize = 4;
const COLOR_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8UnormSrgb;
const DEPTH_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Depth32Float;

const WGPU_VIEWPORT_SHADER: &str = r#"
struct SceneUniforms {
    view_proj: mat4x4<f32>,
    camera_position: vec4<f32>,
    ambient_color_intensity: vec4<f32>,
    directional_directions: array<vec4<f32>, 2>,
    directional_colors: array<vec4<f32>, 2>,
    point_positions_intensity: array<vec4<f32>, 4>,
    point_colors_range: array<vec4<f32>, 4>,
    counts: vec4<u32>,
    background_top: vec4<f32>,
    background_bottom: vec4<f32>,
};

@group(0) @binding(0)
var<uniform> scene: SceneUniforms;

struct SceneVertexInput {
    @location(0) position_and_ambient: vec4<f32>,
    @location(1) normal_and_diffuse: vec4<f32>,
    @location(2) base_color_and_specular_strength: vec4<f32>,
    @location(3) specular_color_and_shininess: vec4<f32>,
};

struct SceneVertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_position: vec3<f32>,
    @location(1) world_normal: vec3<f32>,
    @location(2) base_color: vec3<f32>,
    @location(3) ambient_strength: f32,
    @location(4) diffuse_strength: f32,
    @location(5) specular_color: vec3<f32>,
    @location(6) specular_strength: f32,
    @location(7) shininess: f32,
};

struct BackgroundVertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) gradient_t: f32,
};

@vertex
fn scene_vs_main(input: SceneVertexInput) -> SceneVertexOutput {
    var output: SceneVertexOutput;
    let world_position = vec4<f32>(input.position_and_ambient.xyz, 1.0);
    output.clip_position = scene.view_proj * world_position;
    output.world_position = input.position_and_ambient.xyz;
    output.world_normal = normalize(input.normal_and_diffuse.xyz);
    output.base_color = input.base_color_and_specular_strength.xyz;
    output.ambient_strength = input.position_and_ambient.w;
    output.diffuse_strength = input.normal_and_diffuse.w;
    output.specular_color = input.specular_color_and_shininess.xyz;
    output.specular_strength = input.base_color_and_specular_strength.w;
    output.shininess = input.specular_color_and_shininess.w;
    return output;
}

@vertex
fn background_vs_main(@builtin(vertex_index) vertex_index: u32) -> BackgroundVertexOutput {
    var positions = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -3.0),
        vec2<f32>(3.0, 1.0),
        vec2<f32>(-1.0, 1.0),
    );

    let position = positions[vertex_index];
    var output: BackgroundVertexOutput;
    output.clip_position = vec4<f32>(position, 0.0, 1.0);
    output.gradient_t = clamp(1.0 - ((position.y * 0.5) + 0.5), 0.0, 1.0);
    return output;
}

fn directional_light(
    light_direction: vec3<f32>,
    light_color: vec3<f32>,
    intensity: f32,
    normal: vec3<f32>,
    view_direction: vec3<f32>,
    base_color: vec3<f32>,
    diffuse_strength: f32,
    specular_color: vec3<f32>,
    specular_strength: f32,
    shininess: f32,
) -> vec3<f32> {
    let to_light = normalize(-light_direction);
    let diffuse = max(dot(normal, to_light), 0.0) * diffuse_strength;
    let halfway = normalize(to_light + view_direction);
    let specular = pow(max(dot(normal, halfway), 0.0), max(shininess, 1.0)) * specular_strength;
    return ((base_color * diffuse) + (specular_color * specular)) * light_color * intensity;
}

fn point_light(
    light_position: vec3<f32>,
    light_color: vec3<f32>,
    intensity: f32,
    range: f32,
    world_position: vec3<f32>,
    normal: vec3<f32>,
    view_direction: vec3<f32>,
    base_color: vec3<f32>,
    diffuse_strength: f32,
    specular_color: vec3<f32>,
    specular_strength: f32,
    shininess: f32,
) -> vec3<f32> {
    let to_light = light_position - world_position;
    let distance = max(length(to_light), 0.0001);
    let direction = to_light / distance;
    let attenuation = pow(max(1.0 - (distance / max(range, 0.001)), 0.0), 2.0);
    let diffuse = max(dot(normal, direction), 0.0) * diffuse_strength;
    let reflected = normalize((normal * (2.0 * dot(normal, direction))) - direction);
    let specular = pow(max(dot(reflected, view_direction), 0.0), max(shininess, 1.0)) * specular_strength;
    return ((base_color * diffuse) + (specular_color * specular)) * light_color * intensity * attenuation;
}

@fragment
fn scene_fs_main(input: SceneVertexOutput) -> @location(0) vec4<f32> {
    let normal = normalize(input.world_normal);
    let view_direction = normalize(scene.camera_position.xyz - input.world_position);
    var color =
        scene.ambient_color_intensity.xyz *
        scene.ambient_color_intensity.w *
        input.base_color *
        input.ambient_strength;

    for (var index: u32 = 0u; index < scene.counts.x; index = index + 1u) {
        let light_direction = scene.directional_directions[index];
        let light_color = scene.directional_colors[index];
        color += directional_light(
            light_direction.xyz,
            light_color.xyz,
            light_direction.w,
            normal,
            view_direction,
            input.base_color,
            input.diffuse_strength,
            input.specular_color,
            input.specular_strength,
            input.shininess,
        );
    }

    for (var index: u32 = 0u; index < scene.counts.y; index = index + 1u) {
        let light_position = scene.point_positions_intensity[index];
        let light_color = scene.point_colors_range[index];
        color += point_light(
            light_position.xyz,
            light_color.xyz,
            light_position.w,
            light_color.w,
            input.world_position,
            normal,
            view_direction,
            input.base_color,
            input.diffuse_strength,
            input.specular_color,
            input.specular_strength,
            input.shininess,
        );
    }

    return vec4<f32>(clamp(color, vec3<f32>(0.0), vec3<f32>(1.0)), 1.0);
}

@fragment
fn background_fs_main(input: BackgroundVertexOutput) -> @location(0) vec4<f32> {
    let t = clamp(input.gradient_t, 0.0, 1.0);
    let color = scene.background_top.xyz * (1.0 - t) + scene.background_bottom.xyz * t;
    return vec4<f32>(color, 1.0);
}
"#;

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
    uniform_buffer: wgpu::Buffer,
    uniform_bind_group: wgpu::BindGroup,
    background_pipeline: wgpu::RenderPipeline,
    scene_pipeline: wgpu::RenderPipeline,
    target: Option<WgpuFrameTarget>,
}

struct WgpuFrameTarget {
    resolution: RenderResolution,
    color_texture: wgpu::Texture,
    color_view: wgpu::TextureView,
    _depth_texture: wgpu::Texture,
    depth_view: wgpu::TextureView,
    readback_buffer: wgpu::Buffer,
    padded_bytes_per_row: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
struct GpuVertex {
    position_and_ambient: [f32; 4],
    normal_and_diffuse: [f32; 4],
    base_color_and_specular_strength: [f32; 4],
    specular_color_and_shininess: [f32; 4],
}

impl GpuVertex {
    const ATTRIBUTES: [wgpu::VertexAttribute; 4] = wgpu::vertex_attr_array![
        0 => Float32x4,
        1 => Float32x4,
        2 => Float32x4,
        3 => Float32x4
    ];

    fn layout() -> wgpu::VertexBufferLayout<'static> {
        wgpu::VertexBufferLayout {
            array_stride: size_of::<Self>() as wgpu::BufferAddress,
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: &Self::ATTRIBUTES,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
struct SceneUniforms {
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
}

impl WgpuRenderer {
    pub fn new() -> Result<Self, WgpuRendererInitError> {
        pollster::block_on(Self::new_async())
    }

    async fn new_async() -> Result<Self, WgpuRendererInitError> {
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
            .map_err(|err| WgpuRendererInitError::new(format!("failed to create WGPU device: {err}")))?;

        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("kain-3d-wgpu-shader"),
            source: wgpu::ShaderSource::Wgsl(Cow::Borrowed(WGPU_VIEWPORT_SHADER)),
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
            depth_stencil: None,
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

        Ok(Self {
            device,
            queue,
            uniform_buffer,
            uniform_bind_group,
            background_pipeline,
            scene_pipeline,
            target: None,
        })
    }

    fn render_catalog_scene_internal(
        &mut self,
        catalog: &SceneCatalog,
        scene_name: &str,
        time_seconds: f32,
        resolution: RenderResolution,
        view: &RenderViewSettings,
    ) -> Result<RenderFrame, RenderError> {
        let scene = catalog
            .scene(scene_name)
            .ok_or_else(|| RenderError::MissingScene(scene_name.to_string()))?;
        self.render_scene_internal(scene, time_seconds, resolution, view)
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
        let uniforms = build_scene_uniforms(scene, time_seconds, resolution, view);
        self.queue
            .write_buffer(&self.uniform_buffer, 0, bytemuck::bytes_of(&uniforms));

        let (vertices, mut stats) = build_gpu_scene(scene, time_seconds)?;
        let vertex_buffer = if vertices.is_empty() {
            None
        } else {
            Some(
                self.device
                    .create_buffer_init(&wgpu::util::BufferInitDescriptor {
                        label: Some("kain-3d-scene-vertex-buffer"),
                        contents: bytemuck::cast_slice(&vertices),
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
                render_pass.draw(0..vertices.len() as u32, 0..1);
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
        stats.pixels_shaded = resolution.width * resolution.height;

        Ok(RenderFrame {
            width: resolution.width,
            height: resolution.height,
            rgba,
            stats,
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
}

fn build_scene_uniforms(
    scene: &SceneDescription,
    time_seconds: f32,
    resolution: RenderResolution,
    view: &RenderViewSettings,
) -> SceneUniforms {
    let default_camera_pose;
    let camera = if let Some(camera) = view.camera.as_ref() {
        camera
    } else {
        default_camera_pose = scene.camera.pose_at(time_seconds);
        &default_camera_pose
    };

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
    }
}

fn build_gpu_scene(
    scene: &SceneDescription,
    time_seconds: f32,
) -> Result<(Vec<GpuVertex>, RenderStats), RenderError> {
    let animated_instances = scene.animated_instances(time_seconds);
    let mut vertices = Vec::new();
    let mut triangles_submitted = 0usize;

    for instance in animated_instances {
        let mesh = scene
            .meshes
            .get(&instance.mesh)
            .ok_or_else(|| RenderError::MissingMesh(instance.mesh.clone()))?;
        let material = scene
            .materials
            .get(&instance.material)
            .ok_or_else(|| RenderError::MissingMaterial(instance.material.clone()))?;

        triangles_submitted += mesh.triangles.len();
        append_mesh_vertices(&mut vertices, mesh, &instance.transform, material);
    }

    let particle_count = scene
        .particle_emitters
        .iter()
        .map(|emitter| emitter.particle_count)
        .sum();

    Ok((
        vertices,
        RenderStats {
            triangles_submitted,
            triangles_rasterized: triangles_submitted,
            pixels_shaded: 0,
            particles_submitted: particle_count,
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

    WgpuFrameTarget {
        resolution,
        color_texture,
        color_view,
        _depth_texture: depth_texture,
        depth_view,
        readback_buffer,
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
        .map_err(|err| WgpuRendererInitError::new(format!("failed to map WGPU readback buffer: {err}")))?;

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
