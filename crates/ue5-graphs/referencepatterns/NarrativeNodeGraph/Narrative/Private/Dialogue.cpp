// Copyright Narrative Tools 2022. 

#include "Dialogue.h"
#include "NarrativeDialogueSettings.h"
#include "NarrativeComponent.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "DialogueSM.h"
#include "NarrativeDialogueSequence.h"
#include "NarrativePartyComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include <GameFramework/PlayerState.h>
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "Camera/CameraShakeBase.h"
#include "Sound/SoundBase.h"
#include <EngineUtils.h>


static const FName NAME_PlayerSpeakerID("Player");
static const FName NAME_FaceTag("Face");
static const FName NAME_BodyTag("Body");
static const FString NAME_PlayDialogueNodeTask("PlayDialogueNode");

UDialogue::UDialogue()
{

	bAutoRotateSpeakers = false;
	bAutoStopMovement = true;
	bCanBeExited = true;
	bFreeMovement = false;
	bDeinitialized = false;
	bBeganPlaying = false;
	DefaultHeadBoneName = FName("head");
	DialogueBlendOutTime = 0.f;
	Priority = 0;
	PlayerAutoAdjustTransform = FTransform(FRotator(0.f, 180.f, 0.f), FVector(200.f, 0.f, 0.f));

	//Not sure if this is needed but really want to avoid RF_Public assert issues people were having with UE5
	SetFlags(RF_Public);
}

UWorld* UDialogue::GetWorld() const
{

	if (OwningComp)
	{
		return OwningComp->GetWorld();
	}

	return nullptr;
}

bool UDialogue::Initialize(class UNarrativeComponent* InitializingComp, const FDialoguePlayParams InPlayParams)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		//We need a valid narrative component to make a quest for 
		if (!InitializingComp)
		{
			return false;
		}

		if (UDialogueBlueprintGeneratedClass* BGClass = Cast<UDialogueBlueprintGeneratedClass>(GetClass()))
		{
			BGClass->InitializeDialogue(this);

			//If a dialogue doesn't have any npc replies, or doesn't have a valid root dialogue something has gone wrong 
			if (NPCReplies.Num() == 0 || !RootDialogue)
			{
				UE_LOG(LogNarrative, Warning, TEXT("UDialogue::Initialize was given a dialogue with an invalid RootDialogue, or no NPC replies."));
				return false;
			}


			UDialogueNode_NPC* StartDialogue = InPlayParams.StartFromID.IsNone() ? RootDialogue : GetNPCReplyByID(InPlayParams.StartFromID);

			if (!StartDialogue && !InPlayParams.StartFromID.IsNone())
			{
				//UE_LOG(LogNarrative, Warning, TEXT("UDialogue::Initialize could not find Start node with StartFromID: %s. Falling back to root node."), *StartFromID.ToString());
				return false;
				//StartDialogue = RootDialogue;
			}

			//Initialize all the data required to begin the dialogue 
			if (StartDialogue)
			{
				PlayParams = InPlayParams;
				
				OwningComp = InitializingComp;
				OwningController = OwningComp->GetOwningController();
				OwningPawn = OwningComp->GetOwningPawn();

				if (RootDialogue)
				{
					RootDialogue->OwningDialogue = this;
				}

				for (auto& Reply : NPCReplies)
				{
					if (Reply)
					{
						Reply->OwningDialogue = this;
						Reply->OwningComponent = OwningComp;
					}
				}

				for (auto& Reply : PlayerReplies)
				{
					if (Reply)
					{
						Reply->OwningDialogue = this;
						Reply->OwningComponent = OwningComp;
					}
				}

				//Generate the first chunk of dialogue 
				if (OwningComp->HasAuthority())
				{
					const bool bHasValidDialogue = GenerateDialogueChunk(StartDialogue);

					if (!bHasValidDialogue)
					{
						return false;
					}
				}

				return true;
			}
		}
	}

	return false;
}

void UDialogue::Deinitialize()
{
	if (bDeinitialized)
	{
		return;
	}

	bDeinitialized = true;

	OnEndDialogue();

	StopDialogueSequence();

	if (DialogueAudio)
	{
		DialogueAudio->Stop();
		DialogueAudio->DestroyComponent();
	}

	OwningComp = nullptr; 
	DefaultDialogueShot = nullptr;

	NPCReplies.Empty();
	PlayerReplies.Empty();

	if (RootDialogue)
	{
		RootDialogue->OwningComponent = nullptr;
	}

	for (auto& Reply : NPCReplies)
	{
		if (Reply)
		{
			Reply->SelectingReplyShot = nullptr;
			Reply->Line.Shot = nullptr;
			Reply->OwningComponent = nullptr;
		}
	}

	for (auto& Reply : PlayerReplies)
	{
		if (Reply)
		{
			Reply->Line.Shot = nullptr;
			Reply->OwningComponent = nullptr;
		}
	}
}

void UDialogue::DuplicateAndInitializeFromDialogue(UDialogue* DialogueTemplate)
{
	if (DialogueTemplate)
	{
		//Duplicate the quest template, then steal all its states and branches - TODO this seems unreliable, what if we add new fields to UDialogue? Look into swapping object entirely instead of stealing fields
		UDialogue* NewDialogue = Cast<UDialogue>(StaticDuplicateObject(DialogueTemplate, this, NAME_None, RF_Transactional));
		NewDialogue->SetFlags(RF_Transient | RF_DuplicateTransient);

		RootDialogue = NewDialogue->RootDialogue;
		NPCReplies = NewDialogue->NPCReplies;
		PlayerReplies = NewDialogue->PlayerReplies;
	}
}

#if WITH_EDITOR
void UDialogue::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{	
	Super::PostEditChangeProperty(PropertyChangedEvent);

	
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UDialogue, Speakers))
	{
		if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd || PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate)
		{
			if (const UNarrativeDialogueSettings* DialogueSettings = GetDefault<UNarrativeDialogueSettings>())
			{
				if (DialogueSettings->SpeakerColors.IsValidIndex(Speakers.Num() - 1))
				{
					if (Speakers.Num())
					{
						Speakers[Speakers.Num() - 1].NodeColor = DialogueSettings->SpeakerColors[Speakers.Num() - 1];
					}
				}
			}
		}
	}

	for (auto& Speaker : Speakers)
	{
		PlayerSpeakerInfo.SpeakerID = NAME_PlayerSpeakerID;
	}

	//If any NPC replies don't have a valid speaker set to the first speaker TODO this is broken - figure out why 
	//for (auto& Node : NPCReplies)
	//{
	//	if (Node)
	//	{
	//		bool bSpeakerNotFound = true;

	//		for (auto& Speaker : Speakers)
	//		{
	//			if (Speaker.SpeakerID == Node->SpeakerID)
	//			{
	//				bSpeakerNotFound = false;
	//			}
	//		}

	//		if (bSpeakerNotFound)
	//		{
	//			Node->SpeakerID = Speakers[0].SpeakerID;
	//		}
	//	}
	//}
}

void UDialogue::PreEditChange(FEditPropertyChain& PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);


}

#endif

FSpeakerInfo UDialogue::GetSpeaker(const FName& SpeakerID)
{
	for (auto& Speaker : Speakers)
	{
		if (Speaker.GetSpeakerID() == SpeakerID)
		{
			return Speaker;
		}
	}

	//Nodes created before the release of Speakers won't have their speaker set. Therefore, just return the first speaker.
	if (Speakers.Num() && Speakers.IsValidIndex(0))
	{
		return Speakers[0];
	}

	return FSpeakerInfo();
}

