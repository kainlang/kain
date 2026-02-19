//! Custom Viewport Generation
//!
//! Generates SEditorViewport and FEditorViewportClient subclasses
//! for custom preview rendering (materials, meshes, particles, etc.)
//!
//! Supports:
//! - @viewport structs → SEditorViewport + FEditorViewportClient
//! - @preview_mesh fields → preview mesh setup
//! - @camera fields → camera configuration
//! - Preview scene with configurable lighting

use kain_core::ast::{Field, Struct, Type};
use kain_core::types::TypedStruct;

pub struct ViewportGenerator {
    lines: Vec<String>,
    indent: usize,
}

impl ViewportGenerator {
    pub fn new() -> Self {
        Self {
            lines: Vec::new(),
            indent: 0,
        }
    }
    
    /// Generate SEditorViewport + FEditorViewportClient from a @viewport struct
    pub fn generate_viewport(&mut self, st: &TypedStruct) -> (String, String) {
        let viewport_name = &st.ast.name;
        let widget_class = format!("S{}", viewport_name);
        let client_class = format!("F{}Client", viewport_name);
        
        let header = self.generate_header(&st.ast, &widget_class, &client_class);
        let source = self.generate_source(&st.ast, &widget_class, &client_class);
        
        (header, source)
    }
    
    fn generate_header(&mut self, st: &Struct, widget_class: &str, client_class: &str) -> String {
        self.lines.clear();
        self.indent = 0;
        
        // Detect features from fields
        let has_preview_mesh = st.fields.iter().any(|f| f.attributes.iter().any(|a| a.name == "preview_mesh"));
        let has_camera = st.fields.iter().any(|f| f.attributes.iter().any(|a| a.name == "camera"));
        
        // Forward declare viewport widget so client can reference it in constructor
        self.push_line(&format!("class {};", widget_class));
        self.push_line("");
        
        // === Viewport Client ===
        self.push_line(&format!("class {} : public FEditorViewportClient", client_class));
        self.push_line("{");
        self.push_line("public:");
        self.indent += 1;
        
        self.push_line(&format!(
            "{}(FPreviewScene* InPreviewScene, const TSharedRef<{}>& InViewportWidget);",
            client_class, widget_class
        ));
        self.push_line("");
        
        // Tick override
        self.push_line("virtual void Tick(float DeltaSeconds) override;");
        self.push_line("");
        
        // Input overrides
        self.push_line("virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;");
        self.push_line("");
        
        // Custom methods from struct
        for method in &st.methods {
            let params = method.params.iter()
                .map(|p| format!("{} {}", self.map_type(&p.ty), p.name))
                .collect::<Vec<_>>()
                .join(", ");
            self.push_line(&format!("void {}({});", method.name, params));
        }
        
        self.indent -= 1;
        self.push_line("private:");
        self.indent += 1;
        
        if has_preview_mesh {
            self.push_line("UStaticMeshComponent* PreviewMeshComponent;");
        }
        
        self.indent -= 1;
        self.push_line("};");
        self.push_line("");
        
        // === Viewport Widget ===
        self.push_line(&format!("class {} : public SEditorViewport", widget_class));
        self.push_line("{");
        self.push_line("public:");
        self.indent += 1;
        
        self.push_line(&format!("SLATE_BEGIN_ARGS({}) {{}}", widget_class));
        self.push_line("SLATE_END_ARGS()");
        self.push_line("");
        
        self.push_line("void Construct(const FArguments& InArgs);");
        self.push_line("");
        
        // SEditorViewport overrides
        self.push_line("virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;");
        self.push_line("");
        
        // Access methods
        self.push_line(&format!("TSharedPtr<{}> GetViewportClient() const {{ return ViewportClient; }}", client_class));
        self.push_line("FPreviewScene* GetPreviewScene() const { return PreviewScene.Get(); }");
        
        self.indent -= 1;
        self.push_line("private:");
        self.indent += 1;
        
        self.push_line(&format!("TSharedPtr<{}> ViewportClient;", client_class));
        self.push_line("TSharedPtr<FPreviewScene> PreviewScene;");
        
        self.indent -= 1;
        self.push_line("};");
        
        self.lines.join("\n")
    }
    
