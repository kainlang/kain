//! Slate Style Management - Single Source of Truth
//!
//! Generates FSlateStyleSet registration code from KAIN style definitions.
//! Allows type-safe icon/brush references: Icon("Project.Save") compiles to
//! FAppStyle::Get().GetBrush("Project.Save")

use std::collections::HashMap;

/// Style resource type
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum StyleResourceType {
    Brush,
    Icon,
    Font,
    Color,
    Sound,
}

/// Style resource definition
#[derive(Debug, Clone)]
pub struct StyleResource {
    pub name: String,
    pub resource_type: StyleResourceType,
    pub path: String,
    pub size: Option<(u32, u32)>,
}

/// Style set generator
pub struct StyleGenerator {
    /// Style set name
    style_name: String,
    /// Resources by name
    resources: HashMap<String, StyleResource>,
    /// Generated code lines
    lines: Vec<String>,
    indent: usize,
}

impl StyleGenerator {
    pub fn new(style_name: &str) -> Self {
        Self {
            style_name: style_name.to_string(),
            resources: HashMap::new(),
            lines: Vec::new(),
            indent: 0,
        }
    }
    
    /// Add a style resource
    pub fn add_resource(&mut self, resource: StyleResource) {
        self.resources.insert(resource.name.clone(), resource);
    }
    
    /// Generate style set header
    pub fn generate_header(&mut self) -> String {
        self.lines.clear();
        
        let class_name = format!("F{}Style", self.style_name);
        
        self.push_line(&format!("class {}", class_name));
        self.push_line("{");
        self.push_line("public:");
        self.indent += 1;
        
        self.push_line("static void Initialize();");
        self.push_line("static void Shutdown();");
        self.push_line("");
        self.push_line("static const ISlateStyle& Get();");
        self.push_line("static FName GetStyleSetName();");
        self.push_line("");
        
        // Collect accessor declarations
        let mut accessor_decls = Vec::new();
        for (name, resource) in &self.resources {
            let method_name = self.to_method_name(name);
            match resource.resource_type {
                StyleResourceType::Brush | StyleResourceType::Icon => {
                    accessor_decls.push(format!("static const FSlateBrush* Get{}Brush();", method_name));
                }
                StyleResourceType::Font => {
                    accessor_decls.push(format!("static FSlateFontInfo Get{}Font();", method_name));
                }
                StyleResourceType::Color => {
                    accessor_decls.push(format!("static FSlateColor Get{}Color();", method_name));
                }
                StyleResourceType::Sound => {
                    accessor_decls.push(format!("static FSlateSound Get{}Sound();", method_name));
                }
            }
        }
        
        for decl in accessor_decls {
            self.push_line(&decl);
        }
        
        self.push_line("");
        self.push_line("private:");
        self.push_line("static TSharedPtr<FSlateStyleSet> StyleInstance;");
        
        self.indent -= 1;
        self.push_line("};");
        
        self.lines.join("\n")
    }
    
