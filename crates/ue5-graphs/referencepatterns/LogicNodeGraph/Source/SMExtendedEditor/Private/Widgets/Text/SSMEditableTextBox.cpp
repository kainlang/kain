// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMEditableTextBox.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SPopUpErrorText.h"

#if WITH_FANCY_TEXT

namespace
{
	/**
	 * Helper function to solve some issues with ternary operators inside construction of a widget.
	 */
	TSharedRef< SWidget > AsWidgetRef(const TSharedPtr< SWidget >& InWidget)
	{
		if (InWidget.IsValid())
		{
			return InWidget.ToSharedRef();
		}
		else
		{
			return SNullWidget::NullWidget;
		}
	}
}

/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SSMEditableTextBox::Construct(const FArguments& InArgs)
{
	check(InArgs._Style);
	Style = InArgs._Style;

	BorderImageNormal = &InArgs._Style->BackgroundImageNormal;
	BorderImageHovered = &InArgs._Style->BackgroundImageHovered;
	BorderImageFocused = &InArgs._Style->BackgroundImageFocused;
	BorderImageReadOnly = &InArgs._Style->BackgroundImageReadOnly;

	PaddingOverride = InArgs._Padding;
	HScrollBarPaddingOverride = InArgs._HScrollBarPadding;
	VScrollBarPaddingOverride = InArgs._VScrollBarPadding;
	FontOverride = InArgs._Font;
	ForegroundColorOverride = InArgs._ForegroundColor;
	BackgroundColorOverride = InArgs._BackgroundColor;
	ReadOnlyForegroundColorOverride = InArgs._ReadOnlyForegroundColor;

	bHasExternalHScrollBar = InArgs._HScrollBar.IsValid();
	HScrollBar = InArgs._HScrollBar;
	if (!HScrollBar.IsValid())
	{
		// Create and use our own scrollbar
		HScrollBar = SNew(SScrollBar)
			.Style(&InArgs._Style->ScrollBarStyle)
			.Orientation(Orient_Horizontal)
			.AlwaysShowScrollbar(InArgs._AlwaysShowScrollbars)
			.Thickness(FVector2D(5.0f, 5.0f));
	}

	bHasExternalVScrollBar = InArgs._VScrollBar.IsValid();
	VScrollBar = InArgs._VScrollBar;
	if (!VScrollBar.IsValid())
	{
		// Create and use our own scrollbar
		VScrollBar = SNew(SScrollBar)
			.Style(&InArgs._Style->ScrollBarStyle)
			.Orientation(Orient_Vertical)
			.AlwaysShowScrollbar(InArgs._AlwaysShowScrollbars)
			.Thickness(FVector2D(5.0f, 5.0f));
	}

	SBorder::Construct(SBorder::FArguments()
		.BorderImage(this, &SSMEditableTextBox::GetBorderImage)
		.BorderBackgroundColor(this, &SSMEditableTextBox::DetermineBackgroundColor)
		.ForegroundColor(this, &SSMEditableTextBox::DetermineForegroundColor)
		.Padding(this, &SSMEditableTextBox::DeterminePadding)
		[
			SAssignNew(Box, SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.FillWidth(1)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.FillHeight(1)
				[
					// Use our editable text instead of default.
					SAssignNew(EditableText, SSMEditableText)
					.Text(InArgs._Text)
					.HintText(InArgs._HintText)
					.SearchText(InArgs._SearchText)
					.TextStyle(InArgs._TextStyle)
					.Marshaller(InArgs._Marshaller)
					.Font(this, &SSMEditableTextBox::DetermineFont)
					.IsReadOnly(InArgs._IsReadOnly)
					.AllowMultiLine(InArgs._AllowMultiLine)
					.OnContextMenuOpening(InArgs._OnContextMenuOpening)
					.OnIsTypedCharValid(InArgs._OnIsTypedCharValid)
					.OnTextChanged(InArgs._OnTextChanged)
					.OnTextCommitted(InArgs._OnTextCommitted)
					.OnCursorMoved(InArgs._OnCursorMoved)
					.ContextMenuExtender(InArgs._ContextMenuExtender)
					.CreateSlateTextLayout(InArgs._CreateSlateTextLayout)
					.Justification(InArgs._Justification)
					.RevertTextOnEscape(InArgs._RevertTextOnEscape)
					.SelectAllTextWhenFocused(InArgs._SelectAllTextWhenFocused)
					.ClearTextSelectionOnFocusLoss(InArgs._ClearTextSelectionOnFocusLoss)
					.ClearKeyboardFocusOnCommit(InArgs._ClearKeyboardFocusOnCommit)
					.LineHeightPercentage(InArgs._LineHeightPercentage)
					.Margin(InArgs._Margin)
					.WrapTextAt(InArgs._WrapTextAt)
					.AutoWrapText(InArgs._AutoWrapText)
					.WrappingPolicy(InArgs._WrappingPolicy)
					.HScrollBar(HScrollBar)
					.VScrollBar(VScrollBar)
					.OnHScrollBarUserScrolled(InArgs._OnHScrollBarUserScrolled)
					.OnVScrollBarUserScrolled(InArgs._OnVScrollBarUserScrolled)
					.OnKeyCharHandler(InArgs._OnKeyCharHandler)
					.OnKeyDownHandler(InArgs._OnKeyDownHandler)
					.ModiferKeyForNewLine(InArgs._ModiferKeyForNewLine)
					.VirtualKeyboardOptions(InArgs._VirtualKeyboardOptions)
					.VirtualKeyboardTrigger(InArgs._VirtualKeyboardTrigger)
					.VirtualKeyboardDismissAction(InArgs._VirtualKeyboardDismissAction)
					.TextShapingMethod(InArgs._TextShapingMethod)
					.TextFlowDirection(InArgs._TextFlowDirection)
					.AllowContextMenu(InArgs._AllowContextMenu)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(HScrollBarPaddingBox, SBox)
					.Padding(this, &SSMEditableTextBox::DetermineHScrollBarPadding)
					[
						AsWidgetRef(HScrollBar)
					]
				]
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(VScrollBarPaddingBox, SBox)
				.Padding(this, &SSMEditableTextBox::DetermineVScrollBarPadding)
				[
					AsWidgetRef(VScrollBar)
				]
			]
		]
	);

	ErrorReporting = InArgs._ErrorReporting;
	if (ErrorReporting.IsValid())
	{
		Box->AddSlot()
			.AutoWidth()
			.Padding(3, 0)
			[
				ErrorReporting->AsWidget()
			];
	}

}

