// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMBlueprintEditorModes.h"

#include "BlueprintEditorTabs.h"
#include "Blueprints/SMBlueprintEditor.h"
#include "SBlueprintEditorToolbar.h"
#include "SMBlueprintEditorTabSpawners.h"
#include "UI/SMBlueprintEditorToolbar.h"

#include "ISMPreviewEditorModule.h"

#define LOCTEXT_NAMESPACE "SMBlueprintEditorModes"

const FName FSMBlueprintEditorModes::SMEditorName("SMEditorApp");
const FName FSMBlueprintEditorModes::SMGraphMode("GraphName");
const FName FSMBlueprintEditorModes::SMPreviewMode("PreviewMode");

FSMBlueprintEditorModeBase::FSMBlueprintEditorModeBase(TSharedPtr<FSMBlueprintEditor> EditorIn, FName EditorModeIn) :
	FBlueprintEditorApplicationMode(StaticCastSharedPtr<FBlueprintEditor>(EditorIn), EditorModeIn, FSMBlueprintEditorModes::GetLocalizedMode, false, false)
{
	Editor = EditorIn;
}

FSMBlueprintEditorGraphMode::FSMBlueprintEditorGraphMode(TSharedPtr<class FSMBlueprintEditor> EditorIn)
	: FSMBlueprintEditorModeBase(EditorIn, FSMBlueprintEditorModes::SMGraphMode)
{
	TabLayout = FTabManager::NewLayout("LogicDriverGraphMode_Layout_v1.1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				// Main application area
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					// Left side
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.2f)
					->Split
					// MyBlueprint View (Graphs & Variables)
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(1.f)
						->AddTab(FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab)
						->SetForegroundTab(FBlueprintEditorTabs::MyBlueprintID)
					)
					// Add more to the left side here
				)
				->Split
				(
					// Middle
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.6f)
					->Split
					(
						// Middle top - graph area
						FTabManager::NewStack()
						->SetSizeCoefficient(0.8f)
						->AddTab("Document", ETabState::ClosedTab)
					)
					->Split
					(
						// Middle bottom - compiler results & find
						FTabManager::NewStack()
						->SetSizeCoefficient(0.2f)
						->AddTab(FBlueprintEditorTabs::CompilerResultsID, ETabState::OpenedTab)
						->AddTab(FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab)
					)
				)
				->Split
				(
					// Right side
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.2f)
					->Split
					(
						// Right top - details view
						FTabManager::NewStack()
						->SetSizeCoefficient(1.f)
						->AddTab(FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab)
					)
					// Add more to right side here.
				)
			)
		);

	ToolbarExtender = MakeShareable(new FExtender);

	if (UToolMenu* Toolbar = EditorIn->RegisterModeToolbarIfUnregistered(GetModeName()))
	{
		EditorIn->GetToolbarBuilder()->AddCompileToolbar(Toolbar);
		EditorIn->GetToolbarBuilder()->AddScriptingToolbar(Toolbar);
		EditorIn->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(Toolbar);
		EditorIn->GetToolbarBuilder()->AddDebuggingToolbar(Toolbar);
	}

	Editor.Pin()->GetStateMachineToolbar()->AddModesToolbar(ToolbarExtender);
}

void FSMBlueprintEditorGraphMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = Editor.Pin();

	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(CoreTabFactories);
	BP->PushTabFactories(BlueprintEditorTabFactories);
	BP->PushTabFactories(EditorTabFactories);

	// Add custom tab factories
}

const FName FSMBlueprintEditorPreviewMode::TabID_DetailsView(TEXT("SMBlueprintEditorPreviewTab_DetailsView"));
const FName FSMBlueprintEditorPreviewMode::TabID_ViewportView(TEXT("SMBlueprintEditorPreviewTab_ViewportView"));
const FName FSMBlueprintEditorPreviewMode::TabID_AdvSceneDetailsView(TEXT("SMBlueprintEditorPreviewTab_AdvancedSceneDetailsView"));