    /// Generate style set implementation
    pub fn generate_implementation(&mut self) -> String {
        self.lines.clear();
        
        let class_name = format!("F{}Style", self.style_name);
        
        // Static instance
        self.push_line(&format!("TSharedPtr<FSlateStyleSet> {}::StyleInstance = nullptr;", class_name));
        self.push_line("");
        
        // Initialize
        self.push_line(&format!("void {}::Initialize()", class_name));
        self.push_line("{");
        self.indent += 1;
        self.push_line("if (!StyleInstance.IsValid())");
        self.push_line("{");
        self.indent += 1;
        self.push_line(&format!("StyleInstance = MakeShareable(new FSlateStyleSet(GetStyleSetName()));"));
        self.push_line("");
        
        // Collect registrations
        let mut registration_lines = Vec::new();
        for (name, resource) in &self.resources {
            match resource.resource_type {
                StyleResourceType::Brush | StyleResourceType::Icon => {
                    if let Some((w, h)) = resource.size {
                        registration_lines.push(format!(
                            "StyleInstance->Set(\"{}\", new IMAGE_BRUSH(\"{}\", FVector2D({}, {})));",
                            name, resource.path, w, h
                        ));
                    } else {
                        registration_lines.push(format!(
                            "StyleInstance->Set(\"{}\", new IMAGE_BRUSH(\"{}\", Icon40x40));",
                            name, resource.path
                        ));
                    }
                }
                StyleResourceType::Font => {
                    registration_lines.push(format!(
                        "StyleInstance->Set(\"{}\", FSlateFontInfo(\"{}\", 10));",
                        name, resource.path
                    ));
                }
                StyleResourceType::Color => {
                    registration_lines.push(format!(
                        "StyleInstance->Set(\"{}\", FSlateColor(FLinearColor::White));",
                        name
                    ));
                }
                StyleResourceType::Sound => {
                    registration_lines.push(format!(
                        "StyleInstance->Set(\"{}\", FSlateSound::FromName(\"{}\"));",
                        name, resource.path
                    ));
                }
            }
        }
        
        for line in registration_lines {
            self.push_line(&line);
        }
        
        self.push_line("");
        self.push_line("FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);");
        self.indent -= 1;
        self.push_line("}");
        self.indent -= 1;
        self.push_line("}");
        self.push_line("");
        
        // Shutdown
        self.push_line(&format!("void {}::Shutdown()", class_name));
        self.push_line("{");
        self.indent += 1;
        self.push_line("FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);");
        self.push_line("ensure(StyleInstance.IsUnique());");
        self.push_line("StyleInstance.Reset();");
        self.indent -= 1;
        self.push_line("}");
        self.push_line("");
        
        // Get
        self.push_line(&format!("const ISlateStyle& {}::Get()", class_name));
        self.push_line("{");
        self.indent += 1;
        self.push_line("return *StyleInstance;");
        self.indent -= 1;
        self.push_line("}");
        self.push_line("");
        
        // GetStyleSetName
        self.push_line(&format!("FName {}::GetStyleSetName()", class_name));
        self.push_line("{");
        self.indent += 1;
        self.push_line(&format!("static FName StyleSetName(TEXT(\"{}\"));", self.style_name));
        self.push_line("return StyleSetName;");
        self.indent -= 1;
        self.push_line("}");
        self.push_line("");
        
        // Collect implementation accessors
        let mut impl_lines = Vec::new();
        for (name, resource) in &self.resources {
            let method_name = self.to_method_name(name);
            match resource.resource_type {
                StyleResourceType::Brush | StyleResourceType::Icon => {
                    impl_lines.push(format!("const FSlateBrush* {}::Get{}Brush()", class_name, method_name));
                    impl_lines.push("{".to_string());
                    impl_lines.push(format!("\treturn StyleInstance->GetBrush(\"{}\");", name));
                    impl_lines.push("}".to_string());
                    impl_lines.push("".to_string());
                }
                StyleResourceType::Font => {
                    impl_lines.push(format!("FSlateFontInfo {}::Get{}Font()", class_name, method_name));
                    impl_lines.push("{".to_string());
                    impl_lines.push(format!("\treturn StyleInstance->GetFontStyle(\"{}\");", name));
                    impl_lines.push("}".to_string());
                    impl_lines.push("".to_string());
                }
                StyleResourceType::Color => {
                    impl_lines.push(format!("FSlateColor {}::Get{}Color()", class_name, method_name));
                    impl_lines.push("{".to_string());
                    impl_lines.push(format!("\treturn StyleInstance->GetSlateColor(\"{}\");", name));
                    impl_lines.push("}".to_string());
                    impl_lines.push("".to_string());
                }
                StyleResourceType::Sound => {
                    impl_lines.push(format!("FSlateSound {}::Get{}Sound()", class_name, method_name));
                    impl_lines.push("{".to_string());
                    impl_lines.push(format!("\treturn StyleInstance->GetSound(\"{}\");", name));
                    impl_lines.push("}".to_string());
                    impl_lines.push("".to_string());
                }
            }
        }
        
        for line in impl_lines {
            self.push_line(&line);
        }
        
        self.lines.join("\n")
    }
    
