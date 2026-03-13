//! Editor Module Code Generation
//!
//! This module generates C++ code for editor modules including:
//! - IModuleInterface subclass
//! - IMPLEMENT_MODULE macro
//! - FUICommandList and TCommands for menu entries
//! - Menu extension registration
//! - Toolbar extension registration
//! - FTSTicker registration for editor updates

use crate::editor::editor_module_ir::{
    EditorModuleIR, MenuEntryIR, ToolbarButtonIR, ToolbarPositionIR,
};

/// Output from editor module code generation
#[derive(Debug, Clone)]
pub struct EditorModuleCodegenOutput {
    /// Header file content
    pub header: String,

    /// Source file content
    pub source: String,

    /// Additional includes needed
    pub includes: Vec<String>,
}

/// Generate editor module code from IR
///
/// # Arguments
/// * `ir` - The editor module intermediate representation
/// * `plugin_name` - Name of the plugin (for API macro)
///
/// # Returns
/// * `EditorModuleCodegenOutput` - Generated header and source files
pub fn generate_editor_module_code(
    ir: &EditorModuleIR,
    plugin_name: &str,
) -> EditorModuleCodegenOutput {
    let class_name = format!("F{}", ir.name);
    let api_macro = format!("{}_API", plugin_name.to_uppercase());

    let mut header = String::new();
    let mut source = String::new();

    // Generate header
    generate_header(ir, &class_name, &api_macro, &mut header);

    // Generate source
    generate_source(ir, &class_name, plugin_name, &mut source);

    EditorModuleCodegenOutput {
        header,
        source,
        includes: vec![
            "CoreMinimal.h".to_string(),
            "Modules/ModuleManager.h".to_string(),
            "LevelEditor.h".to_string(),
            "Framework/Commands/Commands.h".to_string(),
            "Framework/MultiBox/MultiBoxBuilder.h".to_string(),
        ],
    }
}

/// Generate header file content
fn generate_header(ir: &EditorModuleIR, class_name: &str, api_macro: &str, output: &mut String) {
    // Header guard
    output.push_str("#pragma once\n\n");

    // Includes
    output.push_str("#include \"CoreMinimal.h\"\n");
    output.push_str("#include \"Modules/ModuleManager.h\"\n\n");

    // Commands class (if menu entries exist)
    if !ir.menu_entries.is_empty() {
        generate_commands_class(ir, output);
    }

    // Module class
    output.push_str(&format!(
        "class {} {} : public IModuleInterface\n",
        api_macro, class_name
    ));
    output.push_str("{\n");
    output.push_str("public:\n");

    // IModuleInterface overrides
    output.push_str("    /** IModuleInterface implementation */\n");
    output.push_str("    virtual void StartupModule() override;\n");
    output.push_str("    virtual void ShutdownModule() override;\n\n");

    // Menu entry callback methods
    if !ir.menu_entries.is_empty() {
        output.push_str("    /** Menu entry callbacks */\n");
        for menu_entry in &ir.menu_entries {
            output.push_str(&format!("    void {}();\n", menu_entry.method_name));
        }
        output.push_str("\n");
    }

    // Toolbar button callback methods
    if !ir.toolbar_buttons.is_empty() {
        output.push_str("    /** Toolbar button callbacks */\n");
        for toolbar_button in &ir.toolbar_buttons {
            output.push_str(&format!("    void {}();\n", toolbar_button.method_name));
        }
        output.push_str("\n");
    }

    output.push_str("private:\n");

    // Extension registration methods
    if !ir.menu_entries.is_empty() {
        output.push_str("    /** Register menu extensions */\n");
        output.push_str("    void RegisterMenuExtensions();\n\n");
    }

    if !ir.toolbar_buttons.is_empty() {
        output.push_str("    /** Register toolbar extensions */\n");
        output.push_str("    void RegisterToolbarExtensions();\n\n");
    }

    // Menu extender
    if !ir.menu_entries.is_empty() {
        output.push_str("    /** Menu extender */\n");
        output.push_str("    TSharedPtr<FExtender> MenuExtender;\n\n");
    }

    // Toolbar extender
    if !ir.toolbar_buttons.is_empty() {
        output.push_str("    /** Toolbar extender */\n");
        output.push_str("    TSharedPtr<FExtender> ToolbarExtender;\n\n");
    }

    output.push_str("};\n");
}

