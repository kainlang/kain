// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMAddActorCombo.h"

#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"

#include "EditorStyleSet.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "SMUnrealTypeDefs.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "SSMAddActorCombo"

SSMAddActorCombo::~SSMAddActorCombo()
{
	OnActorClassSelectedEvent.Unbind();
}

void SSMAddActorCombo::Construct(const FArguments& InArgs, TSharedPtr<FSMBlueprintEditor> InStateMachineEditor)
{
	BlueprintEditor = InStateMachineEditor;
	OnActorClassSelectedEvent = InArgs._OnActorSelected;

	// Create the Construct arguments for the parent class
	SPositiveActionButton::FArguments Args;

	Args.Icon(FSMUnrealAppStyle::Get().GetBrush("Icons.Plus"));
	Args.Text(LOCTEXT("AddActorButtonLabel", "Add Actor"));
	
	Args.OnGetMenuContent(this, &SSMAddActorCombo::GenerateMenuContent);

	SPositiveActionButton::Construct(Args);
}

TSharedRef<SWidget> SSMAddActorCombo::GenerateMenuContent()
{
	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SSMAddActorCombo::OnAssetSelected);
		AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SSMAddActorCombo::OnShouldFilterAsset);
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.SelectionMode = ESelectionMode::Single;
		AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.bShowBottomToolbar = true;
		AssetPickerConfig.bAutohideSearchBar = false;
		AssetPickerConfig.bAllowDragging = false;
		AssetPickerConfig.bCanShowClasses = false;
		AssetPickerConfig.ThumbnailLabel = EThumbnailLabel::AssetName;
		
		AssetPickerConfig.Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		AssetPickerConfig.Filter.bRecursiveClasses = true;
	}
	
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	const TSharedRef<SWidget> AssetPicker = ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig);
	
	return SNew(SBox)
		.MinDesiredWidth(300.f)
		.MaxDesiredHeight(440.f)
		[
			SNew(SBorder)
			.BorderImage(FSMUnrealAppStyle::Get().GetBrush("Menu.Background"))
			[
				AssetPicker
			]
		];
}

void SSMAddActorCombo::OnAssetSelected(const FAssetData& InAssetData)
{
	if (UBlueprint* Blueprint = Cast<UBlueprint>(InAssetData.GetAsset()))
	{
		if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf<AActor>())
		{
			OnActorClassSelectedEvent.ExecuteIfBound(static_cast<TSubclassOf<AActor>>(Blueprint->GeneratedClass));
		}
	}
	
	SetIsMenuOpen(false, false);
}

bool SSMAddActorCombo::OnShouldFilterAsset(const FAssetData& InAssetData)
{
	// Look just for actor types.
	
	if (InAssetData.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName())
	{
		const FString ParentClassPath = InAssetData.GetTagValueRef<FString>("ParentClass");
		if (!ParentClassPath.IsEmpty())
		{
			UClass* ParentClass = FindObject<UClass>(nullptr, *ParentClassPath);
			if ((ParentClass && ParentClass->IsChildOf<AActor>() && !ParentClass->IsChildOf<AGameModeBase>()) &&
				!ParentClass->HasAnyClassFlags(CLASS_Abstract | CLASS_NotPlaceable | CLASS_Transient))
			{
				return false;
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
