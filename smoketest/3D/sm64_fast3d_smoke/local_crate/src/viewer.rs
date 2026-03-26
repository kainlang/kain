use std::{
    path::{Path, PathBuf},
    time::Instant,
};

use eframe::egui;

use crate::model::CombineMode;
use crate::rasterizer::RenderFrame;
pub use crate::runtime::OrbitControls;
use crate::runtime::Fast3dRuntime;

pub fn launch_viewer(
    manifest_path: PathBuf,
    runtime: Fast3dRuntime,
) -> Result<(), Box<dyn std::error::Error>> {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([1024.0, 768.0])
            .with_min_inner_size([820.0, 620.0]),
        ..Default::default()
    };
    let title = runtime.manifest.title.clone();
    eframe::run_native(
        &title,
        options,
        Box::new(move |_creation_context| {
            Ok(Box::new(Fast3dViewerApp::new(manifest_path, runtime)))
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

struct Fast3dViewerApp {
    manifest_path: PathBuf,
    runtime: Fast3dRuntime,
    orbit_controls: OrbitControls,
    frame_texture: Option<egui::TextureHandle>,
    last_frame_started_at: Instant,
    elapsed_seconds: f32,
    auto_rotate: bool,
    combine_override: Option<CombineMode>,
    latest_frame: Option<RenderFrame>,
}

impl Fast3dViewerApp {
    fn new(manifest_path: PathBuf, runtime: Fast3dRuntime) -> Self {
        Self {
            manifest_path,
            runtime,
            orbit_controls: OrbitControls::default(),
            frame_texture: None,
            last_frame_started_at: Instant::now(),
            elapsed_seconds: 1.8,
            auto_rotate: true,
            combine_override: None,
            latest_frame: None,
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
            if input.key_pressed(egui::Key::Space) {
                self.auto_rotate = !self.auto_rotate;
            }
            if input.key_pressed(egui::Key::C) {
                self.cycle_combine_override();
            }
            if input.key_pressed(egui::Key::R) {
                self.orbit_controls = OrbitControls::default();
                self.elapsed_seconds = 0.0;
            }
            if input.key_down(egui::Key::ArrowLeft) || input.key_down(egui::Key::A) {
                self.orbit_controls.yaw_radians -= delta_seconds * 1.75;
            }
            if input.key_down(egui::Key::ArrowRight) || input.key_down(egui::Key::D) {
                self.orbit_controls.yaw_radians += delta_seconds * 1.75;
            }
            if input.key_down(egui::Key::ArrowUp) || input.key_down(egui::Key::W) {
                self.orbit_controls.pitch_radians =
                    (self.orbit_controls.pitch_radians + delta_seconds * 1.25).clamp(-0.9, 0.9);
            }
            if input.key_down(egui::Key::ArrowDown) || input.key_down(egui::Key::S) {
                self.orbit_controls.pitch_radians =
                    (self.orbit_controls.pitch_radians - delta_seconds * 1.25).clamp(-0.9, 0.9);
            }
            let scroll_delta = input.raw_scroll_delta.y;
            if scroll_delta.abs() > f32::EPSILON {
                self.orbit_controls.zoom_delta =
                    (self.orbit_controls.zoom_delta - scroll_delta * 0.01).clamp(-2.8, 4.0);
            }
        });
        if self.auto_rotate {
            self.elapsed_seconds += delta_seconds;
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
        self.apply_keyboard_controls(ctx, delta_seconds);

        match self
            .runtime
            .render_frame(self.elapsed_seconds, &self.orbit_controls, self.combine_override)
        {
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
                ui.label("Controls");
                ui.label("WASD / arrows: orbit camera");
                ui.label("Mouse wheel: zoom");
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
                ui.label(format!("yaw: {:.2}", self.orbit_controls.yaw_radians));
                ui.label(format!("pitch: {:.2}", self.orbit_controls.pitch_radians));
                ui.label(format!("zoom delta: {:.2}", self.orbit_controls.zoom_delta));
                if let Some(frame) = self.latest_frame.as_ref() {
                    ui.separator();
                    ui.label(format!(
                        "triangles submitted: {}",
                        frame.stats.triangles_submitted
                    ));
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
