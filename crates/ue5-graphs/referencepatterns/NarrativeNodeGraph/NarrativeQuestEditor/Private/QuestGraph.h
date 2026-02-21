// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "GraphEditAction.h"
#include "QuestGraph.generated.h"

class UQuestGraphNode;
class UQuestGraphNode_State; 
class UQuestGraphNode_Action;
class UQuest;
class UQuestNode;
class UQuestBranch;
class UQuestState;

/**
 * 
 */
UCLASS()
class UQuestGraph : public UEdGraph
{
	GENERATED_BODY()

public:

	virtual void OnCreated();
	virtual void OnLoaded();
	virtual void Initialize();
	virtual void NotifyGraphChanged(const FEdGraphEditAction& Action);

	void PinRewired(UQuestGraphNode* Node, UEdGraphPin* Pin);
	void NodeAdded(UEdGraphNode* AddedNode);

	//Pointer to the graphs persistent tasks node for convenience
	class UQuestGraphNode_PersistentTasks* PTNode;

protected:

	void PIEStarted(bool bIsSimulating);

	//Called when the players objective is updated in a PIE Session. Used to show debug updates to designers
	UFUNCTION()
	void OnPIEObjectiveUpdated(FText QuestName, TArray<FText> NewObjectives, FText NewStateText);

	//Helper functions when we are processing the nodes into a Quest 
	UQuestState* MakeState(UQuestGraphNode_State* Node, UQuest* Quest);
	UQuestBranch* MakeBranch(UQuestGraphNode_Action* Node, UQuest* Quest);

};
