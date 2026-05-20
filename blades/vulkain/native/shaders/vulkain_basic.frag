#version 450

layout(push_constant) uniform VulkainPush {
    float time_seconds;
    float accent_r;
    float accent_g;
    float accent_b;
    float camera_yaw;
    float camera_pitch;
    float mesh_scale;
    float mesh_twist;
    float depth_bias;
    float energy;
    float clone_count;
    float layout_mode;
    float grid_width;
    float spacing;
    float radial_radius;
    float wave_amount;
    float animation_speed;
    float target_fps;
    float ui_command_count;
    float ui_checksum;
    float viewport_width;
    float viewport_height;
    float honeycomb_bias;
    float row_count;
    float column_count;
    float lod_budget;
    float camera_zoom;
    float camera_pan_x;
    float camera_pan_y;
    float control_overlay;
} pc;

layout(location = 0) in vec3 mesh_color;
layout(location = 1) in vec2 sphere_uv;
layout(location = 2) flat in int sphere_variant;
layout(location = 0) out vec4 out_color;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float rect_mask(vec2 p, vec4 r) {
    vec2 lo = step(r.xy, p);
    vec2 hi = step(p, r.xy + r.zw);
    return lo.x * lo.y * hi.x * hi.y;
}

vec3 tonemap(vec3 color) {
    color = max(color, vec3(0.0));
    return color / (color + vec3(1.0));
}

vec3 kloner_background(vec2 uv, vec2 screen) {
    vec3 accent = vec3(pc.accent_r, pc.accent_g, pc.accent_b);
    vec3 signal = vec3(0.32, 0.85, 1.0);
    float rays = abs(sin((uv.x * 18.0 + uv.y * 8.0) + pc.time_seconds * 0.55));
    float grid = step(0.965, max(fract((uv.x + pc.camera_yaw) * 20.0), fract((uv.y + pc.camera_pitch) * 12.0)));
    float nebula = noise2(uv * 5.0 + pc.time_seconds * 0.035);
    vec3 color = mix(vec3(0.006, 0.009, 0.014), vec3(0.035, 0.052, 0.070), uv.y + 0.5);
    color += accent * (0.08 + 0.12 * nebula);
    color += signal * grid * 0.12;
    color += accent * pow(rays, 12.0) * 0.12;

    float top = rect_mask(screen, vec4(0.018, 0.030, 0.964, 0.072));
    float left = rect_mask(screen, vec4(0.026, 0.130, 0.210, 0.785));
    float right = rect_mask(screen, vec4(0.760, 0.130, 0.214, 0.785));
    float bottom = rect_mask(screen, vec4(0.254, 0.826, 0.488, 0.100));
    float panels = top + left + right + bottom;
    color = mix(color, vec3(0.028, 0.032, 0.040), clamp(panels * 0.82, 0.0, 1.0));

    float clone_ratio = clamp(pc.clone_count / 1000000.0, 0.0, 1.0);
    float fps_ratio = clamp(pc.target_fps / 120.0, 0.0, 1.0);
    float ui_ratio = clamp(pc.ui_command_count / 96.0, 0.0, 1.0);
    float checksum_lane = fract(pc.ui_checksum * 0.000001);

    color = mix(color, accent, rect_mask(screen, vec4(0.052, 0.205, 0.146 * clone_ratio, 0.018)) * 0.82);
    color = mix(color, signal, rect_mask(screen, vec4(0.052, 0.257, 0.146 * fps_ratio, 0.018)) * 0.82);
    color = mix(color, vec3(1.0, 0.72, 0.28), rect_mask(screen, vec4(0.052, 0.309, 0.146 * ui_ratio, 0.018)) * 0.82);
    color = mix(color, vec3(0.92, 0.44, 1.0), rect_mask(screen, vec4(0.792, 0.220, 0.132 * checksum_lane, 0.016)) * 0.82);

    float layout_tick = rect_mask(screen, vec4(0.792 + (pc.layout_mode - 1.0) * 0.036, 0.278, 0.026, 0.048));
    color += accent * layout_tick * 0.65;
    color += vec3(0.05, 0.14, 0.16) * rect_mask(screen, vec4(0.278, 0.858, 0.424, 0.028));
    color = mix(color, signal, rect_mask(screen, vec4(0.278, 0.858, 0.424 * clone_ratio, 0.028)) * 0.72);
    return tonemap(color * 1.25);
}

