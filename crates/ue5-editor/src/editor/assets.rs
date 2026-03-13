//! Asset Editor Toolkit Generation
//!
//! Generates FAssetEditorToolkit subclasses with viewports, details panels, and toolbars

use crate::editor::asset_editor_ir::{convert_to_asset_editor_ir, AssetEditorIR};
use kain_core::types::TypedStruct;
use ue5::ue5::context::Ue5Context;

/// Asset editor toolkit generator
pub struct AssetEditorGenerator {
    context: Ue5Context,
}

impl AssetEditorGenerator {
    pub fn new(context: Ue5Context) -> Self {
        Self { context }
    }

    /// Generate FAssetEditorToolkit header and source from TypedStruct
    pub fn generate_asset_editor(&mut self, st: &TypedStruct) -> Result<(String, String), String> {
        // Convert AST to IR
        let ir = convert_to_asset_editor_ir(&st.ast, &self.context)?;

        // Generate header and source
        let header = self.generate_header(&ir)?;
        let source = self.generate_source(&ir)?;

        Ok((header, source))
    }

    /// Generate FAssetEditorToolkit header file
    fn generate_header(&self, ir: &AssetEditorIR) -> Result<String, String> {
        let class_name = format!("F{}Toolkit", ir.name);
        let mut header = String::new();

        // Forward declarations for viewport/slate widgets
        if let Some(ref viewport) = ir.viewport {
            header.push_str(&format!("class S{};\n", viewport.viewport_type));
        }
        for widget in &ir.custom_widgets {
            header.push_str(&format!("class S{};\n", widget.widget_type));
        }
        if !ir.custom_widgets.is_empty() || ir.viewport.is_some() {
            header.push_str("\n");
        }

        // Class declaration
        header.push_str(&format!(
            "class {} : public FAssetEditorToolkit\n",
            class_name
        ));
        header.push_str("{\n");
        header.push_str("public:\n");

        // Constructor and destructor
        header.push_str(&format!("\t{}();\n", class_name));
        header.push_str(&format!("\tvirtual ~{}();\n\n", class_name));

        // InitEditor method
        header.push_str("\tvoid InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* InAsset);\n\n");

        // FAssetEditorToolkit interface overrides
        header.push_str("\t// FAssetEditorToolkit interface\n");
        header.push_str("\tvirtual FName GetToolkitFName() const override;\n");
        header.push_str("\tvirtual FText GetBaseToolkitName() const override;\n");
        header.push_str("\tvirtual FString GetWorldCentricTabPrefix() const override;\n");
        header.push_str("\tvirtual FLinearColor GetWorldCentricTabColorScale() const override;\n");
        header.push_str("\tvirtual void OnClose() override;\n\n");

        // Tab spawner methods
        if ir.viewport.is_some() {
            header
                .push_str("\tTSharedRef<SDockTab> SpawnViewportTab(const FSpawnTabArgs& Args);\n");
        }
        if ir.details.is_some() {
            header.push_str("\tTSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);\n");
        }
        for widget in &ir.custom_widgets {
            let method_name = format!("Spawn{}Tab", widget.field_name);
            header.push_str(&format!(
                "\tTSharedRef<SDockTab> {}(const FSpawnTabArgs& Args);\n",
                method_name
            ));
        }
        if ir.viewport.is_some() || ir.details.is_some() || !ir.custom_widgets.is_empty() {
            header.push_str("\n");
        }

        // Custom methods
        for method in &ir.custom_methods {
            let return_type = method
                .return_type
                .as_ref()
                .map(|s| s.as_str())
                .unwrap_or("void");
            let params_str = method
                .params
                .iter()
                .map(|(name, ty)| format!("{} {}", ty, name))
                .collect::<Vec<_>>()
                .join(", ");
            header.push_str(&format!(
                "\t{} {}({});\n",
                return_type, method.name, params_str
            ));
        }

        // Private section
        header.push_str("\nprivate:\n");

        // Tab IDs
        if ir.viewport.is_some() {
            header.push_str("\tstatic const FName ViewportTabId;\n");
        }
        if ir.details.is_some() {
            header.push_str("\tstatic const FName DetailsTabId;\n");
        }
        for widget in &ir.custom_widgets {
            header.push_str(&format!(
                "\tstatic const FName {}TabId;\n",
                widget.field_name
            ));
        }
        if ir.viewport.is_some() || ir.details.is_some() || !ir.custom_widgets.is_empty() {
            header.push_str("\n");
        }

        // Member variables
        if ir.viewport.is_some() {
            header.push_str("\tTSharedPtr<SWidget> ViewportWidget;\n");
        }
        if ir.details.is_some() {
            header.push_str("\tTSharedPtr<IDetailsView> DetailsView;\n");
        }
        for widget in &ir.custom_widgets {
            header.push_str(&format!(
                "\tTSharedPtr<S{}> {}Widget;\n",
                widget.widget_type, widget.field_name
            ));
        }
        header.push_str("\tTWeakObjectPtr<UObject> EditingAsset;\n");

        header.push_str("};\n");

        Ok(header)
    }

