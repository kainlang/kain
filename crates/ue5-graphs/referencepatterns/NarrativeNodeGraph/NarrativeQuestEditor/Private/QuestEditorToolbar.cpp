// Copyright Narrative Tools 2022. 

#include "QuestEditorToolbar.h"
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
#include "QuestGraphEditor.h"
#include "WorkflowOrientedApp/SModeWidget.h"
#include "QuestEditorCommands.h"

#define LOCTEXT_NAMESPACE "QuestEditorToolbar"



void FQuestEditorToolbar::AddModesToolbar(TSharedPtr<FExtender> Extender)
{
	check(QuestEditor.IsValid());
	TSharedPtr<FQuestGraphEditor> QuestEditorPtr = QuestEditor.Pin();

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		QuestEditorPtr->GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FQuestEditorToolbar::FillModesToolbar));
}


void FQuestEditorToolbar::AddQuestToolbar(TSharedPtr<FExtender> Extender)
{
	//Not currently using this in the 4.26 release, may add back in later
	return;

	check(QuestEditor.IsValid());
	TSharedPtr<FQuestGraphEditor> QuestGraphEditorPtr = QuestEditor.Pin();

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	ToolbarExtender->AddToolBarExtension("Asset", EExtensionHook::After, QuestGraphEditorPtr->GetToolkitCommands(), FToolBarExtensionDelegate::CreateSP(this, &FQuestEditorToolbar::FillQuestToolbar));
	QuestGraphEditorPtr->AddToolbarExtender(ToolbarExtender);
}

void FQuestEditorToolbar::FillModesToolbar(FToolBarBuilder& ToolbarBuilder)
{
	check(QuestEditor.IsValid());

	TSharedPtr<FQuestGraphEditor> QuestEditorPtr = QuestEditor.Pin();

	TAttribute<FName> GetActiveMode(QuestEditorPtr.ToSharedRef(), &FQuestGraphEditor::GetCurrentMode);
	FOnModeChangeRequested SetActiveMode = FOnModeChangeRequested::CreateSP(QuestEditorPtr.ToSharedRef(), &FQuestGraphEditor::SetCurrentMode);

	// Left side padding
	QuestEditorPtr->AddToolbarWidget(SNew(SSpacer).Size(FVector2D(4.0f, 1.0f)));

	QuestEditorPtr->AddToolbarWidget(
		SNew(SModeWidget, FQuestGraphEditor::GetLocalizedMode(FQuestGraphEditor::QuestEditorMode), FQuestGraphEditor::QuestEditorMode)
		.OnGetActiveMode(GetActiveMode)
		.OnSetActiveMode(SetActiveMode)
		.CanBeSelected(QuestEditorPtr.Get(), &FQuestGraphEditor::CanAccessQuestEditorMode)
		.ToolTipText(LOCTEXT("QuestEditorModeButtonTooltip", "Switch to Quest Designer Mode"))
		.IconImage(FAppStyle::Get().GetBrush("BTEditor.SwitchToBehaviorTreeMode"))
		//.SmallIconImage(FAppStyle::GetBrush("BTEditor.SwitchToBehaviorTreeMode.Small"))
	);

	// Right side padding
	QuestEditorPtr->AddToolbarWidget(SNew(SSpacer).Size(FVector2D(4.0f, 1.0f)));
}

void FQuestEditorToolbar::FillQuestToolbar(FToolBarBuilder& ToolbarBuilder)
{
	check(QuestEditor.IsValid());
	TSharedPtr<FQuestGraphEditor> QuestEditorPtr = QuestEditor.Pin();

	ToolbarBuilder.BeginSection("Quest");
	ToolbarBuilder.AddToolBarButton(FQuestEditorCommands::Get().ShowQuestDetails);
	ToolbarBuilder.AddToolBarButton(FQuestEditorCommands::Get().ViewTutorial);
	ToolbarBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE