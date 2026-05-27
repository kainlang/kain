use pyo3::prelude::*;

const MODULUS: i64 = 1_000_000_007;
const ITERATIONS: i64 = 150_000;
const EXPECTED: i64 = 9_325_307;

fn main() {
    let setup = Python::with_gil(|py| -> PyResult<(PyObject, i64)> {
        let math = py.import("math")?;
        let sqrt_fn = math.getattr("sqrt")?.into_py(py);
        let tau_bias = math.getattr("tau")?.extract::<f64>()? as i64;
        Ok((sqrt_fn, tau_bias))
    });

    let (sqrt_fn, tau_bias) = match setup {
        Ok(values) => values,
        Err(_) => {
            std::process::exit(1);
        }
    };

    let mut acc = 0_i64;
    let mut index = 0_i64;
    while index < ITERATIONS {
        let lane_value = ((index * 17) % 4096) + 1;
        let sqrt_value = match Python::with_gil(|py| -> PyResult<i64> {
            let value = sqrt_fn
                .as_ref(py)
                .call1((lane_value as f64,))?
                .extract::<f64>()? as i64;
            Ok(value)
        }) {
            Ok(value) => value,
            Err(_) => {
                std::process::exit(1);
            }
        };
        acc = (acc + tau_bias + sqrt_value + (index % 29)) % MODULUS;
        index += 1;
    }

    if acc != EXPECTED {
        std::process::exit(1);
    }
}