bool UDialogue::SkipCurrentLine()
{
	if (CanSkipCurrentLine() && OwningComp && OwningComp->HasAuthority())
	{
		bool bSkippingPlayerReply = CurrentNode && CurrentNode->IsA<UDialogueNode_Player>();

		/*If we're skipping a player reply, you can have more than 1 and so skipping the servers player reply will finish the chunk. It will send a new chunk through so no need to do anything.
		*
		If we skipped an NPC reply, we need to tell the client to also skip their NPC reply. However, because of ping etc the clients reply
		may not match the servers, so to ensure sync we'll just send the servers remaining chunk to the client to play. */
		if (!bSkippingPlayerReply)
		{
			if (UNarrativePartyComponent* PartyComp = Cast<UNarrativePartyComponent>(OwningComp))
			{
				for (auto& PartyMember : PartyComp->GetPartyMembers())
				{
					if (PartyMember)
					{
						PartyMember->ClientRecieveDialogueChunk(MakeIDsFromNPCNodes(NPCReplyChain), MakeIDsFromPlayerNodes(AvailableResponses));
					}
				}
			}
			else
			{
				//RPC the dialogue chunk to the client so it can play it
				OwningComp->ClientRecieveDialogueChunk(MakeIDsFromNPCNodes(NPCReplyChain), MakeIDsFromPlayerNodes(AvailableResponses));
			}
		}


		EndCurrentLine();

		return true;
	}

	return false;
}

bool UDialogue::CanSkipCurrentLine() const
{
	if (OwningComp)
	{
		if (CurrentNode && CurrentNode->bIsSkippable)
		{
			return true;
		}
	}

	return false;
}

void UDialogue::EndCurrentLine()
{
	if (CurrentNode)
	{
		//Unbind all listeners for line ending, they need to be reset up when the next line plays 
		if (DialogueAudio)
		{
			DialogueAudio->OnAudioFinished.RemoveAll(this);
		}

		if (DialogueSequencePlayer)
		{
			if (ULevelSequencePlayer* SP = DialogueSequencePlayer->GetSequencePlayer())
			{
				SP->OnFinished.RemoveAll(this);
			}
		}

		if (CurrentNode->IsA<UDialogueNode_NPC>())
		{
			if (GetWorld())
			{
				GetWorld()->GetTimerManager().ClearTimer(TimerHandle_NPCReplyFinished);
			}

			FinishNPCDialogue();
		}
		else
		{
			if (GetWorld())
			{
				GetWorld()->GetTimerManager().ClearTimer(TimerHandle_PlayerReplyFinished);
			}
			FinishPlayerDialogue();
		}
	}
}

bool UDialogue::CanSelectDialogueOption(UDialogueNode_Player* PlayerNode) const
{
	return IsValid(PlayerNode) && AvailableResponses.Contains(PlayerNode);
}

bool UDialogue::SelectDialogueOption(UDialogueNode_Player* Option)
{
	//Validate that the option that was selected is actually one of the available options
	if (CanSelectDialogueOption(Option))// GenerateDialogueChunk() already did this && Option->AreConditionsMet(OwningPawn, OwningController, OwningComp))
	{
		PlayPlayerDialogueNode(Option);

		if (OwningComp)
		{
			OwningComp->OnDialogueOptionSelected.Broadcast(this, Option);
		}

		return true;
	}

	return false;
}

bool UDialogue::HasValidChunk() const
{
	//Validate that the reply chain isn't just full of empty routing nodes and actually has some content to play
	bool bReplyChainHasContent = false;

	for (auto& Reply : NPCReplyChain)
	{
		if (Reply && !Reply->IsRoutingNode())
		{
			bReplyChainHasContent = true;
			break;
		}
	}

	//Chunk is valid if there is something for the player to say, or something for the NPC to say that isn't empty (empty nodes can be used for routing)
	//At this point we have a valid chunk and Play() can be called on this dialogue to play it
	if (AvailableResponses.Num() || bReplyChainHasContent)
	{
		return true;
	}

	return false;
}

bool UDialogue::GenerateDialogueChunk(UDialogueNode_NPC* NPCNode)
{
	if (NPCNode && OwningComp && OwningComp->HasAuthority())
	{	
		//Generate the NPC reply chain
		NPCReplyChain = NPCNode->GetReplyChain(OwningController, OwningPawn, OwningComp);

		//Grab all the players responses to the last thing the NPC had to say
		if (NPCReplyChain.Num() && NPCReplyChain.IsValidIndex(NPCReplyChain.Num() - 1))
		{
			if (UDialogueNode_NPC* LastNPCNode = NPCReplyChain.Last())
			{
				AvailableResponses = LastNPCNode->GetPlayerReplies(OwningController, OwningPawn, OwningComp);
			}
		}

		//Did we generate a valid chunk?
		if (HasValidChunk())
		{
			return true;
		}
	}

	return false;
}

void UDialogue::ClientReceiveDialogueChunk(const TArray<FName>& NPCReplyIDs, const TArray<FName>& PlayerReplyIDs)
{	
	if (OwningComp && !OwningComp->HasAuthority())
	{
		
		/**TODO definitely look at cleaning this up when we refactor 
		We want to end the current line before playing the new chunk, but we can't call EndCurrentLine since it tries skipping to the next line, 
		and even calls EndDialogue if we run out of lines, so here we're just doing most of what EndCurrentLine does manually, only without skipping to the next line. */
		if (CurrentNode)
		{
			if (DialogueAudio)
			{
				DialogueAudio->OnAudioFinished.RemoveAll(this);
			}

			if (DialogueSequencePlayer)
			{
				if (ULevelSequencePlayer* SP = DialogueSequencePlayer->GetSequencePlayer())
				{
					SP->OnFinished.RemoveAll(this);
				}
			}

			if (GetWorld())
			{
				GetWorld()->GetTimerManager().ClearTimer(TimerHandle_NPCReplyFinished);
				GetWorld()->GetTimerManager().ClearTimer(TimerHandle_PlayerReplyFinished);
			}

			FinishDialogueNode(CurrentNode, CurrentLine, CurrentSpeaker, CurrentSpeakerAvatar, CurrentListenerAvatar);
		}

		//Resolve the nodes the server sent us, and play them
		NPCReplyChain = GetNPCRepliesByIDs(NPCReplyIDs);
		AvailableResponses = GetPlayerRepliesByIDs(PlayerReplyIDs);

		UE_LOG(LogTemp, Warning, TEXT("Client received a dialogue chunk: NPCReplyChain: %d, AvailableResponses: %d"), NPCReplyChain.Num(), AvailableResponses.Num());

		Play();
	}
}

void UDialogue::Play()
{
	if (!bBeganPlaying)
	{
		OnBeginDialogue();
		bBeganPlaying = true;
	}

	//Start playing through the NPCs replies until we run out
	if (NPCReplyChain.Num())
	{
		PlayNextNPCReply();
	}
	else
	{
		NPCFinishedTalking();
	}
}

void UDialogue::ExitDialogue()
{
	if (OwningComp)
	{
		OwningComp->ExitDialogue();
	}
}


void UDialogue::TickDialogue_Implementation(const float DeltaTime)
{
	if (CurrentDialogueSequence)
	{
		CurrentDialogueSequence->Tick(DeltaTime);
	}
}

void UDialogue::OnBeginDialogue()
{
	K2_OnBeginDialogue();

	InitSpeakerAvatars();

	if (bAdjustPlayerTransform)
	{
		AdjustPlayerTransform();
	}

	OldViewTarget = OwningController ? OwningController->GetViewTarget() : nullptr;

	/*It doesnt really make sense for the conversation participants to not face each other, so do this automagically (can be turned off) */
	if (OwningPawn)
	{
		if (bAutoStopMovement)
		{
			if (const ACharacter* PlayerChar = Cast<ACharacter>(OwningPawn))
			{
				if (PlayerChar->GetCharacterMovement())
				{
					PlayerChar->GetCharacterMovement()->StopMovementImmediately();
				}
			}
		}
	}

	if (OwningController && OwningController->IsLocalPlayerController())
	{
		if (DialogueCameraShake)
		{
			OwningController->ClientStartCameraShake(DialogueCameraShake);
		}
	}
}

