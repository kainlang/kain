// compute_spectrogram.glsl — reson8 spectrogram FFT compute shader
//
// Reads audio ring buffer, computes FFT power spectrum for a window,
// writes magnitude bins to output buffer for spectrogram display.
// Uses Cooley-Tukey radix-2 FFT with shared memory reduction.

#version 460 core
#extension GL_EXT_shader_explicit_arithmetic_types : require

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0) uniform SpectrogramParams {
    uint   fft_size;           // typically 512, 1024, or 2048
    uint   ring_buffer_offset;
    uint   ring_buffer_size;
    float  sample_rate;
    float  window_overlap;     // 0.0 .. 1.0
    uint   num_columns;        // horizontal resolution
    uint   current_column;     // which column we're computing
} u_params;

layout(binding = 1, std430) readonly buffer AudioRingBuffer {
    float samples[];
} u_audio;

layout(binding = 2, std430) readonly buffer WindowFunction {
    float window[];
} u_window;

layout(binding = 3, std430) writeonly buffer MagnitudeOutput {
    float magnitudes[];
} u_output;

shared float shared_real[256];
shared float shared_imag[256];

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_params.fft_size) return;

    // Load sample with window function applied
    uint sample_idx = (u_params.ring_buffer_offset + idx) % u_params.ring_buffer_size;
    float windowed = u_audio.samples[sample_idx] * u_window.window[idx];

    shared_real[idx] = windowed;
    shared_imag[idx] = 0.0;
    barrier();

    // Cooley-Tukey radix-2 in-place FFT
    for (uint len = u_params.fft_size >> 1; len > 0; len >>= 1) {
        uint group = idx / len;
        uint offset = idx % len;
        uint a = group * len * 2 + offset;
        uint b = a + len;

        float angle = -6.283185307 * float(offset) / float(len * 2);
        float wr = cos(angle);
        float wi = sin(angle);

        float tr = shared_real[a] - shared_real[b];
        float ti = shared_imag[a] - shared_imag[b];
        shared_real[a] = shared_real[a] + shared_real[b];
        shared_imag[a] = shared_imag[a] + shared_imag[b];
        shared_real[b] = tr * wr - ti * wi;
        shared_imag[b] = tr * wi + ti * wr;

        barrier();
    }

    // Compute magnitude (only first half of bins are valid)
    uint bin = idx;
    if (bin < u_params.fft_size / 2) {
        float real = shared_real[bin];
        float imag = shared_imag[bin];
        float magnitude = sqrt(real * real + imag * imag);

        // Normalize and write to column
        uint col = u_params.current_column;
        uint output_idx = col * (u_params.fft_size / 2) + bin;
        u_output.magnitudes[output_idx] = magnitude / float(u_params.fft_size);
    }
}
