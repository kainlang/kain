//! Header-to-vcpkg-port mapping with a small compile-time override table.
//!
//! The default heuristic: take the first path segment of the header path
//! as the port name. e.g. `<openssl/ssl.h>` -> `openssl`,
//! `<sqlite3.h>` -> `sqlite3`.
//!
//! For the ~30 headers where this heuristic fails (e.g.
//! `<nlohmann/json.hpp>` -> `nlohmann-json`), a small TOML table
//! provides overrides. This is NOT a port registry -- vcpkg has 2000+
//! ports; we only override the exceptions.

use std::collections::HashMap;
use once_cell::sync::Lazy;

static PORT_OVERRIDES: Lazy<HashMap<String, String>> = Lazy::new(|| {
    let toml_str = include_str!("port_overrides.toml");
    parse_overrides(toml_str)
});

fn parse_overrides(toml_str: &str) -> HashMap<String, String> {
    let mut map = HashMap::new();
    let mut in_overrides = false;
    for line in toml_str.lines() {
        let trimmed = line.trim();
        if trimmed == "[overrides]" {
            in_overrides = true;
            continue;
        }
        if trimmed.starts_with('[') {
            in_overrides = false;
            continue;
        }
        if !in_overrides || trimmed.is_empty() || trimmed.starts_with('#') {
            continue;
        }
        if let Some((key, value)) = trimmed.split_once('=') {
            let key = key.trim().trim_matches('"').to_string();
            let value = value.trim().trim_matches('"').to_string();
            map.insert(key, value);
        }
    }
    map
}

/// Map a C header include target to a vcpkg port name.
///
/// The target is the raw string from the include statement:
/// - `"sqlite3.h"` -> `"sqlite3"`
/// - `"openssl/ssl.h"` -> `"openssl"`
/// - `"nlohmann/json.hpp"` -> `"nlohmann-json"` (via override table)
pub fn header_to_port(include_target: &str) -> String {
    // First check the override table (exact match on full path)
    if let Some(port) = PORT_OVERRIDES.get(include_target) {
        return port.clone();
    }

    // Default heuristic: take the first path segment as the port name
    let normalized = include_target.replace('\\', "/");
    if let Some(slash_pos) = normalized.find('/') {
        // Multi-segment: <openssl/ssl.h> -> "openssl"
        normalized[..slash_pos].to_string()
    } else {
        // Single segment: <sqlite3.h> -> "sqlite3"
        let stem = normalized
            .rsplit_once('.')
            .map(|(left, _)| left)
            .unwrap_or(&normalized);
        stem.to_string()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_simple_header_to_port() {
        assert_eq!(header_to_port("sqlite3.h"), "sqlite3");
        assert_eq!(header_to_port("zlib.h"), "zlib");
    }

    #[test]
    fn test_subpath_header_to_port() {
        assert_eq!(header_to_port("openssl/ssl.h"), "openssl");
        assert_eq!(header_to_port("curl/curl.h"), "curl");
        assert_eq!(header_to_port("fmt/core.h"), "fmt");
        assert_eq!(header_to_port("boost/filesystem.hpp"), "boost");
    }

    #[test]
    fn test_override_header_to_port() {
        assert_eq!(header_to_port("nlohmann/json.hpp"), "nlohmann-json");
        assert_eq!(header_to_port("catch2/catch_test_macros.hpp"), "catch2");
        assert_eq!(header_to_port("yaml-cpp/yaml.h"), "yaml-cpp");
        assert_eq!(header_to_port("SDL2/SDL.h"), "sdl2");
    }

    #[test]
    fn test_override_table_loads() {
        // Ensure the override table parses without panicking
        assert!(PORT_OVERRIDES.len() > 20);
    }
}
