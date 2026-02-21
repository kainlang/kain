// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMEditableText.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Text/PlainTextLayoutMarshaller.h"
#include "Rendering/DrawElements.h"
#include "Runtime/Slate/Private/Framework/Text/TextEditHelper.h"
#include "Types/ReflectionMetadata.h"
#include "Types/SlateConstants.h"

#define LOCTEXT_NAMESPACE "SMEditableText"

SSMEditableText::SSMEditableText()
	: bSelectAllTextWhenFocused(false)
	, bIsReadOnly(false)
	, AmountScrolledWhileRightMouseDown(0.0f)
	, bIsSoftwareCursor(false)
{
}

SSMEditableText::~SSMEditableText()
{
	// Needed to avoid "deletion of pointer to incomplete type 'FSlateEditableTextLayout'; no destructor called" error when using TUniquePtr
}

void SSMEditableText::Construct(const FArguments& InArgs)
{
	bIsReadOnly = InArgs._IsReadOnly;

	OnIsTypedCharValid = InArgs._OnIsTypedCharValid;
	OnTextChangedCallback = InArgs._OnTextChanged;
	OnTextCommittedCallback = InArgs._OnTextCommitted;
	OnCursorMovedCallback = InArgs._OnCursorMoved;
	bAllowMultiLine = InArgs._AllowMultiLine;
	bSelectAllTextWhenFocused = InArgs._SelectAllTextWhenFocused;
	bClearTextSelectionOnFocusLoss = InArgs._ClearTextSelectionOnFocusLoss;
	bClearKeyboardFocusOnCommit = InArgs._ClearKeyboardFocusOnCommit;
	bAllowContextMenu = InArgs._AllowContextMenu;
	OnContextMenuOpening = InArgs._OnContextMenuOpening;
	bRevertTextOnEscape = InArgs._RevertTextOnEscape;
	VirtualKeyboardOptions = InArgs._VirtualKeyboardOptions;
	VirtualKeyboardTrigger = InArgs._VirtualKeyboardTrigger;
	VirtualKeyboardDismissAction = InArgs._VirtualKeyboardDismissAction;
	OnHScrollBarUserScrolled = InArgs._OnHScrollBarUserScrolled;
	OnVScrollBarUserScrolled = InArgs._OnVScrollBarUserScrolled;
	OnKeyCharHandler = InArgs._OnKeyCharHandler;
	OnKeyDownHandler = InArgs._OnKeyDownHandler;
	ModiferKeyForNewLine = InArgs._ModiferKeyForNewLine;

	HScrollBar = InArgs._HScrollBar;
	if (HScrollBar.IsValid())
	{
		HScrollBar->SetUserVisibility(EVisibility::Collapsed);
		HScrollBar->SetOnUserScrolled(FOnUserScrolled::CreateSP(this, &SSMEditableText::OnHScrollBarMoved));
	}

	VScrollBar = InArgs._VScrollBar;
	if (VScrollBar.IsValid())
	{
		VScrollBar->SetUserVisibility(EVisibility::Collapsed);
		VScrollBar->SetOnUserScrolled(FOnUserScrolled::CreateSP(this, &SSMEditableText::OnVScrollBarMoved));
	}

	FTextBlockStyle TextStyle = *InArgs._TextStyle;
	if (InArgs._Font.IsSet() || InArgs._Font.IsBound())
	{
		TextStyle.SetFont(InArgs._Font.Get());
	}

	TSharedPtr<ITextLayoutMarshaller> Marshaller = InArgs._Marshaller;
	if (!Marshaller.IsValid())
	{
		Marshaller = FPlainTextLayoutMarshaller::Create();
	}

	EditableTextLayout = MakeUnique<FSMEditableTextLayout>(*this, InArgs._Text, TextStyle, InArgs._TextShapingMethod, InArgs._TextFlowDirection, InArgs._CreateSlateTextLayout, Marshaller.ToSharedRef(), Marshaller.ToSharedRef());
	EditableTextLayout->SetHintText(InArgs._HintText);
	EditableTextLayout->SetSearchText(InArgs._SearchText);
	EditableTextLayout->SetTextWrapping(InArgs._WrapTextAt, InArgs._AutoWrapText, InArgs._WrappingPolicy);
	EditableTextLayout->SetMargin(InArgs._Margin);
	EditableTextLayout->SetJustification(InArgs._Justification);
	EditableTextLayout->SetLineHeightPercentage(InArgs._LineHeightPercentage);
	EditableTextLayout->SetDebugSourceInfo(TAttribute<FString>::Create(TAttribute<FString>::FGetter::CreateLambda([this] { return FReflectionMetaData::GetWidgetDebugInfo(this); })));

	// build context menu extender
	MenuExtender = MakeShareable(new FExtender);
	MenuExtender->AddMenuExtension("EditText", EExtensionHook::Before, TSharedPtr<FUICommandList>(), InArgs._ContextMenuExtender);
}


