use std::process::Command;

const ROUNDS: i64 = 300;
const EXPECTED: i64 = 5_988;

fn main() {
    let mut acc = 0_i64;
    let mut index = 0_i64;
    while index < ROUNDS {
        let output = Command::new("cmd.exe")
            .args(["/d", "/c", "echo process-bench"])
            .output()
            .unwrap();
        if !output.status.success() {
            std::process::exit(1);
        }
        let stdout_text = String::from_utf8(output.stdout).unwrap();
        if stdout_text != "process-bench\r\n" {
            std::process::exit(1);
        }
        acc += stdout_text.len() as i64 + (index % 11);
        index += 1;
    }

    if unsafe { std::ptr::read_volatile(&acc) } != EXPECTED {
        std::process::exit(1);
    }
}
