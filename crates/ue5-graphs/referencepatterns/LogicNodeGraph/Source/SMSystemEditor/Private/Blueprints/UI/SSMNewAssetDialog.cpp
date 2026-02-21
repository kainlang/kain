// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMNewAssetDialog.h"

#include "SMNewAssetDialogueOption.h"

#include "Configuration/SMEditorSettings.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "SlateOptMacros.h"
#include "SMUnrealTypeDefs.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Workflow/SWizard.h"

#define LOCTEXT_NAMESPACE "SSMNewAssetDialog"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SSMNewAssetDialog::Construct(const FArguments& InArgs, FText AssetTypeDisplayName, TArray<FSMNewAssetDialogOption> InOptions)
{
	bUserConfirmedSelection = false;

	Options = InOptions;

	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
	SelectedOptionIndex = Settings->NewAssetIndex;

	if (SelectedOptionIndex >= Options.Num())
	{
		SelectedOptionIndex = 0;
	}

	TSharedPtr<SVerticalBox> OptionsBox;
	TSharedPtr<SOverlay> AssetPickerOverlay;

	const TSharedRef<SVerticalBox> RootBox =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0, 5, 0, 5)
		[
			SNew(SBox)
			.Padding(FSMUnrealAppStyle::Get().GetMargin("StandardDialog.SlotPadding"))
			[
				SNew(SBorder)
				.BorderImage(FSMUnrealAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(7))
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.MaxHeight(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateLambda([this]()
					{
						// Max height calculation required or scrollbar won't adjust and footer buttons will overlap.
						return GetViewportSize().Y - 115.f;
					})))
					.Padding(0.f, 4.f, 0.f, 0.f)
					[
						SNew(SScrollBox)
						.Orientation(Orient_Vertical)
						+SScrollBox::Slot()
						[
							SAssignNew(OptionsBox, SVerticalBox)
						]
					]
				]
			]
		];


	int32 OptionIndex = 0;
	for (FSMNewAssetDialogOption& Option : Options)
	{
		OptionsBox->AddSlot()
			.Padding(0, 0, 0, OptionIndex < Options.Num() - 1 ? 7 : 0)
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderBackgroundColor(this, &SSMNewAssetDialog::GetOptionBorderColor, OptionIndex)
				[
					SNew(SCheckBox)
					.Style(FSMUnrealAppStyle::Get(), "ToggleButtonCheckbox")
					.CheckBoxContentUsesAutoWidth(false)
					.IsChecked(this, &SSMNewAssetDialog::GetOptionCheckBoxState, OptionIndex)
					.OnCheckStateChanged(this, &SSMNewAssetDialog::OptionCheckBoxStateChanged, OptionIndex)
					.Content()
					[
						SNew(SBorder)
						.BorderImage(FSMUnrealAppStyle::Get().GetBrush("NoBorder"))
						.OnMouseDoubleClick(this, &SSMNewAssetDialog::OnOptionDoubleClicked, OptionIndex)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(5, 2)
							[
								SNew(STextBlock)
								.ColorAndOpacity(this, &SSMNewAssetDialog::GetOptionTextColor, OptionIndex)
								.Text(Option.OptionText)
								.TextStyle(FSMUnrealAppStyle::Get(), "NormalText.Important")
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
								.AutoWrapText(true)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(5, 2, 5, 7)
							[
								SNew(STextBlock)
								.ColorAndOpacity(this, &SSMNewAssetDialog::GetOptionTextColor, OptionIndex)
								.Text(Option.OptionDescription)
								.TextStyle(FSMUnrealAppStyle::Get(), "SmallText.Subdued")
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
								.AutoWrapText(true)
							]
						]
					]
				]
			];

		OptionIndex++;
	}
	SWindow::Construct(SWindow::FArguments()
		.Title(FText::Format(LOCTEXT("NewAssetDialogTitle", "Pick a starting point for your {0}"), AssetTypeDisplayName))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(400.f, 400.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SAssignNew(Wizard, SWizard)
			.OnCanceled(this, &SSMNewAssetDialog::OnCancelButtonClicked)
			.OnFinished(this, &SSMNewAssetDialog::OnOkButtonClicked)
			.CanFinish(this, &SSMNewAssetDialog::IsOkButtonEnabled)
			.ShowPageList(false)
			+SWizard::Page()
			.CanShow(true)
			.OnEnter(this, &SSMNewAssetDialog::ResetStage)
			[
				RootBox
			]
			+ SWizard::Page()
			.CanShow(this, &SSMNewAssetDialog::HasAssetPage)
			.OnEnter(this, &SSMNewAssetDialog::GetAssetPicker)
			[
				SAssignNew(AssetSettingsPage, SBox)
			]
		]);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SSMNewAssetDialog::GetAssetPicker()
{
	bOnAssetStage = true;
	AssetSettingsPage->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0.f, 0.f, 0.f, 2.5f)
		.FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.FillWidth(1.f)
			[
				Options[SelectedOptionIndex].AssetPicker
			]
		]
	);
}