void SSMEditableText::SetText(const TAttribute< FText >& InText)
{
	EditableTextLayout->SetText(InText);
}

FText SSMEditableText::GetText() const
{
	return EditableTextLayout->GetText();
}

FText SSMEditableText::GetPlainText() const
{
	return EditableTextLayout->GetPlainText();
}

void SSMEditableText::SetHintText(const TAttribute< FText >& InHintText)
{
	EditableTextLayout->SetHintText(InHintText);
}

FText SSMEditableText::GetHintText() const
{
	return EditableTextLayout->GetHintText();
}

void SSMEditableText::SetSearchText(const TAttribute<FText>& InSearchText)
{
	EditableTextLayout->SetSearchText(InSearchText);
}

FText SSMEditableText::GetSearchText() const
{
	return EditableTextLayout->GetSearchText();
}

void SSMEditableText::SetTextStyle(const FTextBlockStyle* InTextStyle)
{
	if (InTextStyle)
	{
		EditableTextLayout->SetTextStyle(*InTextStyle);
	}
	else
	{
		FArguments Defaults;
		check(Defaults._TextStyle);
		EditableTextLayout->SetTextStyle(*Defaults._TextStyle);
	}
}

void SSMEditableText::SetFont(const TAttribute< FSlateFontInfo >& InNewFont)
{
	FTextBlockStyle TextStyle = EditableTextLayout->GetTextStyle();
	TextStyle.SetFont(InNewFont.Get());
	EditableTextLayout->SetTextStyle(TextStyle);
}

void SSMEditableText::SetTextShapingMethod(const TOptional<ETextShapingMethod>& InTextShapingMethod)
{
	EditableTextLayout->SetTextShapingMethod(InTextShapingMethod);
}

void SSMEditableText::SetTextFlowDirection(const TOptional<ETextFlowDirection>& InTextFlowDirection)
{
	EditableTextLayout->SetTextFlowDirection(InTextFlowDirection);
}

void SSMEditableText::SetWrapTextAt(const TAttribute<float>& InWrapTextAt)
{
	EditableTextLayout->SetWrapTextAt(InWrapTextAt);
}

void SSMEditableText::SetAutoWrapText(const TAttribute<bool>& InAutoWrapText)
{
	EditableTextLayout->SetAutoWrapText(InAutoWrapText);
}

void SSMEditableText::SetWrappingPolicy(const TAttribute<ETextWrappingPolicy>& InWrappingPolicy)
{
	EditableTextLayout->SetWrappingPolicy(InWrappingPolicy);
}

void SSMEditableText::SetLineHeightPercentage(const TAttribute<float>& InLineHeightPercentage)
{
	EditableTextLayout->SetLineHeightPercentage(InLineHeightPercentage);
}

void SSMEditableText::SetMargin(const TAttribute<FMargin>& InMargin)
{
	EditableTextLayout->SetMargin(InMargin);
}

void SSMEditableText::SetJustification(const TAttribute<ETextJustify::Type>& InJustification)
{
	EditableTextLayout->SetJustification(InJustification);
}

void SSMEditableText::SetAllowContextMenu(const TAttribute< bool >& InAllowContextMenu)
{
	bAllowContextMenu = InAllowContextMenu;
}

void SSMEditableText::SetVirtualKeyboardDismissAction(TAttribute< EVirtualKeyboardDismissAction > InVirtualKeyboardDismissAction)
{
	VirtualKeyboardDismissAction = InVirtualKeyboardDismissAction;
}

void SSMEditableText::SetIsReadOnly(const TAttribute<bool>& InIsReadOnly)
{
	bIsReadOnly = InIsReadOnly;
}

