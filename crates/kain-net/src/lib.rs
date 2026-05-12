use serde::{Deserialize, Serialize};
use thiserror::Error;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum NetProtocol {
    Tcp,
    Http11,
    Https,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TcpEndpoint {
    pub host: String,
    pub port: i64,
}

impl TcpEndpoint {
    pub fn new(host: impl Into<String>, port: i64) -> Self {
        Self {
            host: host.into(),
            port,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct HttpHeader {
    pub key: String,
    pub value: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct HttpRequestSpec {
    pub method: String,
    pub url: String,
    pub headers: Vec<HttpHeader>,
    pub body_text: String,
    pub timeout_ms: i64,
}

impl HttpRequestSpec {
    pub fn new(method: impl Into<String>, url: impl Into<String>) -> Self {
        Self {
            method: method.into(),
            url: url.into(),
            headers: Vec::new(),
            body_text: String::new(),
            timeout_ms: 30_000,
        }
    }

    pub fn with_header(mut self, key: impl Into<String>, value: impl Into<String>) -> Self {
        self.headers.push(HttpHeader {
            key: key.into(),
            value: value.into(),
        });
        self
    }

    pub fn with_body_text(mut self, body_text: impl Into<String>) -> Self {
        self.body_text = body_text.into();
        self
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct HttpServerSpec {
    pub host: String,
    pub port: i64,
}

impl HttpServerSpec {
    pub fn localhost(port: i64) -> Self {
        Self {
            host: "127.0.0.1".to_string(),
            port,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct HttpRouteSpec {
    pub method: String,
    pub path: String,
    pub actor_id: i64,
    pub message_kind: String,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct TcpConnectionHandle {
    pub id: i64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct TcpListenerHandle {
    pub id: i64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct HttpRequestHandle {
    pub id: i64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct HttpResponseHandle {
    pub id: i64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct HttpServerHandle {
    pub id: i64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum NetLifecycleState {
    Open,
    Closed,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Error)]
pub enum NetError {
    #[error("invalid network handle")]
    InvalidHandle,
    #[error("invalid network specification")]
    InvalidSpec,
    #[error("network subsystem capacity exceeded")]
    CapacityExceeded,
    #[error("network subsystem does not support this platform capability")]
    UnsupportedPlatform,
    #[error("network I/O failed")]
    Io,
    #[error("{message}")]
    Message { message: String },
}

#[cfg(test)]
mod tests {
    use super::{HttpRequestSpec, HttpServerSpec, NetProtocol, TcpEndpoint};

    #[test]
    fn tcp_endpoint_is_plain_host_and_port() {
        let endpoint = TcpEndpoint::new("127.0.0.1", 8080);
        assert_eq!(endpoint.host, "127.0.0.1");
        assert_eq!(endpoint.port, 8080);
    }

    #[test]
    fn http_request_defaults_to_reasonable_timeout() {
        let request = HttpRequestSpec::new("GET", "http://127.0.0.1/");
        assert_eq!(request.method, "GET");
        assert_eq!(request.timeout_ms, 30_000);
        assert!(request.headers.is_empty());
        assert!(request.body_text.is_empty());
    }

    #[test]
    fn http_request_builder_accumulates_headers_and_body() {
        let request = HttpRequestSpec::new("POST", "https://example.test/api")
            .with_header("Content-Type", "text/plain")
            .with_body_text("hello");
        assert_eq!(request.headers.len(), 1);
        assert_eq!(request.headers[0].key, "Content-Type");
        assert_eq!(request.body_text, "hello");
    }

    #[test]
    fn server_localhost_uses_loopback_host() {
        let server = HttpServerSpec::localhost(0);
        assert_eq!(server.host, "127.0.0.1");
        assert_eq!(server.port, 0);
    }

    #[test]
    fn protocol_names_are_stable() {
        assert_eq!(NetProtocol::Tcp, NetProtocol::Tcp);
    }
}