void SSMEditableTextBox::SetStyle(const FEditableTextBoxStyle* InStyle)
{
	if (InStyle)
	{
		Style = InStyle;
	}
	else
	{
		FArguments Defaults;
		Style = Defaults._Style;
	}

	check(Style);

	if (!bHasExternalHScrollBar && HScrollBar.IsValid())
	{
		HScrollBar->SetStyle(&Style->ScrollBarStyle);
	}

	if (!bHasExternalVScrollBar && VScrollBar.IsValid())
	{
		VScrollBar->SetStyle(&Style->ScrollBarStyle);
	}

	BorderImageNormal = &Style->BackgroundImageNormal;
	BorderImageHovered = &Style->BackgroundImageHovered;
	BorderImageFocused = &Style->BackgroundImageFocused;
	BorderImageReadOnly = &Style->BackgroundImageReadOnly;
}

FSlateColor SSMEditableTextBox::DetermineForegroundColor() const
{
	check(Style);

	if (EditableText->IsTextReadOnly())
	{
		if (ReadOnlyForegroundColorOverride.IsSet())
		{
			return ReadOnlyForegroundColorOverride.Get();
		}
		if (ForegroundColorOverride.IsSet())
		{
			return ForegroundColorOverride.Get();
		}

		return Style->ReadOnlyForegroundColor;
	}
	else
	{
		return ForegroundColorOverride.IsSet() ? ForegroundColorOverride.Get() : Style->ForegroundColor;
	}
}

void SSMEditableTextBox::SetText(const TAttribute< FText >& InNewText)
{
	EditableText->SetText(InNewText);
}