void SSMEditableText::SetSelectAllTextWhenFocused(const TAttribute<bool>& InSelectAllTextWhenFocused)
{
	bSelectAllTextWhenFocused = InSelectAllTextWhenFocused;
}

void SSMEditableText::SetClearTextSelectionOnFocusLoss(const TAttribute<bool>& InClearTextSelectionOnFocusLoss)
{
	bClearTextSelectionOnFocusLoss = InClearTextSelectionOnFocusLoss;
}

void SSMEditableText::SetRevertTextOnEscape(const TAttribute<bool>& InRevertTextOnEscape)
{
	bRevertTextOnEscape = InRevertTextOnEscape;
}

void SSMEditableText::SetClearKeyboardFocusOnCommit(const TAttribute<bool>& InClearKeyboardFocusOnCommit)
{
	bClearKeyboardFocusOnCommit = InClearKeyboardFocusOnCommit;
}

void SSMEditableText::OnHScrollBarMoved(const float InScrollOffsetFraction)
{
	EditableTextLayout->SetHorizontalScrollFraction(InScrollOffsetFraction);
	OnHScrollBarUserScrolled.ExecuteIfBound(InScrollOffsetFraction);
}

void SSMEditableText::OnVScrollBarMoved(const float InScrollOffsetFraction)
{
	EditableTextLayout->SetVerticalScrollFraction(InScrollOffsetFraction);
	OnVScrollBarUserScrolled.ExecuteIfBound(InScrollOffsetFraction);
}

bool SSMEditableText::IsTextReadOnly() const
{
	return bIsReadOnly.Get(false);
}

bool SSMEditableText::IsTextPassword() const
{
	return false;
}

bool SSMEditableText::IsMultiLineTextEdit() const
{
	return bAllowMultiLine.Get(true);
}

bool SSMEditableText::ShouldJumpCursorToEndWhenFocused() const
{
	return false;
}

bool SSMEditableText::ShouldSelectAllTextWhenFocused() const
{
	return bSelectAllTextWhenFocused.Get(false);
}

bool SSMEditableText::ShouldClearTextSelectionOnFocusLoss() const
{
	return bClearTextSelectionOnFocusLoss.Get(false);
}

bool SSMEditableText::ShouldRevertTextOnEscape() const
{
	return bRevertTextOnEscape.Get(false);
}

bool SSMEditableText::ShouldClearKeyboardFocusOnCommit() const
{
	return bClearKeyboardFocusOnCommit.Get(false);
}

bool SSMEditableText::ShouldSelectAllTextOnCommit() const
{
	return false;
}

bool SSMEditableText::CanInsertCarriageReturn() const
{
	return FSlateApplication::Get().GetModifierKeys().AreModifersDown(ModiferKeyForNewLine);
}

bool SSMEditableText::CanTypeCharacter(const TCHAR InChar) const
{
	if (OnIsTypedCharValid.IsBound())
	{
		return OnIsTypedCharValid.Execute(InChar);
	}

	return InChar != TEXT('\t');
}

