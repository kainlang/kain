// compute_waveform.glsl — reson8 waveform rendering compute shader
//
// Pure waveform: reads float sample buffer, writes RGBA output.
// Used when waveform view is the ONLY element (for large sample display).

#version 460 core

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0) uniform WaveformParams {
    float sample_rate;
    uint  sample_count;
    float zoom_x;          // pixels per beat
    float scroll_x;
    float start_beat;
    float duration_beat;
    float height;
    vec4  fill_color;
    vec4  outline_color;
    vec4  bg_color;
    vec4  zero_line_color;
    uint  output_width;
    uint  output_height;
} u_params;

layout(binding = 1, std430) readonly buffer SampleData {
    float samples[];
} u_samples;

layout(binding = 2, rgba8) writeonly uniform image2D uOutput;

// Downsample chunk: compute min/max for a range of samples
void compute_minmax(uint start, uint end, out float min_val, out float max_val) {
    min_val = 1.0;
    max_val = -1.0;
    for (uint i = start; i < end && i < u_params.sample_count; i++) {
        float s = u_samples.samples[i];
        if (s < min_val) min_val = s;
        if (s > max_val) max_val = s;
    }
}

void main() {
    uint x = gl_GlobalInvocationID.x;
    if (x >= u_params.output_width) return;

    // Map pixel x to sample range
    float beat = (float(x) + u_params.scroll_x) / u_params.zoom_x;
    float samples_per_pixel = u_params.sample_rate / (u_params.zoom_x * (u_params.bpm / 60.0));
    // ... simplified; full version in reference kernels
}
