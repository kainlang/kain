// Copyright Narrative Tools 2022. 

#include "QuestGraph.h"
#include "Quest.h"
#include "QuestBlueprint.h"
#include "QuestGraphNode.h"
#include "QuestGraphNode_Root.h"
#include "QuestGraphNode_Success.h"
#include "QuestGraphNode_Failure.h"
#include "QuestGraphNode_Action.h"
#include "QuestGraphNode_PersistentTasks.h"
#include "EdGraph/EdGraphPin.h"
#include "QuestSM.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "NarrativeFunctionLibrary.h"
#include "NarrativeDataTask.h"
#include "NarrativeComponent.h"
#include "QuestEditorSettings.h"


void UQuestGraph::OnCreated()
{
}

void UQuestGraph::OnLoaded()
{
}

void UQuestGraph::Initialize()
{
	//Commented out for now as was occasionally causing crashes
	//FEditorDelegates::PostPIEStarted.AddUObject(this, &UQuestGraph::PIEStarted);
}

void UQuestGraph::NotifyGraphChanged(const FEdGraphEditAction& Action)
{
	Super::NotifyGraphChanged(Action);

	if (Action.Action == EEdGraphActionType::GRAPHACTION_RemoveNode)
	{
		UQuestBlueprint* QuestAsset = CastChecked<UQuestBlueprint>(GetOuter());
		UQuest* Quest = QuestAsset->QuestTemplate;

		//If we rewired a pin, we need to update the quest nodes to match the graph nodes
		if (Quest)
		{
			for (auto& DeletedNode : Action.Nodes)
			{
				if (UQuestGraphNode* QuestGraphNode = const_cast<UQuestGraphNode*>(Cast<UQuestGraphNode>(DeletedNode)))
				{
					//If a branch was deleted, remove it from any the states leading to it 
					if (UQuestGraphNode_Action* ActionNode = Cast<UQuestGraphNode_Action>(QuestGraphNode))
					{
						for (auto& State : Quest->GetStates())
						{
							if (State)
							{
								State->Branches.Remove(ActionNode->Branch);
							}
						}
					} //If a state was deleted, remove it from any branches that lead to it
					else if (UQuestGraphNode_State* StateNode = Cast<UQuestGraphNode_State>(QuestGraphNode))
					{
						for (auto& Branch : Quest->GetBranches())
						{
							if (Branch && Branch->DestinationState == StateNode->State)
							{
								Branch->DestinationState = nullptr;
							}
						}
					}
				}
			}
		}
	}
}

void UQuestGraph::PinRewired(UQuestGraphNode* Node, UEdGraphPin* Pin)
{
	UQuestBlueprint* QuestAsset = CastChecked<UQuestBlueprint>(GetOuter());
	UQuest* Quest = QuestAsset->QuestTemplate;

	//If we rewired a pin, we need to update the quest nodes to match the graph nodes
	if (Quest)
	{
		if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			//If a PTNode connected to a new task, all states need to have that task
			if (UQuestGraphNode_PersistentTasks* FromPTNode = Cast<UQuestGraphNode_PersistentTasks>(Node))
			{
				for (auto& LinkedPin : Pin->LinkedTo)
				{
					if (UQuestGraphNode_Action* To = Cast<UQuestGraphNode_Action>(LinkedPin->GetOwningNode()))
					{
						for (auto& State : Quest->GetStates())
						{
							//Don't add the state for failure/success nodes, since failure and success nodes shouldn't have branches
							if (State->StateNodeType == EStateNodeType::Regular)
							{
								State->Branches.AddUnique(To->Branch);
							}
						}
					}
				}

			}//If a states output link now goes somewhere else, update it
			else if (UQuestGraphNode_State* FromState = Cast<UQuestGraphNode_State>(Node))
			{
				FromState->State->Branches.Empty();
				for (auto& LinkedPin : Pin->LinkedTo)
				{
					if (UQuestGraphNode_Action* To = Cast<UQuestGraphNode_Action>(LinkedPin->GetOwningNode()))
					{
						FromState->State->Branches.AddUnique(To->Branch);
					}
				}
			}//An action node now links somewhere else, update it
			else if (UQuestGraphNode_Action* FromAction = Cast<UQuestGraphNode_Action>(Node))
			{
				if (Pin->LinkedTo.IsValidIndex(0))
				{
					UQuestGraphNode_State* To = Cast<UQuestGraphNode_State>(Pin->LinkedTo[0]->GetOwningNode());
					FromAction->Branch->DestinationState = To ? To->State : nullptr;
				}
				else
				{
					FromAction->Branch->DestinationState = nullptr;
				}
			}
		}
	}
}

