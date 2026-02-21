// Copyright Narrative Tools 2022. 

#include "DialogueSM.h"
#include "Dialogue.h"
#include "NarrativeComponent.h"
#include "NarrativeCondition.h"
#include "NarrativeDialogueSettings.h"
#include "NarrativePartyComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "DialogueSM"

UDialogueNode::UDialogueNode()
{

	bIsSkippable = true;

}	

FDialogueLine UDialogueNode::GetRandomLine(const bool bStandalone) const
{
	FDialogueLine NewLine = Line;

	//Construct the line instead of adding it as a member as to not break dialogues made pre 2.2
	if (AlternativeLines.Num())
	{
		TArray<FDialogueLine> AllLines = AlternativeLines;
		AllLines.Add(Line);
		NewLine = AllLines[FMath::RandRange(0, AllLines.Num() - 1)];
	}

	if (NewLine.Duration == ELineDuration::LD_Default)
	{
		if (NewLine.DialogueSound)
		{
			NewLine.Duration = ELineDuration::LD_WhenAudioEnds;
		}
		else if (NewLine.Shot && NewLine.Text.IsEmptyOrWhitespace())
		{
			NewLine.Duration = ELineDuration::LD_WhenSequenceEnds;
		}
		else
		{
			NewLine.Duration = ELineDuration::LD_AfterReadingTime;
		}
	}

	//The server doesn't receieve audio and sequence end events so it needs to use duration
	if (!bStandalone)
	{
		if (NewLine.Duration == ELineDuration::LD_WhenAudioEnds)
		{
			NewLine.Duration = ELineDuration::LD_AfterDuration;
			NewLine.DurationSecondsOverride = NewLine.DialogueSound ? NewLine.DialogueSound->GetDuration() : 0.2f;
		}
		else if (NewLine.Duration == ELineDuration::LD_WhenSequenceEnds)
		{
			UE_LOG(LogNarrative, Warning, TEXT("When Sequence Ends duration isn't supported in networked games. Falling back to audio length. "));
			NewLine.Duration = ELineDuration::LD_AfterDuration;
			NewLine.DurationSecondsOverride = NewLine.DialogueSound ? NewLine.DialogueSound->GetDuration() : 0.2f;
		}
	}

	return NewLine;
}

class UDialogueNode_NPC* UDialogueNode::GetFirstValidNPCReply(APlayerController* OwningController, APawn* OwningPawn, class UNarrativeComponent* NarrativeComponent)
{
	TArray<class UDialogueNode_NPC*> RepliesToCheck = NPCReplies;

	if (const UNarrativeDialogueSettings* DialogueSettings = GetDefault<UNarrativeDialogueSettings>())
	{
		//Sort the replies by their Y position in the graph
		RepliesToCheck.Sort([DialogueSettings](const UDialogueNode_NPC& NodeA, const UDialogueNode_NPC& NodeB) {
			return DialogueSettings->bEnableVerticalWiring ? NodeA.NodePos.X < NodeB.NodePos.X : NodeA.NodePos.Y < NodeB.NodePos.Y;
			});
	}

	for (auto& NPCReply : RepliesToCheck)
	{
		if (NPCReply->AreConditionsMet(OwningPawn, OwningController, NarrativeComponent))
		{
			return NPCReply;
		}
	}

	return nullptr;
}

TArray<class UDialogueNode_Player*> UDialogueNode::GetPlayerReplies(APlayerController* OwningController, APawn* OwningPawn, class UNarrativeComponent* NarrativeComponent)
{
	TArray<class UDialogueNode_Player*> ValidReplies;

	for (auto& PlayerReply : PlayerReplies)
	{
		if (PlayerReply && PlayerReply->AreConditionsMet(OwningPawn, OwningController, NarrativeComponent))
		{
			ValidReplies.Add(PlayerReply);
		}
	}

	if (const UNarrativeDialogueSettings* DialogueSettings = GetDefault<UNarrativeDialogueSettings>())
	{
		//Sort the replies by their Y position in the graph
		ValidReplies.Sort([DialogueSettings](const UDialogueNode_Player& NodeA, const UDialogueNode_Player& NodeB) {
			return DialogueSettings->bEnableVerticalWiring ? NodeA.NodePos.X < NodeB.NodePos.X : NodeA.NodePos.Y < NodeB.NodePos.Y;
			});
	}

	return ValidReplies;
}

