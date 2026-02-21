// Copyright Narrative Tools 2022. 

#include "DialogueGraph.h"
#include "Dialogue.h"
#include "NarrativeFunctionLibrary.h"
#include "NarrativeComponent.h"
#include "DialogueSM.h"
#include "QuestSM.h"
#include "DialogueBlueprint.h"
#include "DialogueGraphNode.h"
#include "DialogueGraphNode_Root.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "DialogueGraphNode_Player.h"
#include "DialogueEditorSettings.h"

#define LOCTEXT_NAMESPACE "DialogueGraph"

void UDialogueGraph::OnCreated()
{
}

void UDialogueGraph::OnLoaded()
{
}

void UDialogueGraph::Initialize()
{
	//Commented out for now as was occasionally causing crashes
	//FEditorDelegates::PostPIEStarted.AddUObject(this, &UDialogueGraph::PIEStarted);
}

void UDialogueGraph::NotifyGraphChanged(const FEdGraphEditAction& Action)
{
	Super::NotifyGraphChanged(Action);

	if (Action.Action == EEdGraphActionType::GRAPHACTION_RemoveNode)
	{
		if (UDialogue* Dialogue = Cast<UDialogueBlueprint>(GetOuter())->DialogueTemplate)
		{
			for (auto& DeletedNode : Action.Nodes)
			{
				if (UDialogueGraphNode* QuestGraphNode = const_cast<UDialogueGraphNode*>(Cast<UDialogueGraphNode>(DeletedNode)))
				{
					//NPC node was deleted, go through all nodes and remove 
					if (UDialogueGraphNode_NPC* NPCNode = Cast<UDialogueGraphNode_NPC>(QuestGraphNode))
					{
						for (auto& NPCR : Dialogue->NPCReplies)
						{
							if (NPCR)
							{
								NPCR->NPCReplies.Remove(CastChecked<UDialogueNode_NPC>(NPCNode->DialogueNode));
							}
						}

						for (auto& PR : Dialogue->PlayerReplies)
						{
							if (PR)
							{
								PR->NPCReplies.Remove(CastChecked<UDialogueNode_NPC>(NPCNode->DialogueNode));
							}
						}

						Dialogue->NPCReplies.Remove(CastChecked<UDialogueNode_NPC>(NPCNode->DialogueNode));
					}
					else if (UDialogueGraphNode_Player* PlayerNode = Cast<UDialogueGraphNode_Player>(QuestGraphNode))
					{
						for (auto& NPCR : Dialogue->NPCReplies)
						{
							NPCR->PlayerReplies.Remove(CastChecked<UDialogueNode_Player>(PlayerNode->DialogueNode));
						}

						for (auto& PR : Dialogue->PlayerReplies)
						{
							PR->PlayerReplies.Remove(CastChecked<UDialogueNode_Player>(PlayerNode->DialogueNode));
						}

						Dialogue->PlayerReplies.Remove(CastChecked<UDialogueNode_Player>(PlayerNode->DialogueNode));
					}
				}
			}
		}
	}
}

void UDialogueGraph::PinRewired(UDialogueGraphNode* Node, UEdGraphPin* Pin)
{
	if (UDialogue* Dialogue = Cast<UDialogueBlueprint>(GetOuter())->DialogueTemplate)
	{
		//Output pin got wired into something so set it 
		if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			if (UDialogueGraphNode* WiredFrom = Cast<UDialogueGraphNode>(Node))
			{
				if (WiredFrom->DialogueNode)
				{
					//Rebuild replies
					WiredFrom->DialogueNode->NPCReplies.Empty();
					WiredFrom->DialogueNode->PlayerReplies.Empty();

					for (auto& LinkedToPin : Pin->LinkedTo)
					{
						//Figure out what we wired into and update the related array 
						if (UDialogueGraphNode_NPC* WiredToNPC = Cast<UDialogueGraphNode_NPC>(LinkedToPin->GetOwningNode()))
						{
							WiredFrom->DialogueNode->NPCReplies.AddUnique(CastChecked<UDialogueNode_NPC>(WiredToNPC->DialogueNode));
						}
						else if (UDialogueGraphNode_Player* WiredToPlayer = Cast<UDialogueGraphNode_Player>(LinkedToPin->GetOwningNode()))
						{
							WiredFrom->DialogueNode->PlayerReplies.AddUnique(CastChecked<UDialogueNode_Player>(WiredToPlayer->DialogueNode));
						}
					}
				}
			}
		}
	}
}