void UQuestGraph::NodeAdded(UEdGraphNode* AddedNode)
{

	UQuestBlueprint* QuestAsset = CastChecked<UQuestBlueprint>(GetOuter());
	UQuest* Quest = QuestAsset->QuestTemplate;

	//Handle initializing the quest and creating the quest root state
	if (QuestAsset)
	{
		check(Quest);
		if (!Quest)
		{
			return;
		}
		//Initialize the quest start state if it hasn't already been created 
		if (UQuestGraphNode_Root* RootGraphNode = Cast<UQuestGraphNode_Root>(AddedNode))
		{
			if (!Quest->GetQuestStartState())
			{
				UQuestState* RootNode = MakeState(RootGraphNode, Quest);
				RootNode->SetID(FName("QuestStart"));
				RootNode->Description = FText::FromString("This is the start of my Quest.");
				Quest->SetQuestStartState(RootNode);
				RootGraphNode->State = RootNode;
			}
			return;
		}
	}

	ensure(Quest);

	//Create a quest node for the new graph node that was added
	if (UQuestGraphNode_State* StateNode = Cast<UQuestGraphNode_State>(AddedNode))
	{
		if (!StateNode->State)
		{
			StateNode->State = MakeState(StateNode, Quest);
		}
	}
	else if (UQuestGraphNode_Action* ActionNode = Cast<UQuestGraphNode_Action>(AddedNode))
	{
		if (!ActionNode->Branch)
		{
			ActionNode->Branch = MakeBranch(ActionNode, Quest);
		}
		else
		{
			//Node already has a branch set, was probably created from context menu 
			if (ActionNode->Branch)
			{
				Quest->AddBranch(ActionNode->Branch);

				//Use rename to fix a bug where old outer wasnt being set properly
				ActionNode->Branch->Rename(nullptr, Quest);
			}
		}
	}
}

void UQuestGraph::PIEStarted(bool bIsSimulating)
{

}

void UQuestGraph::OnPIEObjectiveUpdated(FText QuestName, TArray<FText> NewObjectives, FText NewStateText)
{

}

UQuestState* UQuestGraph::MakeState(UQuestGraphNode_State* Node, UQuest* Quest)
{
	FSoftClassPath StateClassPath = GetDefault<UQuestEditorSettings>()->DefaultQuestState;
	UClass* StateClass = (StateClassPath.IsValid() ? LoadObject<UClass>(NULL, *StateClassPath.ToString()) : UQuestState::StaticClass());

	if (!IsValid(StateClass))
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to load Quest State class '%s'. Falling back to generic UQuestState."), *StateClassPath.ToString());
		StateClass = UQuestState::StaticClass();
	}

	UQuestState* State = NewObject<UQuestState>(Quest, StateClass);

	Node->QuestNode = State;

	//Fill in the completion types
	if (Cast<UQuestGraphNode_Failure>(Node))
	{
		State->StateNodeType = EStateNodeType::Failure;
	}
	else if (Cast<UQuestGraphNode_Success>(Node))
	{
		State->StateNodeType = EStateNodeType::Success;
	}

	Quest->AddState(State);

	return State;
}

UQuestBranch* UQuestGraph::MakeBranch(UQuestGraphNode_Action* Node, UQuest* Quest)
{
	FSoftClassPath BranchClassPath = GetDefault<UQuestEditorSettings>()->DefaultQuestBranch;
	UClass* BranchClass = (BranchClassPath.IsValid() ? LoadObject<UClass>(NULL, *BranchClassPath.ToString()) : UQuestBranch::StaticClass());

	if (!IsValid(BranchClass))
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to load Quest Branch class '%s'. Falling back to generic UQuestBranch."), *BranchClassPath.ToString());
		BranchClass = UQuestBranch::StaticClass();
	}

	UQuestBranch* Branch = NewObject<UQuestBranch>(Quest, BranchClass);

	Node->QuestNode = Branch;

	Quest->AddBranch(Branch);

	return Branch;
}