/// Generate commands class for menu entries
fn generate_commands_class(ir: &EditorModuleIR, output: &mut String) {
    let commands_class_name = format!("F{}Commands", ir.name);

    output.push_str(&format!(
        "class {} : public TCommands<{}>\n",
        commands_class_name, commands_class_name
    ));
    output.push_str("{\n");
    output.push_str("public:\n");

    // Constructor
    output.push_str(&format!("    {}()\n", commands_class_name));
    output.push_str(&format!(
        "        : TCommands<{}>(TEXT(\"{}\"),\n",
        commands_class_name, ir.name
    ));
    output.push_str(&format!(
        "            NSLOCTEXT(\"{}\", \"ContextDescription\", \"{} commands\"),\n",
        ir.name, ir.name
    ));
    output.push_str("            NAME_None,\n");
    output.push_str("            FEditorStyle::GetStyleSetName())\n");
    output.push_str("    {}\n\n");

    // RegisterCommands override
    output.push_str("    virtual void RegisterCommands() override;\n\n");

    // Command declarations
    for menu_entry in &ir.menu_entries {
        let command_name = format!("{}Command", menu_entry.method_name);
        output.push_str(&format!(
            "    TSharedPtr<FUICommandInfo> {};\n",
            command_name
        ));
    }

    output.push_str("};\n\n");
}

/// Generate source file content
fn generate_source(ir: &EditorModuleIR, class_name: &str, plugin_name: &str, output: &mut String) {
    // Include header
    output.push_str(&format!("#include \"{}.h\"\n", ir.name));
    output.push_str("#include \"LevelEditor.h\"\n");
    output.push_str("#include \"Framework/Commands/Commands.h\"\n");
    output.push_str("#include \"Framework/MultiBox/MultiBoxBuilder.h\"\n\n");

    // Commands class implementation (if menu entries exist)
    if !ir.menu_entries.is_empty() {
        generate_commands_implementation(ir, output);
    }

    // StartupModule implementation
    generate_startup_module(ir, class_name, output);

    // ShutdownModule implementation
    generate_shutdown_module(ir, class_name, output);

    // Menu entry callback implementations
    for menu_entry in &ir.menu_entries {
        generate_menu_entry_callback(ir, class_name, menu_entry, output);
    }

    // Toolbar button callback implementations
    for toolbar_button in &ir.toolbar_buttons {
        generate_toolbar_button_callback(ir, class_name, toolbar_button, output);
    }

    // Extension registration implementations
    if !ir.menu_entries.is_empty() {
        generate_menu_extension_registration(ir, class_name, output);
    }

    if !ir.toolbar_buttons.is_empty() {
        generate_toolbar_extension_registration(ir, class_name, output);
    }

    // IMPLEMENT_MODULE macro
    output.push_str(&format!(
        "\nIMPLEMENT_MODULE({}, {})\n",
        class_name, plugin_name
    ));
}

/// Generate commands class RegisterCommands implementation
fn generate_commands_implementation(ir: &EditorModuleIR, output: &mut String) {
    let commands_class_name = format!("F{}Commands", ir.name);

    output.push_str(&format!(
        "void {}::RegisterCommands()\n",
        commands_class_name
    ));
    output.push_str("{\n");

    for menu_entry in &ir.menu_entries {
        let command_name = format!("{}Command", menu_entry.method_name);
        output.push_str(&format!("    UI_COMMAND({}, \"{}\", \"{}\", EUserInterfaceActionType::Button, FInputChord());\n",
            command_name,
            menu_entry.label,
            menu_entry.tooltip.as_deref().unwrap_or(&menu_entry.label)
        ));
    }

    output.push_str("}\n\n");
}