void UDialogue::InitSpeakerAvatars()
{
	//TODO - rewrite the linking process, this could use a massive cleanup
	
	//Spawn any speaker avatars in - just spawn these locally, never seems like we'd want the server
	//to spawn replicated speaker actors for something that is just a visual for a dialogue 
	for (auto& Speaker : Speakers)
	{
		if (AActor* SpeakerActor = LinkSpeakerAvatar(Speaker))
		{
			if (IsValid(SpeakerActor))
			{			
				//Track spawned avatars
				SpeakerAvatars.Add(Speaker.GetSpeakerID(), SpeakerActor);
				Speaker.SpeakerAvatarTransform = SpeakerActor->GetActorTransform();
			}
		}
	}

	//Spawn each party members avatar in
	//if (UNarrativePartyComponent* OwningParty = Cast<UNarrativePartyComponent>(OwningComp))
	//{
	//	TArray<APlayerState*> PartyMembers = OwningParty->GetPartyMemberStates();

	//	for (int32 i = 0; i < PartyMembers.Num(); ++i)
	//	{
	//		if (!PartySpeakerInfo.IsValidIndex(i))
	//		{
	//			FPlayerSpeakerInfo NewSpeaker;
	//			//NewSpeaker.SpeakerID = FName(FString::Printf(TEXT("PartyMember%d"), i));
	//			PartySpeakerInfo.Add(NewSpeaker);
	//		}

	//		if (PartySpeakerInfo.IsValidIndex(i) && PartyMembers.IsValidIndex(i))
	//		{
	//			FPlayerSpeakerInfo& MemberSpeakerInfo = PartySpeakerInfo[i];

	//			if (APlayerState* PartyMember = PartyMembers[i])
	//			{
	//				AActor* SpeakerActor = LinkSpeakerAvatar(MemberSpeakerInfo);

	//				//Fallback to speaker actors pawn if can't link
	//				if (!SpeakerActor)
	//				{
	//					SpeakerActor = PartyMember->GetPawn();
	//				}

	//				if (SpeakerActor)
	//				{
	//					/*There has to be a nicer way to construct an FName from a int but I sure couldnt find it!
	//					Instead of caching speaker avatars via ID, for parties we use the players playerID which is unique.
	//					This gives us a nice convenient way to map someones PlayerState to their Players avatar */
	//					const FName Name_PID = FName(FString::Printf(TEXT("%d"), PartyMember->GetPlayerId()));

	//					MemberSpeakerInfo.SpeakerID = Name_PID;

	//					SpeakerAvatars.Add(MemberSpeakerInfo.SpeakerID, SpeakerActor);

	//					//Hide the party members pawn; we've spawned them an avatar 
	//					if (APawn* PawnOwner = PartyMember->GetPawn())
	//					{
	//						//Store our local pawn in the playerspeakerinfo and the existing systems will just treat it like our solo player 
	//						if (PawnOwner->IsLocallyControlled())
	//						{
	//							SpeakerAvatars.Add(PlayerSpeakerInfo.SpeakerID, SpeakerActor);
	//						}

	//						//If we're using a speaker avatar for this player, we want to hide their pawn
	//						if (SpeakerActor != PartyMember->GetPawn())
	//						{
	//							PawnOwner->SetActorHiddenInGame(true);
	//						}
	//					}
	//				}
	//			}
	//		}
	//	}
	//}
	//else //spawn solo players avatar in 
	//{
	// 
	// 
	// 
	
	//Spawn the players speaker avatar in, or just use the players pawn as their avatar if one isn't set
	if (AActor* SpeakerActor = LinkSpeakerAvatar(PlayerSpeakerInfo))
	{
		SpeakerAvatars.Add(PlayerSpeakerInfo.GetSpeakerID(), SpeakerActor);
		PlayerSpeakerInfo.SpeakerAvatarTransform = SpeakerActor->GetActorTransform();

		//By default if the player has a speaker avatar in the world we'll hide their pawn
		if (OwningPawn && SpeakerActor != OwningPawn)
		{
			OwningPawn->SetActorHiddenInGame(true);
		}
	}
	else if (!OwningPawn)
	{
		UE_LOG(LogNarrative, Warning, TEXT("Narrative wasn't able to find the avatar for the player, as a SpeakerAvatarClass wasn't set, no actors with tag 'Player' were found, and OwningPawn was invalid."));
	}

	//}
}



void UDialogue::CleanUpSpeakerAvatars()
{
	//Clean up any spawned actors
	for (auto& Speaker : Speakers)
	{
		if (AActor* SpeakerAvatar = GetAvatar(Speaker.GetSpeakerID()))
		{
			DestroySpeakerAvatar(Speaker, SpeakerAvatar);
		}
	}

	for (auto& PartySpeaker : PartySpeakerInfo)
	{
		if (AActor* SpeakerAvatar = GetAvatar(PartySpeaker.GetSpeakerID()))
		{
			DestroySpeakerAvatar(PartySpeaker, SpeakerAvatar);
		}
	}

	if (AActor* PlayerAvatar = GetPlayerAvatar())
	{
		DestroySpeakerAvatar(PlayerSpeakerInfo, PlayerAvatar);
	}

	SpeakerAvatars.Empty();
}

void UDialogue::BlendingOutFinished()
{
	if (OwningController && OwningPawn)
	{
		OwningController->EnableInput(OwningController);
		OwningPawn->EnableInput(OwningController);
	}
}

void UDialogue::UpdateSpeakerRotations()
{
	for (auto& SpeakerActorKVP : SpeakerAvatars)
	{
		if (AActor* Avatar = SpeakerActorKVP.Value)
		{
			//If we're not the speaker, face the speaker
			if (Avatar != CurrentSpeakerAvatar && CurrentSpeakerAvatar)
			{
				FVector Offset = (Avatar->GetActorLocation() - CurrentSpeakerAvatar->GetActorLocation()).GetSafeNormal();
				//Generally characters need to be kept upright, so just effect yaw
				Offset.Z = 0.f;
				Avatar->SetActorRotation((-Offset).Rotation());
			}
			else if(Avatar != CurrentListenerAvatar && CurrentListenerAvatar)//If we are the speaker, face the listener 
			{
				FVector Offset = (Avatar->GetActorLocation() - CurrentListenerAvatar->GetActorLocation()).GetSafeNormal();
				Offset.Z = 0.f;
				Avatar->SetActorRotation((-Offset).Rotation());
			}
		}
	}

}

void UDialogue::ProcessNodeEvents(class UDialogueNode* Node, bool bStartEvents)
{
	if (Node)
	{
		struct SOnPlayedStruct
		{
			UDialogueNode* Node;
			bool bStarted;
		};

		SOnPlayedStruct Parms;
		Parms.Node = Node;
		Parms.bStarted = bStartEvents;

		if (UFunction* Func = FindFunction(Node->OnPlayNodeFuncName))
		{
			ProcessEvent(Func, &Parms);
		}

		Node->ProcessEvents(OwningPawn, OwningController, OwningComp, bStartEvents ? EEventRuntime::Start : EEventRuntime::End);
	}
}


bool UDialogue::IsPartyDialogue() const
{
	return OwningComp && OwningComp->IsPartyComponent();
}