void SSMEditableTextBox::SetHintText(const TAttribute< FText >& InHintText)
{
	EditableText->SetHintText(InHintText);
}

void SSMEditableTextBox::SetSearchText(const TAttribute<FText>& InSearchText)
{
	EditableText->SetSearchText(InSearchText);
}

FText SSMEditableTextBox::GetSearchText() const
{
	return EditableText->GetSearchText();
}

void SSMEditableTextBox::SetTextBoxForegroundColor(const TAttribute<FSlateColor>& InForegroundColor)
{
	ForegroundColorOverride = InForegroundColor;
}

void SSMEditableTextBox::SetTextBoxBackgroundColor(const TAttribute<FSlateColor>& InBackgroundColor)
{
	BackgroundColorOverride = InBackgroundColor;
}

void SSMEditableTextBox::SetReadOnlyForegroundColor(const TAttribute<FSlateColor>& InReadOnlyForegroundColor)
{
	ReadOnlyForegroundColorOverride = InReadOnlyForegroundColor;
}

void SSMEditableTextBox::SetTextShapingMethod(const TOptional<ETextShapingMethod>& InTextShapingMethod)
{
	EditableText->SetTextShapingMethod(InTextShapingMethod);
}

void SSMEditableTextBox::SetTextFlowDirection(const TOptional<ETextFlowDirection>& InTextFlowDirection)
{
	EditableText->SetTextFlowDirection(InTextFlowDirection);
}

void SSMEditableTextBox::SetWrapTextAt(const TAttribute<float>& InWrapTextAt)
{
	EditableText->SetWrapTextAt(InWrapTextAt);
}

void SSMEditableTextBox::SetAutoWrapText(const TAttribute<bool>& InAutoWrapText)
{
	EditableText->SetAutoWrapText(InAutoWrapText);
}

void SSMEditableTextBox::SetWrappingPolicy(const TAttribute<ETextWrappingPolicy>& InWrappingPolicy)
{
	EditableText->SetWrappingPolicy(InWrappingPolicy);
}

void SSMEditableTextBox::SetLineHeightPercentage(const TAttribute<float>& InLineHeightPercentage)
{
	EditableText->SetLineHeightPercentage(InLineHeightPercentage);
}

void SSMEditableTextBox::SetMargin(const TAttribute<FMargin>& InMargin)
{
	EditableText->SetMargin(InMargin);
}

void SSMEditableTextBox::SetJustification(const TAttribute<ETextJustify::Type>& InJustification)
{
	EditableText->SetJustification(InJustification);
}

void SSMEditableTextBox::SetAllowContextMenu(const TAttribute< bool >& InAllowContextMenu)
{
	EditableText->SetAllowContextMenu(InAllowContextMenu);
}

void SSMEditableTextBox::SetVirtualKeyboardDismissAction(TAttribute<EVirtualKeyboardDismissAction> InVirtualKeyboardDismissAction)
{
	EditableText->SetVirtualKeyboardDismissAction(InVirtualKeyboardDismissAction);
}

void SSMEditableTextBox::SetIsReadOnly(const TAttribute<bool>& InIsReadOnly)
{
	EditableText->SetIsReadOnly(InIsReadOnly);
}

void SSMEditableTextBox::SetError(const FText& InError)
{
	SetError(InError.ToString());
}

void SSMEditableTextBox::SetError(const FString& InError)
{
	const bool bHaveError = !InError.IsEmpty();

	if (!ErrorReporting.IsValid())
	{
		// No error reporting was specified; make a default one
		TSharedPtr<SPopupErrorText> ErrorTextWidget;
		Box->AddSlot()
			.AutoWidth()
			.Padding(3, 0)
			[
				SAssignNew(ErrorTextWidget, SPopupErrorText)
			];
		ErrorReporting = ErrorTextWidget;
	}

	ErrorReporting->SetError(InError);
}

