use std::io;

use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};
use tokio::sync::oneshot;

const ROUNDS: usize = 240;
const CONCURRENCY: usize = 16;
const MODULUS: u64 = 1_000_000_007;
const EXPECTED: u64 = 5_695;
const REQUEST_BODY: &str = "orbital-bench";
const RESPONSE_BODY: &str = "reply-ok-123";

fn find_header_end(bytes: &[u8]) -> Option<usize> {
    bytes.windows(4).position(|window| window == b"\r\n\r\n")
}

fn parse_content_length(header: &str) -> usize {
    for line in header.lines() {
        if let Some(value) = line.strip_prefix("Content-Length:") {
            return value.trim().parse::<usize>().unwrap();
        }
    }
    0
}

fn parse_request_line(header: &str) -> (String, String) {
    let request_line = header.lines().next().unwrap_or_default();
    let mut parts = request_line.split_whitespace();
    let method = parts.next().unwrap_or_default().to_string();
    let path = parts.next().unwrap_or_default().to_string();
    (method, path)
}

async fn read_request(stream: &mut TcpStream) -> io::Result<(String, String, String)> {
    let mut buffer = Vec::<u8>::with_capacity(256);
    let mut temp = [0_u8; 256];
    let header_end = loop {
        let read = stream.read(&mut temp).await?;
        if read == 0 {
            return Err(io::Error::new(io::ErrorKind::UnexpectedEof, "connection closed before headers"));
        }
        buffer.extend_from_slice(&temp[..read]);
        if let Some(position) = find_header_end(&buffer) {
            break position;
        }
    };
    let header_text = String::from_utf8_lossy(&buffer[..header_end]).into_owned();
    let body_start = header_end + 4;
    let content_length = parse_content_length(&header_text);
    while buffer.len() < body_start + content_length {
        let read = stream.read(&mut temp).await?;
        if read == 0 {
            return Err(io::Error::new(io::ErrorKind::UnexpectedEof, "connection closed before body"));
        }
        buffer.extend_from_slice(&temp[..read]);
    }
    let body = String::from_utf8_lossy(&buffer[body_start..body_start + content_length]).into_owned();
    let (method, path) = parse_request_line(&header_text);
    Ok((method, path, body))
}

async fn handle_client(mut stream: TcpStream) -> io::Result<bool> {
    let (method, path, body) = read_request(&mut stream).await?;
    if method != "POST" || path != "/bench" || body != REQUEST_BODY {
        return Ok(false);
    }
    let response = format!(
        "HTTP/1.1 200 OK\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
        RESPONSE_BODY.len(),
        RESPONSE_BODY
    );
    stream.write_all(response.as_bytes()).await?;
    stream.shutdown().await?;
    Ok(true)
}

async fn send_request(port: u16, request_index: usize) -> io::Result<(usize, String)> {
    let mut stream = TcpStream::connect(("127.0.0.1", port)).await?;
    let request = format!(
        "POST /bench HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
        REQUEST_BODY.len(),
        REQUEST_BODY
    );
    stream.write_all(request.as_bytes()).await?;
    stream.shutdown().await?;
    let mut response_bytes = Vec::<u8>::new();
    stream.read_to_end(&mut response_bytes).await?;
    let response_text = String::from_utf8_lossy(&response_bytes).into_owned();
    let body = if let Some(position) = response_text.find("\r\n\r\n") {
        response_text[position + 4..].to_string()
    } else {
        String::new()
    };
    Ok((request_index, body))
}

async fn run_server(listener: TcpListener, ready_tx: oneshot::Sender<u16>) -> io::Result<bool> {
    let port = listener.local_addr()?.port();
    let _ = ready_tx.send(port);
    let mut handles = Vec::with_capacity(ROUNDS);
    let mut accepted = 0_usize;
    while accepted < ROUNDS {
        let (stream, _) = listener.accept().await?;
        handles.push(tokio::spawn(handle_client(stream)));
        accepted += 1;
    }
    for handle in handles {
        match handle.await {
            Ok(Ok(true)) => {}
            _ => return Ok(false),
        }
    }
    Ok(true)
}

#[tokio::main(flavor = "multi_thread", worker_threads = 4)]
async fn main() {
    let listener = TcpListener::bind(("127.0.0.1", 0)).await.unwrap();
    let (ready_tx, ready_rx) = oneshot::channel::<u16>();
    let server = tokio::spawn(run_server(listener, ready_tx));
    let port = ready_rx.await.unwrap();

    let mut acc = 0_u64;
    let mut batch_start = 0_usize;
    while batch_start < ROUNDS {
        let batch_end = (batch_start + CONCURRENCY).min(ROUNDS);
        let mut handles = Vec::with_capacity(batch_end - batch_start);
        let mut request_index = batch_start;
        while request_index < batch_end {
            handles.push(tokio::spawn(send_request(port, request_index)));
            request_index += 1;
        }
        for handle in handles {
            let (finished_index, body) = handle.await.unwrap().unwrap();
            if body != RESPONSE_BODY {
                std::process::exit(1);
            }
            acc = (acc + REQUEST_BODY.len() as u64 + (finished_index as u64 % 23)) % MODULUS;
        }
        batch_start = batch_end;
    }

    match server.await {
        Ok(Ok(true)) => {}
        _ => std::process::exit(1),
    }

    if acc != EXPECTED {
        std::process::exit(1);
    }
}
