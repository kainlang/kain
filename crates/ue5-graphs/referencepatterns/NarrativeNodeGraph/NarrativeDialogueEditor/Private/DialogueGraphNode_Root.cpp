// Copyright Narrative Tools 2022. 

#include "DialogueGraphNode_Root.h"
#include "DialogueEditorTypes.h"
#include "DialogueEditorSettings.h"

void UDialogueGraphNode_Root::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UDialogueEditorTypes::PinCategory_SingleNode, TEXT(""));
}

bool UDialogueGraphNode_Root::CanUserDeleteNode() const
{
	return false;
}