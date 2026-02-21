// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMContentEditorModule.h"

#include "SMContentEditorLog.h"
#include "Assets/SMContentAsset.h"
#include "Assets/SMContentAssetFactory.h"
#include "Commands/SMContentEditorCommands.h"
#include "Configuration/SMContentEditorStyle.h"
#include "Customization/SMKeyNameCustomization.h"

#include "ISMSystemEditorModule.h"

#include "ISMUtilityLauncherModule.h"

#include "PropertyEditorModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/PackageName.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/SavePackage.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "SMContentEditorModule"

DEFINE_LOG_CATEGORY(LogLogicDriverContentEditor);

static bool GEnableLogicDriverContentCreation = false;
static FAutoConsoleVariableRef CVarLogicDriverContentCreation(
	TEXT("LogicDriver.EnableContentCreation"),
	GEnableLogicDriverContentCreation,
	TEXT("If new content samples should be allowed to be created."),
	ECVF_Default
);

void FSMContentEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	FSMContentEditorCommands::Register();
	CommandList = MakeShared<FUICommandList>();
	BindCommands();

	FSMContentEditorStyle::Initialize();

	const FSMContentEditorCommands& Commands = FSMContentEditorCommands::Get();

	FTabSpawnerEntry& TabSpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TEXT("InstallContent"),
	FOnSpawnTab::CreateRaw(this, &FSMContentEditorModule::SpawnContentInTab))
	.SetDisplayName(Commands.InstallContent->GetLabel())
	.SetTooltipText(Commands.InstallContent->GetDescription())
	.SetIcon(FSlateIcon(FSMContentEditorStyle::GetStyleSetName(), TEXT("InstallContentIcon")));

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(FSMInputActionWrapper::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSMKeyNameCustomization::MakeInstance));

	const ISMSystemEditorModule& SMBlueprintEditorModule = FModuleManager::LoadModuleChecked<ISMSystemEditorModule>(LOGICDRIVER_EDITOR_MODULE_NAME);
	TabSpawnerEntry.SetGroup(SMBlueprintEditorModule.GetToolsWorkspaceGroup().ToSharedRef());

	// Add to utility launcher.
	{
		const ISMUtilityLauncherModule& UtilityLauncherModule = FModuleManager::LoadModuleChecked<ISMUtilityLauncherModule>(LOGICDRIVER_UTILITY_LAUNCHER_MODULE_NAME);

		const TSharedRef<FExtender> Extender = MakeShared<FExtender>();
		Extender->AddMenuExtension("LogicDriver", EExtensionHook::First, CommandList, FMenuExtensionDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(FSMContentEditorCommands::Get().InstallContent,
				NAME_None,
				TAttribute<FText>(),
				TAttribute<FText>(),
				FSlateIcon(FSMContentEditorStyle::GetStyleSetName(), TEXT("InstallContentIcon")));
		}));

		UtilityLauncherModule.GetMenuExtensibilityManager()->AddExtender(Extender);
	}

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FSMContentEditorModule::ParseCommandLine);
}

void FSMContentEditorModule::ShutdownModule()
{
	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	FSMContentEditorStyle::Shutdown();

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomPropertyTypeLayout(FSMInputActionWrapper::StaticStruct()->GetFName());

	if (const FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry")))
	{
		AssetRegistryModule->Get().OnFilesLoaded().RemoveAll(this);
	}
}

bool FSMContentEditorModule::IsContentCreationEnabled() const
{
	return GEnableLogicDriverContentCreation;
}

UFactory* FSMContentEditorModule::CreateFactory() const
{
	USMContentAssetFactory* NewFactory = Cast<USMContentAssetFactory>(NewObject<UFactory>(GetTransientPackage(),
		USMContentAssetFactory::StaticClass()));
	return NewFactory;
}

void FSMContentEditorModule::BindCommands()
{
	const FSMContentEditorCommands& Commands = FSMContentEditorCommands::Get();
	CommandList->MapAction(Commands.InstallContent, FExecuteAction::CreateRaw(this, &FSMContentEditorModule::AddContentToProject));
}

void FSMContentEditorModule::ParseCommandLine()
{
	const bool bAutoPackageContent = FParse::Param(FCommandLine::Get(), TEXT("LogicDriverPackageContent"));
	if (bAutoPackageContent)
	{
		// Package all content assets and exit the editor.

		LDCONTENTEDITOR_LOG_INFO(TEXT("Starting Logic Driver content packaging..."));

		check(GEditor);

		const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry")).Get();

		TArray<FAssetData> OutContentAssets;
		AssetRegistry.GetAssetsByClass(USMContentAsset::StaticClass()->GetClassPathName(), OutContentAssets, true);

		for (FAssetData& AssetData : OutContentAssets)
		{
			if (USMContentAsset* ContentAsset = Cast<USMContentAsset>(AssetData.GetAsset()))
			{
				ContentAsset->PackageAsset();

				// Save the asset
				{
					FSavePackageArgs SaveArgs;
					SaveArgs.Error = GError;
					SaveArgs.TopLevelFlags = RF_Standalone;

					FString FinalPackageFilename = FPackageName::LongPackageNameToFilename(AssetData.PackageName.ToString(), FPackageName::GetAssetPackageExtension());
					GEditor->SavePackage(ContentAsset->GetPackage(), nullptr, *FinalPackageFilename, MoveTemp(SaveArgs));
				}
			}
		}

		LDCONTENTEDITOR_LOG_INFO(TEXT("Finished Logic Driver content packaging..."));

		if (GEngine)
		{
			GEngine->DeferredCommands.Add(TEXT("CLOSE_SLATE_MAINFRAME"));
		}
	}
}

void FSMContentEditorModule::OnFilesLoaded()
{
	if (const FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry")))
	{
		AssetRegistryModule->Get().OnFilesLoaded().RemoveAll(this);
	}

	ParseCommandLine();
}

TSharedRef<SDockTab> FSMContentEditorModule::SpawnContentInTab(const FSpawnTabArgs& SpawnTabArgs)
{
	// This is all a hack in order to spawn a modal window from a nomad tab spawner. A dummy dock tab is returned and
	// real content creation window created next tick to ensure the tab has created. This lets us gracefully close it.

	check(GEditor && GEditor->IsTimerManagerValid());

	TSharedPtr<SDockTab> NewTab = SNew(SDockTab);
	NewTab->SetVisibility(EVisibility::Collapsed);

	GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateLambda([this, NewTab]()
	{
		const FSMContentEditorCommands& Commands = FSMContentEditorCommands::Get();
		CommandList->TryExecuteAction(Commands.InstallContent.ToSharedRef());

		ensure(NewTab->CanCloseTab());
		NewTab->RequestCloseTab();
	}));

	return NewTab.ToSharedRef();
}

IMPLEMENT_MODULE(FSMContentEditorModule, SMContentEditor)

#undef LOCTEXT_NAMESPACE