    /// Generate FAssetEditorToolkit source file
    fn generate_source(&self, ir: &AssetEditorIR) -> Result<String, String> {
        let class_name = format!("F{}Toolkit", ir.name);
        let mut source = String::new();

        // Tab ID definitions
        if let Some(ref viewport) = ir.viewport {
            source.push_str(&format!(
                "const FName {}::ViewportTabId(TEXT(\"{}\"));\n",
                class_name, viewport.tab_id
            ));
        }
        if let Some(ref details) = ir.details {
            source.push_str(&format!(
                "const FName {}::DetailsTabId(TEXT(\"{}\"));\n",
                class_name, details.tab_id
            ));
        }
        for widget in &ir.custom_widgets {
            source.push_str(&format!(
                "const FName {}::{}TabId(TEXT(\"{}\"));\n",
                class_name, widget.field_name, widget.tab_id
            ));
        }
        if ir.viewport.is_some() || ir.details.is_some() || !ir.custom_widgets.is_empty() {
            source.push_str("\n");
        }

        // Constructor and destructor
        source.push_str(&format!("{}::{}() {{}}\n", class_name, class_name));
        source.push_str(&format!("{}::~{}() {{}}\n\n", class_name, class_name));

        // InitEditor implementation
        source.push_str(&self.generate_init_editor(ir, &class_name)?);

        // Tab spawner implementations
        if let Some(ref viewport) = ir.viewport {
            source.push_str(&self.generate_viewport_spawner(ir, &class_name, viewport)?);
        }
        if let Some(ref details) = ir.details {
            source.push_str(&self.generate_details_spawner(ir, &class_name, details)?);
        }
        for widget in &ir.custom_widgets {
            source.push_str(&self.generate_custom_widget_spawner(ir, &class_name, widget)?);
        }

        // FAssetEditorToolkit interface implementations
        source.push_str(&format!("FName {}::GetToolkitFName() const\n", class_name));
        source.push_str("{\n");
        source.push_str(&format!("\treturn FName(\"{}\");\n", ir.name));
        source.push_str("}\n\n");

        source.push_str(&format!(
            "FText {}::GetBaseToolkitName() const\n",
            class_name
        ));
        source.push_str("{\n");
        source.push_str(&format!(
            "\treturn FText::FromString(TEXT(\"{}\"));\n",
            ir.name
        ));
        source.push_str("}\n\n");

        source.push_str(&format!(
            "FString {}::GetWorldCentricTabPrefix() const\n",
            class_name
        ));
        source.push_str("{\n");
        source.push_str(&format!("\treturn TEXT(\"{}\");\n", ir.name));
        source.push_str("}\n\n");

        source.push_str(&format!(
            "FLinearColor {}::GetWorldCentricTabColorScale() const\n",
            class_name
        ));
        source.push_str("{\n");
        source.push_str("\treturn FLinearColor::White;\n");
        source.push_str("}\n\n");

        source.push_str(&format!("void {}::OnClose()\n", class_name));
        source.push_str("{\n");
        source.push_str("\tFAssetEditorToolkit::OnClose();\n");
        source.push_str("}\n\n");

        // Custom method implementations
        for method in &ir.custom_methods {
            let return_type = method
                .return_type
                .as_ref()
                .map(|s| s.as_str())
                .unwrap_or("void");
            let params_str = method
                .params
                .iter()
                .map(|(name, ty)| format!("{} {}", ty, name))
                .collect::<Vec<_>>()
                .join(", ");
            source.push_str(&format!(
                "{} {}::{}({})\n",
                return_type, class_name, method.name, params_str
            ));
            source.push_str("{\n");
            source.push_str(&format!("\t{}\n", method.body));
            source.push_str("}\n\n");
        }

        Ok(source)
    }