    fn generate_source(&mut self, st: &Struct, widget_class: &str, client_class: &str) -> String {
        self.lines.clear();
        self.indent = 0;
        
        let has_preview_mesh = st.fields.iter().any(|f| f.attributes.iter().any(|a| a.name == "preview_mesh"));
        
        // === Viewport Widget Construct ===
        self.push_line(&format!("void {}::Construct(const FArguments& InArgs)", widget_class));
        self.push_line("{");
        self.indent += 1;
        
        self.push_line("PreviewScene = MakeShareable(new FPreviewScene(FPreviewScene::ConstructionValues()));");
        self.push_line("");
        self.push_line("SEditorViewport::Construct(SEditorViewport::FArguments());");
        
        self.indent -= 1;
        self.push_line("}");
        self.push_line("");
        
        // === MakeEditorViewportClient ===
        self.push_line(&format!(
            "TSharedRef<FEditorViewportClient> {}::MakeEditorViewportClient()",
            widget_class
        ));
        self.push_line("{");
        self.indent += 1;
        
        self.push_line(&format!(
            "ViewportClient = MakeShareable(new {}(PreviewScene.Get(), SharedThis(this)));",
            client_class
        ));
        self.push_line("return ViewportClient.ToSharedRef();");
        
        self.indent -= 1;
        self.push_line("}");
        self.push_line("");
        
        // === Viewport Client Constructor ===
        self.push_line(&format!(
            "{}::{}(FPreviewScene* InPreviewScene, const TSharedRef<{}>& InViewportWidget)",
            client_class, client_class, widget_class
        ));
        self.push_line(&format!("\t: FEditorViewportClient(nullptr, InPreviewScene, StaticCastSharedRef<SEditorViewport>(InViewportWidget))"));
        self.push_line("{");
        self.indent += 1;
        
        // Camera defaults
        self.push_line("SetViewLocation(FVector(-200.0f, 0.0f, 100.0f));");
        self.push_line("SetViewRotation(FRotator(-15.0f, 0.0f, 0.0f));");
        self.push_line("SetRealtime(true);");
        self.push_line("");
        
        // Scene lighting
        self.push_line("// Setup default lighting via preview scene");
        self.push_line("if (InPreviewScene)");
        self.push_line("{");
        self.indent += 1;
        self.push_line("InPreviewScene->SetLightDirection(FRotator(-45.0f, 30.0f, 0.0f));");
        self.indent -= 1;
        self.push_line("}");
        
        if has_preview_mesh {
            self.push_line("");
            self.push_line("// Preview mesh initialization");
            self.push_line("PreviewMeshComponent = NewObject<UStaticMeshComponent>();");
            self.push_line("InPreviewScene->AddComponent(PreviewMeshComponent, FTransform::Identity);");
        }
        
        self.indent -= 1;
        self.push_line("}");
        self.push_line("");
        
        // === Tick ===
        self.push_line(&format!("void {}::Tick(float DeltaSeconds)", client_class));
        self.push_line("{");
        self.indent += 1;
        self.push_line("FEditorViewportClient::Tick(DeltaSeconds);");
        self.push_line("Invalidate();");
        self.indent -= 1;
        self.push_line("}");
        self.push_line("");
        
        // === ProcessClick ===
        self.push_line(&format!(
            "void {}::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)",
            client_class
        ));
        self.push_line("{");
        self.indent += 1;
        self.push_line("FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);");
        self.indent -= 1;
        self.push_line("}");
        self.push_line("");
        
        // === Custom methods ===
        for method in &st.methods {
            let params = method.params.iter()
                .map(|p| format!("{} {}", self.map_type(&p.ty), p.name))
                .collect::<Vec<_>>()
                .join(", ");
            self.push_line(&format!("void {}::{}({})", client_class, method.name, params));
            self.push_line("{");
            self.indent += 1;
            
            // Generate method body based on method name patterns
            if method.name.starts_with("Set") || method.name.starts_with("Update") {
                self.push_line("// Update viewport state");
                self.push_line("Invalidate();");
            } else if method.name.starts_with("Get") || method.name.starts_with("Query") {
                self.push_line("// Query viewport state");
                if let Some(ret_ty) = &method.return_type {
                    let cpp_type = self.map_type(ret_ty);
                    if cpp_type.contains("bool") {
                        self.push_line("return false;");
                    } else if cpp_type.contains("int") || cpp_type.contains("float") {
                        self.push_line("return 0;");
                    } else if cpp_type.contains("FVector") {
                        self.push_line("return FVector::ZeroVector;");
                    } else if cpp_type.contains("FRotator") {
                        self.push_line("return FRotator::ZeroRotator;");
                    } else {
                        self.push_line(&format!("return {}();", cpp_type));
                    }
                }
            } else {
                self.push_line("// Custom viewport operation");
                self.push_line("Invalidate();");
            }
            
            self.indent -= 1;
            self.push_line("}");
            self.push_line("");
        }
        
        self.lines.join("\n")
    }
    
    fn map_type(&self, ty: &Type) -> String {
        match ty {
            Type::Named { name, .. } => match name.as_str() {
                "Int" | "int" => "int32".to_string(),
                "Float" | "float" => "float".to_string(),
                "Bool" | "bool" => "bool".to_string(),
                "String" | "str" => "FString".to_string(),
                "Text" => "FText".to_string(),
                "Name" => "FName".to_string(),
                "Vec2" => "FVector2D".to_string(),
                "Vec3" | "Vector" => "FVector".to_string(),
                "Vec4" => "FVector4".to_string(),
                "Rot" | "Rotator" => "FRotator".to_string(),
                "Quat" => "FQuat".to_string(),
                "Transform" => "FTransform".to_string(),
                "Color" | "LinearColor" => "FLinearColor".to_string(),
                "StaticMesh" => "UStaticMesh*".to_string(),
                "Material" | "MyMaterial" => "UMaterialInterface*".to_string(),
                _ => name.clone(),
            },
            _ => "auto".to_string(),
        }
    }
    
    fn push_line(&mut self, line: &str) {
        let indent_str = "\t".repeat(self.indent);
        self.lines.push(format!("{}{}", indent_str, line));
    }
}
