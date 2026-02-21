// Copyright Narrative Tools 2022. 

#include "NarrativeDialogueEditorModule.h"
#include "IDialogueEditor.h"
#include "AssetTypeActions_DialogueBlueprint.h"
#include "AssetTypeActions_DialogueAsset.h"
#include "DialogueGraphEditor.h"
#include "DialogueEditorStyle.h"
#include "PropertyEditorModule.h"
#include "DialogueEditorDetails.h"
#include "DialogueEditorSettings.h"
#include "NarrativeDialogueSettings.h"
#include "EdGraphUtilities.h"
#include "SDialogueGraphNode.h"
#include "DialogueGraphNode.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "ISettingsContainer.h"
#include <ISettingsCategory.h>
#include "KismetCompiler.h"
#include "Engine/World.h"
#include "DialogueBlueprintCompiler.h"

DEFINE_LOG_CATEGORY(LogNarrativeDialogueEditor);

const FName FNarrativeDialogueEditorModule::DialogueEditorAppId(TEXT("DialogueEditorApp"));

#define LOCTEXT_NAMESPACE "FNarrativeModule"

uint32 FNarrativeDialogueEditorModule::GameAssetCategory;

class FGraphPanelNodeFactory_DialogueGraph : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<class SGraphNode> CreateNode(UEdGraphNode* Node) const override
	{
		if (UDialogueGraphNode* DialogueNode = Cast<UDialogueGraphNode>(Node))
		{
			return SNew(SDialogueGraphNode, DialogueNode);
		}
		return NULL;
	}
};

TSharedPtr<FGraphPanelNodeFactory> GraphPanelNodeFactory_DialogueGraph;



void FNarrativeDialogueEditorModule::StartupModule()
{
	FDialogueEditorStyle::Initialize();

	RegisterSettings();
	
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	GraphPanelNodeFactory_DialogueGraph = MakeShareable(new FGraphPanelNodeFactory_DialogueGraph());
	FEdGraphUtilities::RegisterVisualNodeFactory(GraphPanelNodeFactory_DialogueGraph);

	IAssetTools& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	//Narrative Quest editor has already defined the narrative asset category, so find it 
	GameAssetCategory = AssetToolsModule.FindAdvancedAssetCategory(FName(TEXT("Narrative")));

	TSharedPtr<FAssetTypeActions_DialogueBlueprint> DialogueAssetTypeAction = MakeShareable(new FAssetTypeActions_DialogueBlueprint(GameAssetCategory));
	DialogueAssetTypeActions = DialogueAssetTypeAction;

	//Need to register old asset type actions so people can convert their old DialogueAssets into DialogueBlueprints 
	LegacyDialogueAssetTypeActions = MakeShareable(new FAssetTypeActions_DialogueAsset(GameAssetCategory));

	AssetToolsModule.RegisterAssetTypeActions(DialogueAssetTypeAction.ToSharedRef());
	AssetToolsModule.RegisterAssetTypeActions(LegacyDialogueAssetTypeActions.ToSharedRef());

	FKismetCompilerContext::RegisterCompilerForBP(UDialogueBlueprint::StaticClass(), [](UBlueprint* InBlueprint, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
		{
			return MakeShared<FDialogueBlueprintCompilerContext>(CastChecked<UDialogueBlueprint>(InBlueprint), InMessageLog, InCompileOptions);
		});

	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompilerModule.GetCompilers().Add(&DialogueBlueprintCompiler);

	//Register details panel for quest editor
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("DialogueNode_NPC", FOnGetDetailCustomizationInstance::CreateStatic(&FDialogueEditorDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("DialogueNode_Player", FOnGetDetailCustomizationInstance::CreateStatic(&FDialogueEditorDetails::MakeInstance));
	//PropertyModule.RegisterCustomPropertyTypeLayout("SpeakerSelector", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSpeakerSelectorCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

}


void FNarrativeDialogueEditorModule::ShutdownModule()
{
	ToolBarExtensibilityManager.Reset();
	MenuExtensibilityManager.Reset();

	if (UObjectInitialized())
	{
		UnregisterSettings();
	}

	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		if (DialogueAssetTypeActions.IsValid())
		{
			AssetToolsModule.UnregisterAssetTypeActions(DialogueAssetTypeActions.ToSharedRef());
		}
	}

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomPropertyTypeLayout("SpeakerSelector");

	FDialogueEditorStyle::Shutdown();
}

void FNarrativeDialogueEditorModule::RegisterSettings()
{
	// Registering some settings is just a matter of exposing the default UObject of
		// your desired class, feel free to add here all those settings you want to expose
		// to your LDs or artists.

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		// Register the settings
		SettingsModule->RegisterSettings("Project", "Plugins", "Narrative Dialogues - Editor",
			LOCTEXT("NarrativeDialogueSettingsName", "Narrative Dialogues - Editor"),
			LOCTEXT("NarrativeDialogueSettingsDescription", "Configuration Settings for the Narrative Dialogue Editor"),
			GetMutableDefault<UDialogueEditorSettings>()
		);

		// Register the runtime settings
		SettingsModule->RegisterSettings("Project", "Plugins", "Narrative Dialogues - Gameplay",
			LOCTEXT("NarrativeRuntimeDialogueSettingsName", "Narrative Dialogues - Gameplay"),
			LOCTEXT("NarrativeRuntimeDialogueSettingsDescription", "Configuration Settings for the Narrative Dialogue Runtime"),
			GetMutableDefault<UNarrativeDialogueSettings>()
		);
	}
}

void FNarrativeDialogueEditorModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Narrative Dialogues - Editor");
		SettingsModule->UnregisterSettings("Project", "Plugins", "Narrative Dialogues - Gameplay");
	}
}

TSharedRef<IDialogueEditor> FNarrativeDialogueEditorModule::CreateDialogueEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UDialogueBlueprint* DialogueAsset)
{
	TSharedRef< FDialogueGraphEditor > NewDialogueEditor(new FDialogueGraphEditor());
	NewDialogueEditor->InitDialogueEditor(Mode, InitToolkitHost, DialogueAsset);
	return NewDialogueEditor;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNarrativeDialogueEditorModule, NarrativeDialogueEditor)