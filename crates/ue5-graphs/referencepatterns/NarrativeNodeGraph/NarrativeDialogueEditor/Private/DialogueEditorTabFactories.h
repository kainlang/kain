// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "GraphEditor.h"
#include "DialogueGraphEditor.h"
#include "EdGraph/EdGraph.h"
#include "Widgets/SWidget.h"
#include "Misc/Attribute.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"
#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"

struct FDialogueGraphEditorSummoner : public FDocumentTabFactoryForObjects<UEdGraph>
{
public:

	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SGraphEditor>, FOnCreateGraphEditorWidget, UEdGraph*);

	FDialogueGraphEditorSummoner(TSharedPtr<class FDialogueGraphEditor> InDialogueEditorPtr);

	virtual void OnTabActivated(TSharedPtr<SDockTab> Tab) const override;
	virtual void OnTabRefreshed(TSharedPtr<SDockTab> Tab) const override;

protected:
	virtual TAttribute<FText> ConstructTabNameForObject(UEdGraph* DocumentID) const override;
	virtual TSharedRef<SWidget> CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const override;
	virtual const FSlateBrush* GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const override;
	virtual void SaveState(TSharedPtr<SDockTab> Tab, TSharedPtr<FTabPayload> Payload) const override;

protected:
	TWeakPtr<class FDialogueGraphEditor> DialogueEditorPtr;
	FOnCreateGraphEditorWidget OnCreateGraphEditorWidget;

};


struct FDialogueDetailsSummoner : public FWorkflowTabFactory
{
public:
	FDialogueDetailsSummoner(TSharedPtr<class FDialogueGraphEditor> InDialogueEditorPtr);

	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<class FDialogueGraphEditor> DialogueEditorPtr;
};