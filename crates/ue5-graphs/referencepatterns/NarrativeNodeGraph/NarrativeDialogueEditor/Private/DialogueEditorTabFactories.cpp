// Copyright Narrative Tools 2022. 

#include "DialogueEditorTabFactories.h"
#include "DialogueBlueprint.h"
#include "Widgets/Docking/SDockTab.h"
#include "DialogueEditorTabs.h"
#include "EditorStyleSet.h"
#include "Engine/Blueprint.h"
#include "DialogueEditorStyle.h"
#include "DialogueGraphEditor.h"

#define LOCTEXT_NAMESPACE "DialogueGraphEditorFactories"

FDialogueGraphEditorSummoner::FDialogueGraphEditorSummoner(TSharedPtr<class FDialogueGraphEditor> InDialogueEditorPtr)
	: FDocumentTabFactoryForObjects<UEdGraph>(FDialogueEditorTabs::GraphEditorID, InDialogueEditorPtr)
	, DialogueEditorPtr(InDialogueEditorPtr)
{

}

void FDialogueGraphEditorSummoner::OnTabActivated(TSharedPtr<SDockTab> Tab) const
{
	check(DialogueEditorPtr.IsValid());	
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	DialogueEditorPtr.Pin()->OnGraphEditorFocused(GraphEditor);
}

void FDialogueGraphEditorSummoner::OnTabRefreshed(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	GraphEditor->NotifyGraphChanged();
}

TAttribute<FText> FDialogueGraphEditorSummoner::ConstructTabNameForObject(UEdGraph* DocumentID) const
{
	return TAttribute<FText>(FText::FromString(DocumentID->GetName()));
}

TSharedRef<SWidget> FDialogueGraphEditorSummoner::CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const
{
	return OnCreateGraphEditorWidget.Execute(DocumentID);
}

const FSlateBrush* FDialogueGraphEditorSummoner::GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const
{
	return FDialogueEditorStyle::Get()->GetBrush("ClassIcon.DialogueAsset");
}

void FDialogueGraphEditorSummoner::SaveState(TSharedPtr<SDockTab> Tab, TSharedPtr<FTabPayload> Payload) const
{
	check(DialogueEditorPtr.IsValid());
	check(DialogueEditorPtr.Pin()->GetDialogueAsset());


	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());

	FVector2D ViewLocation;
	float ZoomAmount;
	GraphEditor->GetViewLocation(ViewLocation, ZoomAmount);

	UEdGraph* Graph = FTabPayload_UObject::CastChecked<UEdGraph>(Payload);
	DialogueEditorPtr.Pin()->GetDialogueAsset()->LastEditedDocuments.Add(FEditedDocumentInfo(Graph, ViewLocation, ZoomAmount));
}

FDialogueDetailsSummoner::FDialogueDetailsSummoner(TSharedPtr<class FDialogueGraphEditor> InDialogueEditorPtr)
	: FWorkflowTabFactory(FDialogueEditorTabs::GraphDetailsID, InDialogueEditorPtr)
	, DialogueEditorPtr(InDialogueEditorPtr)
{

	TabLabel = LOCTEXT("DialogueDetailsLabel", "Details");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.Tabs.Components");

	bIsSingleton = true;


	ViewMenuDescription = LOCTEXT("DialogueDetailsView", "Details");
	ViewMenuTooltip = LOCTEXT("DialogueDetailsView_ToolTip", "Show the details view");

}

FText FDialogueDetailsSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return LOCTEXT("DialogueDetailsTabTooltip", "The dialogue editor details tab allows editing of dialogue graph nodes");
}

#undef LOCTEXT_NAMESPACE