// © 2025 mrbaconvn. All Rights Reserved.

#include "ComboGraphEditor.h"
#include "ComboGraphAssetTypeActions.h"
#include "IAssetTools.h" 
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "Graph/Factory/ComboNodeFactory.h"
#include "Graph/Factory/ComboPinFactory.h"

#define LOCTEXT_NAMESPACE "ComboGraphEditor"
#define PLUGIN_NAME "BaconComboGraph" 
#define CATEGORY_DISPLAY_NAME LOCTEXT(PLUGIN_NAME, "Bacon Combo Graph")

#define THUMBNAIL_PATH "ClassThumbnail.ComboGraphAsset"
#define ICON_PATH "ClassIcon.ComboGraphAsset"
#define ICON_NAME "ComboGraphIcon"
#define ICON_EXTENSION ".png"

void FComboGraphEditor::StartupModule()
{
	IAssetTools& AssetToolsModule = IAssetTools::Get();
	EAssetTypeCategories::Type AssetType = AssetToolsModule.RegisterAdvancedAssetCategory(PLUGIN_NAME, CATEGORY_DISPLAY_NAME);

	InitializeStyleSet();

	// ------------------------------
	// Register Combo Graph Asset
	// ------------------------------
	TSharedPtr<UComboGraphAssetTypeActions> ComboGraphAction = MakeShareable(new UComboGraphAssetTypeActions(AssetType));
	AssetToolsModule.RegisterAssetTypeActions(ComboGraphAction.ToSharedRef());

	// ------------------------------
	// Register Combo Graph Pin Factory
	// ------------------------------
	PinFactory = MakeShareable(new FComboPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(PinFactory);

	// ------------------------------
	// Register Combo Graph Node Factory
	// ------------------------------
	NodeFactory = MakeShareable(new FComboNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(NodeFactory);
}

void FComboGraphEditor::ShutdownModule()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
	}

	if (PinFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(PinFactory);
	}

	if (NodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(NodeFactory);
	}
}

void FComboGraphEditor::InitializeStyleSet()
{
	StyleSet = MakeShareable(new FSlateStyleSet(TEXT(PLUGIN_NAME)));
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PLUGIN_NAME);
	FString contentDir = Plugin->GetContentDir();
	StyleSet->SetContentRoot(contentDir);

	FSlateImageBrush* IconBrush = new FSlateImageBrush(StyleSet->RootToContentDir(TEXT(ICON_NAME), TEXT(ICON_EXTENSION)), FVector2D(150.0, 150.0));
	FSlateImageBrush* ThumbnailBrush = new FSlateImageBrush(StyleSet->RootToContentDir(TEXT(ICON_NAME), TEXT(ICON_EXTENSION)), FVector2D(15.0, 15.0));
	StyleSet->Set(TEXT(THUMBNAIL_PATH), IconBrush);
	StyleSet->Set(TEXT(ICON_PATH), ThumbnailBrush);
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FComboGraphEditor, ComboGraphEditor)