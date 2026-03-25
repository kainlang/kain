/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "DialogAssetEditor.h"
#include "DialogAssetAction.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "DialogAssetEditorSettings.h"
#include "DialogGraphNode.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "EdGraphUtilities.h"
#include "KismetPins/SGraphPinColor.h"
#include "EdGraph/EdGraphPin.h"
#include "SDialogGraphNode.h"
#include "DialogGraphPin.h"
#include "AssetTypeActions_DialogueMasterActions.h"
#include "ISettingsModule.h"
#include "VoiceSelectionSettingsDetails.h"

#define LOCTEXT_NAMESPACE "FDialogAssetEditorModule"

uint32 FDialogAssetEditorModule::AssetCategory;

class SDialogStartGraphPin : public SGraphPin {
public:
	SLATE_BEGIN_ARGS(SDialogGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& inArgs, UEdGraphPin* inGraphPinObj) {
		SGraphPin::Construct(SGraphPin::FArguments(), inGraphPinObj);
	}

protected:
	virtual FSlateColor GetPinColor() const override {
		return FSlateColor(FLinearColor(1.0f, 0.2f, 0.2f));
	}
};

class SDialogEndGraphPin : public SGraphPin {
public:
	SLATE_BEGIN_ARGS(SDialogGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& inArgs, UEdGraphPin* inGraphPinObj) {
		SGraphPin::Construct(SGraphPin::FArguments(), inGraphPinObj);
	}

protected:
	virtual FSlateColor GetPinColor() const override {
		return FSlateColor(FLinearColor(0.2f, 0.2f, 1.0f));
	}
};

struct FDialogPinFactory : public FGraphPanelPinFactory {
public:
	virtual ~FDialogPinFactory() {}
	virtual TSharedPtr<SGraphPin> CreatePin(UEdGraphPin* pin) const override {
		if (FName(TEXT("DialogPin")) == pin->PinType.PinSubCategory) {
			return SNew(SDialogGraphPin, pin);
		} else if (FName(TEXT("StartPin")) == pin->PinType.PinSubCategory) {
			return SNew(SDialogStartGraphPin, pin);
		} else if (FName(TEXT("EndPin")) == pin->PinType.PinSubCategory) {
			return SNew(SDialogEndGraphPin, pin);
		} 

		return nullptr;
	}
};

// Addition to customize node visual :
struct FDialogNodeFactory : public FGraphPanelNodeFactory
{
public:
	virtual ~FDialogNodeFactory() {}
	virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* Node) const override
	{
		if(UDialogGraphNode * DialogNode = Cast<UDialogGraphNode>(Node)) {
			return SNew(SDialogGraphNode, DialogNode);
		}
		
		return nullptr;
	}
};

void FDialogAssetEditorModule::InitializeStyleset()
{
	_styleSet = MakeShareable(new FSlateStyleSet(TEXT("DialogAssetEditorStyle")));
	TSharedPtr<IPlugin> plugin = IPluginManager::Get().FindPlugin("DialogueMasterAssetEditor");
	FString contentDir = plugin->GetContentDir();
	_styleSet->SetContentRoot(contentDir);

	FSlateImageBrush* dialogAssetThumbnailBrush = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("DialogAsset_64x64"), TEXT(".png")), FVector2D(64.0, 64.0));
	FSlateImageBrush* dialogAssetIconBrush = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("DialogAsset_16x16"), TEXT(".png")), FVector2D(16.0, 16.0));
	FSlateImageBrush* nodeAddIcon = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("NodeAddPinIcon"), TEXT(".png")), FVector2D(128.0, 128.0));
	FSlateImageBrush* nodeDeletePinIcon = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("NodeDeletePinIcon"), TEXT(".png")), FVector2D(128.0, 128.0));
	FSlateImageBrush* nodeDeleteNodeIcon = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("NodeDeleteNodeIcon"), TEXT(".png")), FVector2D(128.0, 128.0));

	FSlateImageBrush* conditionThumbnailBrush = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("ConditionAsset_64x64"), TEXT(".png")), FVector2D(64.0, 64.0));
	FSlateImageBrush* conditionIconBrush = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("ConditionAsset_16x16"), TEXT(".png")), FVector2D(16.0, 16.0));

	FSlateImageBrush* actionThumbnailBrush = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("ActionAsset_64x64"), TEXT(".png")), FVector2D(64.0, 64.0));
	FSlateImageBrush* actionIconBrush = new FSlateImageBrush(_styleSet->RootToContentDir(TEXT("ActionAsset_16x16"), TEXT(".png")), FVector2D(16.0, 16.0));

	
	_styleSet->Set(TEXT("ClassThumbnail.DialogAsset"), dialogAssetThumbnailBrush);
	_styleSet->Set(TEXT("ClassIcon.DialogAsset"), dialogAssetIconBrush);
	_styleSet->Set(TEXT("DialogueMasterAssetEditor.NodeAddPinIcon"), nodeAddIcon);
	_styleSet->Set(TEXT("DialogueMasterAssetEditor.NodeDeletePinIcon"), nodeDeletePinIcon);
	_styleSet->Set(TEXT("DialogueMasterAssetEditor.NodeDeleteNodeIcon"), nodeDeleteNodeIcon);
	_styleSet->Set(TEXT("ClassThumbnail.AdvancedPrerequisiteBase"), conditionThumbnailBrush);
	_styleSet->Set(TEXT("ClassIcon.AdvancedPrerequisiteBase"), conditionIconBrush);
	_styleSet->Set(TEXT("ClassThumbnail.DialogueMasterAction"), actionThumbnailBrush);
	_styleSet->Set(TEXT("ClassIcon.DialogueMasterAction"), actionIconBrush);
	FSlateStyleRegistry::RegisterSlateStyle(*_styleSet);
}

