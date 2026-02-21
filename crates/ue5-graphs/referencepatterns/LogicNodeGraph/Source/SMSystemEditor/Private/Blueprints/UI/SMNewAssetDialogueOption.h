// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Widgets/SWidget.h"

class FSMNewAssetDialogOption
{
public:
	DECLARE_DELEGATE_RetVal(bool, FOnCanContinue);

	FText OptionText;
	FText OptionDescription;
	FText AssetPickerHeader;
	TSharedRef<SWidget> AssetPicker;
	// Whether a given asset can be selected (wizard finished)
	FOnCanContinue OnCanSelectAsset;
	// Whether the factory should create the asset or not
	FOnCanContinue OnCanAssetBeCreated;
	// When the wizard has been confirmed. Returning false prevents the window from closing
	FOnCanContinue OnSelectionConfirmed;

	FSMNewAssetDialogOption(FText InOptionText, FText InOptionDescription, FText InAssetPickerHeader,
		FOnCanContinue InOnGetSelectedAssetsFromPicker, FOnCanContinue InOnCanAssetBeCreated, FOnCanContinue InOnSelectionConfirmed,
		TSharedRef<SWidget> InAssetPicker)
		: OptionText(InOptionText)
		, OptionDescription(InOptionDescription)
		, AssetPickerHeader(InAssetPickerHeader)
		, AssetPicker(InAssetPicker)
		, OnCanSelectAsset(InOnGetSelectedAssetsFromPicker)
		, OnCanAssetBeCreated(InOnCanAssetBeCreated)
		, OnSelectionConfirmed(InOnSelectionConfirmed)
	{
	}
};