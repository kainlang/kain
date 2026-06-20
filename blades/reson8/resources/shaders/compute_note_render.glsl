// compute_note_render.glsl — reson8 MIDI note grid render compute shader
//
// Renders the piano roll note grid directly on GPU.
// Reads MIDI note data from storage buffer, writes pixel-level output.

#version 460 core

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0) uniform NoteRenderParams {
    uint  canvas_width;
    uint  canvas_height;
    float zoom_x;           // pixels per beat
    float zoom_y;           // pixels per MIDI note (semitone)
    float scroll_x;
    float scroll_y;
    float root_note;        // bottom visible note (0-127)
    uint  note_count;
    vec4  note_on_color;
    vec4  note_off_color;
    vec4  grid_major_color;
    vec4  grid_minor_color;
    vec4  bg_color;
    vec4  playhead_color;
    float playhead_beat;
} u_params;

struct MidiNote {
    float start_beat;
    float duration_beat;
    float pitch;            // 0..127
    float velocity;         // 0..127
    uint  flags;            // bit0: selected, bit1: muted
};

layout(binding = 1, std430) readonly buffer MidiNotes {
    MidiNote notes[];
} u_notes;

layout(binding = 2, rgba8) writeonly uniform image2D uOutput;

// Check if pixel (x, y) falls within a MIDI note rectangle
bool hit_test_note(MidiNote n, float x, float y) {
    float nx = n.start_beat * u_params.zoom_x - u_params.scroll_x;
    float ny = (u_params.root_note + 127.0 - n.pitch) * u_params.zoom_y - u_params.scroll_y;
    float nw = n.duration_beat * u_params.zoom_x;
    float nh = u_params.zoom_y;
    return x >= nx && x < nx + nw && y >= ny && y < ny + nh;
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (coord.x >= int(u_params.canvas_width) ||
        coord.y >= int(u_params.canvas_height))
        return;

    float x = float(coord.x);
    float y = float(coord.y);

    vec4 color = u_params.bg_color;

    // Draw grid lines (minor every semitone, major every octave)
    float note_y = y + u_params.scroll_y;
    float note_idx = note_y / u_params.zoom_y;
    float octave_pos = mod(note_idx, 12.0);
    if (abs(octave_pos) < 0.5 || abs(octave_pos - 11.0) < 0.5) {
        // Emphasize octave boundaries (C notes)
        color = mix(color, u_params.grid_major_color, 0.3);
    } else {
        color = mix(color, u_params.grid_minor_color, 0.15);
    }

    // Draw beat grid lines (vertical)
    float beat_x = x + u_params.scroll_x;
    float beat = beat_x / u_params.zoom_x;
    float beat_frac = fract(beat);
    if (beat_frac < 0.02) {
        color = mix(color, u_params.grid_major_color, 0.2);
    }

    // Draw MIDI notes
    for (uint i = 0; i < u_params.note_count; i++) {
        MidiNote n = u_notes.notes[i];
        if (hit_test_note(n, x, y)) {
            vec4 nc = u_params.note_on_color;
            if ((n.flags & 1u) != 0u) {
                nc = mix(nc, vec4(1.0), 0.3); // selected = highlight
            }
            color = nc;
            break;
        }
    }

    // Draw playhead
    float ph_x = u_params.playhead_beat * u_params.zoom_x - u_params.scroll_x;
    if (abs(x - ph_x) < 1.5) {
        color = u_params.playhead_color;
    }

    imageStore(uOutput, coord, color);
}