UWorld* UDialogueNode::GetWorld() const
{
	return OwningComponent ? OwningComponent->GetWorld() : nullptr;
}

const bool UDialogueNode::IsMissingCues() const
{
	if (!Line.Text.IsEmpty() && !Line.DialogueSound)
	{
		return true;
	}

	if (!AlternativeLines.Num())
	{
		return false;
	}

	for (auto& AltLine : AlternativeLines)
	{
		if (AltLine.Text.IsEmptyOrWhitespace() && !AltLine.DialogueSound)
		{
			return true;
		}
	}

	return false;
}

bool UDialogueNode::IsRoutingNode() const
{
	if (Line.Shot || Line.DialogueSound || Events.Num() || !Line.Text.IsEmptyOrWhitespace())
	{
		return false;
	}

	return true;
}

#if WITH_EDITOR

void UDialogueNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty)
	{ 
		//If we changed the ID, make sure it doesn't conflict with any other IDs in the Dialogue
		if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FDialogueLine, Text))
		{
			FString DialogueAssetName = "";

			if (UDialogue* Dialogue = Cast<UDialogue>(GetOuter()))
			{
				if (UObject* DialogueBP = Cast<UObject>(Dialogue->GetOuter()))
				{
					DialogueAssetName = DialogueBP->GetFName().ToString();
				}
			}

			//Only autogenerate the ID if an ID hasn't been assigned yet 
			if (HasDefaultID())
			{
				GenerateIDFromText();
			}
		}
	}
}

void UDialogueNode::EnsureUniqueID()
{
	if (OwningDialogue)
	{
		TArray<UDialogueNode*> DialogueNodes = OwningDialogue->GetNodes();
		TArray<FName> NodeIDs;

		for (auto& Node : DialogueNodes)
		{
			if (Node && Node != this)
			{
				NodeIDs.Add(Node->ID);
			}
		}

		int32 Suffix = 1;
		FName NewID = ID;

		if (!NodeIDs.Contains(NewID))
		{
			return;
		}

		// Check if the new ID already exists in the array
		while (NodeIDs.Contains(NewID))
		{
			// If it does, add a numeric suffix and try again
			NewID = FName(*FString::Printf(TEXT("%s%d"), *ID.ToString(), Suffix));
			Suffix++;
		}

		ID = NewID;
	}
}

void UDialogueNode::GenerateIDFromText()
{
	//When the text for this node is entered, give the node a sensible ID: {SpeakerID}_{FirstFourWords}
	FString TextString = Line.Text.ToString();

	TArray<FString> ContentArray;
	FString ContentString = "";
	TextString.ParseIntoArrayWS(ContentArray);
	ContentArray.SetNum(4);

	for (auto& Content : ContentArray)
	{
		if (Content.Len() > 0)
		{
			Content[0] = FChar::ToUpper(Content[0]);
			ContentString += Content;
		}
	}

	FString Prefix = "";

	if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(this))
	{
		Prefix = NPCNode->GetSpeakerID().ToString();
	}
	else
	{
		Prefix = "Player";
	}

	//Remove special chars and numeric 
	FString FinalString = "";

	for (TCHAR& Char : ContentString)
	{
		if (FChar::IsAlpha(Char))
		{
			FinalString += Char;
		}
	}

	FString DialogueAssetName = "";

	if (UDialogue* Dialogue = Cast<UDialogue>(GetOuter()))
	{
		if (UObject* DialogueBP = Cast<UObject>(Dialogue->GetOuter()))
		{
			DialogueAssetName += DialogueBP->GetFName().ToString();
			DialogueAssetName += "_";
		}
	}

	SetID(FName(DialogueAssetName + Prefix + '_' + FinalString));
}

bool UDialogueNode::HasDefaultID() const
{
	FString DialogueAssetName = "";

	if (UDialogue* Dialogue = Cast<UDialogue>(GetOuter()))
	{
		if (UObject* DialogueBP = Cast<UObject>(Dialogue->GetOuter()))
		{
			DialogueAssetName = DialogueBP->GetName();
		}
	}

	//Only autogenerate the ID if an ID hasn't been assigned yet 
	return (ID.ToString() == DialogueAssetName + "_" + GetName());
}

