use std::io::{Read, Write};
use std::net::{Shutdown, TcpStream};
use std::time::Duration;

use actix_web::{web, App, HttpResponse, HttpServer};

const ROUNDS: usize = 320;
const MODULUS: u64 = 1_000_000_007;
const EXPECTED: u64 = 7_019;
const REQUEST_BODY: &str = "framework-ping";
const RESPONSE_BODY: &str = "stack-ok-2026";

async fn bench(body: String) -> HttpResponse {
    if body != REQUEST_BODY {
        return HttpResponse::BadRequest().finish();
    }
    HttpResponse::Ok().body(RESPONSE_BODY)
}

fn send_request(port: u16) -> std::io::Result<String> {
    let mut stream = TcpStream::connect(("127.0.0.1", port))?;
    let request = format!(
        "POST /bench HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
        REQUEST_BODY.len(),
        REQUEST_BODY
    );
    stream.write_all(request.as_bytes())?;
    stream.shutdown(Shutdown::Write)?;
    let mut response_bytes = Vec::<u8>::new();
    stream.read_to_end(&mut response_bytes)?;
    let response_text = String::from_utf8_lossy(&response_bytes).into_owned();
    if let Some(position) = response_text.find("\r\n\r\n") {
        return Ok(response_text[position + 4..].to_string());
    }
    Ok(String::new())
}

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    let server = HttpServer::new(|| App::new().route("/bench", web::post().to(bench)))
        .workers(2)
        .bind(("127.0.0.1", 0))?;
    let port = server.addrs()[0].port();
    let server = server.run();
    let handle = server.handle();
    let join = actix_web::rt::spawn(server);
    actix_web::rt::time::sleep(Duration::from_millis(25)).await;

    let mut acc = 0_u64;
    let mut index = 0_usize;
    while index < ROUNDS {
        let body = send_request(port)?;
        if body != RESPONSE_BODY {
            std::process::exit(1);
        }
        acc = (acc + REQUEST_BODY.len() as u64 + (index as u64 % 17)) % MODULUS;
        index += 1;
    }

    handle.stop(true).await;
    let _ = join.await;

    if acc != EXPECTED {
        std::process::exit(1);
    }
    Ok(())
}