void SSMEditableText::EnsureActiveTick()
{
	TSharedPtr<FActiveTimerHandle> ActiveTickTimerPin = ActiveTickTimer.Pin();
	if (ActiveTickTimerPin.IsValid())
	{
		return;
	}

	auto DoActiveTick = [this](double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType
	{
		// Continue if we still have focus, otherwise treat as a fire-and-forget Tick() request
		const bool bShouldAppearFocused = HasKeyboardFocus() || EditableTextLayout->HasActiveContextMenu();
		return (bShouldAppearFocused) ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
	};

	const float TickPeriod = EditableTextDefs::BlinksPerSecond * 0.5f;
	ActiveTickTimer = RegisterActiveTimer(TickPeriod, FWidgetActiveTimerDelegate::CreateLambda(DoActiveTick));
}

EKeyboardType SSMEditableText::GetVirtualKeyboardType() const
{
	return Keyboard_Default;
}

FVirtualKeyboardOptions SSMEditableText::GetVirtualKeyboardOptions() const
{
	return VirtualKeyboardOptions;
}

EVirtualKeyboardTrigger SSMEditableText::GetVirtualKeyboardTrigger() const
{
	return VirtualKeyboardTrigger.Get();
}

EVirtualKeyboardDismissAction SSMEditableText::GetVirtualKeyboardDismissAction() const
{
	return VirtualKeyboardDismissAction.Get();
}

TSharedRef<SWidget> SSMEditableText::GetSlateWidget()
{
	return AsShared();
}

TSharedPtr<SWidget> SSMEditableText::GetSlateWidgetPtr()
{
	if (DoesSharedInstanceExist())
	{
		return AsShared();
	}
	return nullptr;
}

TSharedPtr<SWidget> SSMEditableText::BuildContextMenuContent() const
{
	if (!bAllowContextMenu.Get())
	{
		return nullptr;
	}

	if (OnContextMenuOpening.IsBound())
	{
		return OnContextMenuOpening.Execute();
	}

	return EditableTextLayout->BuildDefaultContextMenu(MenuExtender);
}

void SSMEditableText::OnTextChanged(const FText& InText)
{
	OnTextChangedCallback.ExecuteIfBound(InText);
}

void SSMEditableText::OnTextCommitted(const FText& InText, const ETextCommit::Type InTextAction)
{
	OnTextCommittedCallback.ExecuteIfBound(InText, InTextAction);
}

void SSMEditableText::OnCursorMoved(const FTextLocation& InLocation)
{
	OnCursorMovedCallback.ExecuteIfBound(InLocation);
}

float SSMEditableText::UpdateAndClampHorizontalScrollBar(const float InViewOffset, const float InViewFraction, const EVisibility InVisiblityOverride)
{
	if (HScrollBar.IsValid())
	{
		HScrollBar->SetState(InViewOffset, InViewFraction);
		HScrollBar->SetUserVisibility(InVisiblityOverride);
		if (!HScrollBar->IsNeeded())
		{
			// We cannot scroll, so ensure that there is no offset
			return 0.0f;
		}
	}

	return EditableTextLayout->GetScrollOffset().X;
}

float SSMEditableText::UpdateAndClampVerticalScrollBar(const float InViewOffset, const float InViewFraction, const EVisibility InVisiblityOverride)
{
	if (VScrollBar.IsValid())
	{
		VScrollBar->SetState(InViewOffset, InViewFraction);
		VScrollBar->SetUserVisibility(InVisiblityOverride);
		if (!VScrollBar->IsNeeded())
		{
			// We cannot scroll, so ensure that there is no offset
			return 0.0f;
		}
	}

	return EditableTextLayout->GetScrollOffset().Y;
}

FReply SSMEditableText::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	EditableTextLayout->HandleFocusReceived(InFocusEvent);
	return FReply::Handled();
}

void SSMEditableText::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	bIsSoftwareCursor = false;
	EditableTextLayout->HandleFocusLost(InFocusEvent);
}

bool SSMEditableText::AnyTextSelected() const
{
	return EditableTextLayout->AnyTextSelected();
}

void SSMEditableText::SelectAllText()
{
	EditableTextLayout->SelectAllText();
}

void SSMEditableText::ClearSelection()
{
	EditableTextLayout->ClearSelection();
}

FText SSMEditableText::GetSelectedText() const
{
	return EditableTextLayout->GetSelectedText();
}

void SSMEditableText::InsertTextAtCursor(const FText& InText)
{
	EditableTextLayout->InsertTextAtCursor(InText.ToString());
}

void SSMEditableText::InsertTextAtCursor(const FString& InString)
{
	EditableTextLayout->InsertTextAtCursor(InString);
}

void SSMEditableText::InsertRunAtCursor(TSharedRef<IRun> InRun)
{
	EditableTextLayout->InsertRunAtCursor(InRun);
}

void SSMEditableText::GoTo(const FTextLocation& NewLocation)
{
	EditableTextLayout->GoTo(NewLocation);
}

void SSMEditableText::GoTo(ETextLocation GoToLocation)
{
	EditableTextLayout->GoTo(GoToLocation);
}

void SSMEditableText::ScrollTo(const FTextLocation& NewLocation)
{
	EditableTextLayout->ScrollTo(NewLocation);
}

void SSMEditableText::ApplyToSelection(const FRunInfo& InRunInfo, const FTextBlockStyle& InStyle)
{
	EditableTextLayout->ApplyToSelection(InRunInfo, InStyle);
}

void SSMEditableText::BeginSearch(const FText& InSearchText, const ESearchCase::Type InSearchCase, const bool InReverse)
{
	EditableTextLayout->BeginSearch(InSearchText, InSearchCase, InReverse);
}

