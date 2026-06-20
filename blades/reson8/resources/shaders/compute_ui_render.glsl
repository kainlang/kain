// compute_ui_render.glsl — reson8 unified UI compute shader
//
// GPU-accelerated UI composition: draws waveforms, spectrograms, VU meters,
// note grids, and text via compute shader.  Reads theme uniform data at @0.
//
// This shader is compiled to SPIR-V via `kain gpu-artifacts` and embedded
// in the reson8 capsule.  During Kain-side compilation it lives as a
// `comptime` block inside a shader compute item; this GLSL reference is
// the canonical algorithm.

#version 460 core
#extension GL_EXT_shader_explicit_arithmetic_types : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// ── Binding convention (@0-@9: per-frame, @10-@19: UI data) ──

// Uniforms
layout(binding = 0) uniform PerFrame {
    float time;          // elapsed seconds
    float dt;            // frame delta
    float mouse_x;       // normalized mouse coords
    float mouse_y;
    uint  canvas_width;  // output canvas size
    uint  canvas_height;
    float theme_data[64]; // packed theme RGBA vec4 × 16 slots
} u_per_frame;

// UI element buffer (one element = one rectangle + state)
struct UiElement {
    vec4  rect;          // x, y, width, height
    vec4  color;         // RGBA
    uint  type;          // 0=rect, 1=waveform, 2=spectrogram, 3=vu_meter, 4=note
    uint  state;         // per-element flags
    float data0;         // waveform: zoom_x; VU: level; note: velocity
    float data1;         // waveform: scroll_x; VU: peak; note: duration
    float data2;         // spectrogram: fft_bins; note: start_beat
    float data3;         // unused / padding
};

layout(binding = 1, std430) readonly buffer UiElements {
    UiElement elements[];
} u_elements;

layout(binding = 2, rgba8) writeonly uniform image2D uOutput;

// Sample data for waveform rendering (ring buffer)
layout(binding = 3, std430) readonly buffer SampleData {
    float samples[];
} u_samples;

// FFT data for spectrogram
layout(binding = 4, std430) readonly buffer FftData {
    float fft_bins[];
} u_fft;

// ── Helper: rounded rectangle SDF ──
float rounded_rect_sdf(vec2 p, vec2 size, float radius) {
    vec2 d = abs(p) - size + vec2(radius);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - radius;
}

// ── Helper: blend source over destination ──
vec4 blend_over(vec4 src, vec4 dst) {
    return src + dst * (1.0 - src.a);
}

// ── Helper: sample waveform at pixel x ──
float sample_waveform(uint sample_offset, uint sample_count, float x_pixel,
                      float zoom_x, float scroll_x, float canvas_w) {
    float beat = (x_pixel + scroll_x) / zoom_x;
    uint idx = uint(beat * 44100.0 / 60.0) % sample_count; // rough
    return u_samples.samples[(sample_offset + idx) % sample_count];
}

// ── Main dispatch ──
void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (coord.x >= int(u_per_frame.canvas_width) ||
        coord.y >= int(u_per_frame.canvas_height))
        return;

    vec4 color = vec4(u_per_frame.theme_data[0],
                      u_per_frame.theme_data[1],
                      u_per_frame.theme_data[2],
                      1.0); // background

    // Process each UI element that overlaps this pixel
    for (int i = 0; i < u_elements.elements.length(); i++) {
        UiElement el = u_elements.elements[i];

        // Check if pixel is inside this element's rect
        vec2 el_pos = vec2(coord) - el.rect.xy;
        if (el_pos.x < 0 || el_pos.x > el.rect.z ||
            el_pos.y < 0 || el_pos.y > el.rect.w)
            continue;

        vec2 el_uv = el_pos / vec2(el.rect.zw);

        if (el.type == 0) {
            // Solid rectangle
            color = blend_over(el.color, color);

        } else if (el.type == 1) {
            // Waveform
            float sample = sample_waveform(
                uint(el.data0), uint(el.data1),
                el_pos.x, el.data0, el.data1,
                el.rect.z);
            float center = el.rect.w * 0.5;
            float y_pos = el_pos.y - center;
            if (abs(y_pos) < abs(sample * center)) {
                color = blend_over(el.color, color);
            }

        } else if (el.type == 2) {
            // Spectrogram
            uint bin = uint(el_uv.y * el.data2);
            float magnitude = u_fft.fft_bins[bin]; // 0..1
            float threshold = 1.0 - el_uv.x;
            if (magnitude > threshold) {
                color = blend_over(
                    vec4(el.color.rgb * (magnitude * 2.0), 1.0),
                    color);
            }

        } else if (el.type == 3) {
            // VU meter
            float level = el.data0;
            float seg_height = el.rect.w / 12.0;
            uint seg = uint(el_pos.y / seg_height);
            float seg_norm = float(seg) / 12.0;
            vec4 seg_color;
            if (seg_norm < 0.6)      seg_color = vec4(0.3, 0.8, 0.5, 1.0);
            else if (seg_norm < 0.85) seg_color = vec4(0.8, 0.7, 0.2, 1.0);
            else                     seg_color = vec4(0.9, 0.2, 0.2, 1.0);
            if (seg_norm < level) color = blend_over(seg_color, color);

        } else if (el.type == 4) {
            // MIDI note
            float note_height = max(1.0, el.data0);
            if (el_pos.y < note_height) {
                color = blend_over(el.color, color);
                // Note border
                if (el_pos.y < 1.0 || el_pos.y > note_height - 1.0 ||
                    el_pos.x < 1.0 || el_pos.x > el.rect.z - 1.0)
                    color = blend_over(vec4(0.2, 0.2, 0.3, 1.0), color);
            }
        }
    }

    imageStore(uOutput, coord, color);
}
