// Copyright Narrative Tools 2022. 

#include "QuestEditorModes.h"
#include "QuestEditorTabs.h"
#include "QuestEditorToolbar.h"
#include "QuestEditorDetails.h"
#include "QuestEditorTabFactories.h"
#include "BlueprintEditorTabs.h"

#define LOCTEXT_NAMESPACE "QuestGraphApplicationMode"

FQuestEditorApplicationMode::FQuestEditorApplicationMode(TSharedPtr<class FQuestGraphEditor> InQuestGraphEditor)
	: FBlueprintEditorApplicationMode(InQuestGraphEditor, FQuestGraphEditor::QuestEditorMode, FQuestGraphEditor::GetLocalizedMode, false, false)
{
	QuestGraphEditor = InQuestGraphEditor;

	QuestEditorTabFactories.RegisterFactory(MakeShareable(new FQuestDetailsSummoner(InQuestGraphEditor)));

	//This is removed for now in 4.26
	//TabLayout = FTabManager::NewLayout("Standalone_QuestGraphEditor_Layout_v2")
	//->AddArea
	//(
	//	FTabManager::NewPrimaryArea()
	//	->SetOrientation(Orient_Vertical)
	//	->Split
	//	(
	//		// Main application area
	//		FTabManager::NewSplitter()
	//		->SetOrientation(Orient_Horizontal)
	//		->Split
	//		(
	//			// Left side
	//			FTabManager::NewSplitter()
	//			->SetSizeCoefficient(0.25f)
	//			->SetOrientation(Orient_Vertical)
	//			//->Split
	//			//(
	//			//	// Left top - viewport
	//			//	FTabManager::NewStack()
	//			//	->SetSizeCoefficient(0.5f)
	//			//	->SetHideTabWell(true)
	//			//	->AddTab(AnimationBlueprintEditorTabs::ViewportTab, ETabState::OpenedTab)
	//			//)
	//			->Split
	//			(
	//				//	Left bottom - preview settings
	//				FTabManager::NewStack()
	//				->SetSizeCoefficient(0.5f)
	//				//->AddTab(AnimationBlueprintEditorTabs::CurveNamesTab, ETabState::ClosedTab)
	//				//->AddTab(AnimationBlueprintEditorTabs::SkeletonTreeTab, ETabState::ClosedTab)
	//				->AddTab(FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab)
	//			)
	//		)
	//		->Split
	//		(
	//			// Middle 
	//			FTabManager::NewSplitter()
	//			->SetOrientation(Orient_Vertical)
	//			->SetSizeCoefficient(0.55f)
	//			->Split
	//			(
	//				// Middle top - document edit area
	//				FTabManager::NewStack()
	//				->SetSizeCoefficient(0.8f)
	//				->AddTab(FQuestEditorTabs::GraphEditorID, ETabState::OpenedTab)
	//			)
	//			->Split
	//			(
	//				// Middle bottom - compiler results & find
	//				FTabManager::NewStack()
	//				->SetSizeCoefficient(0.2f)
	//				->AddTab(FBlueprintEditorTabs::CompilerResultsID, ETabState::ClosedTab)
	//				->AddTab(FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab)
	//			)
	//		)
	//		->Split
	//		(
	//			// Right side
	//			FTabManager::NewSplitter()
	//			->SetSizeCoefficient(0.2f)
	//			->SetOrientation(Orient_Vertical)
	//			->Split
	//			(
	//				// Right top - selection details panel & overrides
	//				FTabManager::NewStack()
	//				->SetHideTabWell(false)
	//				->SetSizeCoefficient(1.f)
	//				->AddTab(FQuestEditorTabs::GraphDetailsID, ETabState::OpenedTab)
	//			)
	//		)
	//	)
	//);


	if (UToolMenu* Toolbar = InQuestGraphEditor->RegisterModeToolbarIfUnregistered(GetModeName()))
	{
		//InQuestGraphEditor->GetToolbarBuilder()->AddQuestToolbar(ToolbarExtender);
		//InQuestGraphEditor->GetToolbarBuilder()->AddModesToolbar(ToolbarExtender);

		InQuestGraphEditor->GetToolbarBuilder()->AddCompileToolbar(Toolbar);
		InQuestGraphEditor->GetToolbarBuilder()->AddScriptingToolbar(Toolbar);
		InQuestGraphEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(Toolbar);
		InQuestGraphEditor->GetToolbarBuilder()->AddDebuggingToolbar(Toolbar);
	}

	LayoutExtender = MakeShared<FLayoutExtender>();
	TabLayout->ProcessExtensions(*LayoutExtender.Get());
}

void FQuestEditorApplicationMode::RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager)
{
	FBlueprintEditorApplicationMode::RegisterTabFactories(InTabManager);
	check(QuestGraphEditor.IsValid());
	TSharedPtr<FQuestGraphEditor> QuestEditorPtr = QuestGraphEditor.Pin();

	//QuestEditorPtr->RegisterToolbarTab(InTabManager.ToSharedRef());

	QuestEditorPtr->DocumentManager->RegisterDocumentFactory(MakeShareable(new FQuestGraphEditorSummoner(QuestEditorPtr)));

	// Mode-specific setup
	QuestEditorPtr->PushTabFactories(QuestEditorTabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FQuestEditorApplicationMode::PreDeactivateMode()
{
	check(QuestGraphEditor.IsValid());
	TSharedPtr<FQuestGraphEditor> QuestEditorPtr = QuestGraphEditor.Pin();

	QuestEditorPtr->SaveEditedObjectState();

	FBlueprintEditorApplicationMode::PreDeactivateMode();

}

void FQuestEditorApplicationMode::PostActivateMode()
{
	// Reopen any documents that were open when the blueprint was last saved
	check(QuestGraphEditor.IsValid());
	TSharedPtr<FQuestGraphEditor> QuestEditorPtr = QuestGraphEditor.Pin();
	QuestEditorPtr->RestoreQuestGraph();

	FBlueprintEditorApplicationMode::PostActivateMode();
}

#undef LOCTEXT_NAMESPACE