#endif


TArray<class UDialogueNode_NPC*> UDialogueNode_NPC::GetReplyChain(APlayerController* OwningController, APawn* OwningPawn, class UNarrativeComponent* NarrativeComponent)
{
	TArray<UDialogueNode_NPC*> NPCFollowUpReplies;
	UDialogueNode_NPC* CurrentNode = this;

	NPCFollowUpReplies.Add(CurrentNode);

	while (CurrentNode)
	{
		if (CurrentNode != this)
		{
			NPCFollowUpReplies.Add(CurrentNode);
		}

		TArray<UDialogueNode_NPC*> NPCRepliesToRet = CurrentNode->NPCReplies;
		if (const UNarrativeDialogueSettings* DialogueSettings = GetDefault<UNarrativeDialogueSettings>())
		{
			//Need to process the conditions using higher/leftmost nodes first 
			NPCRepliesToRet.Sort([DialogueSettings](const UDialogueNode_NPC& NodeA, const UDialogueNode_NPC& NodeB) {
				return DialogueSettings->bEnableVerticalWiring ? NodeA.NodePos.X < NodeB.NodePos.X : NodeA.NodePos.Y < NodeB.NodePos.Y;
				});
		}
		//If we don't find another node after this the loop will exit
		CurrentNode = nullptr;

		//if(UNarrativePartyComponent* PartyComponent = Cast<UNarrativePartyComponent>(NarrativeComponent))
		//{
		//	int32 Idx = 0;
		//	int32 BestReplyIdx = INT_MAX;
		//	for (auto& Reply : NPCRepliesToRet)
		//	{
		//		//If we're a party, find the earliest passing condition across all players
		//		for (auto& PartyMember : PartyComponent->GetPartyMembers())
		//		{
		//			if (Reply != this && Reply->AreConditionsMet(PartyMember->GetOwningPawn(), PartyMember->GetOwningController(), PartyMember))
		//			{
		//				if (Idx < BestReplyIdx)
		//				{
		//					CurrentNode = Reply;
		//					BestReplyIdx = Idx;
		//				}
		//			}
		//		}

		//		++Idx;
		//	}
		//}
		//else
		//{
		//Find the next valid reply. We'll then repeat this cycle until we run out
		for (auto& Reply : NPCRepliesToRet)
		{
			if (Reply != this && Reply->AreConditionsMet(OwningPawn, OwningController, NarrativeComponent))
			{
				CurrentNode = Reply;
				break; // just use the first reply with valid conditions
			}
		}
		//}

	}

	return NPCFollowUpReplies;
}


FText UDialogueNode_Player::GetOptionText(class UDialogue* InDialogue) const
{
	FText TextToUse = OptionText.IsEmptyOrWhitespace() ? Line.Text : OptionText;

	if (InDialogue)
	{
		//No dialogue line yet because we're just replacing a player replies text
		FDialogueLine DummyLine;
		InDialogue->ReplaceStringVariables(this, Line, TextToUse);
	}

	return TextToUse;
}

FText UDialogueNode_Player::GetHintText(class UDialogue* InDialogue) const
{
	if (!HintText.IsEmptyOrWhitespace())
	{
		// Replace string variables on the hint text
		FText newHintText = HintText;
		InDialogue->ReplaceStringVariables(this, Line, newHintText);
		return newHintText;
	}

	FText TextToUse = FText::GetEmpty();
	TArray<FText> Hints;

	for (auto& Event : Events)
	{
		if (Event)
		{
			FText EventHintText = Event->GetHintText();
			if (!EventHintText.IsEmptyOrWhitespace())
			{
				Hints.Add(Event->GetHintText());
			}
		}
	}

	if (Hints.Num())
	{
		TextToUse = TextToUse.Join(LOCTEXT("CommaDelim", ", "), Hints);
	}

	// Replace string variables on the hint text
	InDialogue->ReplaceStringVariables(this, Line, TextToUse);

	return TextToUse;
}

#undef LOCTEXT_NAMESPACE