void UDialogue::SetPartyCurrentSpeaker(class APlayerState* Speaker)
{
	CurrentPartySpeakerAvatar = Speaker;
}

FVector UDialogue::GetConversationCenterPoint() const
{
	TArray<AActor*> SpeakerActors;

	//Figure out where the middle of the conversation is 
	for (auto& Speaker : SpeakerAvatars)
	{
		if (Speaker.Value)
		{
			SpeakerActors.Add(Speaker.Value);
		}
	}

	FVector LocationSum(0, 0, 0); // sum of locations
	int32 ActorCount = 0; // num actors
	// iterate over actors
	for (int32 ActorIdx = 0; ActorIdx < SpeakerActors.Num(); ActorIdx++)
	{
		AActor* A = SpeakerActors[ActorIdx];
		// Check actor is non-null, not deleted, and has a root component
		if (IsValid(A) && A->GetRootComponent())
		{
			LocationSum += A->GetActorLocation();
			ActorCount++;
		}
	}

	// Find average
	FVector Average(0, 0, 0);
	if (ActorCount > 0)
	{
		Average = LocationSum / ((float)ActorCount);
	}

	return Average;
}

void UDialogue::OnEndDialogue()
{
	K2_OnEndDialogue();

	SetPartyCurrentSpeaker(nullptr);

	if (OwningController && DialogueBlendOutTime > 0.f)
	{
		AActor* ViewTargetToUse = OldViewTarget ? OldViewTarget : OwningController->GetPawn();

		if(ViewTargetToUse && GetWorld())
		{
			OwningController->SetViewTargetWithBlend(ViewTargetToUse, DialogueBlendOutTime, VTBlend_EaseInOut, 2.f, true);
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UDialogue::BlendingOutFinished, DialogueBlendOutTime);
		}
	}

	if (DialogueSequencePlayer)
	{
		if (ULevelSequencePlayer* SP = DialogueSequencePlayer->GetSequencePlayer())
		{
			SP->Stop();
		}

		DialogueSequencePlayer->Destroy();
	}

	if (OwningPawn)
	{
		OwningPawn->SetActorHiddenInGame(false);
	}

	//Reshow any party members that may have been hidden 
	if (UNarrativePartyComponent* Party = Cast<UNarrativePartyComponent>(OwningComp))
	{
		for (APlayerState* PartyMember : Party->GetPartyMemberStates())
		{
			PartyMember->SetActorHiddenInGame(false);
		}
	}

	CleanUpSpeakerAvatars();

	if (DialogueCameraShake && OwningController)
	{
		OwningController->ClientStopCameraShake(DialogueCameraShake);
	}
}

void UDialogue::NPCFinishedTalking()
{
	//Ensure that a narrative event etc hasn't started a new dialogue
	if (IsInitialized() && AvailableResponses.Num() && OwningComp && OwningComp->CurrentDialogue == this)
	{
		//Auto-selects should be handled by the server - clients never need to ask server to auto-select, since it will anyway
		if (OwningComp->HasAuthority())
		{
			bool bWantsAutoSelect = bFreeMovement;

			if (const UNarrativeDialogueSettings* DialogueSettings = GetDefault<UNarrativeDialogueSettings>())
			{
				if (DialogueSettings->bAutoSelectSingleResponse && AvailableResponses.Num() == 1)
				{
					bWantsAutoSelect = true; 
				}
			}


			//If a response is autoselect, select it and early out 
			for (auto& AvailableResponse : AvailableResponses)
			{
				if (AvailableResponse && (AvailableResponse->IsAutoSelect() || bWantsAutoSelect))
				{
					OwningComp->TrySelectDialogueOption(AvailableResponse);
					return;
				}
			}
		}

		SetPartyCurrentSpeaker(nullptr);

		AActor* PlayerAvatar = GetPlayerAvatar();
		AActor* ListeningActor = nullptr;

		//We want all the players to look at us since we need to talk. Set CurrentSpeakerAvatar temporarily to fool UpdateSpeakerRotations() into pointing everyone at us. 
		AActor* OldCurrentSpeaker = CurrentSpeakerAvatar;
		CurrentSpeakerAvatar = PlayerAvatar;

		if (bAutoRotateSpeakers)
		{
			UpdateSpeakerRotations();
		}

		CurrentSpeakerAvatar = OldCurrentSpeaker;

		/**
		* Things like Over The Shoulder shots require a listener and a speaker to line up the shot, but since we're selecting a reply, the 
		* selecting reply shot will use the last NPC that spoke as the listener 
		*/
		if (SpeakerAvatars.Contains(CurrentSpeaker.GetSpeakerID()))
		{
			ListeningActor = SpeakerAvatars[CurrentSpeaker.GetSpeakerID()];
		}

		/*
		* Order of precendence for what camera shot to choose :
		*
		* 1. Last NPC node that played has a select reply sequence defined 
		* 2. Player speaker info has a sequence defined
		* 3. Player speaker info has a shot defined
		* 4. Dialogue has any shots added to its DialogueShots
		*/
		UDialogueNode_NPC* LastNode = Cast<UDialogueNode_NPC>(CurrentNode);

		if (LastNode && LastNode->SelectingReplyShot)
		{
			PlayDialogueSequence(LastNode->SelectingReplyShot, PlayerAvatar, ListeningActor);
		}
		else if (PlayerSpeakerInfo.SelectingReplyShot)
		{
			PlayDialogueSequence(PlayerSpeakerInfo.SelectingReplyShot, PlayerAvatar, ListeningActor);
		}
		else if (DefaultDialogueShot)
		{
			PlayDialogueSequence(DefaultDialogueShot, PlayerAvatar, ListeningActor);
		}

		//NPC has finished talking. Let UI know it can show the player replies. Party comps don't need to broadcast this, clients put their own ones up
		OwningComp->OnDialogueRepliesAvailable.Broadcast(this, AvailableResponses);

		//Also make sure we stop playing any dialogue audio that was previously playing
		if (DialogueAudio)
		{
			DialogueAudio->Stop();
			DialogueAudio->DestroyComponent();
		}
	}
	else
	{
		//There were no replies for the player, end the dialogue 
		ExitDialogue();
	}
}

void UDialogue::PlayNPCDialogueNode(class UDialogueNode_NPC* NPCReply)
{
	check(OwningComp && NPCReply);

	//FString RoleString = OwningComp && OwningComp->HasAuthority() ? "Server" : "Client";
	//UE_LOG(LogNarrative, Warning, TEXT("PlayNPCDialogueNode called on %s with node %s"), *RoleString, *GetNameSafe(NPCReply));

	if (NPCReply)
	{
		CurrentNode = NPCReply;
		CurrentLine = NPCReply->GetRandomLine(OwningComp->GetNetMode() == NM_Standalone);
		ReplaceStringVariables(NPCReply, CurrentLine, CurrentLine.Text);

		CurrentSpeaker = GetSpeaker(NPCReply->GetSpeakerID());

		ProcessNodeEvents(NPCReply, true);

		//ProcessNodeEvents can result in a call to deinit, nulling out owning comp. Check if this occured
		if (!OwningComp)
		{
			return;
		}

		//If a node has no text, just finish it, firing its events 
		if (NPCReply->IsRoutingNode())
		{
			FinishNPCDialogue();
			return;
		}

		//Actual playing of the node is inside a BlueprintNativeEvent so designers can override how NPC dialogues are played 
		PlayNPCDialogue(NPCReply, CurrentLine, CurrentSpeaker);

		if (OwningComp)
		{
			//Call delegates and BPNativeEvents
			OwningComp->OnNPCDialogueLineStarted.Broadcast(this, NPCReply, CurrentLine, CurrentSpeaker);
		}

		OnNPCDialogueLineStarted(NPCReply, CurrentLine, CurrentSpeaker);

		const float Duration = GetLineDuration(CurrentNode, CurrentLine);

		if (!FMath::IsNearlyEqual(Duration, -1.f))
		{
			if (Duration > 0.01f && GetWorld())
			{
				GetWorld()->GetTimerManager().ClearTimer(TimerHandle_NPCReplyFinished);
				//Give the reply time to play, then play the next one! 
				GetWorld()->GetTimerManager().SetTimer(TimerHandle_NPCReplyFinished, this, &UDialogue::FinishNPCDialogue, Duration, false);
			}
			else
			{
				FinishNPCDialogue();
			}
		}
	}
	else 
	{
		//Somehow we were given a null NPC reply to play, just try play the next one
		PlayNextNPCReply();
	}
}

