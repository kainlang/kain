#include "SketchfabImporterEditorModule.h"
#include "SSketchfabBrowser.h"
#include "LevelEditor.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "IMergeActorsModule.h"

#define LOCTEXT_NAMESPACE "SketchfabImporterEditor"

static const FName SketchfabBrowserTabName("SketchfabBrowser");

void FSketchfabImporterEditorModule::StartupModule()
{
	// Register tab spawner
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		SketchfabBrowserTabName,
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
		{
			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				[
					SNew(SSketchfabBrowser)
				];
		})
	)
	.SetDisplayName(LOCTEXT("SketchfabBrowserTitle", "Sketchfab Browser"))
	.SetMenuType(ETabSpawnerMenuType::Hidden);
	
	// Register menu
	RegisterMenus();
}

void FSketchfabImporterEditorModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SketchfabBrowserTabName);
}

void FSketchfabImporterEditorModule::RegisterMenus()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSketchfabImporterEditorModule::OnSketchfabBrowserClicked));
	
	// Add to Window Menu
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
		
		Section.AddMenuEntry(
			"SketchfabBrowser",
			LOCTEXT("SketchfabBrowser", "Sketchfab Browser"),
			LOCTEXT("SketchfabBrowserTooltip", "Open the Sketchfab model browser"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"),
			FUIAction(FExecuteAction::CreateRaw(this, &FSketchfabImporterEditorModule::OnSketchfabBrowserClicked))
		);
	}

	// Add to Main Toolbar
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginOperations");
		
		FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			"SketchfabBrowser",
			FUIAction(FExecuteAction::CreateRaw(this, &FSketchfabImporterEditorModule::OnSketchfabBrowserClicked)),
			LOCTEXT("SketchfabBrowser_Label", "Sketchfab"),
			LOCTEXT("SketchfabBrowser_Tooltip", "Open Sketchfab Browser"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details") // Using a details icon as a placeholder
		));
		Entry.StyleNameOverride = "CalloutToolbar";
		
		// Combine Meshes Button
		FToolMenuEntry& CombineEntry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			"CombineMeshes",
			FUIAction(FExecuteAction::CreateRaw(this, &FSketchfabImporterEditorModule::OnCombineMeshesClicked)),
			LOCTEXT("CombineMeshes_Label", "Combine"),
			LOCTEXT("CombineMeshes_Tooltip", "Open Merge Actors Tool"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.MergeActors")
		));
		CombineEntry.StyleNameOverride = "CalloutToolbar";
	}
}

void FSketchfabImporterEditorModule::OnSketchfabBrowserClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(SketchfabBrowserTabName);
}

void FSketchfabImporterEditorModule::OnCombineMeshesClicked()
{
	// Ensure the module is loaded so the tab spawner is registered
	FModuleManager::Get().LoadModule("MergeActors");
	FGlobalTabmanager::Get()->TryInvokeTab(FName("MergeActors"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSketchfabImporterEditorModule, SketchfabImporterEditor)