void SSMEditableText::AdvanceSearch(const bool InReverse)
{
	EditableTextLayout->AdvanceSearch(InReverse);
}

TSharedPtr<const IRun> SSMEditableText::GetRunUnderCursor() const
{
	return EditableTextLayout->GetRunUnderCursor();
}

TArray<TSharedRef<const IRun>> SSMEditableText::GetSelectedRuns() const
{
	return EditableTextLayout->GetSelectedRuns();
}

TSharedPtr<const SScrollBar> SSMEditableText::GetHScrollBar() const
{
	return HScrollBar;
}

TSharedPtr<const SScrollBar> SSMEditableText::GetVScrollBar() const
{
	return VScrollBar;
}

void SSMEditableText::Refresh()
{
	EditableTextLayout->Refresh();
}

void SSMEditableText::ForceScroll(int32 UserIndex, float ScrollAxisMagnitude)
{
	const FGeometry& CachedGeom = GetCachedGeometry();
	FVector2D ScrollPos = (CachedGeom.LocalToAbsolute(FVector2D::ZeroVector) + CachedGeom.LocalToAbsolute(CachedGeom.GetLocalSize())) * 0.5f;
	TSet<FKey> PressedKeys;

	OnMouseWheel(CachedGeom, FPointerEvent(UserIndex, 0, ScrollPos, ScrollPos, PressedKeys, EKeys::Invalid, ScrollAxisMagnitude, FModifierKeysState()));
}

void SSMEditableText::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	EditableTextLayout->Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

int32 SSMEditableText::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FTextBlockStyle& EditableTextStyle = EditableTextLayout->GetTextStyle();
	const FLinearColor ForegroundColor = EditableTextStyle.ColorAndOpacity.GetColor(InWidgetStyle);

	FWidgetStyle TextWidgetStyle = FWidgetStyle(InWidgetStyle)
		.SetForegroundColor(ForegroundColor);

	LayerId = EditableTextLayout->OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, TextWidgetStyle, ShouldBeEnabled(bParentEnabled));

	if (bIsSoftwareCursor)
	{
		const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("SoftwareCursor_Grab"));
		const FVector2f CursorSize = Brush->ImageSize / AllottedGeometry.Scale;
		
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(CursorSize, FSlateLayoutTransform(SoftwareCursorPosition - (CursorSize * 0.5f))),
			Brush
		);
	}

	return LayerId;
}

void SSMEditableText::CacheDesiredSize(float LayoutScaleMultiplier)
{
	EditableTextLayout->CacheDesiredSize(LayoutScaleMultiplier);
	SWidget::CacheDesiredSize(LayoutScaleMultiplier);
}

FVector2D SSMEditableText::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return EditableTextLayout->ComputeDesiredSize(LayoutScaleMultiplier);
}

FChildren* SSMEditableText::GetChildren()
{
	return EditableTextLayout->GetChildren();
}

void SSMEditableText::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	EditableTextLayout->OnArrangeChildren(AllottedGeometry, ArrangedChildren);
}

bool SSMEditableText::SupportsKeyboardFocus() const
{
	return true;
}

FReply SSMEditableText::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	FReply Reply = FReply::Unhandled();

	// First call the user defined key handler, there might be overrides to normal functionality
	if (OnKeyCharHandler.IsBound())
	{
		Reply = OnKeyCharHandler.Execute(MyGeometry, InCharacterEvent);
	}

	if (!Reply.IsEventHandled())
	{
		Reply = EditableTextLayout->HandleKeyChar(InCharacterEvent);
	}

	return Reply;
}

FReply SSMEditableText::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FReply Reply = FReply::Unhandled();

	// First call the user defined key handler, there might be overrides to normal functionality
	if (OnKeyDownHandler.IsBound())
	{
		Reply = OnKeyDownHandler.Execute(MyGeometry, InKeyEvent);
	}

	if (!Reply.IsEventHandled())
	{
		Reply = EditableTextLayout->HandleKeyDown(InKeyEvent);
	}

	return Reply;
}

FReply SSMEditableText::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return EditableTextLayout->HandleKeyUp(InKeyEvent);
}

