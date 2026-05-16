const ROUNDS: i64 = 250_000;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 35_749_995;
const PAYLOAD_A: &str = "{\"id\":17,\"name\":\"orbital\",\"enabled\":true,\"count\":42}";
const PAYLOAD_B: &str = "{\"id\":23,\"name\":\"lattice\",\"enabled\":false,\"count\":57}";

fn parse_positive_int(text: &str, start: usize) -> i64 {
    let bytes = text.as_bytes();
    let mut index = start;
    let mut value = 0_i64;
    while index < bytes.len() && bytes[index].is_ascii_digit() {
        value = (value * 10) + i64::from(bytes[index] - b'0');
        index += 1;
    }
    value
}

fn parse_int_field(text: &str, key: &str) -> i64 {
    let start = text.find(key).unwrap() + key.len();
    parse_positive_int(text, start)
}

fn parse_name_field(text: &str) -> String {
    let key = "\"name\":\"";
    let start = text.find(key).unwrap() + key.len();
    let finish = text[start..].find('"').unwrap() + start;
    text[start..finish].to_string()
}

fn parse_enabled_field(text: &str) -> bool {
    let key = "\"enabled\":";
    let start = text.find(key).unwrap() + key.len();
    text[start..].starts_with("true")
}

fn render_payload(id: i64, name: &str, enabled: bool, count: i64) -> String {
    format!(
        "{{\"id\":{},\"name\":\"{}\",\"enabled\":{},\"count\":{}}}",
        id,
        name,
        if enabled { "true" } else { "false" },
        count
    )
}

fn main() {
    let mut acc = 0_i64;
    let mut index = 0_i64;
    while index < ROUNDS {
        let payload = if index & 1 == 0 { PAYLOAD_A } else { PAYLOAD_B };
        let id = parse_int_field(payload, "\"id\":");
        let name = parse_name_field(payload);
        let enabled = parse_enabled_field(payload);
        let count = parse_int_field(payload, "\"count\":");
        let rendered = render_payload(id, &name, enabled, count);
        if rendered != payload {
            std::process::exit(1);
        }
        let enabled_score = if enabled { 17 } else { 5 };
        acc = (acc + id + count + name.len() as i64 + enabled_score + rendered.len() as i64 + (index % 7)) % MODULUS;
        index += 1;
    }

    if unsafe { std::ptr::read_volatile(&acc) } != EXPECTED {
        std::process::exit(1);
    }
}
