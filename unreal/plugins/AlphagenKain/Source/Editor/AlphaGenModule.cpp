// Copyright 2026 K-Studio. All Rights Reserved.

#include "AlphaGenModule.h"
#include "AlphaGenStyle.h"
#include "AlphaGenCommands.h"
#include "Widgets/SAlphaGenWidget.h"

#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "LevelEditor.h"
#include "Editor.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FAlphaGenModule"

const FName FAlphaGenModule::AlphaGenTabName(TEXT("AlphaGenTab"));

void FAlphaGenModule::StartupModule()
{
	// Register shader directory for compute shaders - use IPluginManager to get actual plugin location
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AlphaGen"));
	if (Plugin.IsValid())
	{
		FString PluginShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/AlphaGen"), PluginShaderDir);
	}
	
	// Defer all UI/editor registration until engine is fully initialized
	// WorkspaceMenuStructure and other editor modules aren't ready at PostConfigInit
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FAlphaGenModule::OnPostEngineInit);
}

void FAlphaGenModule::OnPostEngineInit()
{
	// Now it's safe to register editor UI elements
	RegisterStyleSet();
	FAlphaGenCommands::Register();
	RegisterTabSpawner();
	
	// Register toolbar extension after UToolMenus is ready
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAlphaGenModule::RegisterToolbarExtension));
}

void FAlphaGenModule::ShutdownModule()
{
	// Unregister in reverse order
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	
	UnregisterTabSpawner();
	
	FAlphaGenCommands::Unregister();
	
	UnregisterStyleSet();
}

FAlphaGenModule& FAlphaGenModule::Get()
{
	return FModuleManager::LoadModuleChecked<FAlphaGenModule>("AlphaGenEditor");
}

bool FAlphaGenModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("AlphaGenEditor");
}

void FAlphaGenModule::RegisterToolbarExtension()
{
	// Get the LevelEditor toolbar menu
	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	
	if (ToolbarMenu)
	{
		// Add our section
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("AlphaGen");
		
		// Simple toolbar button - click to open, zero friction
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			"AlphaGenButton",
			FUIAction(
				FExecuteAction::CreateRaw(this, &FAlphaGenModule::OnToolbarButtonClicked),
				FCanExecuteAction()
			),
			LOCTEXT("AlphaGenButtonLabel", "AlphaGen"),
			LOCTEXT("AlphaGenButtonTooltip", "Open AlphaGen procedural texture generator"),
			FSlateIcon(FAlphaGenStyle::GetStyleSetName(), "AlphaGen.ToolbarIcon")
		));
	}
}

void FAlphaGenModule::UnregisterToolbarExtension()
{
	// Handled by UToolMenus::UnregisterOwner
}

void FAlphaGenModule::RegisterStyleSet()
{
	FAlphaGenStyle::Initialize();
	FAlphaGenStyle::ReloadTextures();
}

void FAlphaGenModule::UnregisterStyleSet()
{
	FAlphaGenStyle::Shutdown();
}

void FAlphaGenModule::RegisterTabSpawner()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		AlphaGenTabName,
		FOnSpawnTab::CreateRaw(this, &FAlphaGenModule::SpawnAlphaGenTab)
	)
	.SetDisplayName(LOCTEXT("TabTitle", "AlphaGen"))
	.SetTooltipText(LOCTEXT("TabTooltip", "Procedural alpha texture generation toolkit"))
	.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
	.SetIcon(FSlateIcon(FAlphaGenStyle::GetStyleSetName(), "AlphaGen.MenuIcon"));
}

void FAlphaGenModule::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AlphaGenTabName);
}

void FAlphaGenModule::OnToolbarButtonClicked()
{
	// Invoke the tab - this will either focus an existing one or spawn a new one
	FGlobalTabmanager::Get()->TryInvokeTab(AlphaGenTabName);
}

TSharedRef<SDockTab> FAlphaGenModule::SpawnAlphaGenTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SAlphaGenWidget)
		];
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAlphaGenModule, AlphaGenEditor)