/** @return Border image for the text box based on the hovered and focused state */
const FSlateBrush* SSMEditableTextBox::GetBorderImage() const
{
	if (EditableText->IsTextReadOnly())
	{
		return BorderImageReadOnly;
	}
	else if (EditableText->HasKeyboardFocus())
	{
		return BorderImageFocused;
	}
	else
	{
		if (EditableText->IsHovered())
		{
			return BorderImageHovered;
		}
		else
		{
			return BorderImageNormal;
		}
	}
}

bool SSMEditableTextBox::SupportsKeyboardFocus() const
{
	return StaticCastSharedPtr<SWidget>(EditableText)->SupportsKeyboardFocus();
}

bool SSMEditableTextBox::HasKeyboardFocus() const
{
	// Since keyboard focus is forwarded to our editable text, we will test it instead
	return SBorder::HasKeyboardFocus() || EditableText->HasKeyboardFocus();
}

FReply SSMEditableTextBox::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	FReply Reply = FReply::Handled();

	if (InFocusEvent.GetCause() != EFocusCause::Cleared)
	{
		// Forward keyboard focus to our editable text widget
		Reply.SetUserFocus(EditableText.ToSharedRef(), InFocusEvent.GetCause());
	}

	return Reply;
}

bool SSMEditableTextBox::AnyTextSelected() const
{
	return EditableText->AnyTextSelected();
}

void SSMEditableTextBox::SelectAllText()
{
	EditableText->SelectAllText();
}

void SSMEditableTextBox::ClearSelection()
{
	EditableText->ClearSelection();
}

FText SSMEditableTextBox::GetSelectedText() const
{
	return EditableText->GetSelectedText();
}

void SSMEditableTextBox::InsertTextAtCursor(const FText& InText)
{
	EditableText->InsertTextAtCursor(InText);
}

void SSMEditableTextBox::InsertTextAtCursor(const FString& InString)
{
	EditableText->InsertTextAtCursor(InString);
}

void SSMEditableTextBox::InsertRunAtCursor(TSharedRef<IRun> InRun)
{
	EditableText->InsertRunAtCursor(InRun);
}

void SSMEditableTextBox::GoTo(const FTextLocation& NewLocation)
{
	EditableText->GoTo(NewLocation);
}

void SSMEditableTextBox::ScrollTo(const FTextLocation& NewLocation)
{
	EditableText->ScrollTo(NewLocation);
}

void SSMEditableTextBox::ApplyToSelection(const FRunInfo& InRunInfo, const FTextBlockStyle& InStyle)
{
	EditableText->ApplyToSelection(InRunInfo, InStyle);
}

void SSMEditableTextBox::BeginSearch(const FText& InSearchText, const ESearchCase::Type InSearchCase, const bool InReverse)
{
	EditableText->BeginSearch(InSearchText, InSearchCase, InReverse);
}

void SSMEditableTextBox::AdvanceSearch(const bool InReverse)
{
	EditableText->AdvanceSearch(InReverse);
}

TSharedPtr<const IRun> SSMEditableTextBox::GetRunUnderCursor() const
{
	return EditableText->GetRunUnderCursor();
}

TArray<TSharedRef<const IRun>> SSMEditableTextBox::GetSelectedRuns() const
{
	return EditableText->GetSelectedRuns();
}

TSharedPtr<const SScrollBar> SSMEditableTextBox::GetHScrollBar() const
{
	return EditableText->GetHScrollBar();
}

TSharedPtr<const SScrollBar> SSMEditableTextBox::GetVScrollBar() const
{
	return EditableText->GetVScrollBar();
}

void SSMEditableTextBox::Refresh()
{
	return EditableText->Refresh();
}

void SSMEditableTextBox::SetOnKeyCharHandler(FOnKeyChar InOnKeyCharHandler)
{
	EditableText->SetOnKeyCharHandler(InOnKeyCharHandler);
}

void SSMEditableTextBox::SetOnKeyDownHandler(FOnKeyDown InOnKeyDownHandler)
{
	EditableText->SetOnKeyDownHandler(InOnKeyDownHandler);
}

void SSMEditableTextBox::ForceScroll(int32 UserIndex, float ScrollAxisMagnitude)
{
	EditableText->ForceScroll(UserIndex, ScrollAxisMagnitude);
}

#endif //WITH_FANCY_TEXT
