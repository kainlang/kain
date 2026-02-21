// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include <SBlueprintEditorToolbar.h>
#include "QuestGraphEditor.h"

class FQuestGraphEditor;
class FExtender;
class FToolBarBuilder;

/**
 * 
 */
class FQuestEditorToolbar : public FBlueprintEditorToolbar
{
public:
	FQuestEditorToolbar(TSharedPtr<FQuestGraphEditor> InQuestEditor)
		: FBlueprintEditorToolbar(InQuestEditor)
		{
			QuestEditor = InQuestEditor;
		}

	void AddModesToolbar(TSharedPtr<FExtender> Extender);
	void AddQuestToolbar(TSharedPtr<FExtender> Extender);

private:
	void FillModesToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillQuestToolbar(FToolBarBuilder& ToolbarBuilder);

protected:
	/** Pointer back to the blueprint editor tool that owns us */
	TWeakPtr<FQuestGraphEditor> QuestEditor;
};