FSMBlueprintEditorPreviewMode::FSMBlueprintEditorPreviewMode(TSharedPtr<FSMBlueprintEditor> EditorIn)
	: FSMBlueprintEditorModeBase(EditorIn, FSMBlueprintEditorModes::SMPreviewMode)
{
	ISMPreviewEditorModule& PreviewModule = FModuleManager::LoadModuleChecked<ISMPreviewEditorModule>(LOGICDRIVER_PREVIEW_MODULE_NAME);
	ViewportView = PreviewModule.CreatePreviewViewportWidget(EditorIn);
	DefaultsView = PreviewModule.CreatePreviewEditorWidget(EditorIn, TabID_DetailsView);
	AdvancedDetailsView = PreviewModule.CreateAdvancedSceneDetailsWidget(EditorIn, ViewportView);
	
	TabLayout = FTabManager::NewLayout("LogicDriverPreviewMode_Layout_v1.1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				// Main application area
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					// Left side
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.2f)
					->Split
					// MyBlueprint View (Graphs & Variables)
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(1.f)
						->AddTab(FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab)
						->SetForegroundTab(FBlueprintEditorTabs::MyBlueprintID)
					)
					// Add more to the left side here
				)
				->Split
				(
					// Middle
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.6f)
					->Split
					(
						// Middle top - preview area
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->AddTab(FSMBlueprintEditorPreviewMode::TabID_ViewportView, ETabState::OpenedTab)
						->SetHideTabWell(true)
						->SetForegroundTab(FSMBlueprintEditorPreviewMode::TabID_ViewportView)
					)
					->Split
					(
						// Middle bottom - graph area
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->AddTab("Document", ETabState::ClosedTab)
					)
					->Split
					(
						// Middle bottom - compiler results & find
						FTabManager::NewStack()
						->SetSizeCoefficient(0.2f)
						->AddTab(FBlueprintEditorTabs::CompilerResultsID, ETabState::ClosedTab)
						->AddTab(FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab)
					)
				)
				->Split
				(
					// Right side
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.2f)
					->Split
					(
						// Right top - debug defaults view
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->AddTab(FSMBlueprintEditorPreviewMode::TabID_DetailsView, ETabState::OpenedTab)
						->AddTab(FSMBlueprintEditorPreviewMode::TabID_AdvSceneDetailsView, ETabState::OpenedTab)
						->SetForegroundTab(FSMBlueprintEditorPreviewMode::TabID_DetailsView)
					)
					->Split
					(
						// Right bottom - details view
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->AddTab(FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab)
					)
					// Add more to right side here.
				)
			)
		);
	
	EditorTabFactories.RegisterFactory(MakeShareable(new FSMPreviewDefaultsViewSummoner(EditorIn, DefaultsView)));
	EditorTabFactories.RegisterFactory(MakeShareable(new FSMPreviewViewportViewSummoner(EditorIn, ViewportView)));
	EditorTabFactories.RegisterFactory(MakeShareable(new FSMPreviewAdvancedDetailsViewSummoner(EditorIn, AdvancedDetailsView)));
	
	ToolbarExtender = MakeShareable(new FExtender);

	if (UToolMenu* Toolbar = EditorIn->RegisterModeToolbarIfUnregistered(GetModeName()))
	{
		EditorIn->GetToolbarBuilder()->AddCompileToolbar(Toolbar);
		EditorIn->GetToolbarBuilder()->AddScriptingToolbar(Toolbar);
		EditorIn->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(Toolbar);
	}

	Editor.Pin()->GetStateMachineToolbar()->AddPreviewToolbar(ToolbarExtender);
	Editor.Pin()->GetStateMachineToolbar()->AddModesToolbar(ToolbarExtender);
}

FSMBlueprintEditorPreviewMode::~FSMBlueprintEditorPreviewMode()
{
	ViewportView.Reset();
	DefaultsView.Reset();
}

void FSMBlueprintEditorPreviewMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = Editor.Pin();

	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(CoreTabFactories);
	BP->PushTabFactories(BlueprintEditorTabFactories);
	BP->PushTabFactories(EditorTabFactories);
}

#undef LOCTEXT_NAMESPACE
