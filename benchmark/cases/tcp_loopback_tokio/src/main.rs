use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};

const ROUNDS: u64 = 400;
const EXPECTED: u64 = 31_090;
const REQUEST: &[u8] = b"kain-net-benchmark";
const RESPONSE: &[u8] = b"kain-net-pong";

#[tokio::main(flavor = "current_thread")]
async fn main() {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let addr = listener.local_addr().unwrap();
    let mut acc = 0_u64;

    for i in 0..ROUNDS {
        let server = async {
            let (mut socket, _) = listener.accept().await.unwrap();
            let mut request = [0_u8; REQUEST.len()];
            socket.read_exact(&mut request).await.unwrap();
            if request != REQUEST {
                std::process::exit(5);
            }
            socket.write_all(RESPONSE).await.unwrap();
        };

        let client = async {
            let mut stream = TcpStream::connect(addr).await.unwrap();
            stream.write_all(REQUEST).await.unwrap();
            let mut response = [0_u8; RESPONSE.len()];
            stream.read_exact(&mut response).await.unwrap();
            if response != RESPONSE {
                std::process::exit(6);
            }
        };

        tokio::join!(server, client);
        acc = (acc + (i % 97) + REQUEST.len() as u64 + RESPONSE.len() as u64) % 1_000_000_007;
    }

    if acc != EXPECTED {
        std::process::exit(7);
    }
}