FReply SSMEditableText::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		AmountScrolledWhileRightMouseDown = 0.0f;
	}

	return EditableTextLayout->HandleMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SSMEditableText::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bool bWasRightClickScrolling = IsRightClickScrolling();
		AmountScrolledWhileRightMouseDown = 0.0f;

		if (bWasRightClickScrolling)
		{
			bIsSoftwareCursor = false;
			const FVector2D CursorPosition = MyGeometry.LocalToAbsolute(SoftwareCursorPosition);
			const FIntPoint OriginalMousePos(CursorPosition.X, CursorPosition.Y);
			return FReply::Handled().ReleaseMouseCapture().SetMousePos(OriginalMousePos);
		}
	}

	return EditableTextLayout->HandleMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SSMEditableText::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		const float ScrollByAmount = MouseEvent.GetCursorDelta().Y / MyGeometry.Scale;

		// If scrolling with the right mouse button, we need to remember how much we scrolled.
		// If we did not scroll at all, we will bring up the context menu when the mouse is released.
		AmountScrolledWhileRightMouseDown += FMath::Abs(ScrollByAmount);

		if (IsRightClickScrolling())
		{
			const FVector2D PreviousScrollOffset = EditableTextLayout->GetScrollOffset();

			FVector2D NewScrollOffset = PreviousScrollOffset;
			NewScrollOffset.Y -= ScrollByAmount;
			EditableTextLayout->SetScrollOffset(NewScrollOffset, MyGeometry);

			if (!bIsSoftwareCursor)
			{
				SoftwareCursorPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
				bIsSoftwareCursor = true;
			}

			if (PreviousScrollOffset.Y != NewScrollOffset.Y)
			{
				const float ScrollMax = EditableTextLayout->GetSize().Y - MyGeometry.GetLocalSize().Y;
				const float ScrollbarOffset = (ScrollMax != 0.0f) ? NewScrollOffset.Y / ScrollMax : 0.0f;
				OnVScrollBarUserScrolled.ExecuteIfBound(ScrollbarOffset);
				SoftwareCursorPosition.Y += (PreviousScrollOffset.Y - NewScrollOffset.Y);
			}

			return FReply::Handled().UseHighPrecisionMouseMovement(AsShared());
		}
	}

	return EditableTextLayout->HandleMouseMove(MyGeometry, MouseEvent);
}

FReply SSMEditableText::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (VScrollBar.IsValid() && VScrollBar->IsNeeded())
	{
		const float ScrollAmount = -MouseEvent.GetWheelDelta() * GetGlobalScrollAmount();

		const FVector2D PreviousScrollOffset = EditableTextLayout->GetScrollOffset();

		FVector2D NewScrollOffset = PreviousScrollOffset;
		NewScrollOffset.Y += ScrollAmount;
		EditableTextLayout->SetScrollOffset(NewScrollOffset, MyGeometry);

		if (PreviousScrollOffset.Y != NewScrollOffset.Y)
		{
			const float ScrollMax = EditableTextLayout->GetSize().Y - MyGeometry.GetLocalSize().Y;
			const float ScrollbarOffset = (ScrollMax != 0.0f) ? NewScrollOffset.Y / ScrollMax : 0.0f;
			OnVScrollBarUserScrolled.ExecuteIfBound(ScrollbarOffset);
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SSMEditableText::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return EditableTextLayout->HandleMouseButtonDoubleClick(MyGeometry, MouseEvent);
}

FCursorReply SSMEditableText::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (IsRightClickScrolling() && CursorEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		return FCursorReply::Cursor(EMouseCursor::None);
	}
	else
	{
		return FCursorReply::Cursor(EMouseCursor::TextEditBeam);
	}
}

bool SSMEditableText::ShouldSelectWordOnMouseDoubleClick() const
{
	return true;
}

bool SSMEditableText::IsInteractable() const
{
	return IsEnabled();
}

bool SSMEditableText::ComputeVolatility() const
{
	return SWidget::ComputeVolatility()
		|| HasKeyboardFocus()
		|| EditableTextLayout->ComputeVolatility()
		|| bIsReadOnly.IsBound();
}

bool SSMEditableText::IsRightClickScrolling() const
{
	return AmountScrolledWhileRightMouseDown >= FSlateApplication::Get().GetDragTriggerDistance() && VScrollBar.IsValid() && VScrollBar->IsNeeded();
}

#undef LOCTEXT_NAMESPACE
