use pyo3::prelude::*;

const MODULUS: i64 = 1_000_000_007;
const ITERATIONS: i64 = 150_000;
const EXPECTED: i64 = 9_325_307;

fn main() {
    let status = Python::with_gil(|py| -> PyResult<i64> {
        let math = py.import("math")?;
        let sqrt_fn = math.getattr("sqrt")?;
        let tau_bias = math.getattr("tau")?.extract::<f64>()? as i64;
        let mut acc = 0_i64;
        let mut index = 0_i64;

        while index < ITERATIONS {
            let lane_value = ((index * 17) % 4096) + 1;
            let sqrt_value = sqrt_fn.call1((lane_value as f64,))?.extract::<f64>()? as i64;
            acc = (acc + tau_bias + sqrt_value + (index % 29)) % MODULUS;
            index += 1;
        }

        Ok(acc)
    });

    match status {
        Ok(value) if value == EXPECTED => {}
        _ => std::process::exit(1),
    }
}