void UDialogue::PlayPlayerDialogueNode(class UDialogueNode_Player* PlayerReply)
{
	//NPC replies should be fully gone before we play a player response
	check(!NPCReplyChain.Num());
	check(OwningComp && PlayerReply);

	if (OwningComp && PlayerReply)
	{
		//Player started talking, clear responses 
		AvailableResponses.Empty();

		CurrentNode = PlayerReply;
		
		ProcessNodeEvents(PlayerReply, true);

		//ProcessNodeEvents can result in a call to deinit, nulling out owning comp. Check if this occured
		if (!OwningComp)
		{
			return;
		}

		//If a node has no text, just process events then go to the next line 
		if (PlayerReply->IsRoutingNode())
		{
			FinishPlayerDialogue();
			return;
		}

		CurrentLine = PlayerReply->GetRandomLine(OwningComp->GetNetMode() == NM_Standalone);
		ReplaceStringVariables(PlayerReply, CurrentLine, CurrentLine.Text);

		//Call delegates and BPNativeEvents
		OwningComp->OnPlayerDialogueLineStarted.Broadcast(this, PlayerReply, CurrentLine);

		OnPlayerDialogueLineStarted(PlayerReply, CurrentLine);

		//Actual playing of the node is inside a BlueprintNativeEvent so designers can override how NPC dialogues are played 
		PlayPlayerDialogue(PlayerReply, CurrentLine);

		CurrentSpeaker = PlayerSpeakerInfo;

		const float Duration = GetLineDuration(CurrentNode, CurrentLine);

		if (!FMath::IsNearlyEqual(Duration, -1.f))
		{
			if (Duration > 0.01f && GetWorld())
			{
				//Give the reply time to play, then play the next one! 
				GetWorld()->GetTimerManager().SetTimer(TimerHandle_PlayerReplyFinished, this, &UDialogue::FinishPlayerDialogue, Duration, false);
			}
			else
			{
				FinishPlayerDialogue();
			}
		}

	}
}

void UDialogue::ReplaceStringVariables(const class UDialogueNode* Node, const FDialogueLine& Line, FText& OutLine)
{
	//Replace variables in dialogue line
	FString LineString = OutLine.ToString();

	int32 OpenBraceIdx = -1;
	int32 CloseBraceIdx = -1;
	bool bFoundOpenBrace = LineString.FindChar('{', OpenBraceIdx);
	bool bFoundCloseBrace = LineString.FindChar('}', CloseBraceIdx);
	uint32 Iters = 0; // More than 50 wildcard replaces and something has probably gone wrong, so safeguard against that

	while (bFoundOpenBrace && bFoundCloseBrace && OpenBraceIdx < CloseBraceIdx && Iters < 50)
	{
		const FString VariableName = LineString.Mid(OpenBraceIdx + 1, CloseBraceIdx - OpenBraceIdx - 1);
		const FString VariableVal = GetStringVariable(Node, Line, VariableName);

		if (!VariableVal.IsEmpty())
		{
			LineString.RemoveAt(OpenBraceIdx, CloseBraceIdx - OpenBraceIdx + 1);
			LineString.InsertAt(OpenBraceIdx, VariableVal);
		}

		bFoundOpenBrace = LineString.FindChar('{', OpenBraceIdx);
		bFoundCloseBrace = LineString.FindChar('}', CloseBraceIdx);

		Iters++;
	}

	if (Iters > 0)
	{
		OutLine = FText::FromString(LineString);
	}
}

AActor* UDialogue::GetPlayerAvatar() const
{
	if (CurrentPartySpeakerAvatar)
	{
		if (UNarrativePartyComponent* Party = Cast<UNarrativePartyComponent>(OwningComp))
		{
			TArray<APlayerState*> PartyMembers = Party->GetPartyMemberStates();

			int32 StateIdx;
			if (PartyMembers.Find(CurrentPartySpeakerAvatar, StateIdx))
			{
				const FName PID = FName(FString::FromInt(PartyMembers[StateIdx]->GetPlayerId()));
				if (SpeakerAvatars.Contains(PID))
				{
					//There doesnt seem to be an elegant way to do int->fname
					return SpeakerAvatars[PID];
				}
			}
		}
	}

	if (SpeakerAvatars.Contains(PlayerSpeakerInfo.GetSpeakerID()))
	{
		return SpeakerAvatars[PlayerSpeakerInfo.GetSpeakerID()];
	}
	else
	{
		return OwningPawn;
	}
}

AActor* UDialogue::GetAvatar(const FName& SpeakerID) const
{
	if (SpeakerAvatars.Contains(SpeakerID))
	{
		return SpeakerAvatars[SpeakerID];
	}

	return nullptr;
}

FVector UDialogue::GetSpeakerHeadLocation_Implementation(class AActor* Actor) const
{
	if (!Actor)
	{
		return FVector::ZeroVector;
	}
	
	FVector EyesLoc;
	FRotator EyesRot;
	Actor->GetActorEyesViewPoint(EyesLoc, EyesRot);

	const bool bIsChar = Actor->IsA<ACharacter>();

	for (auto& ActorSkelMesh : Actor->GetComponents())
	{	
		if (USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(ActorSkelMesh))
		{
			//Dont use face mesh, body meshes seem more consistent in testing
			if (SkelMesh->ComponentHasTag(NAME_BodyTag))
			{
				EyesLoc = SkelMesh->GetBoneLocation(DefaultHeadBoneName);
				//EyesLoc.Z = SkelMesh->GetBoneLocation(DefaultHeadBoneName).Z;
				break;
			}
		}
	}

	return EyesLoc;
}

void UDialogue::PlayNextNPCReply()
{
	//Keep going through the NPC replies until we run out
	if (NPCReplyChain.IsValidIndex(0) && IsInitialized())
	{
		UDialogueNode_NPC* NPCNode = NPCReplyChain[0];
		NPCReplyChain.Remove(NPCNode);
		PlayNPCDialogueNode(NPCNode);
	}
	else //NPC has nothing left to say 
	{
		NPCFinishedTalking();
	}
}

void UDialogue::FinishNPCDialogue()
{
	if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(CurrentNode))
	{
		FinishDialogueNode(NPCNode, CurrentLine, CurrentSpeaker, CurrentSpeakerAvatar, CurrentListenerAvatar);

		if (OwningComp)
		{
			if (OwningComp->HasAuthority())
			{
				OwningComp->CompleteNarrativeDataTask(NAME_PlayDialogueNodeTask, NPCNode->GetID().ToString());
			}

			ProcessNodeEvents(NPCNode, false);

			//We need to re-check OwningComp validity, as ProcessEvents may have ended this dialogue
			if (OwningComp)
			{
				//Call delegates and BPNativeEvents
				OwningComp->OnNPCDialogueLineFinished.Broadcast(this, NPCNode, CurrentLine, CurrentSpeaker);
				OnNPCDialogueLineFinished(NPCNode, CurrentLine, CurrentSpeaker);

				PlayNextNPCReply();
			}
		}
	}
}

