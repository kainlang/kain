// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Widgets/SWindow.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "AssetRegistry/AssetData.h"

class FSMNewAssetDialogOption;

/**
 * Shown when creating a new state machine blueprint. Accepts configurable options for custom widgets and selection
 * behavior.
 */
class SSMNewAssetDialog : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SSMNewAssetDialog)
	{
	}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, FText AssetTypeDisplayName, TArray<FSMNewAssetDialogOption> InOptions);
	void GetAssetPicker();
	void ResetStage();
	bool GetUserConfirmedSelection() const;

	/** Confirms the current selection if possible, potentially closing the window. */
	void TryConfirmSelection();

protected:
	void ConfirmSelection();
	int32 GetSelectedObjectIndex() const { return SelectedOptionIndex; };

private:
	FSlateColor GetOptionBorderColor(int32 OptionIndex) const;
	ECheckBoxState GetOptionCheckBoxState(int32 OptionIndex) const;

	FReply OnOptionDoubleClicked(const FGeometry& Geometry, const FPointerEvent& PointerEvent, int32 OptionIndex);
	void OptionCheckBoxStateChanged(ECheckBoxState InCheckBoxState, int32 OptionIndex);
	FSlateColor GetOptionTextColor(int32 OptionIndex) const;

	FText GetAssetPickersLabelText() const;
	bool IsOkButtonEnabled() const;
	void OnOkButtonClicked();
	void OnCancelButtonClicked();
	bool HasAssetPage() const;

private:
	TSharedPtr<class SWizard> Wizard;
	TSharedPtr<SBox> AssetSettingsPage;
	TArray<FAssetData> SelectedAssets;
	TArray<FSMNewAssetDialogOption> Options;
	int32 SelectedOptionIndex = 0;
	bool bUserConfirmedSelection = false;
	bool bOnAssetStage = false;
};
