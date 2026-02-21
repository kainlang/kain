// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "GraphEditAction.h"
#include "DialogueGraph.generated.h"

class UDialogueGraphNode;
class UDialogue;
class UDialogueBranch;
class UDialogueState;

/**
 * 
 */
UCLASS()
class UDialogueGraph : public UEdGraph
{
	GENERATED_BODY()

public:

	virtual void OnCreated();
	virtual void OnLoaded();
	virtual void Initialize();
	virtual void NotifyGraphChanged(const FEdGraphEditAction& Action);

	void PinRewired(UDialogueGraphNode* Node, UEdGraphPin* Pin);
	void NodeAdded(UEdGraphNode* AddedNode);

protected:

	class UDialogueNode_NPC* MakeNPCReply(class UDialogueGraphNode_NPC* Node, class UDialogue* Dialogue);
	class UDialogueNode_Player* MakePlayerReply(class UDialogueGraphNode_Player* Node, class UDialogue* Dialogue);

};