/// Generate StartupModule implementation
fn generate_startup_module(ir: &EditorModuleIR, class_name: &str, output: &mut String) {
    output.push_str(&format!("void {}::StartupModule()\n", class_name));
    output.push_str("{\n");

    // Register commands (if menu entries exist)
    if !ir.menu_entries.is_empty() {
        let commands_class_name = format!("F{}Commands", ir.name);
        output.push_str(&format!("    {}::Register();\n\n", commands_class_name));
    }

    // Register menu extensions
    if !ir.menu_entries.is_empty() {
        output.push_str("    RegisterMenuExtensions();\n");
    }

    // Register toolbar extensions
    if !ir.toolbar_buttons.is_empty() {
        output.push_str("    RegisterToolbarExtensions();\n");
    }

    output.push_str("}\n\n");
}

/// Generate ShutdownModule implementation
fn generate_shutdown_module(ir: &EditorModuleIR, class_name: &str, output: &mut String) {
    output.push_str(&format!("void {}::ShutdownModule()\n", class_name));
    output.push_str("{\n");

    // Unregister commands (if menu entries exist)
    if !ir.menu_entries.is_empty() {
        let commands_class_name = format!("F{}Commands", ir.name);
        output.push_str(&format!("    {}::Unregister();\n\n", commands_class_name));
    }

    // Remove menu extender
    if !ir.menu_entries.is_empty() {
        output.push_str("    if (MenuExtender.IsValid())\n");
        output.push_str("    {\n");
        output.push_str("        FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(\"LevelEditor\");\n");
        output.push_str("        LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender(MenuExtender);\n");
        output.push_str("    }\n\n");
    }

    // Remove toolbar extender
    if !ir.toolbar_buttons.is_empty() {
        output.push_str("    if (ToolbarExtender.IsValid())\n");
        output.push_str("    {\n");
        output.push_str("        FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(\"LevelEditor\");\n");
        output.push_str("        LevelEditorModule.GetToolBarExtensibilityManager()->RemoveExtender(ToolbarExtender);\n");
        output.push_str("    }\n");
    }

    output.push_str("}\n\n");
}

/// Generate menu entry callback implementation
fn generate_menu_entry_callback(
    ir: &EditorModuleIR,
    class_name: &str,
    menu_entry: &MenuEntryIR,
    output: &mut String,
) {
    output.push_str(&format!(
        "void {}::{}()\n",
        class_name, menu_entry.method_name
    ));
    output.push_str("{\n");
    output.push_str(&format!(
        "    // TODO: Implement {} callback\n",
        menu_entry.label
    ));
    output.push_str(&format!(
        "    UE_LOG(LogTemp, Log, TEXT(\"Menu entry clicked: {}\"));\n",
        menu_entry.label
    ));
    output.push_str("}\n\n");
}

/// Generate toolbar button callback implementation
fn generate_toolbar_button_callback(
    ir: &EditorModuleIR,
    class_name: &str,
    toolbar_button: &ToolbarButtonIR,
    output: &mut String,
) {
    output.push_str(&format!(
        "void {}::{}()\n",
        class_name, toolbar_button.method_name
    ));
    output.push_str("{\n");
    let label = toolbar_button.label.as_deref().unwrap_or("Toolbar button");
    output.push_str(&format!("    // TODO: Implement {} callback\n", label));
    output.push_str(&format!(
        "    UE_LOG(LogTemp, Log, TEXT(\"Toolbar button clicked: {}\"));\n",
        label
    ));
    output.push_str("}\n\n");
}

