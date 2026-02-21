// Copyright Narrative Tools 2022. 

#include "NarrativeQuestEditorModule.h"
#include "IQuestEditor.h"
#include "AssetTypeActions_QuestAsset.h"
#include "AssetTypeActions_QuestAction.h"
#include "AssetTypeActions_NarrativeQuestTask.h"
#include "QuestGraphEditor.h"
#include "QuestEditorStyle.h"
#include "PropertyEditorModule.h"
#include "QuestEditorDetails.h"
#include "QuestEditorSettings.h"
#include "NarrativeQuestSettings.h"
#include "QuestTask.h"
#include "EdGraphUtilities.h"
#include "SQuestGraphNode.h"
#include "QuestGraphNode.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "ISettingsContainer.h"
#include <ISettingsCategory.h>
#include "KismetCompiler.h"
#include "QuestBlueprintCompiler.h"
#include <AssetRegistry/AssetRegistryModule.h>
#include "UObject/CoreRedirects.h"

DEFINE_LOG_CATEGORY(LogNarrativeQuestEditor);

const FName FNarrativeQuestEditorModule::QuestEditorAppId(TEXT("QuestEditorApp"));

#define LOCTEXT_NAMESPACE "FNarrativeModule"

uint32 FNarrativeQuestEditorModule::GameAssetCategory;

class FGraphPanelNodeFactory_QuestGraph : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<class SGraphNode> CreateNode(UEdGraphNode* Node) const override
	{
		if (UQuestGraphNode* QuestNode = Cast<UQuestGraphNode>(Node))
		{
			return SNew(SQuestGraphNode, QuestNode);
		}
		return NULL;
	}
};

TSharedPtr<FGraphPanelNodeFactory> GraphPanelNodeFactory_QuestGraph;

