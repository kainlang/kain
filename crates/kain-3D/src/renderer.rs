use crate::{BlackHole, CameraPose, ColorRgb, Mat4, ParticleEmitter, SceneCatalog, SceneDescription, Vec3};
use crate::{DirectionalLight, LightingRig, Material, PointLight};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RenderResolution {
    pub width: usize,
    pub height: usize,
}

impl RenderResolution {
    pub fn new(width: usize, height: usize) -> Self {
        Self {
            width: width.max(1),
            height: height.max(1),
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SoftwareRendererConfig {
    pub wireframe_overlay: bool,
    pub rim_light_strength: f32,
}

impl Default for SoftwareRendererConfig {
    fn default() -> Self {
        Self {
            wireframe_overlay: false,
            rim_light_strength: 0.18,
        }
    }
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct RenderViewSettings {
    pub camera: Option<CameraPose>,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct RenderStats {
    pub triangles_submitted: usize,
    pub triangles_rasterized: usize,
    pub pixels_shaded: usize,
    pub particles_submitted: usize,
    pub particles_shaded: usize,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct RenderFrame {
    pub width: usize,
    pub height: usize,
    pub rgba: Vec<u8>,
    pub stats: RenderStats,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum RenderError {
    MissingMesh(String),
    MissingMaterial(String),
    MissingScene(String),
}

impl std::fmt::Display for RenderError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::MissingMesh(name) => write!(f, "scene references missing mesh `{name}`"),
            Self::MissingMaterial(name) => write!(f, "scene references missing material `{name}`"),
            Self::MissingScene(name) => write!(f, "scene `{name}` was not found in the catalog"),
        }
    }
}

impl std::error::Error for RenderError {}

#[derive(Clone, Debug)]
pub struct SoftwareRenderer {
    pub config: SoftwareRendererConfig,
    scratch_rgba: Vec<u8>,
    scratch_depth: Vec<f32>,
}

impl Default for SoftwareRenderer {
    fn default() -> Self {
        Self {
            config: SoftwareRendererConfig::default(),
            scratch_rgba: Vec::new(),
            scratch_depth: Vec::new(),
        }
    }
}

impl SoftwareRenderer {
    pub fn render_catalog_scene(
        &mut self,
        catalog: &SceneCatalog,
        scene_name: &str,
        time_seconds: f32,
        resolution: RenderResolution,
    ) -> Result<RenderFrame, RenderError> {
        self.render_catalog_scene_with_view(
            catalog,
            scene_name,
            time_seconds,
            resolution,
            &RenderViewSettings::default(),
        )
    }

    pub fn render_catalog_scene_with_view(
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
        self.render_scene_with_view(scene, time_seconds, resolution, view)
    }

    pub fn render_scene(
        &mut self,
        scene: &SceneDescription,
        time_seconds: f32,
        resolution: RenderResolution,
    ) -> Result<RenderFrame, RenderError> {
        self.render_scene_with_view(scene, time_seconds, resolution, &RenderViewSettings::default())
    }

    pub fn render_scene_with_view(
        &mut self,
        scene: &SceneDescription,
        time_seconds: f32,
        resolution: RenderResolution,
        view: &RenderViewSettings,
    ) -> Result<RenderFrame, RenderError> {
        self.ensure_framebuffers(resolution);

        let config = self.config;
        let rgba = &mut self.scratch_rgba;
        let depth = &mut self.scratch_depth;
        let mut stats = RenderStats::default();

        fill_background(rgba, resolution, scene.background.top, scene.background.bottom);
        depth.fill(f32::INFINITY);

        let aspect_ratio = resolution.width as f32 / resolution.height as f32;
        let default_camera_pose;
        let camera = if let Some(camera) = view.camera.as_ref() {
            camera
        } else {
            default_camera_pose = scene.camera.pose_at(time_seconds);
            &default_camera_pose
        };
        let view_matrix = Mat4::look_at(camera.position, camera.target, camera.up);
        let projection = Mat4::perspective(
            camera.fov_y_degrees.to_radians(),
            aspect_ratio,
            camera.near_plane,
            camera.far_plane,
        );
        let view_projection = projection.mul_mat4(view_matrix);

        for instance in scene.animated_instances(time_seconds) {
            let mesh = scene
                .meshes
                .get(&instance.mesh)
                .ok_or_else(|| RenderError::MissingMesh(instance.mesh.clone()))?;
            let material = scene
                .materials
                .get(&instance.material)
                .ok_or_else(|| RenderError::MissingMaterial(instance.material.clone()))?;

            let model = instance.transform.matrix();
            let normal_matrix = Mat4::rotation_xyz(instance.transform.rotation_radians);

            for triangle in &mesh.triangles {
                stats.triangles_submitted += 1;

                let transformed = triangle.map(|index| {
                    let vertex = &mesh.vertices[index];
                    let world_position = to_vec3(model.transform_point(vertex.position));
                    let world_normal = normal_matrix.transform_vector(vertex.normal).normalize();
                    let clip_position = view_projection.transform_point(world_position);

                    RasterVertex {
                        world_position,
                        world_normal,
                        clip_position,
                    }
                });

                if transformed.iter().any(|vertex| vertex.clip_position[3] <= 0.001) {
                    continue;
                }

                let screen = transformed.map(|vertex| project_vertex(vertex.clip_position, resolution));

                if is_backface(screen[0], screen[1], screen[2]) {
                    continue;
                }

                stats.triangles_rasterized += 1;
                rasterize_triangle(
                    rgba,
                    depth,
                    resolution,
                    &mut stats,
                    material,
                    &scene.lighting,
                    camera.position,
                    screen,
                    transformed,
                    config,
                );
            }
        }

        render_particles(
            rgba,
            depth,
            resolution,
            &mut stats,
            &scene.particle_emitters,
            time_seconds,
            camera,
            view_projection,
        );
        if let Some(black_hole) = scene.black_hole.as_ref() {
            render_black_hole_overlay(rgba, resolution, camera, black_hole, view_projection);
        }

        Ok(RenderFrame {
            width: resolution.width,
            height: resolution.height,
            rgba: rgba.clone(),
            stats,
        })
    }

    fn ensure_framebuffers(&mut self, resolution: RenderResolution) {
        let pixel_count = resolution.width * resolution.height;
        let rgba_len = pixel_count * 4;
        if self.scratch_rgba.len() != rgba_len {
            self.scratch_rgba.resize(rgba_len, 0);
        }
        if self.scratch_depth.len() != pixel_count {
            self.scratch_depth.resize(pixel_count, f32::INFINITY);
        }
    }
}

#[derive(Clone, Copy, Debug)]
struct RasterVertex {
    world_position: Vec3,
    world_normal: Vec3,
    clip_position: [f32; 4],
}

#[derive(Clone, Copy, Debug)]
struct ScreenVertex {
    x: f32,
    y: f32,
    depth: f32,
}

#[derive(Clone, Copy, Debug)]
struct ParticleSample {
    world_position: Vec3,
    color: ColorRgb,
    radius: f32,
    softness: f32,
    emissive_strength: f32,
    depth_test: bool,
}

fn fill_background(rgba: &mut [u8], resolution: RenderResolution, top: ColorRgb, bottom: ColorRgb) {
    for y in 0..resolution.height {
        let t = if resolution.height <= 1 {
            0.0
        } else {
            y as f32 / (resolution.height - 1) as f32
        };
        let color = top * (1.0 - t) + bottom * t;
        let pixel = color.to_rgba8();
        for x in 0..resolution.width {
            let index = (y * resolution.width + x) * 4;
            rgba[index..index + 4].copy_from_slice(&pixel);
        }
    }
}

fn project_vertex(clip: [f32; 4], resolution: RenderResolution) -> ScreenVertex {
    let inv_w = 1.0 / clip[3];
    let ndc_x = clip[0] * inv_w;
    let ndc_y = clip[1] * inv_w;
    let ndc_z = clip[2] * inv_w;

    ScreenVertex {
        x: (ndc_x * 0.5 + 0.5) * (resolution.width as f32 - 1.0),
        y: (1.0 - (ndc_y * 0.5 + 0.5)) * (resolution.height as f32 - 1.0),
        depth: ndc_z * 0.5 + 0.5,
    }
}

fn is_backface(a: ScreenVertex, b: ScreenVertex, c: ScreenVertex) -> bool {
    let signed_area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    signed_area <= 0.0
}

#[allow(clippy::too_many_arguments)]
fn rasterize_triangle(
    rgba: &mut [u8],
    depth: &mut [f32],
    resolution: RenderResolution,
    stats: &mut RenderStats,
    material: &Material,
    lighting: &LightingRig,
    camera_position: Vec3,
    screen: [ScreenVertex; 3],
    transformed: [RasterVertex; 3],
    config: SoftwareRendererConfig,
) {
    let min_x = screen
        .iter()
        .map(|vertex| vertex.x)
        .fold(f32::INFINITY, f32::min)
        .floor()
        .clamp(0.0, resolution.width as f32 - 1.0) as usize;
    let max_x = screen
        .iter()
        .map(|vertex| vertex.x)
        .fold(f32::NEG_INFINITY, f32::max)
        .ceil()
        .clamp(0.0, resolution.width as f32 - 1.0) as usize;
    let min_y = screen
        .iter()
        .map(|vertex| vertex.y)
        .fold(f32::INFINITY, f32::min)
        .floor()
        .clamp(0.0, resolution.height as f32 - 1.0) as usize;
    let max_y = screen
        .iter()
        .map(|vertex| vertex.y)
        .fold(f32::NEG_INFINITY, f32::max)
        .ceil()
        .clamp(0.0, resolution.height as f32 - 1.0) as usize;

    let denominator = edge_function(screen[0], screen[1], screen[2].x, screen[2].y);
    if denominator.abs() <= f32::EPSILON {
        return;
    }

    for y in min_y..=max_y {
        for x in min_x..=max_x {
            let sample_x = x as f32 + 0.5;
            let sample_y = y as f32 + 0.5;

            let w0 = edge_function(screen[1], screen[2], sample_x, sample_y) / denominator;
            let w1 = edge_function(screen[2], screen[0], sample_x, sample_y) / denominator;
            let w2 = 1.0 - w0 - w1;

            if w0 < 0.0 || w1 < 0.0 || w2 < 0.0 {
                continue;
            }

            let pixel_index = y * resolution.width + x;
            let depth_value = screen[0].depth * w0 + screen[1].depth * w1 + screen[2].depth * w2;
            if !(0.0..=1.0).contains(&depth_value) || depth_value >= depth[pixel_index] {
                continue;
            }

            depth[pixel_index] = depth_value;
            stats.pixels_shaded += 1;

            let world_position = transformed[0].world_position * w0
                + transformed[1].world_position * w1
                + transformed[2].world_position * w2;
            let world_normal = (transformed[0].world_normal * w0
                + transformed[1].world_normal * w1
                + transformed[2].world_normal * w2)
                .normalize();

            let shaded = shade_pixel(
                material,
                lighting,
                world_position,
                world_normal,
                camera_position,
                config,
            );
            let pixel = shaded.to_rgba8();
            let rgba_index = pixel_index * 4;
            rgba[rgba_index..rgba_index + 4].copy_from_slice(&pixel);
        }
    }

    if config.wireframe_overlay {
        draw_line(rgba, resolution, screen[0], screen[1], [76, 214, 255, 255]);
        draw_line(rgba, resolution, screen[1], screen[2], [76, 214, 255, 255]);
        draw_line(rgba, resolution, screen[2], screen[0], [76, 214, 255, 255]);
    }
}

fn render_particles(
    rgba: &mut [u8],
    depth: &mut [f32],
    resolution: RenderResolution,
    stats: &mut RenderStats,
    emitters: &[ParticleEmitter],
    time_seconds: f32,
    camera: &CameraPose,
    view_projection: Mat4,
) {
    for emitter in emitters {
        let axis = emitter.axis_or_up();
        let (basis_u, basis_v) = orthonormal_basis(axis);
        for index in 0..emitter.particle_count {
            stats.particles_submitted += 1;
            let sample = sample_particle(emitter, index, time_seconds, axis, basis_u, basis_v);
            let clip_position = view_projection.transform_point(sample.world_position);
            if clip_position[3] <= 0.001 {
                continue;
            }

            let screen = project_vertex(clip_position, resolution);
            if !(0.0..=1.0).contains(&screen.depth) {
                continue;
            }

            draw_particle(
                rgba,
                depth,
                resolution,
                stats,
                camera,
                screen,
                clip_position[3],
                sample,
            );
        }
    }
}

fn render_black_hole_overlay(
    rgba: &mut [u8],
    resolution: RenderResolution,
    camera: &CameraPose,
    black_hole: &BlackHole,
    view_projection: Mat4,
) {
    let clip_position = view_projection.transform_point(black_hole.center);
    if clip_position[3] <= 0.001 {
        return;
    }
    let screen = project_vertex(clip_position, resolution);
    let distance = (black_hole.center - camera.position).length().max(0.001);
    let horizon_radius = (black_hole.radius * resolution.height as f32 / distance * 0.34).clamp(6.0, 96.0);
    let lens_radius = (black_hole.lens_radius * resolution.height as f32 / distance * 0.34)
        .clamp(horizon_radius + 4.0, 132.0);

    draw_glow_ring(
        rgba,
        resolution,
        screen.x,
        screen.y,
        lens_radius,
        black_hole.lens_color,
        0.32,
    );
    draw_glow_ring(
        rgba,
        resolution,
        screen.x,
        screen.y,
        lens_radius * 0.82,
        black_hole.disk_color,
        0.18,
    );
    draw_solid_disc(
        rgba,
        resolution,
        screen.x,
        screen.y,
        horizon_radius,
        black_hole.inner_color,
    );
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
    let color = emitter.color_start * (1.0 - color_mix) + emitter.color_end * color_mix;
    let angular_velocity = emitter.orbit_radians_per_second
        * (1.0 + emitter.swirl * (hash01(seed * 4.73) * 2.0 - 1.0));
    let angle = phase + time_seconds * angular_velocity;
    let vertical_wave = vertical_extent * (time_seconds * (0.55 + hash01(seed * 5.11)) + phase).sin();
    let drift_cycle = hash01(seed * 6.07) + time_seconds * (0.08 + emitter.orbit_radians_per_second.abs() * 0.025);
    let drift_t = fract01(drift_cycle) * 2.0 - 1.0;

    ParticleSample {
        world_position: emitter.center
            + basis_u * angle.cos() * radius
            + basis_v * angle.sin() * radius
            + axis * vertical_wave
            + emitter.drift * drift_t,
        color,
        radius: size,
        softness: emitter.softness.max(0.8),
        emissive_strength: emitter.emissive_strength.max(0.0),
        depth_test: emitter.depth_test,
    }
}

#[allow(clippy::too_many_arguments)]
fn draw_particle(
    rgba: &mut [u8],
    depth: &[f32],
    resolution: RenderResolution,
    stats: &mut RenderStats,
    camera: &CameraPose,
    screen: ScreenVertex,
    clip_w: f32,
    particle: ParticleSample,
) {
    let distance = (particle.world_position - camera.position).length().max(0.001);
    let perspective_scale = (resolution.height as f32 / clip_w.max(0.001)) * 0.22;
    let particle_radius = (particle.radius * perspective_scale * (1.0 + 0.05 / distance))
        .clamp(1.0, resolution.height as f32 * 0.16);

    let min_x = (screen.x - particle_radius)
        .floor()
        .clamp(0.0, resolution.width as f32 - 1.0) as usize;
    let max_x = (screen.x + particle_radius)
        .ceil()
        .clamp(0.0, resolution.width as f32 - 1.0) as usize;
    let min_y = (screen.y - particle_radius)
        .floor()
        .clamp(0.0, resolution.height as f32 - 1.0) as usize;
    let max_y = (screen.y + particle_radius)
        .ceil()
        .clamp(0.0, resolution.height as f32 - 1.0) as usize;

    let inverse_radius = 1.0 / particle_radius.max(1.0);
    for y in min_y..=max_y {
        for x in min_x..=max_x {
            let dx = (x as f32 + 0.5 - screen.x) * inverse_radius;
            let dy = (y as f32 + 0.5 - screen.y) * inverse_radius;
            let radial = dx * dx + dy * dy;
            if radial >= 1.0 {
                continue;
            }

            let pixel_index = y * resolution.width + x;
            if particle.depth_test && screen.depth >= depth[pixel_index] {
                continue;
            }

            let intensity = (1.0 - radial).powf(particle.softness) * particle.emissive_strength;
            if intensity <= 0.002 {
                continue;
            }

            blend_additive(rgba, pixel_index, particle.color * intensity);
            stats.particles_shaded += 1;
        }
    }
}

fn draw_solid_disc(
    rgba: &mut [u8],
    resolution: RenderResolution,
    center_x: f32,
    center_y: f32,
    radius: f32,
    color: ColorRgb,
) {
    let min_x = (center_x - radius)
        .floor()
        .clamp(0.0, resolution.width as f32 - 1.0) as usize;
    let max_x = (center_x + radius)
        .ceil()
        .clamp(0.0, resolution.width as f32 - 1.0) as usize;
    let min_y = (center_y - radius)
        .floor()
        .clamp(0.0, resolution.height as f32 - 1.0) as usize;
    let max_y = (center_y + radius)
        .ceil()
        .clamp(0.0, resolution.height as f32 - 1.0) as usize;
    let radius_sq = radius * radius;
    let pixel = color.to_rgba8();

    for y in min_y..=max_y {
        for x in min_x..=max_x {
            let dx = x as f32 + 0.5 - center_x;
            let dy = y as f32 + 0.5 - center_y;
            if dx * dx + dy * dy > radius_sq {
                continue;
            }
            let rgba_index = (y * resolution.width + x) * 4;
            rgba[rgba_index..rgba_index + 4].copy_from_slice(&pixel);
        }
    }
}

fn draw_glow_ring(
    rgba: &mut [u8],
    resolution: RenderResolution,
    center_x: f32,
    center_y: f32,
    radius: f32,
    color: ColorRgb,
    intensity: f32,
) {
    let ring_width = (radius * 0.22).max(3.0);
    let outer = radius + ring_width;
    let min_x = (center_x - outer)
        .floor()
        .clamp(0.0, resolution.width as f32 - 1.0) as usize;
    let max_x = (center_x + outer)
        .ceil()
        .clamp(0.0, resolution.width as f32 - 1.0) as usize;
    let min_y = (center_y - outer)
        .floor()
        .clamp(0.0, resolution.height as f32 - 1.0) as usize;
    let max_y = (center_y + outer)
        .ceil()
        .clamp(0.0, resolution.height as f32 - 1.0) as usize;

    for y in min_y..=max_y {
        for x in min_x..=max_x {
            let dx = x as f32 + 0.5 - center_x;
            let dy = y as f32 + 0.5 - center_y;
            let distance = (dx * dx + dy * dy).sqrt();
            let falloff = 1.0 - ((distance - radius).abs() / ring_width);
            if falloff <= 0.0 {
                continue;
            }
            blend_additive(
                rgba,
                y * resolution.width + x,
                color * (falloff * falloff * intensity),
            );
        }
    }
}

fn shade_pixel(
    material: &Material,
    lighting: &LightingRig,
    world_position: Vec3,
    world_normal: Vec3,
    camera_position: Vec3,
    config: SoftwareRendererConfig,
) -> ColorRgb {
    let view_direction = (camera_position - world_position).normalize();

    let ambient =
        lighting.ambient_color.to_vec3() * lighting.ambient_intensity * material.ambient_strength;
    let mut color = material.base_color.to_vec3().component_mul(ambient);

    for light in &lighting.directional_lights {
        color += shade_directional(material, world_normal, view_direction, light);
    }

    for light in &lighting.point_lights {
        color += shade_point(material, world_position, world_normal, view_direction, light);
    }

    let rim = (1.0 - world_normal.dot(view_direction).max(0.0)).powf(2.0) * config.rim_light_strength;
    color += Vec3::new(rim, rim, rim * 1.25);

    ColorRgb::from_vec3(color)
}

fn shade_directional(
    material: &Material,
    world_normal: Vec3,
    view_direction: Vec3,
    light: &DirectionalLight,
) -> Vec3 {
    let light_direction = (light.direction * -1.0).normalize();
    let diffuse = world_normal.dot(light_direction).max(0.0);
    let halfway = (light_direction + view_direction).normalize();
    let specular = world_normal
        .dot(halfway)
        .max(0.0)
        .powf(material.shininess.max(1.0));

    material.base_color.to_vec3().component_mul(light.color.to_vec3())
        * diffuse
        * light.intensity
        * material.diffuse_strength
        + material
            .specular_color
            .to_vec3()
            .component_mul(light.color.to_vec3())
            * specular
            * light.intensity
            * material.specular_strength
}

fn shade_point(
    material: &Material,
    world_position: Vec3,
    world_normal: Vec3,
    view_direction: Vec3,
    light: &PointLight,
) -> Vec3 {
    let to_light = light.position - world_position;
    let distance = to_light.length();
    if distance <= f32::EPSILON || distance > light.range {
        return Vec3::ZERO;
    }

    let light_direction = to_light / distance;
    let attenuation = (1.0 - (distance / light.range)).powi(2);
    let diffuse = world_normal.dot(light_direction).max(0.0);
    let reflected =
        (world_normal * (2.0 * world_normal.dot(light_direction)) - light_direction).normalize();
    let specular = reflected
        .dot(view_direction)
        .max(0.0)
        .powf(material.shininess.max(1.0));

    material.base_color.to_vec3().component_mul(light.color.to_vec3())
        * diffuse
        * attenuation
        * light.intensity
        * material.diffuse_strength
        + material
            .specular_color
            .to_vec3()
            .component_mul(light.color.to_vec3())
            * specular
            * attenuation
            * light.intensity
            * material.specular_strength
}

fn draw_line(
    rgba: &mut [u8],
    resolution: RenderResolution,
    start: ScreenVertex,
    end: ScreenVertex,
    color: [u8; 4],
) {
    let dx = end.x - start.x;
    let dy = end.y - start.y;
    let steps = dx.abs().max(dy.abs()).max(1.0) as usize;
    for step in 0..=steps {
        let t = step as f32 / steps as f32;
        let x = start.x + dx * t;
        let y = start.y + dy * t;
        let xi = x.round() as isize;
        let yi = y.round() as isize;
        if xi < 0 || yi < 0 || xi >= resolution.width as isize || yi >= resolution.height as isize {
            continue;
        }
        let index = (yi as usize * resolution.width + xi as usize) * 4;
        rgba[index..index + 4].copy_from_slice(&color);
    }
}

fn edge_function(a: ScreenVertex, b: ScreenVertex, x: f32, y: f32) -> f32 {
    (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x)
}

fn orthonormal_basis(axis: Vec3) -> (Vec3, Vec3) {
    let helper = if axis.y.abs() > 0.92 {
        Vec3::new(1.0, 0.0, 0.0)
    } else {
        Vec3::UP
    };
    let basis_u = axis.cross(helper).normalize();
    let basis_v = axis.cross(basis_u).normalize();
    (basis_u, basis_v)
}

fn blend_additive(rgba: &mut [u8], pixel_index: usize, color: ColorRgb) {
    let rgba_index = pixel_index * 4;
    let existing = Vec3::new(
        rgba[rgba_index] as f32 / 255.0,
        rgba[rgba_index + 1] as f32 / 255.0,
        rgba[rgba_index + 2] as f32 / 255.0,
    );
    let blended = existing + color.to_vec3();
    let pixel = ColorRgb::from_vec3(blended).to_rgba8();
    rgba[rgba_index..rgba_index + 4].copy_from_slice(&pixel);
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

fn to_vec3(point: [f32; 4]) -> Vec3 {
    Vec3::new(point[0], point[1], point[2])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn retirement_demo_renders_non_empty_frame() {
        let catalog = SceneCatalog::default();
        let mut renderer = SoftwareRenderer::default();
        let frame = renderer
            .render_catalog_scene(&catalog, "retirement_demo", 1.25, RenderResolution::new(192, 128))
            .expect("demo scene should render");

        assert_eq!(frame.rgba.len(), 192 * 128 * 4);
        assert!(frame.stats.triangles_submitted > 0);
        assert!(frame.stats.triangles_rasterized > 0);
        assert!(frame.stats.pixels_shaded > 0);
    }

    #[test]
    fn black_hole_scene_renders_particles() {
        let catalog = SceneCatalog::default();
        let mut renderer = SoftwareRenderer::default();
        let frame = renderer
            .render_catalog_scene(&catalog, "kerr_black_hole", 2.0, RenderResolution::new(256, 160))
            .expect("black hole scene should render");

        assert!(frame.stats.particles_submitted > 0);
        assert!(frame.stats.particles_shaded > 0);
    }
}