void SSMNewAssetDialog::ResetStage()
{
	bOnAssetStage = false;
}

bool SSMNewAssetDialog::GetUserConfirmedSelection() const
{
	const FSMNewAssetDialogOption& SelectedOption = Options[SelectedOptionIndex];
	if (SelectedOption.OnCanAssetBeCreated.IsBound())
	{
		return SelectedOption.OnCanAssetBeCreated.Execute();
	}
	return bUserConfirmedSelection;
}

void SSMNewAssetDialog::TryConfirmSelection()
{
	if (IsOkButtonEnabled())
	{
		ConfirmSelection();
	}
}

void SSMNewAssetDialog::ConfirmSelection()
{
	const FSMNewAssetDialogOption& SelectedOption = Options[SelectedOptionIndex];
	if (SelectedOption.OnCanSelectAsset.IsBound())
	{
		if (!SelectedOption.OnCanSelectAsset.Execute())
		{
			return;
		}
	}
	if (SelectedOption.OnSelectionConfirmed.IsBound() &&
		!SelectedOption.OnSelectionConfirmed.Execute())
	{
		return;
	}
	bUserConfirmedSelection = true;

	RequestDestroyWindow();
}

FSlateColor SSMNewAssetDialog::GetOptionBorderColor(int32 OptionIndex) const
{
	return SelectedOptionIndex == OptionIndex
		? FSlateColor::UseSubduedForeground()
		: FSlateColor(FLinearColor::Transparent);
}

FSlateColor SSMNewAssetDialog::GetOptionTextColor(int32 OptionIndex) const
{
	return SelectedOptionIndex == OptionIndex
		? FSlateColor(FLinearColor::White)
		: FSlateColor::UseForeground();
}

ECheckBoxState SSMNewAssetDialog::GetOptionCheckBoxState(int32 OptionIndex) const
{
	return SelectedOptionIndex == OptionIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FReply SSMNewAssetDialog::OnOptionDoubleClicked(const FGeometry& Geometry, const FPointerEvent& PointerEvent,
	int32 OptionIndex)
{
	SelectedOptionIndex = OptionIndex;
	if (Wizard->CanShowPage(Wizard->GetCurrentPageIndex() + 1))
	{
		Wizard->AdvanceToPage(Wizard->GetCurrentPageIndex() + 1);
		return FReply::Handled();
	}

	if (IsOkButtonEnabled())
	{
		OnOkButtonClicked();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SSMNewAssetDialog::OptionCheckBoxStateChanged(ECheckBoxState InCheckBoxState, int32 OptionIndex)
{
	if (InCheckBoxState == ECheckBoxState::Checked)
	{
		SelectedOptionIndex = OptionIndex;
		
		USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableEditorSettings();
		Settings->NewAssetIndex = SelectedOptionIndex;
		Settings->SaveConfig();
	}
}

FText SSMNewAssetDialog::GetAssetPickersLabelText() const
{
	if (SelectedOptionIndex < 0 || SelectedOptionIndex >= Options.Num())
	{
		return FText::GetEmpty();
	}
	return Options[SelectedOptionIndex].AssetPickerHeader;
}

bool SSMNewAssetDialog::IsOkButtonEnabled() const
{
	const FSMNewAssetDialogOption& SelectedOption = Options[SelectedOptionIndex];
	if (SelectedOption.OnCanSelectAsset.IsBound())
	{
		return bOnAssetStage && SelectedOption.OnCanSelectAsset.Execute();
	}

	return true;
}

void SSMNewAssetDialog::OnOkButtonClicked()
{
	ConfirmSelection();
}

void SSMNewAssetDialog::OnCancelButtonClicked()
{
	bUserConfirmedSelection = false;
	SelectedAssets.Empty();

	RequestDestroyWindow();
}

bool SSMNewAssetDialog::HasAssetPage() const
{
	return !IsOkButtonEnabled();
}

#undef LOCTEXT_NAMESPACE