void UDialogue::FinishPlayerDialogue()
{
	//FString RoleString = OwningComp && OwningComp->HasAuthority() ? "Server" : "Client";
	//UE_LOG(LogNarrative, Warning, TEXT("FinishPlayerDialogue called on %s with node %s"), *RoleString, *GetNameSafe(CurrentNode));
	//Players dialogue node has finished, generate the next chunk of dialogue! 
	if (UDialogueNode_Player* PlayerNode = Cast<UDialogueNode_Player>(CurrentNode))
	{
		if (!OwningComp || !PlayerNode)
		{
			UE_LOG(LogNarrative, Verbose, TEXT("UDialogue::PlayerDialogueNodeFinished was called but had a null OwningComp or PlayerNode. "));
			return;
		}

		FinishDialogueNode(PlayerNode, CurrentLine, CurrentSpeaker, CurrentSpeakerAvatar, CurrentListenerAvatar);

		//Call delegates and BPNativeEvents
		OwningComp->OnPlayerDialogueLineFinished.Broadcast(this, PlayerNode, CurrentLine);
		OnPlayerDialogueLineFinished(PlayerNode, CurrentLine);

		//No need, generate dialogue chunk already did this: if (PlayerNode->AreConditionsMet(OwningPawn, OwningController, OwningComp))
		{
			//Both auth and local need to run the events
			ProcessNodeEvents(PlayerNode, false);

			if (OwningComp && OwningComp->HasAuthority())
			{
				OwningComp->CompleteNarrativeDataTask(NAME_PlayDialogueNodeTask, PlayerNode->GetID().ToString());

				//Player selected a reply with nothing leading off it, dialogue has ended 
				if (PlayerNode->NPCReplies.Num() <= 0)
				{
					ExitDialogue();
					return;
				}

				//Find the first valid NPC reply after the option we selected
				UDialogueNode_NPC* NextReply = PlayerNode->GetFirstValidNPCReply(OwningController, OwningPawn, OwningComp);

				//If we can generate more dialogue from the reply that was selected, do so, otherwise exit dialogue 
				if (GenerateDialogueChunk(NextReply))
				{
					//If we're Party, inform all party members a new chunk has arrived to play
					if (UNarrativePartyComponent* PartyComp = Cast<UNarrativePartyComponent>(OwningComp))//OwningComp->IsPartyComponent())
					{
						for (auto& PartyMember : PartyComp->GetPartyMembers())
						{
							if (PartyMember)
							{
								PartyMember->ClientRecieveDialogueChunk(MakeIDsFromNPCNodes(NPCReplyChain), MakeIDsFromPlayerNodes(AvailableResponses));
							}
						}
					}
					else
					{
						//RPC the dialogue chunk to the client so it can play it
						OwningComp->ClientRecieveDialogueChunk(MakeIDsFromNPCNodes(NPCReplyChain), MakeIDsFromPlayerNodes(AvailableResponses));
					}

					Play();
				}
				else
				{
					UE_LOG(LogNarrative, Warning, TEXT("No more chunks generated from response. Ending dialogue! "));
					ExitDialogue();
				}
			}
		}

	}
}

UDialogueNode_NPC* UDialogue::GetNPCReplyByID(const FName& ID) const
{
	for (auto& NPCReply : NPCReplies)
	{
		if (NPCReply->GetID() == ID)
		{
			return NPCReply;
		}
	}
	return nullptr;
}

UDialogueNode_Player* UDialogue::GetPlayerReplyByID(const FName& ID) const
{
	for (auto& PlayerReply : PlayerReplies)
	{
		if (PlayerReply->GetID() == ID)
		{
			return PlayerReply;
		}
	}
	return nullptr;
}

TArray<UDialogueNode_NPC*> UDialogue::GetNPCRepliesByIDs(const TArray<FName>& IDs) const
{
	TArray<UDialogueNode_NPC*> Replies;

	for (auto& ID : IDs)
	{
		for (auto& Reply : NPCReplies)
		{
			if (Reply && Reply->GetID() == ID)
			{
				Replies.Add(Reply);
				break;
			}
		}
	}

	return Replies;
}

TArray <UDialogueNode_Player*> UDialogue::GetPlayerRepliesByIDs(const TArray<FName>& IDs) const
{
	TArray<UDialogueNode_Player*> Replies;

	for (auto& ID : IDs)
	{
		for (auto& Reply : PlayerReplies)
		{
			if (Reply && Reply->GetID() == ID)
			{
				Replies.Add(Reply);
				break;
			}
		}
	}

	return Replies;
}

TArray<FName> UDialogue::MakeIDsFromNPCNodes(const TArray<UDialogueNode_NPC*> Nodes) const
{
	TArray<FName> IDs;

	for (auto& Node : Nodes)
	{
		IDs.Add(Node->GetID());
	}

	return IDs;
}

TArray<FName> UDialogue::MakeIDsFromPlayerNodes(const TArray<UDialogueNode_Player*> Nodes) const
{
	TArray<FName> IDs;

	for (auto& Node : Nodes)
	{
		IDs.Add(Node->GetID());
	}

	return IDs;
}

TArray<UDialogueNode*> UDialogue::GetNodes() const
{
	TArray<UDialogueNode*> Ret;

	for (auto& NPCReply : NPCReplies)
	{
		Ret.Add(NPCReply);
	}

	for (auto& PlayerReply : PlayerReplies)
	{
		Ret.Add(PlayerReply);
	}

	return Ret;
}

AActor* UDialogue::LinkSpeakerAvatar_Implementation(const FSpeakerInfo& Info)
{
	//Default to using the OwningPawn, or DefaultNPCAvatar, unless something else can be found... 
	AActor* SpawnedActor = Info.GetSpeakerID() == PlayerSpeakerInfo.GetSpeakerID() ? OwningPawn : nullptr;

	if (!Info.GetSpeakerID().IsNone())
	{
		if (!SpeakerAvatars.Contains(Info.GetSpeakerID()) && IsValid(Info.SpeakerAvatarClass))
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.bNoFail = true;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParams.Owner = OwningController;

			if (UWorld* World = GetWorld())
			{
				SpawnedActor = World->SpawnActor(Info.SpeakerAvatarClass, &Info.SpeakerAvatarTransform, SpawnParams);
			}
		}
		else
		{
			//If the user doesn't want narrative to spawn their dialogue avatar, search the world an actor tagged with the Speakers ID
			TArray<AActor*> FoundActors;

			for (FActorIterator It(GetWorld()); It; ++It)
			{
				AActor* Actor = *It;

				if (Actor && Actor->ActorHasTag(Info.GetSpeakerID()))
				{
					FoundActors.Add(Actor);
				}
			}

			if (FoundActors.Num() > 1 && OwningPawn)
			{
				UE_LOG(LogNarrative, Warning, TEXT("UDialogue::LinkSpeakerAvatar_Implementation tried find an actor tagged with the speakers ID but found multiple. Reverting to closest..."));

				float Dist;

				SpawnedActor = UGameplayStatics::FindNearestActor(OwningPawn->GetActorLocation(), FoundActors, Dist);
			}
			else if (FoundActors.IsValidIndex(0))
			{
				SpawnedActor = FoundActors[0];
			}
		}
	}

	return SpawnedActor;
}

void UDialogue::DestroySpeakerAvatar_Implementation(const FSpeakerInfo& Info, AActor* const SpeakerAvatar)
{
	//In general, the NPCActor and OwningPawn shouldn't be cleaned up automatically, since these weren't spawned in for the dialogue - they were already around
	//Also, don't remove any speaker avatars unless narrative spawned them. If they were already in the world they shouldn't get deleted 
	if (SpeakerAvatar != OwningPawn && IsValid(Info.SpeakerAvatarClass))
	{
		SpeakerAvatar->Destroy();
	}
}