void FDialogAssetEditorModule::StartupModule()
{
	RegisterDialogueEditorSettings();

	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
	
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	//IAssetTools& assetToolsModule = IAssetTools::Get();
	IAssetTools& assetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetCategory = assetToolsModule.RegisterAdvancedAssetCategory(FName(TEXT("DialogueMaster")), LOCTEXT("DialogueMasterCategory", "Dialogue Master"));
	TSharedPtr<DialogAssetAction> dialogAssetAction = MakeShareable(new DialogAssetAction(AssetCategory));
	DialogueAssetTypeActions = dialogAssetAction;
	TSharedPtr<FAssetTypeActions_DialogueMasterCondition> dialogueMasterConditionAction = MakeShareable(new FAssetTypeActions_DialogueMasterCondition(AssetCategory));
	DialogueConditionTypeActions = dialogueMasterConditionAction;
	TSharedPtr<FAssetTypeActions_DialogueMasterAction> dialogueMasterActionAction = MakeShareable(new FAssetTypeActions_DialogueMasterAction(AssetCategory));
	DialogueActionTypeActions = dialogueMasterActionAction;
	TSharedPtr<FAssetTypeActions_DialogueMasterCustomCameraShot> dialogueMasterCustomCameraShotAction = MakeShareable(new FAssetTypeActions_DialogueMasterCustomCameraShot(AssetCategory));
	DialogueCameraShotTypeActions = dialogueMasterCustomCameraShotAction;

	assetToolsModule.RegisterAssetTypeActions(DialogueConditionTypeActions.ToSharedRef());
	assetToolsModule.RegisterAssetTypeActions(DialogueAssetTypeActions.ToSharedRef());
	assetToolsModule.RegisterAssetTypeActions(DialogueActionTypeActions.ToSharedRef());
	assetToolsModule.RegisterAssetTypeActions(DialogueCameraShotTypeActions.ToSharedRef());
	
	InitializeStyleset();

	_pinFactory = MakeShareable(new FDialogPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(_pinFactory);

	// Addition to customize node visual :
	_nodeFactory = MakeShareable(new FDialogNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(_nodeFactory);

	// Voice selection combobox:
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomPropertyTypeLayout(
		"ElevenLabsParameters",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FVoiceSelectionSettingsDetails::MakeInstance)
	);
}

void FDialogAssetEditorModule::ShutdownModule()
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

		AssetToolsModule.UnregisterAssetTypeActions(DialogueAssetTypeActions.ToSharedRef());
		AssetToolsModule.UnregisterAssetTypeActions(DialogueConditionTypeActions.ToSharedRef());
		AssetToolsModule.UnregisterAssetTypeActions(DialogueActionTypeActions.ToSharedRef());
		AssetToolsModule.UnregisterAssetTypeActions(DialogueCameraShotTypeActions.ToSharedRef());
	}
	
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FSlateStyleRegistry::UnRegisterSlateStyle(*_styleSet);
	FEdGraphUtilities::UnregisterVisualPinFactory(_pinFactory);
	FEdGraphUtilities::UnregisterVisualNodeFactory(_nodeFactory);

	// Voice selection combobox:
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomPropertyTypeLayout("ElevenLabsParameters");
}

void FDialogAssetEditorModule::RegisterDialogueEditorSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "Dialogue Master - Dialogue Editor",
			LOCTEXT("DialogueMasterSettingsName", "Dialogue Master - Dialogue Editor"),
			LOCTEXT("DialogueMasterSettingsDescription", "Configuration Settings for the Dialogue Master Dialogue Editor"),
			GetMutableDefault<UDialogAssetEditorSettings>()
		);
	}
}

void FDialogAssetEditorModule::UnregisterDialogueEditorSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Dialogue Master - Dialogue Editor");
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDialogAssetEditorModule, DialogAssetEditor)