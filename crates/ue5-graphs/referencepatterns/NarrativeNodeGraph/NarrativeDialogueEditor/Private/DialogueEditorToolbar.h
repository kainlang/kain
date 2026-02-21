// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"

class FDialogueGraphEditor;
class FExtender;
class FToolBarBuilder;

/**
 * 
 */
class FDialogueEditorToolbar : public TSharedFromThis<FDialogueEditorToolbar>
{
public:
	FDialogueEditorToolbar(TSharedPtr<FDialogueGraphEditor> InDialogueEditor)
		: DialogueEditor(InDialogueEditor) {}

	void AddModesToolbar(TSharedPtr<FExtender> Extender);
	void AddDialogueToolbar(TSharedPtr<FExtender> Extender);

private:
	void FillModesToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillDialogueToolbar(FToolBarBuilder& ToolbarBuilder);

protected:
	/** Pointer back to the blueprint editor tool that owns us */
	TWeakPtr<FDialogueGraphEditor> DialogueEditor;
};