void UDialogueGraph::NodeAdded(UEdGraphNode* AddedNode)
{
	if (UDialogueBlueprint* DialogueAsset = Cast<UDialogueBlueprint>(GetOuter()))
	{
		check(DialogueAsset->DialogueTemplate);

		if (UDialogueGraphNode_Root* RootGraphNode = Cast<UDialogueGraphNode_Root>(AddedNode))
		{	
			//Add text to the root node
			UDialogue* DialogueCDO = Cast<UDialogue>(DialogueAsset->GeneratedClass->GetDefaultObject());

			if (!DialogueAsset->DialogueTemplate->RootDialogue)
			{
				UDialogueNode_NPC* RootNode = MakeNPCReply(RootGraphNode, DialogueAsset->DialogueTemplate);
			
				//if (DialogueCDO->Speakers.IsValidIndex(0))
				//{
				//	RootNode->Line.Text = FText::Format(LOCTEXT("DefaultRootNodeText", "Hi there, i'm {0}."), FText::FromString(DialogueCDO->Speakers[0].SpeakerID.ToString()));
				//	RootNode->SpeakerID = DialogueCDO->Speakers[0].SpeakerID;
				//}

				DialogueAsset->DialogueTemplate->RootDialogue = RootNode;
			}
			return;
		}
	}

	UDialogueBlueprint* DialogueAsset = CastChecked<UDialogueBlueprint>(GetOuter());
	UDialogue* Dialogue = DialogueAsset->DialogueTemplate;

	ensure(Dialogue);

	if (UDialogueGraphNode_NPC* NPCNode = Cast<UDialogueGraphNode_NPC>(AddedNode))
	{
		if (!NPCNode->DialogueNode)
		{
			NPCNode->DialogueNode = MakeNPCReply(NPCNode, Dialogue);
		}
		else
		{
			NPCNode->DialogueNode->Rename(nullptr, Dialogue);
		}
	}
	else if (UDialogueGraphNode_Player* PlayerNode = Cast<UDialogueGraphNode_Player>(AddedNode))
	{
		if (!PlayerNode->DialogueNode)
		{
			PlayerNode->DialogueNode = MakePlayerReply(PlayerNode, Dialogue);
		}
		else
		{
			PlayerNode->DialogueNode->Rename(nullptr, Dialogue);
		}
	}
}

UDialogueNode_NPC* UDialogueGraph::MakeNPCReply(class UDialogueGraphNode_NPC* Node, class UDialogue* Dialogue)
{
	if (UDialogueBlueprint* DialogueAsset = Cast<UDialogueBlueprint>(GetOuter()))
	{
		FSoftClassPath NPCReplyClassPath = GetDefault<UDialogueEditorSettings>()->DefaultNPCDialogueClass;
		UClass* NPCReplyClass = (NPCReplyClassPath.IsValid() ? LoadObject<UClass>(NULL, *NPCReplyClassPath.ToString()) : UDialogueNode_NPC::StaticClass());

		if (NPCReplyClass == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("Unable to load Dialogue Class '%s'. Falling back to generic UDialogueNode_NPC."), *NPCReplyClassPath.ToString());
			NPCReplyClass = UDialogueNode_NPC::StaticClass();
		}

		UDialogueNode_NPC* NPCReply = NewObject<UDialogueNode_NPC>(Dialogue, NPCReplyClass);
		NPCReply->OwningDialogue = Dialogue;
		Node->DialogueNode = NPCReply;
		Dialogue->NPCReplies.AddUnique(NPCReply);

		NPCReply->SetID(*(DialogueAsset->GetName() + "_" + NPCReply->GetName()));

		return NPCReply;
	}

	return nullptr;
}

UDialogueNode_Player* UDialogueGraph::MakePlayerReply(class UDialogueGraphNode_Player* Node, class UDialogue* Dialogue)
{
	if (UDialogueBlueprint* DialogueAsset = Cast<UDialogueBlueprint>(GetOuter()))
	{
		FSoftClassPath PlayerReplyClassPath = GetDefault<UDialogueEditorSettings>()->DefaultPlayerDialogueClass;
		UClass* PlayerReplyClass = (PlayerReplyClassPath.IsValid() ? LoadObject<UClass>(NULL, *PlayerReplyClassPath.ToString()) : UDialogueNode_Player::StaticClass());

		if (PlayerReplyClass == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("Unable to load Dialogue Class '%s'. Falling back to generic UDialogueNode_Player."), *PlayerReplyClassPath.ToString());
			PlayerReplyClass = UDialogueNode_Player::StaticClass();
		}

		UDialogueNode_Player* PlayerReply = NewObject<UDialogueNode_Player>(Dialogue, PlayerReplyClass);
		Node->DialogueNode = PlayerReply;
		PlayerReply->OwningDialogue = Dialogue;
		Dialogue->PlayerReplies.AddUnique(PlayerReply);

		PlayerReply->SetID(*(DialogueAsset->GetName() + "_" + PlayerReply->GetName()));

		return PlayerReply;
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE 