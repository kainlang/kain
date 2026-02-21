// Copyright Narrative Tools 2022. 

#include "DialogueEditorToolbar.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Widgets/Input/SComboButton.h"
#include "EditorStyleSet.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "DialogueGraphEditor.h"
#include "WorkflowOrientedApp/SModeWidget.h"
#include "DialogueEditorCommands.h"

#define LOCTEXT_NAMESPACE "DialogueEditorToolbar"

void FDialogueEditorToolbar::AddModesToolbar(TSharedPtr<FExtender> Extender)
{
	check(DialogueEditor.IsValid());
	TSharedPtr<FDialogueGraphEditor> DialogueEditorPtr = DialogueEditor.Pin();

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		DialogueEditorPtr->GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FDialogueEditorToolbar::FillModesToolbar));
}


void FDialogueEditorToolbar::AddDialogueToolbar(TSharedPtr<FExtender> Extender)
{
	check(DialogueEditor.IsValid());
	TSharedPtr<FDialogueGraphEditor> DialogueGraphEditorPtr = DialogueEditor.Pin();

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	ToolbarExtender->AddToolBarExtension("Asset", EExtensionHook::After, DialogueGraphEditorPtr->GetToolkitCommands(), FToolBarExtensionDelegate::CreateSP(this, &FDialogueEditorToolbar::FillDialogueToolbar));
	DialogueGraphEditorPtr->AddToolbarExtender(ToolbarExtender);
}

void FDialogueEditorToolbar::FillModesToolbar(FToolBarBuilder& ToolbarBuilder)
{
	check(DialogueEditor.IsValid());

	TSharedPtr<FDialogueGraphEditor> DialogueEditorPtr = DialogueEditor.Pin();

	TAttribute<FName> GetActiveMode(DialogueEditorPtr.ToSharedRef(), &FDialogueGraphEditor::GetCurrentMode);
	FOnModeChangeRequested SetActiveMode = FOnModeChangeRequested::CreateSP(DialogueEditorPtr.ToSharedRef(), &FDialogueGraphEditor::SetCurrentMode);

	// Left side padding
	DialogueEditorPtr->AddToolbarWidget(SNew(SSpacer).Size(FVector2D(4.0f, 1.0f)));

	DialogueEditorPtr->AddToolbarWidget(
		SNew(SModeWidget, FDialogueGraphEditor::GetLocalizedMode(FDialogueGraphEditor::DialogueEditorMode), FDialogueGraphEditor::DialogueEditorMode)
		.OnGetActiveMode(GetActiveMode)
		.OnSetActiveMode(SetActiveMode)
		.CanBeSelected(DialogueEditorPtr.Get(), &FDialogueGraphEditor::CanAccessDialogueEditorMode)
		.ToolTipText(LOCTEXT("DialogueEditorModeButtonTooltip", "Switch to Dialogue Designer Mode"))
		.IconImage(FAppStyle::Get().GetBrush("BTEditor.SwitchToBehaviorTreeMode"))
		//.SmallIconImage(FAppStyle::GetBrush("BTEditor.SwitchToBehaviorTreeMode.Small"))
	);

	// Right side padding
	DialogueEditorPtr->AddToolbarWidget(SNew(SSpacer).Size(FVector2D(4.0f, 1.0f)));
}

void FDialogueEditorToolbar::FillDialogueToolbar(FToolBarBuilder& ToolbarBuilder)
{
	check(DialogueEditor.IsValid());
	//TSharedPtr<FDialogueGraphEditor> DialogueEditorPtr = DialogueEditor.Pin();

	//ToolbarBuilder.BeginSection("Dialogue");
	//ToolbarBuilder.AddToolBarButton(FDialogueEditorCommands::Get().ShowDialogueDetails);
	//ToolbarBuilder.AddToolBarButton(FDialogueEditorCommands::Get().ViewTutorial);
	//ToolbarBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE