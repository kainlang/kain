#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ReplBuildMetadata {
    pub language_name: String,
    pub version: String,
    pub build_number: String,
    pub target_triple: String,
}

impl ReplBuildMetadata {
    pub fn new(
        language_name: impl Into<String>,
        version: impl Into<String>,
        build_number: impl Into<String>,
        target_triple: impl Into<String>,
    ) -> Self {
        Self {
            language_name: language_name.into(),
            version: version.into(),
            build_number: build_number.into(),
            target_triple: target_triple.into(),
        }
    }

    pub fn banner(&self) -> String {
        format!(
            "{} {} (build {}) [{}]",
            self.language_name, self.version, self.build_number, self.target_triple
        )
    }
}

impl Default for ReplBuildMetadata {
    fn default() -> Self {
        Self::new("Kain", "dev", "dev", "unknown")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn renders_existing_cli_banner_shape() {
        let metadata = ReplBuildMetadata::new("Kain", "0.1.0", "77", "x86_64-pc-windows-msvc");
        assert_eq!(
            metadata.banner(),
            "Kain 0.1.0 (build 77) [x86_64-pc-windows-msvc]"
        );
    }
}