void FNarrativeQuestEditorModule::StartupModule()
{
	FQuestEditorStyle::Initialize();

	RegisterSettings();

	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	GraphPanelNodeFactory_QuestGraph = MakeShareable(new FGraphPanelNodeFactory_QuestGraph());
	FEdGraphUtilities::RegisterVisualNodeFactory(GraphPanelNodeFactory_QuestGraph);

	IAssetTools& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	GameAssetCategory = AssetToolsModule.RegisterAdvancedAssetCategory(FName(TEXT("Narrative")), LOCTEXT("NarrativeCategory", "Narrative"));

	TSharedPtr<FAssetTypeActions_QuestAsset> QuestAssetTypeAction = MakeShareable(new FAssetTypeActions_QuestAsset(GameAssetCategory));
	QuestAssetTypeActions = QuestAssetTypeAction;
	AssetToolsModule.RegisterAssetTypeActions(QuestAssetTypeAction.ToSharedRef());

	//These are being deprecated in Narrative 3, remove the ability to right click make them but we're still keeping them around so old legacy quests work 
	TSharedPtr<FAssetTypeActions_QuestAction> QuestActionTypeAction = MakeShareable(new FAssetTypeActions_QuestAction(GameAssetCategory));
	QuestActionTypeActions = QuestActionTypeAction;
	AssetToolsModule.RegisterAssetTypeActions(QuestActionTypeAction.ToSharedRef());

	TSharedPtr<FAssetTypeActions_NarrativeQuestTask> NarrativeQuestTaskTypeAction = MakeShareable(new FAssetTypeActions_NarrativeQuestTask(GameAssetCategory));
	NarrativeQuestTaskTypeActions = NarrativeQuestTaskTypeAction;
	AssetToolsModule.RegisterAssetTypeActions(NarrativeQuestTaskTypeAction.ToSharedRef());


	TSharedPtr<FAssetTypeActions_NarrativeCondition> NarrativeConditionTypeAction = MakeShareable(new FAssetTypeActions_NarrativeCondition(GameAssetCategory));
	NarrativeConditionTypeActions = NarrativeConditionTypeAction;
	AssetToolsModule.RegisterAssetTypeActions(NarrativeConditionTypeAction.ToSharedRef());


	TSharedPtr<FAssetTypeActions_NarrativeEvent> NarrativeEventTypeAction = MakeShareable(new FAssetTypeActions_NarrativeEvent(GameAssetCategory));
	NarrativeEventTypeActions = NarrativeEventTypeAction;
	AssetToolsModule.RegisterAssetTypeActions(NarrativeEventTypeAction.ToSharedRef());

	FKismetCompilerContext::RegisterCompilerForBP(UQuestBlueprint::StaticClass(), [](UBlueprint* InBlueprint, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
		{
			return MakeShared<FQuestBlueprintCompilerContext>(CastChecked<UQuestBlueprint>(InBlueprint), InMessageLog, InCompileOptions);
		});

	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompilerModule.GetCompilers().Add(&QuestBlueprintCompiler);

	//Register details panel for quest editor
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("QuestBranch", FOnGetDetailCustomizationInstance::CreateStatic(&FQuestEditorDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("QuestState", FOnGetDetailCustomizationInstance::CreateStatic(&FQuestEditorDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

	TArray<FCoreRedirect> Redirects;

	//Redirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("NarrativeTask"), TEXT("NarrativeDataTask"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("NarrativeQuestTask"), TEXT("NarrativeTask"));

	FCoreRedirects::AddRedirectList(Redirects, "NarrativeQuestEditor");

}

void FNarrativeQuestEditorModule::ShutdownModule()
{
	ToolBarExtensibilityManager.Reset();
	MenuExtensibilityManager.Reset();

	if (UObjectInitialized())
	{
		UnregisterSettings();
	}

	if (GraphPanelNodeFactory_QuestGraph.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(GraphPanelNodeFactory_QuestGraph);
		GraphPanelNodeFactory_QuestGraph.Reset();
	}

	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		if (QuestAssetTypeActions.IsValid())
		{
			AssetToolsModule.UnregisterAssetTypeActions(QuestAssetTypeActions.ToSharedRef());
			AssetToolsModule.UnregisterAssetTypeActions(QuestActionTypeActions.ToSharedRef());			
			AssetToolsModule.UnregisterAssetTypeActions(NarrativeQuestTaskTypeActions.ToSharedRef());
			AssetToolsModule.UnregisterAssetTypeActions(NarrativeConditionTypeActions.ToSharedRef());
			AssetToolsModule.UnregisterAssetTypeActions(NarrativeEventTypeActions.ToSharedRef());
		}
	}

	// Unregister the details customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("QuestEditor_Properties");
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	//UE_LOG(LogNarrativeQuestEditor, Warning, TEXT("Quest Editor unloaded."));
	FQuestEditorStyle::Shutdown();
}

void FNarrativeQuestEditorModule::RegisterSettings()
{
	// Registering some settings is just a matter of exposing the default UObject of
		// your desired class, feel free to add here all those settings you want to expose
		// to your LDs or artists.

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		// Register the settings
		SettingsModule->RegisterSettings("Project", "Plugins", "Narrative Quests - Editor",
			LOCTEXT("NarrativeQuestSettingsName", "Narrative Quests - Editor"),
			LOCTEXT("NarrativeQuestSettingsDescription", "Configuration Settings for the Narrative Quest Editor"),
			GetMutableDefault<UQuestEditorSettings>()
		);

		// Register the runtime settings
		SettingsModule->RegisterSettings("Project", "Plugins", "Narrative Quests - Gameplay",
			LOCTEXT("NarrativeRuntimeQuestSettingsName", "Narrative Quests - Gameplay"),
			LOCTEXT("NarrativeRuntimeQuestSettingsDescription", "Configuration Settings for the Narrative Quest Runtime"),
			GetMutableDefault<UNarrativeQuestSettings>()
		);
	}
}

void FNarrativeQuestEditorModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Narrative - Quests");
	}
}

TSharedRef<IQuestEditor> FNarrativeQuestEditorModule::CreateQuestEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UQuestBlueprint* QuestAsset)
{

	TSharedRef< FQuestGraphEditor > NewQuestEditor(new FQuestGraphEditor());
	NewQuestEditor->InitQuestEditor(Mode, InitToolkitHost, QuestAsset);
	return NewQuestEditor;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNarrativeQuestEditorModule, NarrativeQuestEditor)