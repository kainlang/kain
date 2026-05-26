pub fn normalize_script_source(source: String) -> String {
    let source = source.trim_start_matches('\u{feff}').to_string();
    if let Some(rest) = source.strip_prefix("#!") {
        if let Some(newline_index) = rest.find('\n') {
            rest[(newline_index + 1)..].to_string()
        } else {
            String::new()
        }
    } else {
        source
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn removes_utf8_bom() {
        assert_eq!(
            normalize_script_source("\u{feff}fn main():\n".to_string()),
            "fn main():\n"
        );
    }

    #[test]
    fn removes_shebang_line() {
        assert_eq!(
            normalize_script_source("#!/usr/bin/env kn\nfn main():\n".to_string()),
            "fn main():\n"
        );
    }
}
