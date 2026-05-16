#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
use std::arch::x86_64::*;

const CELLS: usize = 32_768;
const PASSES: i64 = 256;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 194_810_730;

fn fill_buffers(left: &mut [i32], right: &mut [i32]) {
    let mut index = 0_usize;
    while index < CELLS {
        left[index] = (((index as i32) * 31) + 7) % 1024;
        right[index] = (((index as i32) * 17) + 3) % 512;
        index += 1;
    }
}

#[inline(never)]
fn dot_scalar(left: &[i32], right: &[i32], lane_bias: i32) -> i64 {
    let mut total = 0_i64;
    let mut index = 0_usize;
    while index < CELLS {
        total += i64::from(left[index] + lane_bias) * i64::from(right[index]);
        index += 1;
    }
    total
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[target_feature(enable = "avx2")]
unsafe fn dot_avx2(left: &[i32], right: &[i32], lane_bias: i32) -> i64 {
    let bias = _mm256_set1_epi32(lane_bias);
    let mut scratch = [0_i32; 8];
    let mut total = 0_i64;
    let mut index = 0_usize;
    while index < CELLS {
        let left_vec = _mm256_loadu_si256(left.as_ptr().add(index).cast::<__m256i>());
        let right_vec = _mm256_loadu_si256(right.as_ptr().add(index).cast::<__m256i>());
        let biased_left = _mm256_add_epi32(left_vec, bias);
        let products = _mm256_mullo_epi32(biased_left, right_vec);
        _mm256_storeu_si256(scratch.as_mut_ptr().cast::<__m256i>(), products);
        total += scratch.iter().map(|value| i64::from(*value)).sum::<i64>();
        index += 8;
    }
    total
}

#[inline(never)]
fn dot_products(left: &[i32], right: &[i32], lane_bias: i32) -> i64 {
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    {
        if is_x86_feature_detected!("avx2") {
            return unsafe { dot_avx2(left, right, lane_bias) };
        }
    }
    dot_scalar(left, right, lane_bias)
}

fn main() {
    let mut left = vec![0_i32; CELLS];
    let mut right = vec![0_i32; CELLS];
    fill_buffers(&mut left, &mut right);

    let mut acc = 0_i64;
    let mut pass = 0_i64;
    while pass < PASSES {
        let inner = dot_products(&left, &right, (pass % 13) as i32) % MODULUS;
        acc = (acc + inner + (pass % 29)) % MODULUS;
        pass += 1;
    }

    if unsafe { std::ptr::read_volatile(&acc) } != EXPECTED {
        std::process::exit(1);
    }
}
