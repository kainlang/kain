// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "AssetRegistry/AssetData.h"

/**
 * List various assets for user selection.
 */
class SSMAssetPickerList : public SCompoundWidget
{
public:
	enum class EAssetPickerMode : uint8
	{
		AssetPicker,
		ClassPicker
	};

	DECLARE_DELEGATE_OneParam(FOnAssetSelected, const FAssetData&);
	DECLARE_DELEGATE_OneParam(FOnClassSelected, UClass* Class);
	DECLARE_DELEGATE(FOnItemDoubleClicked);

	SLATE_BEGIN_ARGS(SSMAssetPickerList)
	: _AssetPickerMode(EAssetPickerMode::AssetPicker),
	_OnAssetSelected(), _OnClassSelected(), _OnItemDoubleClicked(),
	_InitiallySelectedClass(nullptr)
	{}
	SLATE_ARGUMENT(EAssetPickerMode, AssetPickerMode)
	SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
	SLATE_EVENT(FOnAssetSelected, OnAssetSelected)
	SLATE_EVENT(FOnClassSelected, OnClassSelected)
	SLATE_EVENT(FOnItemDoubleClicked, OnItemDoubleClicked) // Only valid for assets currently.
	/** The initial selected class if using the class picker. */
	SLATE_ARGUMENT(UClass*, InitiallySelectedClass)
	SLATE_END_ARGS()

	virtual ~SSMAssetPickerList() override;

	void Construct(const FArguments& InArgs);

	const TArray<FAssetData>& GetSelectedAssets() const { return SelectedAssets; }
	const TArray<UClass*>& GetSelectedClasses() const { return SelectedClasses; }

private:
	void OnAssetSelected(const FAssetData& InAssetData);
	void OnAssetDoubleClicked(const FAssetData& InAssetData);
	void OnClassSelected(UClass* InClass);
	bool OnShouldFilterAsset(const FAssetData& InAssetData);

private:
	FOnAssetSelected OnAssetSelectedEvent;
	FOnClassSelected OnClassSelectedEvent;
	FOnItemDoubleClicked OnItemDoubleClicked;

	TArray<FAssetData> SelectedAssets;
	TArray<UClass*> SelectedClasses;
	EAssetPickerMode AssetPickerMode = EAssetPickerMode::AssetPicker;
};
