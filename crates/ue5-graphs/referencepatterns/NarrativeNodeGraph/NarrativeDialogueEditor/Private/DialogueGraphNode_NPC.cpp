// Copyright Narrative Tools 2022. 

#include "DialogueGraphNode_NPC.h"
#include "DialogueGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "DialogueEditorTypes.h"
#include "DialogueGraphNode.h"
#include "DialogueSM.h"
#include "Dialogue.h"
#include "DialogueEditorSettings.h"
#include <DialogueBlueprint.h>

void UDialogueGraphNode_NPC::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UDialogueEditorTypes::PinCategory_SingleNode, TEXT(""));
	CreatePin(EGPD_Output, UDialogueEditorTypes::PinCategory_SingleNode, TEXT(""));
}

FLinearColor UDialogueGraphNode_NPC::GetGraphNodeColor() const
{
	if (UDialogueBlueprint* DialogueBlueprint = Cast<UDialogueBlueprint>(GetGraph()->GetOuter()))
	{
		if (UDialogue* DialogueCDO = Cast<UDialogue>(DialogueBlueprint->GeneratedClass->GetDefaultObject()))
		{
			if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(DialogueNode))
			{
				return DialogueCDO->GetSpeaker(NPCNode->GetSpeakerID()).NodeColor;
			}
		}
	}

	return GetDefault<UDialogueEditorSettings>()->NPCNodeColor;
}

FText UDialogueGraphNode_NPC::GetNodeTitleText() const
{
	if (UDialogueBlueprint* DialogueBlueprint = Cast<UDialogueBlueprint>(GetGraph()->GetOuter()))
	{
		if (UDialogue* DialogueCDO = Cast<UDialogue>(DialogueBlueprint->GeneratedClass->GetDefaultObject()))
		{
			if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(DialogueNode))
			{
				return FText::FromName(DialogueCDO->GetSpeaker(NPCNode->GetSpeakerID()).SpeakerID);
			}
		}
	}
	return FText::GetEmpty();
}
