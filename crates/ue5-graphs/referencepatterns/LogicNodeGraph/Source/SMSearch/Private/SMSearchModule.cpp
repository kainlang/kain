// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMSearchModule.h"

#include "Configuration/SMSearchSettings.h"
#include "Configuration/SMSearchStyle.h"
#include "Search/SMSearch.h"
#include "Search/Views/SSMSearchView.h"
#include "SMSearchLog.h"

#include "ISMSystemEditorModule.h"

#include "ISMUtilityLauncherModule.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ISettingsModule.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SToolTip.h"

#define LOCTEXT_NAMESPACE "SMSearchModule"

DEFINE_LOG_CATEGORY(LogLogicDriverSearch)

const FName FSMSearchModule::TabName = TEXT("LogicDriverSearchTab");
FText TabTitle = LOCTEXT("TabTitle", "Search (Beta)");
FText TabTooltip = LOCTEXT("TabTooltip", "Search exposed property values within Logic Driver assets.");

void FSMSearchModule::StartupModule()
{
	FSMSearchStyle::Initialize();
	RegisterSettings();

	FTabSpawnerEntry& TabSpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabName,
		FOnSpawnTab::CreateStatic(&FSMSearchModule::SpawnSearchInTab))
		.SetDisplayName(TabTitle)
		.SetTooltipText(TabTooltip)
		.SetIcon(FSlateIcon(FSMSearchStyle::GetStyleSetName(), "SMSearch.Tabs.Find"));

	const ISMSystemEditorModule& SMBlueprintEditorModule = FModuleManager::LoadModuleChecked<ISMSystemEditorModule>(LOGICDRIVER_EDITOR_MODULE_NAME);
	TabSpawnerEntry.SetGroup(SMBlueprintEditorModule.GetToolsWorkspaceGroup().ToSharedRef());

	// Add to utility launcher.
	{
		const ISMUtilityLauncherModule& UtilityLauncherModule = FModuleManager::LoadModuleChecked<ISMUtilityLauncherModule>(LOGICDRIVER_UTILITY_LAUNCHER_MODULE_NAME);

		const TSharedRef<FExtender> Extender = MakeShared<FExtender>();
		Extender->AddMenuExtension("Tools", EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(TabTitle,
				TabTooltip,
				FSlateIcon(FSMSearchStyle::GetStyleSetName(), "SMSearch.Tabs.Find"),
				FUIAction(FExecuteAction::CreateLambda([&]()
				{
					FGlobalTabmanager::Get()->TryInvokeTab(GetSearchTabName());
				})));
		}));

		UtilityLauncherModule.GetMenuExtensibilityManager()->AddExtender(Extender);
	}
}

void FSMSearchModule::ShutdownModule()
{
	FSMSearchStyle::Shutdown();
	UnregisterSettings();

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
	}
}

TSharedPtr<ISMSearch> FSMSearchModule::GetSearchInterface() const
{
	if (!SearchInterface.IsValid())
	{
		SearchInterface = MakeShared<FSMSearch>();
	}

	return SearchInterface;
}

void FSMSearchModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Editor", "Plugins", "LogicDriverSearch",
			LOCTEXT("SMSearchSettingsName", "Logic Driver Search"),
			LOCTEXT("SMSearchSettingsDescription", "Manage the search settings for Logic Driver assets."),
			GetMutableDefault<USMSearchSettings>());
	}
}

void FSMSearchModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "Plugins", "LogicDriverSearch");
	}
}

TSharedRef<SDockTab> FSMSearchModule::SpawnSearchInTab(const FSpawnTabArgs& SpawnTabArgs)
{
	const TSharedRef<SDockTab> MajorTab = SNew(SDockTab)
	.TabRole(ETabRole::NomadTab);

	MajorTab->SetTabToolTipWidget(SNew(SToolTip).Text(TabTooltip));
	MajorTab->SetContent(SNew(SSMSearchView));
	return MajorTab;
}

IMPLEMENT_MODULE(FSMSearchModule, SMSearch)

#undef LOCTEXT_NAMESPACE