    fn to_method_name(&self, resource_name: &str) -> String {
        // Convert "Project.Save" to "ProjectSave"
        resource_name.replace(".", "").replace("_", "")
    }
    
    fn push_line(&mut self, line: &str) {
        let indent_str = "\t".repeat(self.indent);
        self.lines.push(format!("{}{}", indent_str, line));
    }
}

/// Parse style definition from KAIN code
pub fn parse_style_definition(content: &str) -> Vec<StyleResource> {
    let mut resources = Vec::new();
    
    // Simple parser for style definitions
    // Format: Icon("Name", "Path/To/Icon.png", 40, 40)
    for line in content.lines() {
        let line = line.trim();
        
        if line.starts_with("Icon(") {
            if let Some(resource) = parse_icon_definition(line) {
                resources.push(resource);
            }
        } else if line.starts_with("Brush(") {
            if let Some(resource) = parse_brush_definition(line) {
                resources.push(resource);
            }
        } else if line.starts_with("Font(") {
            if let Some(resource) = parse_font_definition(line) {
                resources.push(resource);
            }
        } else if line.starts_with("Color(") {
            if let Some(resource) = parse_color_definition(line) {
                resources.push(resource);
            }
        }
    }
    
    resources
}

fn parse_icon_definition(line: &str) -> Option<StyleResource> {
    // Parse: Icon("Name", "Path", 40, 40)
    // Simplified parser - production would use proper parsing
    let parts: Vec<&str> = line.trim_start_matches("Icon(")
        .trim_end_matches(')')
        .split(',')
        .map(|s| s.trim().trim_matches('"'))
        .collect();
    
    if parts.len() >= 2 {
        let name = parts[0].to_string();
        let path = parts[1].to_string();
        let size = if parts.len() >= 4 {
            Some((parts[2].parse().ok()?, parts[3].parse().ok()?))
        } else {
            None
        };
        
        Some(StyleResource {
            name,
            resource_type: StyleResourceType::Icon,
            path,
            size,
        })
    } else {
        None
    }
}

fn parse_brush_definition(line: &str) -> Option<StyleResource> {
    // Similar to icon
    parse_icon_definition(&line.replace("Brush(", "Icon("))
        .map(|mut r| {
            r.resource_type = StyleResourceType::Brush;
            r
        })
}

fn parse_font_definition(line: &str) -> Option<StyleResource> {
    let parts: Vec<&str> = line.trim_start_matches("Font(")
        .trim_end_matches(')')
        .split(',')
        .map(|s| s.trim().trim_matches('"'))
        .collect();
    
    if parts.len() >= 2 {
        Some(StyleResource {
            name: parts[0].to_string(),
            resource_type: StyleResourceType::Font,
            path: parts[1].to_string(),
            size: None,
        })
    } else {
        None
    }
}

fn parse_color_definition(line: &str) -> Option<StyleResource> {
    let parts: Vec<&str> = line.trim_start_matches("Color(")
        .trim_end_matches(')')
        .split(',')
        .map(|s| s.trim().trim_matches('"'))
        .collect();
    
    if !parts.is_empty() {
        Some(StyleResource {
            name: parts[0].to_string(),
            resource_type: StyleResourceType::Color,
            path: String::new(),
            size: None,
        })
    } else {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_icon_parsing() {
        let line = r#"Icon("Project.Save", "Icons/Save.png", 40, 40)"#;
        let resource = parse_icon_definition(line).unwrap();
        
        assert_eq!(resource.name, "Project.Save");
        assert_eq!(resource.path, "Icons/Save.png");
        assert_eq!(resource.size, Some((40, 40)));
    }
    
    #[test]
    fn test_style_generation() {
        let mut gen = StyleGenerator::new("MyEditor");
        gen.add_resource(StyleResource {
            name: "Project.Save".to_string(),
            resource_type: StyleResourceType::Icon,
            path: "Icons/Save.png".to_string(),
            size: Some((40, 40)),
        });
        
        let header = gen.generate_header();
        assert!(header.contains("FMyEditorStyle"));
        assert!(header.contains("GetProjectSaveBrush"));
    }
}