void UDialogue::PlayDialogueAnimation_Implementation(class UDialogueNode* Node, const FDialogueLine& Line, class AActor* Speaker, class AActor* Listener)
{
	AActor* ActorToUse = Speaker;

	if (ActorToUse)
	{ 
		///Play a facial anim if one is set 
		if(Line.FacialAnimation)
		{
			TArray<UActorComponent*> FaceMeshes = ActorToUse->GetComponentsByTag(USkeletalMeshComponent::StaticClass(), NAME_FaceTag);
			if (FaceMeshes.Num())
			{
				for (UActorComponent* FaceAC : FaceMeshes)
				{
					if (USkeletalMeshComponent* FaceMesh = Cast<USkeletalMeshComponent>(FaceAC))
					{
						if (FaceMesh->GetAnimInstance())
						{
							FaceMesh->GetAnimInstance()->Montage_Play(Line.FacialAnimation);
						}
					}
				}
			}
			else
			{
				UE_LOG(LogNarrative, Warning, TEXT("Narrative tried playing your face anim on avatar %s but couldn't find skeletal mesh tagged with 'Face' please tag the correct mesh with 'Face'"), *GetNameSafe(Speaker));
			}
		}

		if (Line.DialogueMontage)
		{
			TArray<UActorComponent*> BodyMeshes = ActorToUse->GetComponentsByTag(USkeletalMeshComponent::StaticClass(), NAME_BodyTag);

			if (BodyMeshes.Num())
			{
				for (UActorComponent* BodyAC : BodyMeshes)
				{
					if (USkeletalMeshComponent* BodyMesh = Cast<USkeletalMeshComponent>(BodyAC))
					{
						if (BodyMesh->GetAnimInstance())
						{
							BodyMesh->GetAnimInstance()->Montage_Play(Line.DialogueMontage);
						}
					}
				}
			}
			else
			{
				UE_LOG(LogNarrative, Warning, TEXT("Narrative tried playing your body anim on avatar %s but couldn't find skeletal mesh tagged with 'Body' please tag your body mesh with tag 'Body'"), *GetNameSafe(Speaker));
			}
		}
	}
}

void UDialogue::StopDialogueAnimation_Implementation()
{

	if (CurrentSpeakerAvatar)
	{
		if (CurrentLine.FacialAnimation)
		{
			TArray<UActorComponent*> FaceMeshes = CurrentSpeakerAvatar->GetComponentsByTag(USkeletalMeshComponent::StaticClass(), NAME_FaceTag);
			if (FaceMeshes.Num())
			{
				for (UActorComponent* FaceAC : FaceMeshes)
				{
					if (USkeletalMeshComponent* FaceMesh = Cast<USkeletalMeshComponent>(FaceAC))
					{
						if (FaceMesh->GetAnimInstance())
						{
							const float BlendOutTime = CurrentLine.FacialAnimation->BlendOut.GetBlendTime();
							FaceMesh->GetAnimInstance()->Montage_Stop(BlendOutTime, CurrentLine.FacialAnimation);
						}
					}
				}
			}
			else
			{
				UE_LOG(LogNarrative, Warning, TEXT("Narrative tried ending your face anim on avatar %s but couldn't find skeletal mesh tagged with 'Face' please tag the correct mesh with 'Face'"), *GetNameSafe(CurrentSpeakerAvatar));
			}
		}

		if (CurrentLine.DialogueMontage)
		{
			TArray<UActorComponent*> BodyMeshes = CurrentSpeakerAvatar->GetComponentsByTag(USkeletalMeshComponent::StaticClass(), NAME_BodyTag);

			if (BodyMeshes.Num())
			{
				for (UActorComponent* BodyAC : BodyMeshes)
				{
					if (USkeletalMeshComponent* BodyMesh = Cast<USkeletalMeshComponent>(BodyAC))
					{
						if (BodyMesh->GetAnimInstance())
						{
							const float BlendOutTime = CurrentLine.DialogueMontage->BlendOut.GetBlendTime();
							BodyMesh->GetAnimInstance()->Montage_Stop(BlendOutTime, CurrentLine.DialogueMontage);
						}
					}
				}
			}
			else
			{
				UE_LOG(LogNarrative, Warning, TEXT("Narrative tried ending your body anim on avatar %s but couldn't find skeletal mesh tagged with 'Body' please tag the correct mesh with 'Body'"), *GetNameSafe(CurrentSpeakerAvatar));
			}
		}
	}
}

void UDialogue::PlayDialogueSound_Implementation(const FDialogueLine& Line, class AActor* Speaker, class AActor* Listener)
{
	//Stop the existing audio regardless of whether the new line has audio
	if (DialogueAudio)
	{
		//Remove any previously added bindings
		DialogueAudio->OnAudioFinished.RemoveAll(this);
		DialogueAudio->Stop();
		DialogueAudio->DestroyComponent();
	}

	if (Line.DialogueSound)
	{
		if (Speaker)
		{
			//DialogueAudio = UGameplayStatics::SpawnSoundAtLocation(OwningComp, Line.DialogueSound, Speaker->GetActorLocation(), Speaker->GetActorForwardVector().Rotation(), 1.f, 1.f, 0.f, DialogueSoundAttenuation);
			DialogueAudio = UGameplayStatics::SpawnSoundAttached(Line.DialogueSound, Speaker->GetRootComponent(), NAME_None, FVector::ZeroVector, EAttachLocation::SnapToTarget, false, 1.f, 1.f, 0.f, DialogueSoundAttenuation);
		}
		else //Else just play 2D audio 
		{
			DialogueAudio = UGameplayStatics::SpawnSound2D(OwningComp, Line.DialogueSound);
		}

		if (DialogueAudio && Line.Duration == ELineDuration::LD_WhenAudioEnds)
		{
			DialogueAudio->OnAudioFinished.AddDynamic(this, &UDialogue::EndCurrentLine);
		}
	}
}

void UDialogue::PlayDialogueNode_Implementation(class UDialogueNode* Node, const FDialogueLine& Line, const FSpeakerInfo& Speaker, class AActor* SpeakerActor, class AActor* ListenerActor)
{
	if (Node)
	{
		CurrentSpeakerAvatar = SpeakerActor;
		CurrentListenerAvatar = ListenerActor;

		//If autorotate speakers is enabled, make all the speakers face the person currently talking
		if (bAutoRotateSpeakers)
		{
			UpdateSpeakerRotations();

			// Speaker may have just been facing someone, we need to put him back
			//SpeakerActor->SetActorTransform(Speaker.SpeakerAvatarTransform);
		}

		//
		if (OwningComp)
		{
			PlayDialogueSound(Line, SpeakerActor, ListenerActor);
			PlayDialogueAnimation(Node, Line, SpeakerActor, ListenerActor);

			/*Order of precedence for camera shot :
			* Dialogue line has a sequence set
			* Dialogue line has a shot set
			* speaker has a sequence set
			* speaker has a shot(s) set
			* Dialogue has dialogue shots added
			* If none are set, stop any currently running shots
			* */
			if (Line.Shot)
			{
				PlayDialogueSequence(Line.Shot, SpeakerActor, ListenerActor);
			}
			else if (Speaker.DefaultSpeakerShot)
			{
				PlayDialogueSequence(Speaker.DefaultSpeakerShot, SpeakerActor, ListenerActor);
			}
			else if (DefaultDialogueShot)
			{
				PlayDialogueSequence(DefaultDialogueShot, SpeakerActor, ListenerActor);
			}
			else // If there is no shot to play, we should end any previously playing one 
			{
				StopDialogueSequence();
			}
		}
	}
}