vec4 kloner_overlay(vec2 screen) {
    vec3 accent = vec3(pc.accent_r, pc.accent_g, pc.accent_b);
    vec3 signal = vec3(0.32, 0.85, 1.0);
    vec3 hot = vec3(1.0, 0.72, 0.28);
    float clone_ratio = clamp(pc.clone_count / 1000000.0, 0.0, 1.0);
    float fps_ratio = clamp(pc.target_fps / 120.0, 0.0, 1.0);
    float zoom_ratio = clamp((pc.camera_zoom - 1.0) / 160.0, 0.0, 1.0);
    float ui_ratio = clamp(pc.ui_command_count / 96.0, 0.0, 1.0);
    float live = 0.42 + pc.control_overlay * 0.58;
    vec3 color = vec3(0.0);
    float alpha = 0.0;

    float top = rect_mask(screen, vec4(0.018, 0.030, 0.964, 0.076));
    float left = rect_mask(screen, vec4(0.026, 0.128, 0.228, 0.796));
    float right = rect_mask(screen, vec4(0.742, 0.128, 0.232, 0.796));
    float bottom = rect_mask(screen, vec4(0.274, 0.836, 0.452, 0.092));
    float shell = clamp(top + left + right + bottom, 0.0, 1.0);
    color += vec3(0.018, 0.022, 0.030) * shell * 1.8;
    color += accent * top * 0.18 + signal * bottom * 0.12;
    alpha = max(alpha, shell * 0.95);

    float title_bar = rect_mask(screen, vec4(0.038, 0.052, 0.316, 0.018));
    float fps_badge = rect_mask(screen, vec4(0.846, 0.052, 0.096 * fps_ratio, 0.018));
    color += accent * title_bar * 2.4 + signal * fps_badge * 1.7;
    alpha = max(alpha, (title_bar + fps_badge) * 0.98);

    float clone_track = rect_mask(screen, vec4(0.056, 0.212, 0.162, 0.016));
    float clone_fill = rect_mask(screen, vec4(0.056, 0.212, 0.162 * clone_ratio, 0.016));
    float zoom_track = rect_mask(screen, vec4(0.056, 0.278, 0.162, 0.016));
    float zoom_fill = rect_mask(screen, vec4(0.056, 0.278, 0.162 * (1.0 - zoom_ratio), 0.016));
    float ui_track = rect_mask(screen, vec4(0.056, 0.344, 0.162, 0.016));
    float ui_fill = rect_mask(screen, vec4(0.056, 0.344, 0.162 * ui_ratio, 0.016));
    color += vec3(0.06, 0.08, 0.10) * (clone_track + zoom_track + ui_track);
    color += accent * clone_fill * 2.2 + signal * zoom_fill * 1.8 + hot * ui_fill * 1.6;
    alpha = max(alpha, clamp(clone_track + zoom_track + ui_track, 0.0, 1.0) * 0.96);

    float layout_base = rect_mask(screen, vec4(0.784, 0.220, 0.148, 0.052));
    float layout_tick = rect_mask(screen, vec4(0.792 + (pc.layout_mode - 1.0) * 0.036, 0.226, 0.026, 0.040));
    color += vec3(0.055, 0.070, 0.086) * layout_base + accent * layout_tick * 2.4;
    alpha = max(alpha, (layout_base + layout_tick) * 0.96);

    float cam_track_x = rect_mask(screen, vec4(0.786, 0.324, 0.148, 0.014));
    float cam_x = rect_mask(screen, vec4(0.858 + clamp(pc.camera_pan_x, -0.08, 0.08), 0.318, 0.010, 0.026));
    float cam_track_y = rect_mask(screen, vec4(0.786, 0.374, 0.148, 0.014));
    float cam_y = rect_mask(screen, vec4(0.858 + clamp(pc.camera_pan_y, -0.08, 0.08), 0.368, 0.010, 0.026));
    color += vec3(0.05, 0.07, 0.08) * (cam_track_x + cam_track_y) + signal * (cam_x + cam_y) * 2.0;
    alpha = max(alpha, clamp(cam_track_x + cam_track_y + cam_x + cam_y, 0.0, 1.0) * 0.94);

    float center_x = rect_mask(screen, vec4(0.497, 0.468, 0.006, 0.064));
    float center_y = rect_mask(screen, vec4(0.468, 0.497, 0.064, 0.006));
    float focus = clamp(center_x + center_y, 0.0, 1.0);
    color += mix(signal, accent, pc.control_overlay) * focus * 1.6;
    alpha = max(alpha, focus * 0.88);

    float help_a = rect_mask(screen, vec4(0.300, 0.865, 0.048, 0.022));
    float help_b = rect_mask(screen, vec4(0.360, 0.865, 0.048, 0.022));
    float help_c = rect_mask(screen, vec4(0.420, 0.865, 0.048, 0.022));
    float help_d = rect_mask(screen, vec4(0.480, 0.865, 0.048, 0.022));
    float help_e = rect_mask(screen, vec4(0.540, 0.865, 0.048, 0.022));
    color += (accent * help_a + signal * help_b + hot * help_c + vec3(0.92, 0.44, 1.0) * help_d + vec3(0.72, 1.0, 0.58) * help_e) * live;
    alpha = max(alpha, clamp(help_a + help_b + help_c + help_d + help_e, 0.0, 1.0) * 0.96);

    return vec4(tonemap(color * 1.35), alpha);
}