/// Generate menu extension registration
fn generate_menu_extension_registration(
    ir: &EditorModuleIR,
    class_name: &str,
    output: &mut String,
) {
    let commands_class_name = format!("F{}Commands", ir.name);

    output.push_str(&format!("void {}::RegisterMenuExtensions()\n", class_name));
    output.push_str("{\n");
    output.push_str("    MenuExtender = MakeShareable(new FExtender);\n\n");

    // Group menu entries by path
    let mut paths: Vec<String> = ir.menu_entries.iter().map(|e| e.path.clone()).collect();
    paths.sort();
    paths.dedup();

    for path in &paths {
        output.push_str(&format!("    // Menu entries for {}\n", path));
        output.push_str("    MenuExtender->AddMenuExtension(\n");
        output.push_str(&format!("        \"{}\",\n", path));
        output.push_str("        EExtensionHook::After,\n");
        output.push_str(&format!(
            "        {}::Get().GetCommandList(),\n",
            commands_class_name
        ));
        output.push_str(&format!(
            "        FMenuExtensionDelegate::CreateRaw(this, &{}::Extend{}Menu));\n\n",
            class_name,
            path.replace("/", "")
        ));
    }

    output.push_str("    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(\"LevelEditor\");\n");
    output.push_str(
        "    LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);\n",
    );
    output.push_str("}\n\n");

    // Generate menu extension delegate methods
    for path in &paths {
        let entries: Vec<&MenuEntryIR> =
            ir.menu_entries.iter().filter(|e| &e.path == path).collect();

        let method_name = format!("Extend{}Menu", path.replace("/", ""));
        output.push_str(&format!(
            "void {}::{}(FMenuBuilder& MenuBuilder)\n",
            class_name, method_name
        ));
        output.push_str("{\n");

        for entry in entries {
            let command_name = format!("{}Command", entry.method_name);
            output.push_str(&format!(
                "    MenuBuilder.AddMenuEntry(F{}Commands::Get().{});\n",
                ir.name, command_name
            ));
        }

        output.push_str("}\n\n");
    }
}