    /// Generate InitEditor method implementation
    fn generate_init_editor(&self, ir: &AssetEditorIR, class_name: &str) -> Result<String, String> {
        let mut code = String::new();

        code.push_str(&format!("void {}::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* InAsset)\n", class_name));
        code.push_str("{\n");
        code.push_str("\tEditingAsset = InAsset;\n\n");

        // Create tab layout
        code.push_str(&format!("\tconst TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT(\"{}Layout\"))\n", ir.name));
        code.push_str("\t\t->AddArea\n");
        code.push_str("\t\t(\n");
        code.push_str("\t\t\tFTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)\n");

        // Add tabs based on layout
        match ir.layout.arrangement {
            crate::editor::asset_editor_ir::TabArrangement::ViewportDetailsHorizontal => {
                if ir.viewport.is_some() && ir.details.is_some() {
                    code.push_str("\t\t\t\t->Split\n");
                    code.push_str("\t\t\t\t(\n");
                    code.push_str(
                        "\t\t\t\t\tFTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)\n",
                    );
                    if let Some(ref viewport) = ir.viewport {
                        code.push_str(&format!("\t\t\t\t\t\t->Split(FTabManager::NewStack()->AddTab(ViewportTabId, ETabState::OpenedTab)->SetSizeCoefficient({}))\n", 
                            viewport.size_coefficient));
                    }
                    if let Some(ref details) = ir.details {
                        code.push_str(&format!("\t\t\t\t\t\t->Split(FTabManager::NewStack()->AddTab(DetailsTabId, ETabState::OpenedTab)->SetSizeCoefficient({}))\n", 
                            details.size_coefficient));
                    }
                    code.push_str("\t\t\t\t)\n");
                }
            }
            _ => {
                // Single stack or custom arrangement
                if ir.viewport.is_some() {
                    code.push_str("\t\t\t\t->Split(FTabManager::NewStack()->AddTab(ViewportTabId, ETabState::OpenedTab))\n");
                }
                if ir.details.is_some() {
                    code.push_str("\t\t\t\t->Split(FTabManager::NewStack()->AddTab(DetailsTabId, ETabState::OpenedTab))\n");
                }
            }
        }

        // Add custom widgets
        for widget in &ir.custom_widgets {
            code.push_str(&format!("\t\t\t\t->Split(FTabManager::NewStack()->AddTab({}TabId, ETabState::OpenedTab)->SetSizeCoefficient({}))\n", 
                widget.field_name, widget.size_coefficient));
        }

        code.push_str("\t\t);\n\n");

        // Initialize the toolkit
        code.push_str(&format!("\tInitAssetEditor(Mode, InitToolkitHost, FName(TEXT(\"{}\")), Layout, true, true, InAsset);\n\n", ir.name));

        // Register tab spawners
        if ir.viewport.is_some() {
            code.push_str(&format!("\tTabManager->RegisterTabSpawner(ViewportTabId, FOnSpawnTab::CreateSP(this, &{}::SpawnViewportTab))\n", class_name));
            code.push_str("\t\t.SetDisplayName(FText::FromString(TEXT(\"Viewport\")))\n");
            code.push_str("\t\t.SetGroup(WorkspaceMenuCategory.ToSharedRef());\n");
        }
        if ir.details.is_some() {
            code.push_str(&format!("\tTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &{}::SpawnDetailsTab))\n", class_name));
            code.push_str("\t\t.SetDisplayName(FText::FromString(TEXT(\"Details\")))\n");
            code.push_str("\t\t.SetGroup(WorkspaceMenuCategory.ToSharedRef());\n");
        }
        for widget in &ir.custom_widgets {
            let method_name = format!("Spawn{}Tab", widget.field_name);
            code.push_str(&format!(
                "\tTabManager->RegisterTabSpawner({}TabId, FOnSpawnTab::CreateSP(this, &{}::{}))\n",
                widget.field_name, class_name, method_name
            ));
            code.push_str(&format!(
                "\t\t.SetDisplayName(FText::FromString(TEXT(\"{}\")))\n",
                widget.tab_name
            ));
            code.push_str("\t\t.SetGroup(WorkspaceMenuCategory.ToSharedRef());\n");
        }

        code.push_str("}\n\n");

        Ok(code)
    }

