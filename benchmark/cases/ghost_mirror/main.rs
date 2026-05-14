use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::thread;

const UPDATES: usize = 64;
const BYTES_PER_PAYLOAD: usize = 1_048_576;
const EXPECTED_CHECKSUM: u64 = 2_080;

fn read_exact_payload(mut stream: TcpStream) -> u64 {
    let mut payload = vec![0_u8; BYTES_PER_PAYLOAD];
    let mut checksum = 0_u64;
    for _ in 0..UPDATES {
        let mut header = [0_u8; 8];
        stream.read_exact(&mut header).expect("read header");
        let revision = u64::from_le_bytes(header);
        stream.read_exact(&mut payload).expect("read payload");
        checksum = (checksum + revision) % 1_000_000_007;
    }
    checksum
}

fn main() {
    let listener = TcpListener::bind("127.0.0.1:0").expect("bind");
    let address = listener.local_addr().expect("local addr");
    let receiver = thread::spawn(move || {
        let (stream, _) = listener.accept().expect("accept");
        read_exact_payload(stream)
    });

    let mut stream = TcpStream::connect(address).expect("connect");
    let mut payload = vec![0_u8; BYTES_PER_PAYLOAD];
    for revision in 1..=UPDATES {
        let seed = (revision & 0xff) as u8;
        let mut index = 0_usize;
        while index < payload.len() {
            payload[index] = seed.wrapping_add((index & 0xff) as u8);
            index += 4096;
        }
        stream
            .write_all(&(revision as u64).to_le_bytes())
            .expect("write revision");
        stream.write_all(&payload).expect("write payload");
    }
    drop(stream);

    let checksum = receiver.join().expect("receiver thread panicked");
    if checksum != EXPECTED_CHECKSUM {
        std::process::exit(1);
    }
}
