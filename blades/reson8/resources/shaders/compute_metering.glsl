// compute_metering.glsl — reson8 VU/peak metering compute shader
//
// Reads audio ring buffer, computes per-channel peak/RMS levels.
// Output written to metering buffer readable by the UI mixer.

#version 460 core

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0) uniform MeteringParams {
    uint  ring_buffer_offset;
    uint  ring_buffer_size;
    uint  num_channels;
    uint  window_frames;       // RMS window size in frames
    float decay_rate;          // peak hold decay per frame
    float sample_rate;
} u_params;

layout(binding = 1, std430) readonly buffer AudioRingBuffer {
    float samples[];
} u_audio;

layout(binding = 2, std430) readonly buffer PrevMeterValues {
    float prev_peak[];
    float prev_rms[];
} u_prev;

layout(binding = 3, std430) writeonly buffer MeterOutput {
    float peak_level[];
    float rms_level[];
    uint  clip_flags[];
} u_output;

shared float local_sum[64];
shared float local_max[64];

void main() {
    uint channel = gl_GlobalInvocationID.x;
    if (channel >= u_params.num_channels) return;

    // Compute RMS and peak for this channel
    float sum_sq = 0.0;
    float peak = 0.0;

    uint start = u_params.ring_buffer_offset + channel;
    uint end = min(start + u_params.window_frames, u_params.ring_buffer_size);

    for (uint i = start; i < end; i += u_params.num_channels) {
        float s = u_audio.samples[i % u_params.ring_buffer_size];
        sum_sq += s * s;
        float abs_s = abs(s);
        if (abs_s > peak) peak = abs_s;
    }

    float rms = sqrt(sum_sq / float(u_params.window_frames));

    // Apply decay to peak (auto-release)
    float prev_peak = u_prev.prev_peak[channel];
    if (peak < prev_peak) {
        peak = prev_peak * u_params.decay_rate;
    }

    // Clip detection
    uint clip = 0;
    if (peak > 0.999) clip = 1;

    // Write output
    u_output.peak_level[channel] = peak;
    u_output.rms_level[channel] = rms;
    u_output.clip_flags[channel] = clip;
}
