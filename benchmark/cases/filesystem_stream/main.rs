use std::fs::{self, File};
use std::io::{Read, Write};
use std::path::PathBuf;

const ROUNDS: i64 = 80;
const EXPECTED: i64 = 6_846_690;

fn build_payload(line_count: usize) -> String {
    let mut text = String::new();
    let mut index = 0_usize;
    while index < line_count {
        text.push_str("line-");
        text.push_str(&(index % 97).to_string());
        text.push_str("-orbital-flux\n");
        index += 1;
    }
    text
}

fn copy_streaming(source: &PathBuf, dest: &PathBuf) -> std::io::Result<i64> {
    let mut reader = File::open(source)?;
    let mut writer = File::create(dest)?;
    let mut buffer = [0_u8; 256];
    let mut total = 0_i64;
    loop {
        let read = reader.read(&mut buffer)?;
        if read == 0 {
            break;
        }
        writer.write_all(&buffer[..read])?;
        total += read as i64;
    }
    writer.flush()?;
    Ok(total)
}

fn main() {
    let payload = build_payload(2_048);
    let mut dir = std::env::temp_dir();
    dir.push("kain-benchmark-fs");
    fs::create_dir_all(&dir).unwrap();
    let source_path = dir.join("source.txt");
    let dest_path = dir.join("copy.txt");

    let mut acc = 0_i64;
    let mut index = 0_i64;
    while index < ROUNDS {
        fs::write(&source_path, &payload).unwrap();
        let copied = copy_streaming(&source_path, &dest_path).unwrap();
        let readback = fs::read_to_string(&dest_path).unwrap();
        if readback != payload {
            std::process::exit(1);
        }
        acc += copied + readback.len() as i64 + (index % 17);
        index += 1;
    }

    let _ = fs::remove_file(&source_path);
    let _ = fs::remove_file(&dest_path);
    let _ = fs::remove_dir_all(&dir);

    if unsafe { std::ptr::read_volatile(&acc) } != EXPECTED {
        std::process::exit(1);
    }
}
