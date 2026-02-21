// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "QuestGraphEditor.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "BlueprintEditorModes.h"

/** Application mode for main behavior tree editing mode */
class FQuestEditorApplicationMode : public FBlueprintEditorApplicationMode
{
public:

	FQuestEditorApplicationMode(TSharedPtr<class FQuestGraphEditor> InQuestGraphEditor);

	virtual void RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager) override;
	virtual void PreDeactivateMode() override;
	virtual void PostActivateMode() override;

protected:
	TWeakPtr<class FQuestGraphEditor> QuestGraphEditor;

	// Set of spawnable tabs in behavior tree editing mode
	FWorkflowAllowedTabSet QuestEditorTabFactories;
};