/// Generate toolbar extension registration
fn generate_toolbar_extension_registration(
    ir: &EditorModuleIR,
    class_name: &str,
    output: &mut String,
) {
    output.push_str(&format!(
        "void {}::RegisterToolbarExtensions()\n",
        class_name
    ));
    output.push_str("{\n");
    output.push_str("    ToolbarExtender = MakeShareable(new FExtender);\n\n");

    // Group toolbar buttons by section
    let mut sections: Vec<String> = ir
        .toolbar_buttons
        .iter()
        .map(|b| b.section.clone())
        .collect();
    sections.sort();
    sections.dedup();

    for section in &sections {
        output.push_str(&format!("    // Toolbar buttons for {} section\n", section));
        output.push_str("    ToolbarExtender->AddToolBarExtension(\n");
        output.push_str(&format!("        \"{}\",\n", section));
        output.push_str("        EExtensionHook::After,\n");
        output.push_str("        nullptr,\n");
        output.push_str(&format!(
            "        FToolBarExtensionDelegate::CreateRaw(this, &{}::Extend{}Toolbar));\n\n",
            class_name, section
        ));
    }

    output.push_str("    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(\"LevelEditor\");\n");
    output.push_str(
        "    LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);\n",
    );
    output.push_str("}\n\n");

    // Generate toolbar extension delegate methods
    for section in &sections {
        let buttons: Vec<&ToolbarButtonIR> = ir
            .toolbar_buttons
            .iter()
            .filter(|b| &b.section == section)
            .collect();

        let method_name = format!("Extend{}Toolbar", section);
        output.push_str(&format!(
            "void {}::{}(FToolBarBuilder& ToolbarBuilder)\n",
            class_name, method_name
        ));
        output.push_str("{\n");

        for button in buttons {
            let label = button.label.as_deref().unwrap_or("");
            let tooltip = button.tooltip.as_deref().unwrap_or(label);

            output.push_str("    ToolbarBuilder.AddToolBarButton(\n");
            output.push_str(&format!(
                "        FUIAction(FExecuteAction::CreateRaw(this, &{}::{})),\n",
                class_name, button.method_name
            ));
            output.push_str(&format!("        NAME_None,\n"));
            output.push_str(&format!(
                "        LOCTEXT(\"{}_Label\", \"{}\"),\n",
                button.method_name, label
            ));
            output.push_str(&format!(
                "        LOCTEXT(\"{}_Tooltip\", \"{}\"),\n",
                button.method_name, tooltip
            ));
            output.push_str(&format!(
                "        FSlateIcon(FEditorStyle::GetStyleSetName(), TEXT(\"{}\")));\n",
                button.icon
            ));
        }

        output.push_str("}\n\n");
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use crate::editor::editor_module_ir::{EditorModuleIR, MenuEntryIR, ToolbarButtonIR};

    fn make_simple_editor_module() -> EditorModuleIR {
        EditorModuleIR {
            name: "WeaponEditorModule".to_string(),
            menu_entries: vec![MenuEntryIR {
                path: "Tools/Weapons".to_string(),
                label: "Open Weapon Editor".to_string(),
                method_name: "on_open_editor".to_string(),
                icon: Some("Icons.Weapon".to_string()),
                tooltip: Some("Open the weapon editor".to_string()),
            }],
            toolbar_buttons: vec![ToolbarButtonIR {
                section: "Content".to_string(),
                label: Some("Quick Create".to_string()),
                icon: "Icons.Weapon".to_string(),
                method_name: "on_quick_create".to_string(),
                tooltip: Some("Quick create weapon".to_string()),
            }],
            toolbar_widgets: vec![],
        }
    }

    #[test]
    fn test_generate_editor_module_header() {
        let ir = make_simple_editor_module();
        let output = generate_editor_module_code(&ir, "TestPlugin");

        // Check header contains expected elements
        assert!(output
            .header
            .contains("class TESTPLUGIN_API FWeaponEditorModule"));
        assert!(output.header.contains("public IModuleInterface"));
        assert!(output
            .header
            .contains("virtual void StartupModule() override"));
        assert!(output
            .header
            .contains("virtual void ShutdownModule() override"));
        assert!(output.header.contains("void on_open_editor()"));
        assert!(output.header.contains("void on_quick_create()"));
        assert!(output.header.contains("TSharedPtr<FExtender> MenuExtender"));
        assert!(output
            .header
            .contains("TSharedPtr<FExtender> ToolbarExtender"));
    }

    #[test]
    fn test_generate_commands_class() {
        let ir = make_simple_editor_module();
        let output = generate_editor_module_code(&ir, "TestPlugin");

        assert!(output.header.contains(
            "class FWeaponEditorModuleCommands : public TCommands<FWeaponEditorModuleCommands>"
        ));
        assert!(output
            .header
            .contains("virtual void RegisterCommands() override"));
        assert!(output
            .header
            .contains("TSharedPtr<FUICommandInfo> on_open_editorCommand"));
    }

    #[test]
    fn test_generate_startup_module() {
        let ir = make_simple_editor_module();
        let output = generate_editor_module_code(&ir, "TestPlugin");

        assert!(output
            .source
            .contains("void FWeaponEditorModule::StartupModule()"));
        assert!(output
            .source
            .contains("FWeaponEditorModuleCommands::Register()"));
        assert!(output.source.contains("RegisterMenuExtensions()"));
        assert!(output.source.contains("RegisterToolbarExtensions()"));
    }

    #[test]
    fn test_generate_shutdown_module() {
        let ir = make_simple_editor_module();
        let output = generate_editor_module_code(&ir, "TestPlugin");

        assert!(output
            .source
            .contains("void FWeaponEditorModule::ShutdownModule()"));
        assert!(output
            .source
            .contains("FWeaponEditorModuleCommands::Unregister()"));
        assert!(output.source.contains("RemoveExtender(MenuExtender)"));
        assert!(output.source.contains("RemoveExtender(ToolbarExtender)"));
    }

    #[test]
    fn test_generate_implement_module() {
        let ir = make_simple_editor_module();
        let output = generate_editor_module_code(&ir, "TestPlugin");

        assert!(output
            .source
            .contains("IMPLEMENT_MODULE(FWeaponEditorModule, TestPlugin)"));
    }

    #[test]
    fn test_generate_menu_entry_callback() {
        let ir = make_simple_editor_module();
        let output = generate_editor_module_code(&ir, "TestPlugin");

        assert!(output
            .source
            .contains("void FWeaponEditorModule::on_open_editor()"));
        assert!(output
            .source
            .contains("Menu entry clicked: Open Weapon Editor"));
    }

    #[test]
    fn test_generate_toolbar_button_callback() {
        let ir = make_simple_editor_module();
        let output = generate_editor_module_code(&ir, "TestPlugin");

        assert!(output
            .source
            .contains("void FWeaponEditorModule::on_quick_create()"));
        assert!(output
            .source
            .contains("Toolbar button clicked: Quick Create"));
    }
}