    /// Generate viewport tab spawner
    fn generate_viewport_spawner(
        &self,
        _ir: &AssetEditorIR,
        class_name: &str,
        viewport: &crate::editor::asset_editor_ir::ViewportPanelIR,
    ) -> Result<String, String> {
        let mut code = String::new();

        code.push_str(&format!(
            "TSharedRef<SDockTab> {}::SpawnViewportTab(const FSpawnTabArgs& Args)\n",
            class_name
        ));
        code.push_str("{\n");
        code.push_str(&format!(
            "\tViewportWidget = SNew(S{});\n\n",
            viewport.viewport_type
        ));
        code.push_str("\treturn SNew(SDockTab)\n");
        code.push_str(&format!(
            "\t\t.Label(FText::FromString(TEXT(\"{}\")))\n",
            viewport.tab_name
        ));
        code.push_str("\t\t[\n");
        code.push_str("\t\t\tViewportWidget.ToSharedRef()\n");
        code.push_str("\t\t];\n");
        code.push_str("}\n\n");

        Ok(code)
    }

    /// Generate details tab spawner
    fn generate_details_spawner(
        &self,
        _ir: &AssetEditorIR,
        class_name: &str,
        details: &crate::editor::asset_editor_ir::DetailsPanelIR,
    ) -> Result<String, String> {
        let mut code = String::new();

        code.push_str(&format!(
            "TSharedRef<SDockTab> {}::SpawnDetailsTab(const FSpawnTabArgs& Args)\n",
            class_name
        ));
        code.push_str("{\n");
        code.push_str("\tFPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(\"PropertyEditor\");\n");
        code.push_str("\tFDetailsViewArgs DetailsViewArgs;\n");
        code.push_str("\tDetailsViewArgs.bUpdatesFromSelection = false;\n");
        code.push_str(&format!(
            "\tDetailsViewArgs.bLockable = {};\n",
            if details.lockable { "true" } else { "false" }
        ));
        code.push_str(&format!(
            "\tDetailsViewArgs.NameAreaSettings = {};\n",
            if details.show_name_area {
                "FDetailsViewArgs::ObjectsUseNameArea"
            } else {
                "FDetailsViewArgs::HideNameArea"
            }
        ));
        code.push_str("\n");
        code.push_str("\tDetailsView = PropertyModule.CreateDetailView(DetailsViewArgs);\n");
        code.push_str("\tDetailsView->SetObject(EditingAsset.Get());\n\n");
        code.push_str("\treturn SNew(SDockTab)\n");
        code.push_str(&format!(
            "\t\t.Label(FText::FromString(TEXT(\"{}\")))\n",
            details.tab_name
        ));
        code.push_str("\t\t[\n");
        code.push_str("\t\t\tDetailsView.ToSharedRef()\n");
        code.push_str("\t\t];\n");
        code.push_str("}\n\n");

        Ok(code)
    }

    /// Generate custom widget tab spawner
    fn generate_custom_widget_spawner(
        &self,
        _ir: &AssetEditorIR,
        class_name: &str,
        widget: &crate::editor::asset_editor_ir::CustomWidgetIR,
    ) -> Result<String, String> {
        let mut code = String::new();
        let method_name = format!("Spawn{}Tab", widget.field_name);

        code.push_str(&format!(
            "TSharedRef<SDockTab> {}::{}(const FSpawnTabArgs& Args)\n",
            class_name, method_name
        ));
        code.push_str("{\n");
        code.push_str(&format!(
            "\t{}Widget = SNew(S{});\n\n",
            widget.field_name, widget.widget_type
        ));
        code.push_str("\treturn SNew(SDockTab)\n");
        code.push_str(&format!(
            "\t\t.Label(FText::FromString(TEXT(\"{}\")))\n",
            widget.tab_name
        ));
        code.push_str("\t\t[\n");
        code.push_str(&format!(
            "\t\t\t{}Widget.ToSharedRef()\n",
            widget.field_name
        ));
        code.push_str("\t\t];\n");
        code.push_str("}\n\n");

        Ok(code)
    }
}
