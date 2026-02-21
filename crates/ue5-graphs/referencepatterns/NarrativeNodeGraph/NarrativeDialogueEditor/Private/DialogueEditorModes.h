// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "DialogueGraphEditor.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "BlueprintEditorModes.h"

/** Application mode for main behavior tree editing mode */
class FDialogueEditorApplicationMode : public FBlueprintEditorApplicationMode
{
public:
	FDialogueEditorApplicationMode(TSharedPtr<class FDialogueGraphEditor> InDialogueGraphEditor);

	virtual void RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager) override;
	virtual void PreDeactivateMode() override;
	virtual void PostActivateMode() override;

protected:
	TWeakPtr<class FDialogueGraphEditor> DialogueGraphEditor;

	// Set of spawnable tabs in behavior tree editing mode
	FWorkflowAllowedTabSet DialogueEditorTabFactories;
};