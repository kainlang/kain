/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "QuestAssetEditor.h"
#include "QuestAssetAction.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_QuestActions.h"
#include "QuestAssetEditorSettings.h"
#include "QuestGraphNode.h"
#include "QuestGraphPin.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "EdGraphUtilities.h"
#include "KismetPins/SGraphPinColor.h"
#include "EdGraph/EdGraphPin.h"
#include "SQuestGraphNode.h"
#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "FQuestAssetEditorModule"

uint32 FQuestAssetEditorModule::AssetCategory;

class SQuestStartGraphPin : public SGraphPin {
public:
	SLATE_BEGIN_ARGS(SQuestGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& inArgs, UEdGraphPin* inGraphPinObj) {
		SGraphPin::Construct(SGraphPin::FArguments(), inGraphPinObj);
	}

protected:
	virtual FSlateColor GetPinColor() const override {
		return FSlateColor(FLinearColor(1.0f, 0.2f, 0.2f));
	}
};

class SQuestStepGraphPin : public SGraphPin {
public:
	SLATE_BEGIN_ARGS(SQuestGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& inArgs, UEdGraphPin* inGraphPinObj) {
		SGraphPin::Construct(SGraphPin::FArguments(), inGraphPinObj);
	}

protected:
	virtual FSlateColor GetPinColor() const override {
		return FSlateColor(FLinearColor(0.2f, 1.0f, 0.2f));
	}
};

class SQuestTaskListGraphPin : public SGraphPin {
public:
	SLATE_BEGIN_ARGS(SQuestGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& inArgs, UEdGraphPin* inGraphPinObj) {
		SGraphPin::Construct(SGraphPin::FArguments(), inGraphPinObj);
	}

protected:
	virtual FSlateColor GetPinColor() const override {
		return FSlateColor(FLinearColor(0.2f, 0.2f, 1.0f));
	}
};

struct FQuestPinFactory : public FGraphPanelPinFactory {
public:
	virtual ~FQuestPinFactory() {}
	virtual TSharedPtr<SGraphPin> CreatePin(UEdGraphPin* pin) const override {
		if (FName(TEXT(QUEST_STEP_PIN_CATEGORY)) == pin->PinType.PinSubCategory) {
			return SNew(SQuestStepGraphPin, pin);
		} else if (FName(TEXT(QUEST_START_PIN_CATEGORY)) == pin->PinType.PinSubCategory) {
			return SNew(SQuestStartGraphPin, pin);
		} else if (FName(TEXT(QUEST_TASK_LIST_PIN_CATEGORY)) == pin->PinType.PinSubCategory) {
			return SNew(SQuestTaskListGraphPin, pin);
		} 

		return nullptr;
	}
};

// Addition to customize node visual :
struct FQuestNodeFactory : public FGraphPanelNodeFactory
{
public:
	virtual ~FQuestNodeFactory() {}
	virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* Node) const override
	{
		if(UQuestGraphNode * DialogNode = Cast<UQuestGraphNode>(Node)) {
			return SNew(SQuestGraphNode, DialogNode);
		}
		
		return nullptr;
	}
};

void FQuestAssetEditorModule::InitializeStyleset()
{
	_styleSet = MakeShareable(new FSlateStyleSet(TEXT("QuestAssetEditorStyle")));
	TSharedPtr<IPlugin> plugin = IPluginManager::Get().FindPlugin("DialogueMasterAssetEditor");
	FString contentDir = plugin->GetContentDir();
	_styleSet->SetContentRoot(contentDir);

	FSlateImageBrush* questAssetThumbnailBrush = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("QuestAsset_64x64"), TEXT(".png")), FVector2D(64.0, 64.0));
	FSlateImageBrush* questAssetIconBrush = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("QuestAsset_16x16"), TEXT(".png")), FVector2D(16.0, 16.0));
	
	FSlateImageBrush* questTaskThumbnailBrush = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("QuestTask_64x64"), TEXT(".png")), FVector2D(64.0, 64.0));
	FSlateImageBrush* questTaskIconBrush = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("QuestTask_16x16"), TEXT(".png")), FVector2D(16.0, 16.0));

	FSlateImageBrush* statisticThumbnailBrush = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("Statistic_64x64"), TEXT(".png")), FVector2D(64.0, 64.0));
	FSlateImageBrush* statisticIconBrush = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("Statistic_16x16"), TEXT(".png")), FVector2D(16.0, 16.0));
	
	_styleSet->Set(TEXT("ClassThumbnail.QuestAsset"), questAssetThumbnailBrush);
	_styleSet->Set(TEXT("ClassIcon.QuestAsset"), questAssetIconBrush);
	_styleSet->Set(TEXT("ClassThumbnail.DialogueMasterTask"), questTaskThumbnailBrush);
	_styleSet->Set(TEXT("ClassIcon.DialogueMasterTask"), questTaskIconBrush);
	_styleSet->Set(TEXT("ClassThumbnail.DialogueMasterStatistic"), statisticThumbnailBrush);
	_styleSet->Set(TEXT("ClassIcon.DialogueMasterStatistic"), statisticIconBrush);
	FSlateStyleRegistry::RegisterSlateStyle(*_styleSet);
}