void main() {
    if (pc.clone_count > 1.5 && pc.depth_bias > -4.0) {
        float d = dot(sphere_uv, sphere_uv);
        if (d > 1.0) {
            discard;
        }
        vec3 n = normalize(vec3(sphere_uv, sqrt(max(1.0 - d, 0.0))));
        vec3 light_dir = normalize(vec3(-0.35, 0.80, 0.42));
        vec3 accent = vec3(pc.accent_r, pc.accent_g, pc.accent_b);
        vec3 variants[5] = vec3[](
            accent,
            vec3(0.32, 0.85, 1.0),
            vec3(1.0, 0.62, 0.30),
            vec3(0.92, 0.44, 1.0),
            vec3(0.72, 1.0, 0.58)
        );
        vec3 base = variants[sphere_variant % 5];
        float ndotl = max(dot(n, light_dir), 0.0);
        float rim = pow(1.0 - max(n.z, 0.0), 2.0);
        float pulse = 0.85 + 0.15 * sin(pc.time_seconds * pc.animation_speed + float(sphere_variant) * 1.7);
        vec3 color = base * (0.22 + ndotl * 1.45) * pulse + vec3(1.0, 0.84, 0.56) * rim * 0.35;
        out_color = vec4(tonemap(color * (1.0 + pc.energy * 0.08)), 1.0);
        return;
    }

    if (pc.depth_bias < -6.0) {
        vec2 extent = vec2(max(pc.viewport_width, 1.0), max(pc.viewport_height, 1.0));
        vec2 screen = gl_FragCoord.xy / extent;
        vec4 overlay = kloner_overlay(screen);
        if (overlay.a <= 0.01) {
            discard;
        }
        out_color = vec4(overlay.rgb, 1.0);
        return;
    }

    if (pc.depth_bias < -4.0) {
        vec2 extent = vec2(max(pc.viewport_width, 1.0), max(pc.viewport_height, 1.0));
        vec2 screen = gl_FragCoord.xy / extent;
        vec2 uv = screen * 2.0 - 1.0;
        uv.x *= extent.x / extent.y;
        out_color = vec4(kloner_background(uv, screen), 1.0);
        return;
    }

    out_color = vec4(mesh_color, 1.0);
}
