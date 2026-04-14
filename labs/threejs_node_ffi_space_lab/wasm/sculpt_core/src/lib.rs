use std::slice;

#[no_mangle]
pub extern "C" fn alloc_f32(length: usize) -> *mut f32 {
    let mut buffer = Vec::<f32>::with_capacity(length);
    let pointer = buffer.as_mut_ptr();
    std::mem::forget(buffer);
    pointer
}

#[no_mangle]
pub unsafe extern "C" fn free_f32(pointer: *mut f32, length: usize) {
    if pointer.is_null() {
        return;
    }

    let _ = Vec::from_raw_parts(pointer, length, length);
}

#[no_mangle]
pub unsafe extern "C" fn sculpt_apply_brush(
    pointer: *mut f32,
    vertex_count: usize,
    center_x: f32,
    center_y: f32,
    center_z: f32,
    normal_x: f32,
    normal_y: f32,
    normal_z: f32,
    radius: f32,
    strength: f32,
    operation_code: u32,
    falloff_power: f32,
) -> usize {
    if pointer.is_null() || vertex_count == 0 || radius <= 0.0 || strength <= 0.0 {
        return 0;
    }

    let positions = slice::from_raw_parts_mut(pointer, vertex_count * 3);
    let center = [center_x, center_y, center_z];
    let normal = normalize3([normal_x, normal_y, normal_z]);
    let safe_falloff = falloff_power.max(0.35);
    let safe_radius = radius.max(0.001);
    let safe_strength = strength.max(0.0);

    let mut affected_vertex_count = 0usize;

    for vertex_index in 0..vertex_count {
        let offset = vertex_index * 3;
        let mut vertex = [positions[offset], positions[offset + 1], positions[offset + 2]];
        let delta = subtract3(vertex, center);
        let distance = length3(delta);

        if distance > safe_radius {
            continue;
        }

        let falloff = (1.0 - distance / safe_radius).max(0.0).powf(safe_falloff);

        if falloff <= 0.0 {
            continue;
        }

        affected_vertex_count += 1;

        match operation_code {
            0 => {
                let displacement = scale3(normal, safe_strength * falloff);
                vertex = add3(vertex, displacement);
            }
            1 => {
                let displacement = scale3(normal, -safe_strength * falloff);
                vertex = add3(vertex, displacement);
            }
            2 => {
                let signed_distance = dot3(delta, normal);
                let displacement = scale3(normal, -signed_distance * safe_strength * falloff * 0.65);
                vertex = add3(vertex, displacement);
            }
            3 => {
                let radial_toward_center = subtract3(center, vertex);
                let tangential_motion = subtract3(
                    radial_toward_center,
                    scale3(normal, dot3(radial_toward_center, normal)),
                );
                vertex = add3(vertex, scale3(tangential_motion, safe_strength * falloff * 0.18));
            }
            _ => {}
        }

        positions[offset] = vertex[0];
        positions[offset + 1] = vertex[1];
        positions[offset + 2] = vertex[2];
    }

    affected_vertex_count
}

fn add3(left: [f32; 3], right: [f32; 3]) -> [f32; 3] {
    [left[0] + right[0], left[1] + right[1], left[2] + right[2]]
}

fn subtract3(left: [f32; 3], right: [f32; 3]) -> [f32; 3] {
    [left[0] - right[0], left[1] - right[1], left[2] - right[2]]
}

fn scale3(vector: [f32; 3], scalar: f32) -> [f32; 3] {
    [vector[0] * scalar, vector[1] * scalar, vector[2] * scalar]
}

fn dot3(left: [f32; 3], right: [f32; 3]) -> f32 {
    left[0] * right[0] + left[1] * right[1] + left[2] * right[2]
}

fn length3(vector: [f32; 3]) -> f32 {
    dot3(vector, vector).sqrt()
}

fn normalize3(vector: [f32; 3]) -> [f32; 3] {
    let length = length3(vector);

    if length <= f32::EPSILON {
        return [0.0, 1.0, 0.0];
    }

    scale3(vector, 1.0 / length)
}