void FQuestAssetEditorModule::StartupModule()
{
	RegisterDialogueEditorSettings();

	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
	
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	//IAssetTools& assetToolsModule = IAssetTools::Get();
	IAssetTools& assetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetCategory = assetToolsModule.RegisterAdvancedAssetCategory(FName(TEXT("DialogueMaster")), LOCTEXT("DialogueMasterCategory", "Dialogue Master"));
	TSharedPtr<QuestAssetAction> dialogAssetAction = MakeShareable(new QuestAssetAction(AssetCategory));
	QuestAssetTypeActions = dialogAssetAction;

	TSharedPtr<FAssetTypeActions_DialogueMasterQuestTask> dialogueMasterQuestTaskAction = MakeShareable(new FAssetTypeActions_DialogueMasterQuestTask(AssetCategory));
	DialogueMasterQuestTaskAction = dialogueMasterQuestTaskAction;

	TSharedPtr<FAssetTypeActions_DialogueMasterStatistic> dialogueMasterStatisticAction = MakeShareable(new FAssetTypeActions_DialogueMasterStatistic(AssetCategory));
	DialogueMasterStatisticAction = dialogueMasterStatisticAction;
/*
	TSharedPtr<FAssetTypeActions_DialogueMasterAction> dialogueMasterActionAction = MakeShareable(new FAssetTypeActions_DialogueMasterAction(AssetCategory));
	DialogueActionTypeActions = dialogueMasterActionAction;
	TSharedPtr<FAssetTypeActions_DialogueMasterCustomCameraShot> dialogueMasterCustomCameraShotAction = MakeShareable(new FAssetTypeActions_DialogueMasterCustomCameraShot(AssetCategory));
	DialogueCameraShotTypeActions = dialogueMasterCustomCameraShotAction;
*/
	
	assetToolsModule.RegisterAssetTypeActions(QuestAssetTypeActions.ToSharedRef());
	assetToolsModule.RegisterAssetTypeActions(DialogueMasterQuestTaskAction.ToSharedRef());
	assetToolsModule.RegisterAssetTypeActions(DialogueMasterStatisticAction.ToSharedRef());
	
	InitializeStyleset();

	_pinFactory = MakeShareable(new FQuestPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(_pinFactory);

	// Addition to customize node visual :
	_nodeFactory = MakeShareable(new FQuestNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(_nodeFactory);
}

void FQuestAssetEditorModule::ShutdownModule()
{
	ToolBarExtensibilityManager.Reset();
	MenuExtensibilityManager.Reset();
	
	if(UObjectInitialized())
	{
		UnregisterDialogueEditorSettings();
	}
	
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		AssetToolsModule.UnregisterAssetTypeActions(DialogueMasterStatisticAction.ToSharedRef());
		AssetToolsModule.UnregisterAssetTypeActions(DialogueMasterQuestTaskAction.ToSharedRef());
		AssetToolsModule.UnregisterAssetTypeActions(QuestAssetTypeActions.ToSharedRef());
	}
	
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FSlateStyleRegistry::UnRegisterSlateStyle(*_styleSet);
	FEdGraphUtilities::UnregisterVisualPinFactory(_pinFactory);
	FEdGraphUtilities::UnregisterVisualNodeFactory(_nodeFactory);
}

void FQuestAssetEditorModule::RegisterDialogueEditorSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "Dialogue Master - Quest Editor",
			LOCTEXT("DialogueMasterQuestSettingsName", "Dialogue Master - Quest Editor"),
			LOCTEXT("DialogueMasterQuestSettingsDescription", "Configuration Settings for the Dialogue Master Quest Editor"),
			GetMutableDefault<UQuestAssetEditorSettings>()
		);
	}
}

void FQuestAssetEditorModule::UnregisterDialogueEditorSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Dialogue Master - Quest Editor");
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FQuestAssetEditorModule, QuestAssetEditor)