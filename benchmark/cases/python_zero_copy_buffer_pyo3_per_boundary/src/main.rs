use pyo3::buffer::PyBuffer;
use pyo3::prelude::*;

const MODULUS: i64 = 1_000_000_007;
const ITERATIONS: i64 = 20_000;
const BUFFER_CELLS: i64 = 512;
const EXPECTED: i64 = 20_899_830;

fn main() {
    let setup = Python::with_gil(|py| -> PyResult<PyObject> {
        let numpy = py.import("numpy")?;
        let source = numpy
            .getattr("ascontiguousarray")?
            .call1((numpy.getattr("arange")?.call1((BUFFER_CELLS,))?.call_method1("astype", ("uint8",))?,))?;
        Ok(source.into_py(py))
    });

    let source = match setup {
        Ok(value) => value,
        Err(_) => {
            std::process::exit(1);
        }
    };

    let mut acc = 0_i64;
    let mut index = 0_i64;
    while index < ITERATIONS {
        let lane = match Python::with_gil(|py| -> PyResult<i64> {
            let buffer = PyBuffer::<u8>::get(source.as_ref(py))?;
            Ok(
                buffer.len_bytes() as i64
                    + buffer.item_count() as i64
                    + buffer.item_size() as i64
                    + if buffer.is_c_contiguous() { 1 } else { 0 }
                    + if !buffer.readonly() { 1 } else { 0 }
                    + (index % 37),
            )
        }) {
            Ok(value) => value,
            Err(_) => {
                std::process::exit(1);
            }
        };
        acc = (acc + lane) % MODULUS;
        index += 1;
    }

    if acc != EXPECTED {
        std::process::exit(1);
    }
}