void UDialogue::FinishDialogueNode_Implementation(class UDialogueNode* Node, const FDialogueLine& Line, const FSpeakerInfo& Speaker, class AActor* SpeakerActor, class AActor* ListenerActor)
{
	if (DialogueAudio)
	{
		DialogueAudio->Stop();
	}

	StopDialogueAnimation();

	SetPartyCurrentSpeaker(nullptr);

}

void UDialogue::PlayNPCDialogue_Implementation(class UDialogueNode_NPC* NPCReply, const FDialogueLine& LineToPlay, const FSpeakerInfo& SpeakerInfo)
{
	//Figure out what actor is saying this line
	AActor* SpeakingActor = nullptr;
	AActor* ListeningActor = GetAvatar(NPCReply->DirectedAtSpeakerID); // For now, all NPC lines are considered to be spoken at the player (should probably refactor)

	//If the shot isn't directed at someone, direct the line at the player by default
	if (!ListeningActor)
	{
		ListeningActor = GetPlayerAvatar();
	}

	if (SpeakerAvatars.Contains(SpeakerInfo.GetSpeakerID()))
	{
		SpeakingActor = *SpeakerAvatars.Find(SpeakerInfo.GetSpeakerID());
	}

	PlayDialogueNode(NPCReply, LineToPlay, SpeakerInfo, SpeakingActor, ListeningActor);
}

void UDialogue::PlayPlayerDialogue_Implementation(class UDialogueNode_Player* PlayerReply, const FDialogueLine& Line)
{
	//Figure out what actor is saying this line
	AActor* const SpeakingActor = GetPlayerAvatar();
	AActor* ListeningActor = GetAvatar(PlayerReply->DirectedAtSpeakerID);

	if (!ListeningActor)
	{
		ListeningActor = GetAvatar(CurrentSpeaker.GetSpeakerID());
	}

	PlayDialogueNode(PlayerReply, Line, PlayerSpeakerInfo, SpeakingActor, ListeningActor);
}

float UDialogue::GetLineDuration_Implementation(class UDialogueNode* Node, const FDialogueLine& Line)
{
	//Duration will only still be default if we didn't 
	if (Line.Duration == ELineDuration::LD_AfterReadingTime)
	{
		/*
		* By default, we wait until the Dialogue Audio is finished, or if no audio is supplied
		* we wait at a rate of 25 letters per second (configurable in .ini) to give the reader time to finish reading the dialogue line.
		*/
		float LettersPerSecondLineDuration = 25.f;
		float MinDialogueTextDisplayTime = 2.f;

		if (const UNarrativeDialogueSettings* DialogueSettings = GetDefault<UNarrativeDialogueSettings>())
		{
			LettersPerSecondLineDuration = DialogueSettings->LettersPerSecondLineDuration;
			MinDialogueTextDisplayTime = DialogueSettings->MinDialogueTextDisplayTime;
		}

		// if the lines text is empty, return a duration of 0
		if (Line.Text.IsEmptyOrWhitespace())
		{
			return 0.f;
		}

		return FMath::Max(Line.Text.ToString().Len() / LettersPerSecondLineDuration, MinDialogueTextDisplayTime);
	}
	else if (Line.Duration == ELineDuration::LD_AfterDuration)
	{
		return Line.DurationSecondsOverride;
	}

	//All other line durations don't use a fixed time, they wait for a Audio/Sequence delegates to fire, so just return -1
	return -1.f;
}

FString UDialogue::GetStringVariable_Implementation(const class UDialogueNode* Node, const FDialogueLine& Line, const FString& VariableName)
{
	return VariableName;
}

void UDialogue::OnNPCDialogueLineStarted_Implementation(class UDialogueNode_NPC* Node, const FDialogueLine& DialogueLine, const FSpeakerInfo& Speaker)
{

}

void UDialogue::OnNPCDialogueLineFinished_Implementation(class UDialogueNode_NPC* Node, const FDialogueLine& DialogueLine, const FSpeakerInfo& Speaker)
{

}

void UDialogue::OnPlayerDialogueLineStarted_Implementation(class UDialogueNode_Player* Node, const FDialogueLine& DialogueLine)
{

}

void UDialogue::OnPlayerDialogueLineFinished_Implementation(class UDialogueNode_Player* Node, const FDialogueLine& DialogueLine)
{

}

void UDialogue::AdjustPlayerTransform_Implementation()
{
	//Only adjust for 1 on 1 dialogue 
	if (Speakers.Num() == 1 && Speakers.IsValidIndex(0))
	{
		if (OwningPawn && OwningController)
		{
			const FTransform PlayerDesiredTransform = PlayerAutoAdjustTransform * Speakers[0].SpeakerAvatarTransform;

			OwningPawn->TeleportTo(PlayerDesiredTransform.GetLocation(), PlayerDesiredTransform.GetRotation().Rotator());
			OwningController->SetControlRotation(PlayerDesiredTransform.GetRotation().Rotator());
		}
	}
}

void UDialogue::PlayDialogueSequence(class UNarrativeDialogueSequence* Sequence, class AActor* Speaker, class AActor* Listener)
{
	if (Sequence && Sequence->GetSequenceAssets().Num() && OwningController && OwningController->IsLocalPlayerController())
	{
		//Playing a sequence will re-show our character if hidden so just cache that and immediately set back to override ue5 functionality
		AActor* PAvatar = GetPlayerAvatar();
		const bool bAvatarHidden = PAvatar ? PAvatar->IsHidden() : false;

		//Narrative needs to initialize its cutscene player 
		if (!DialogueSequencePlayer)
		{
			ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(), Sequence->GetSequenceAssets().Last(), Sequence->GetPlaybackSettings(), DialogueSequencePlayer);
			//passing CreateLevelSequencePlayer null asset makes it fail so we need to set it afterwards
			if (DialogueSequencePlayer)
			{
				DialogueSequencePlayer->SetSequence(nullptr);
			}
		}

		if (DialogueSequencePlayer)
		{
			if (ULevelSequencePlayer* SP = DialogueSequencePlayer->GetSequencePlayer())
			{
				SP->OnFinished.RemoveAll(this);

				if (CurrentDialogueSequence)
				{
					CurrentDialogueSequence->EndSequence();
				}

				Sequence->BeginPlaySequence(DialogueSequencePlayer, this, Speaker, Listener);

				CurrentDialogueSequence = Sequence;

				if (CurrentLine.Duration == ELineDuration::LD_WhenSequenceEnds)
				{
					SP->OnFinished.AddDynamic(this, &UDialogue::EndCurrentLine);
				}
			}
		}

		if (PAvatar)
		{
			PAvatar->SetActorHiddenInGame(bAvatarHidden);
		}

	}
}
void UDialogue::StopDialogueSequence()
{
	if (OwningController && OwningController->IsLocalPlayerController())
	{
		if (DialogueSequencePlayer)
		{
			if (ULevelSequencePlayer* SP = DialogueSequencePlayer->GetSequencePlayer())
			{
				if (CurrentDialogueSequence)
				{
					CurrentDialogueSequence->EndSequence();
				}

				SP->Stop();

				/*
				If your pawn has a dialogue avatar, narrative hides your pawn as you wouldn't want it to show up in a dialogue.
				However a UE5 bug - Stop() will re-show player pawn even if it was already hidden - we want to keep it hidden*/
				if (OwningPawn && GetPlayerAvatar() != OwningPawn)
				{
					OwningPawn->SetActorHiddenInGame(true);
				}
			}
		}
	}
}
