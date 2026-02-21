// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMAssetPickerList.h"

#include "Blueprints/SMAssetClassFilter.h"

#include "Blueprints/SMBlueprint.h"
#include "SMInstance.h"

#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ClassViewerModule.h"
#include "SClassViewer.h"

#define LOCTEXT_NAMESPACE "SSMAssetPickerList"

SSMAssetPickerList::~SSMAssetPickerList()
{
	OnAssetSelectedEvent.Unbind();
}

void SSMAssetPickerList::Construct(const FArguments& InArgs)
{
	OnAssetSelectedEvent = InArgs._OnAssetSelected;
	OnClassSelectedEvent = InArgs._OnClassSelected;
	OnItemDoubleClicked = InArgs._OnItemDoubleClicked;
	AssetPickerMode = InArgs._AssetPickerMode;

	TSharedPtr<SWidget> AssetPickerWidget;

	switch (AssetPickerMode)
	{
	case EAssetPickerMode::AssetPicker:
		{
			FAssetPickerConfig AssetPickerConfig;
			{
				AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SSMAssetPickerList::OnAssetSelected);
				AssetPickerConfig.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateSP(this, &SSMAssetPickerList::OnAssetDoubleClicked);
				AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SSMAssetPickerList::OnShouldFilterAsset);
				AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
				AssetPickerConfig.SelectionMode = ESelectionMode::Single;
				AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
				AssetPickerConfig.bAllowNullSelection = false;
				AssetPickerConfig.bShowBottomToolbar = true;
				AssetPickerConfig.bAutohideSearchBar = false;
				AssetPickerConfig.bAllowDragging = false;
				AssetPickerConfig.bCanShowClasses = false;
				AssetPickerConfig.ThumbnailLabel = EThumbnailLabel::AssetName;

				AssetPickerConfig.Filter.ClassPaths.Add(USMBlueprint::StaticClass()->GetClassPathName());
				AssetPickerConfig.Filter.bRecursiveClasses = true;
			}

			const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
			AssetPickerWidget = ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig);
			break;
		}
	case EAssetPickerMode::ClassPicker:
		{
			FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

			// Fill in options
			FClassViewerInitializationOptions Options;
			Options.Mode = EClassViewerMode::ClassPicker;
			Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
			Options.DisplayMode = EClassViewerDisplayMode::TreeView;
			Options.InitiallySelectedClass = InArgs._InitiallySelectedClass;

			const TSharedPtr<FSMAssetClassParentFilter> Filter = MakeShared<FSMAssetClassParentFilter>();
			Filter->AllowedChildrenOfClasses.Add(USMInstance::StaticClass());
			Filter->DisallowedClassFlags = CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown;
			Options.ClassFilters.Add(Filter.ToSharedRef());

			AssetPickerWidget = ClassViewerModule.CreateClassViewer(MoveTemp(Options), FOnClassPicked::CreateRaw(this, &SSMAssetPickerList::OnClassSelected));
			break;
		}
	}

	ChildSlot
		[
			AssetPickerWidget.ToSharedRef()
		];
}

void SSMAssetPickerList::OnAssetSelected(const FAssetData& InAssetData)
{
	SelectedAssets.Reset();
	SelectedAssets.Add(InAssetData);
	OnAssetSelectedEvent.ExecuteIfBound(InAssetData);
}

void SSMAssetPickerList::OnAssetDoubleClicked(const FAssetData& InAssetData)
{
	OnItemDoubleClicked.ExecuteIfBound();
}

void SSMAssetPickerList::OnClassSelected(UClass* InClass)
{
	SelectedClasses.Reset();
	SelectedClasses.Add(InClass);
	OnClassSelectedEvent.ExecuteIfBound(InClass);
}

bool SSMAssetPickerList::OnShouldFilterAsset(const FAssetData& InAssetData)
{
	if (InAssetData.AssetClassPath == USMBlueprint::StaticClass()->GetClassPathName())
	{
		const FString ParentClassPath = InAssetData.GetTagValueRef<FString>("ParentClass");
		if (!ParentClassPath.IsEmpty())
		{
			const UClass* ParentClass = FindObject<UClass>(nullptr, *ParentClassPath);
			if (ParentClass && ParentClass->IsChildOf<USMInstance>())
			{
				return false;
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE