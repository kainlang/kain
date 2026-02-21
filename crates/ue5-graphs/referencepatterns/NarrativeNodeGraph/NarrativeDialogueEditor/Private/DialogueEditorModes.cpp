// Copyright Narrative Tools 2022. 

#include "DialogueEditorModes.h"
#include "DialogueEditorTabs.h"
#include "DialogueEditorToolbar.h"
#include "DialogueEditorDetails.h"
#include "DialogueEditorTabFactories.h"
#include "BlueprintEditorTabs.h"
#include "SBlueprintEditorToolbar.h"
#include "DialogueGraphEditor.h"

#define LOCTEXT_NAMESPACE "DialogueGraphApplicationMode"

FDialogueEditorApplicationMode::FDialogueEditorApplicationMode(TSharedPtr<class FDialogueGraphEditor> InDialogueGraphEditor)
	: FBlueprintEditorApplicationMode(InDialogueGraphEditor, FDialogueGraphEditor::DialogueEditorMode, FDialogueGraphEditor::GetLocalizedMode, false, false)
{
	DialogueGraphEditor = InDialogueGraphEditor;

	DialogueEditorTabFactories.RegisterFactory(MakeShareable(new FDialogueDetailsSummoner(InDialogueGraphEditor)));

	//

	//TabLayout = FTabManager::NewLayout("Standalone_DialogueGraphEditor_Layout_v1")
	//->AddArea
	//(
	//	FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
	//		->Split
	//		(
	//			FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
	//			->SetSizeCoefficient(0.3f)
	//			->Split
	//			(
	//				FTabManager::NewStack()
	//				->SetSizeCoefficient(0.7f)
	//				->SetHideTabWell(true)
	//				->AddTab(FDialogueEditorTabs::GraphEditorID, ETabState::OpenedTab)
	//			)
	//			->Split
	//			(
	//				FTabManager::NewStack()
	//				->SetSizeCoefficient(0.3f)
	//				->SetHideTabWell(true)
	//				->AddTab(FDialogueEditorTabs::GraphDetailsID, ETabState::OpenedTab)
	//			)
	//		)
	//);

	//InDialogueGraphEditor->GetToolbarBuilder()->AddDialogueToolbar(ToolbarExtender);
	//InDialogueGraphEditor->GetToolbarBuilder()->AddModesToolbar(ToolbarExtender);

	if (UToolMenu* Toolbar = InDialogueGraphEditor->RegisterModeToolbarIfUnregistered(GetModeName()))
	{
		//InDialogueGraphEditor->GetToolbarBuilder()->AddDialogueToolbar(ToolbarExtender);
		//InDialogueGraphEditor->GetToolbarBuilder()->AddModesToolbar(ToolbarExtender);

		InDialogueGraphEditor->GetToolbarBuilder()->AddCompileToolbar(Toolbar);
		InDialogueGraphEditor->GetToolbarBuilder()->AddScriptingToolbar(Toolbar);
		InDialogueGraphEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(Toolbar);
		InDialogueGraphEditor->GetToolbarBuilder()->AddDebuggingToolbar(Toolbar);
	}

	LayoutExtender = MakeShared<FLayoutExtender>();
	TabLayout->ProcessExtensions(*LayoutExtender.Get());
}

void FDialogueEditorApplicationMode::RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager)
{
	FBlueprintEditorApplicationMode::RegisterTabFactories(InTabManager);
	check(DialogueGraphEditor.IsValid());
	TSharedPtr<FDialogueGraphEditor> DialogueEditorPtr = DialogueGraphEditor.Pin();

	//DialogueEditorPtr->RegisterToolbarTab(InTabManager.ToSharedRef());

	DialogueEditorPtr->DocumentManager->RegisterDocumentFactory(MakeShareable(new FDialogueGraphEditorSummoner(DialogueEditorPtr)));

	// Mode-specific setup
	DialogueEditorPtr->PushTabFactories(DialogueEditorTabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FDialogueEditorApplicationMode::PreDeactivateMode()
{
	check(DialogueGraphEditor.IsValid());
	TSharedPtr<FDialogueGraphEditor> DialogueEditorPtr = DialogueGraphEditor.Pin();

	DialogueEditorPtr->SaveEditedObjectState();

	FBlueprintEditorApplicationMode::PreDeactivateMode();
}

void FDialogueEditorApplicationMode::PostActivateMode()
{
	// Reopen any documents that were open when the blueprint was last saved
	check(DialogueGraphEditor.IsValid());
	TSharedPtr<FDialogueGraphEditor> DialogueEditorPtr = DialogueGraphEditor.Pin();
	DialogueEditorPtr->RestoreDialogueGraph();

	FBlueprintEditorApplicationMode::PostActivateMode();
}

#undef LOCTEXT_NAMESPACE
