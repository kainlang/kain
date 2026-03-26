use std::{
    path::{Path, PathBuf},
    time::Instant,
};

use eframe::egui;

use crate::host_documents::{Fast3dGameplayStateDocument, Fast3dShaderOverrideDocument};
use crate::model::CombineMode;
use crate::rasterizer::RenderFrame;
use crate::runtime::{Fast3dRuntime, FreeCameraPose, OrbitControls};

pub use crate::runtime::OrbitControls as OrbitControlsExport;

pub fn launch_viewer(
    manifest_path: PathBuf,
    runtime: Fast3dRuntime,
    gameplay_state: Option<Fast3dGameplayStateDocument>,
    shader_overrides: Option<Fast3dShaderOverrideDocument>,
) -> Result<(), Box<dyn std::error::Error>> {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([1280.0, 768.0])
            .with_min_inner_size([820.0, 620.0]),
        ..Default::default()
    };
    let title = runtime.manifest.title.clone();
    eframe::run_native(
        &title,
        options,
        Box::new(move |_creation_context| {
            Ok(Box::new(Fast3dViewerApp::new(
                manifest_path,
                runtime,
                gameplay_state,
                shader_overrides,
            )))
        }),
    )?;
    Ok(())
}

pub fn write_snapshot_png(
    path: &Path,
    frame: &RenderFrame,
) -> Result<(), Box<dyn std::error::Error>> {
    let file = std::fs::File::create(path)?;
    let mut encoder = png::Encoder::new(file, frame.width as u32, frame.height as u32);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    let mut writer = encoder.write_header()?;
    writer.write_image_data(&frame.rgba)?;
    Ok(())
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum CameraMode {
    Orbit,
    FreeFly,
}

/// Free-fly / first-person camera controller state.
/// `F` key in the viewer toggles between orbit and free-fly mode.
#[derive(Clone, Copy, Debug)]
pub struct FreeFlyControls {
    pub position: [f32; 3],
    pub yaw_radians: f32,
    pub pitch_radians: f32,
    /// Movement speed in world units per second.
    pub speed: f32,
}

impl Default for FreeFlyControls {
    fn default() -> Self {
        Self {
            position: [0.0, 20.0, 80.0],
            yaw_radians: std::f32::consts::PI,
            pitch_radians: -0.2,
            speed: 30.0,
        }
    }
}

struct Fast3dViewerApp {
    manifest_path: PathBuf,
    runtime: Fast3dRuntime,
    orbit_controls: OrbitControls,
    free_fly: FreeFlyControls,
    camera_mode: CameraMode,
    frame_texture: Option<egui::TextureHandle>,
    last_frame_started_at: Instant,
    elapsed_seconds: f32,
    auto_rotate: bool,
    combine_override: Option<CombineMode>,
    latest_frame: Option<RenderFrame>,
    gameplay_state: Option<Fast3dGameplayStateDocument>,
    shader_overrides: Option<Fast3dShaderOverrideDocument>,
    /// Accumulated mouse drag this frame for free-fly look.
    drag_delta: egui::Vec2,
}

impl Fast3dViewerApp {
    fn new(
        manifest_path: PathBuf,
        mut runtime: Fast3dRuntime,
        gameplay_state: Option<Fast3dGameplayStateDocument>,
        shader_overrides: Option<Fast3dShaderOverrideDocument>,
    ) -> Self {
        // Seed free-fly start position from the manifest camera target + pull-back
        let camera = &runtime.manifest.camera;
        let start_position = [
            camera.target[0],
            camera.target[1] + camera.orbit_height,
            camera.target[2] + camera.orbit_radius,
        ];
        if let Some(shader_overrides_document) = shader_overrides.as_ref() {
            runtime.apply_shader_overrides(shader_overrides_document);
        }
        Self {
            manifest_path,
            runtime,
            orbit_controls: OrbitControls::default(),
            free_fly: FreeFlyControls {
                position: start_position,
                ..FreeFlyControls::default()
            },
            camera_mode: CameraMode::Orbit,
            frame_texture: None,
            last_frame_started_at: Instant::now(),
            elapsed_seconds: 1.8,
            auto_rotate: true,
            combine_override: None,
            latest_frame: None,
            gameplay_state,
            shader_overrides,
            drag_delta: egui::Vec2::ZERO,
        }
    }

    fn cycle_combine_override(&mut self) {
        self.combine_override = match self.combine_override {
            None => Some(CombineMode::Texture),
            Some(CombineMode::Texture) => Some(CombineMode::TextureVertex),
            Some(CombineMode::TextureVertex) => Some(CombineMode::TexturePrimitive),
            Some(CombineMode::TexturePrimitive) => Some(CombineMode::TextureEnvMix),
            Some(CombineMode::TextureEnvMix) => Some(CombineMode::Primitive),
            Some(CombineMode::Primitive) => Some(CombineMode::Vertex),
            Some(CombineMode::Vertex) => Some(CombineMode::TextureVertexPrimitive),
            Some(CombineMode::TextureVertexPrimitive) => None,
        };
    }

    fn apply_keyboard_controls(&mut self, ctx: &egui::Context, delta_seconds: f32) {
        ctx.input(|input| {
            // ── Global keys ────────────────────────────────────────────────
            if input.key_pressed(egui::Key::Space) {
                self.auto_rotate = !self.auto_rotate;
            }
            if input.key_pressed(egui::Key::C) {
                self.cycle_combine_override();
            }
            if input.key_pressed(egui::Key::R) {
                self.orbit_controls = OrbitControls::default();
                self.free_fly = FreeFlyControls::default();
                self.elapsed_seconds = 0.0;
            }
            if input.key_pressed(egui::Key::F) {
                self.camera_mode = match self.camera_mode {
                    CameraMode::Orbit => CameraMode::FreeFly,
                    CameraMode::FreeFly => CameraMode::Orbit,
                };
            }

            // ── Mode-specific controls ──────────────────────────────────────
            match self.camera_mode {
                CameraMode::Orbit => {
                    if input.key_down(egui::Key::ArrowLeft) || input.key_down(egui::Key::A) {
                        self.orbit_controls.yaw_radians -= delta_seconds * 1.75;
                    }
                    if input.key_down(egui::Key::ArrowRight) || input.key_down(egui::Key::D) {
                        self.orbit_controls.yaw_radians += delta_seconds * 1.75;
                    }
                    if input.key_down(egui::Key::ArrowUp) || input.key_down(egui::Key::W) {
                        self.orbit_controls.pitch_radians = (self.orbit_controls.pitch_radians
                            + delta_seconds * 1.25)
                            .clamp(-0.9, 0.9);
                    }
                    if input.key_down(egui::Key::ArrowDown) || input.key_down(egui::Key::S) {
                        self.orbit_controls.pitch_radians = (self.orbit_controls.pitch_radians
                            - delta_seconds * 1.25)
                            .clamp(-0.9, 0.9);
                    }
                    let scroll_delta = input.raw_scroll_delta.y;
                    if scroll_delta.abs() > f32::EPSILON {
                        self.orbit_controls.zoom_delta =
                            (self.orbit_controls.zoom_delta - scroll_delta * 0.01).clamp(-2.8, 4.0);
                    }
                }
                CameraMode::FreeFly => {
                    let speed = self.free_fly.speed * delta_seconds;
                    let yaw = self.free_fly.yaw_radians;
                    // Horizontal movement vectors (ignore pitch for WASD strafe)
                    let (sin_yaw, cos_yaw) = (yaw.sin(), yaw.cos());
                    let fwd = [cos_yaw, 0.0_f32, sin_yaw];
                    let right = [sin_yaw, 0.0_f32, -cos_yaw];

                    let mv_fwd = input.key_down(egui::Key::W) || input.key_down(egui::Key::ArrowUp);
                    let mv_back = input.key_down(egui::Key::S) || input.key_down(egui::Key::ArrowDown);
                    let mv_left = input.key_down(egui::Key::A) || input.key_down(egui::Key::ArrowLeft);
                    let mv_right = input.key_down(egui::Key::D) || input.key_down(egui::Key::ArrowRight);
                    let mv_up = input.key_down(egui::Key::Q);
                    let mv_down = input.key_down(egui::Key::E);

                    let fs = if mv_fwd { 1.0 } else if mv_back { -1.0 } else { 0.0 };
                    let rs = if mv_right { 1.0 } else if mv_left { -1.0 } else { 0.0 };
                    let us = if mv_up { 1.0 } else if mv_down { -1.0 } else { 0.0 };

                    self.free_fly.position[0] += fwd[0] * fs * speed + right[0] * rs * speed;
                    self.free_fly.position[1] += us * speed;
                    self.free_fly.position[2] += fwd[2] * fs * speed + right[2] * rs * speed;

                    // Look with mouse drag
                    let drag = self.drag_delta;
                    self.free_fly.yaw_radians += drag.x * 0.004;
                    self.free_fly.pitch_radians =
                        (self.free_fly.pitch_radians - drag.y * 0.004).clamp(-1.4, 1.4);

                    // Scroll to adjust fly speed
                    let scroll = input.raw_scroll_delta.y;
                    if scroll.abs() > f32::EPSILON {
                        self.free_fly.speed = (self.free_fly.speed + scroll * 2.0).clamp(1.0, 500.0);
                    }
                }
            }
        });

        self.drag_delta = egui::Vec2::ZERO;
        if self.auto_rotate && self.camera_mode == CameraMode::Orbit {
            self.elapsed_seconds += delta_seconds;
        }
    }

    fn render_current_frame(&mut self) -> Result<RenderFrame, String> {
        if let Some(gameplay_state) = self.gameplay_state.as_ref() {
            self.runtime
                .apply_gameplay_state(self.elapsed_seconds, gameplay_state);
        } else {
            self.runtime.clear_actor_transforms();
        }
        if let Some(shader_overrides) = self.shader_overrides.as_ref() {
            self.runtime.apply_shader_overrides(shader_overrides);
        }
        match self.camera_mode {
            CameraMode::Orbit => {
                self.runtime
                    .render_frame(self.elapsed_seconds, &self.orbit_controls, self.combine_override)
            }
            CameraMode::FreeFly => {
                let camera = &self.runtime.manifest.camera;
                self.runtime.render_frame_with_pose(
                    &FreeCameraPose {
                        position: self.free_fly.position,
                        yaw_radians: self.free_fly.yaw_radians,
                        pitch_radians: self.free_fly.pitch_radians,
                        fov_y_degrees: camera.fov_y_degrees,
                        near_plane: camera.near_plane,
                        far_plane: camera.far_plane,
                    },
                    self.combine_override,
                )
            }
        }
    }

    fn update_frame_texture(&mut self, ctx: &egui::Context, frame: &RenderFrame) {
        let image =
            egui::ColorImage::from_rgba_unmultiplied([frame.width, frame.height], &frame.rgba);
        if let Some(texture) = self.frame_texture.as_mut() {
            texture.set(image, egui::TextureOptions::NEAREST);
        } else {
            self.frame_texture =
                Some(ctx.load_texture("sm64_fast3d_frame", image, egui::TextureOptions::NEAREST));
        }
    }
}

impl eframe::App for Fast3dViewerApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        let now = Instant::now();
        let delta_seconds = (now - self.last_frame_started_at).as_secs_f32().min(0.05);
        self.last_frame_started_at = now;

        // Capture mouse drag for free-fly look before keyboard processing
        ctx.input(|input| {
            if input.pointer.is_decidedly_dragging() {
                self.drag_delta = input.pointer.delta();
            }
        });

        self.apply_keyboard_controls(ctx, delta_seconds);

        match self.render_current_frame() {
            Ok(frame) => {
                self.update_frame_texture(ctx, &frame);
                self.latest_frame = Some(frame);
            }
            Err(error) => {
                egui::CentralPanel::default().show(ctx, |ui| {
                    ui.heading("Fast3D viewer failed");
                    ui.label(error);
                });
                ctx.request_repaint();
                return;
            }
        }

        egui::SidePanel::left("sm64_fast3d_controls")
            .default_width(260.0)
            .show(ctx, |ui| {
                ui.heading("Fast3D Smoke");
                ui.label(self.runtime.manifest.title.as_str());
                ui.monospace(self.manifest_path.display().to_string());
                ui.separator();

                // Camera mode indicator + controls hint
                match self.camera_mode {
                    CameraMode::Orbit => {
                        ui.label("🎥 Camera: Orbit  [F = FreeFly]");
                        ui.label("WASD / arrows: orbit");
                        ui.label("Mouse wheel: zoom");
                    }
                    CameraMode::FreeFly => {
                        ui.label("🚀 Camera: FreeFly  [F = Orbit]");
                        ui.label("WASD / arrows: move");
                        ui.label("Q/E: ascend / descend");
                        ui.label("Mouse drag: look");
                        ui.label("Scroll: adjust speed");
                        ui.label(format!("speed: {:.1} u/s", self.free_fly.speed));
                        ui.label(format!(
                            "pos: [{:.1}, {:.1}, {:.1}]",
                            self.free_fly.position[0],
                            self.free_fly.position[1],
                            self.free_fly.position[2],
                        ));
                    }
                }
                ui.separator();
                ui.label("Space: pause auto-rotation");
                ui.label("C: cycle combiner override");
                ui.label("R: reset camera and time");
                ui.separator();
                ui.label(format!("time: {:.2}s", self.elapsed_seconds));
                ui.label(format!("auto rotate: {}", self.auto_rotate));
                ui.label(format!(
                    "combine override: {}",
                    self.combine_override
                        .map(|mode| format!("{mode:?}"))
                        .unwrap_or_else(|| "manifest".to_string())
                ));
                if self.camera_mode == CameraMode::Orbit {
                    ui.label(format!("yaw: {:.2}", self.orbit_controls.yaw_radians));
                    ui.label(format!("pitch: {:.2}", self.orbit_controls.pitch_radians));
                    ui.label(format!("zoom delta: {:.2}", self.orbit_controls.zoom_delta));
                }
                ui.label(format!(
                    "gameplay bindings: {}",
                    self.gameplay_state
                        .as_ref()
                        .map(|document| document.actor_bindings.len())
                        .unwrap_or(0)
                ));
                ui.label(format!(
                    "material overrides: {}",
                    self.shader_overrides
                        .as_ref()
                        .map(|document| document.display_list_overrides.len())
                        .unwrap_or(0)
                ));
                if let Some(frame) = self.latest_frame.as_ref() {
                    ui.separator();
                    ui.label(format!("triangles submitted: {}", frame.stats.triangles_submitted));
                    ui.label(format!(
                        "triangles rasterized: {}",
                        frame.stats.triangles_rasterized
                    ));
                    ui.label(format!("shaded pixels: {}", frame.stats.shaded_pixels));
                }
            });

        egui::CentralPanel::default().show(ctx, |ui| {
            if let Some(texture) = self.frame_texture.as_ref() {
                let available = ui.available_size();
                let size = egui::Vec2::new(available.x.max(64.0), available.y.max(64.0));
                ui.image((texture.id(), size));
            }
        });

        ctx.request_repaint();
    }
}
