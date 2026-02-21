// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/TextProperty.h" //Fixes a build error complaining about incomplete type UTextProperty
#include "Components/ActorComponent.h"
#include "Quest.h"
#include "QuestSM.h"
#include "DialogueSM.h"
#include "Dialogue.h"
#include "NarrativeSaveGame.h"
#include "NarrativeComponent.generated.h"

class UDialogueBlueprint;

class UNarrativeDataTask;
class UQuestState;
class UQuestGraphNode;

USTRUCT(BlueprintType)
struct FDialogueInfo
{
	GENERATED_BODY()

	FDialogueInfo()
	{
		Dialogue = nullptr;
		NPC = nullptr;
	};

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	class UDialogue* Dialogue = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	class AActor* NPC = nullptr;

};

UENUM()
enum class EUpdateType : uint8
{
	UT_None, 
	UT_CompleteTask,
	UT_BeginQuest,
	UT_ForgetQuest,
	UT_RestartQuest,
	UT_QuestNewState,
	UT_TaskProgressMade
};

/**
UQuests aren't replicated the proper UE way (replicated flag on objects), because this is really complex and hasn't been very successful in testing.
It would also hog a lot more network performance, as quests often have hundreds of states, tasks, and branches,
all of which would be sending network updates and replicating. 

So in the meantime, the server authoritatively Begins, Forgets, Restarts, Updates quests etc, and then informs the client about
these changes via the FNarrativeUpdate stream so the clients can "replay" these actions in the order they happened and keep sync with the server. 

Using this mechanism instead of RPCs ensures the updates are sent in the correct order. This is really important
for ensuring the client correctly stays in sync with the server. 
*/
USTRUCT()
struct FNarrativeUpdate
{

	GENERATED_BODY()

public:

	FNarrativeUpdate()
	{
		bAcked = false;
		UpdateType = EUpdateType::UT_None;
		QuestClass = UQuest::StaticClass();
	}

	virtual ~FNarrativeUpdate() {} //dynamic_cast needs RTTI 

	//What sort of update this is
	UPROPERTY(VisibleAnywhere, Category = "Debug")
	EUpdateType UpdateType;

	//The quest that was updated 
	UPROPERTY(VisibleAnywhere, Category = "Debug")
	TSubclassOf<class UQuest> QuestClass;

	//Optional payload with some string data about the update
	UPROPERTY(VisibleAnywhere, Category = "Debug")
	FString Payload;

	//Some updates need to send some ints over the network, for example TaskProgressMade uses task idx and new progress 
	UPROPERTY()
	TArray<uint8> IntPayload;

	bool bAcked; //flag client uses to prevent processing update multiple times 
	float CreationTime; // Timestamp server created update at

	//Tell the client a quest has a new state and we need to go to that state - not called for QuestStartState, BeginQuest update handles that 
	static FNarrativeUpdate QuestNewState(const TSubclassOf<class UQuest>& QuestClass, const FName& NewStateID)
	{
		FNarrativeUpdate Update;
		Update.UpdateType = EUpdateType::UT_QuestNewState;
		Update.QuestClass = QuestClass;
		Update.Payload = NewStateID.ToString();
		return Update;
	}

	//Tell a client to complete an Task 
	static FNarrativeUpdate CompleteTask(const TSubclassOf<class UQuest>& QuestClass, const FString& RawTask, const int32 Quantity)
	{
		FNarrativeUpdate Update;
		Update.UpdateType = EUpdateType::UT_CompleteTask;
		Update.QuestClass = QuestClass;
		Update.IntPayload.Add(Quantity);
		Update.Payload = RawTask;
		return Update;
	};

	//Tell a client to begin one of its quests
	static FNarrativeUpdate BeginQuest(const TSubclassOf<class UQuest>& QuestClass, const FName& StartFromID = NAME_None)
	{
		FNarrativeUpdate Update;
		Update.UpdateType = EUpdateType::UT_BeginQuest;
		Update.QuestClass = QuestClass;
		Update.Payload = StartFromID.ToString();
		return Update;
	};

	//Tell a client to restart one of its quests 
	static FNarrativeUpdate RestartQuest(const TSubclassOf<class UQuest>& QuestClass, const FName& StartFromID = NAME_None)
	{
		FNarrativeUpdate Update;
		Update.UpdateType = EUpdateType::UT_RestartQuest;
		Update.QuestClass = QuestClass;
		Update.Payload = StartFromID.ToString();
		return Update;
	};

	//Tell a client to forget one of its quests. 
	static FNarrativeUpdate ForgetQuest(const TSubclassOf<class UQuest>& QuestClass)
	{
		FNarrativeUpdate Update;
		Update.UpdateType = EUpdateType::UT_ForgetQuest;
		Update.QuestClass = QuestClass;
		return Update;
	};

	static FNarrativeUpdate TaskProgressMade(const TSubclassOf<class UQuest>& QuestClass, uint8 UpdatedTaskIdx, uint8 NewProgress, const FName& BranchID)
	{
		FNarrativeUpdate Update;

		Update.UpdateType = EUpdateType::UT_TaskProgressMade;
		Update.QuestClass = QuestClass;
		Update.Payload = BranchID.ToString();

		Update.IntPayload.Add(UpdatedTaskIdx);
		Update.IntPayload.Add(NewProgress);

		return Update;
	}

};

DECLARE_LOG_CATEGORY_EXTERN(LogNarrative, Log, All);

//Narrative makes a point to expose everything via delegates so your game can update your UI, or do whatever it needs to do when an update happens. 

//General
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNarrativeTaskCompleted, const UNarrativeDataTask*, NarrativeTask, const FString&, Name);

//Dialogue
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueBegan, class UDialogue*, Dialogue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueFinished, class UDialogue*, Dialogue, const bool, bStartingNewDialogue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDialogueOptionSelected, class UDialogue*, Dialogue, class UDialogueNode_Player*, PlayerReply);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDialogueRepliesAvailable, class UDialogue*, Dialogue, const TArray<UDialogueNode_Player*>&, PlayerReplies);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FNPCDialogueLineStarted, class UDialogue*, Dialogue, class UDialogueNode_NPC*, Node, const FDialogueLine&, DialogueLine, const FSpeakerInfo&, Speaker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FNPCDialogueLineFinished, class UDialogue*, Dialogue, class UDialogueNode_NPC*, Node, const FDialogueLine&, DialogueLine, const FSpeakerInfo&, Speaker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerDialogueLineStarted, class UDialogue*, Dialogue, class UDialogueNode_Player*, Node, const FDialogueLine&, DialogueLine);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerDialogueLineFinished, class UDialogue*, Dialogue, class UDialogueNode_Player*, Node, const FDialogueLine&, DialogueLine);

//Save/Load functionality
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeginSave, FString, SaveGameName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveComplete, FString, SaveGameName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeginLoad, FString, SaveGameName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadComplete, FString, SaveGameName);

//Parties
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnJoinedParty, class UNarrativePartyComponent*, NewParty, class UNarrativePartyComponent*, LeftParty);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeaveParty, class UNarrativePartyComponent*, LeftParty);

/**
* Add this component to your Player Controller. 
* Narrative Component acts as the connection to the Narrative system and allows you to start quests, dialogues, complete Tasks, etc.
*/
UCLASS( ClassGroup=(Narrative), DisplayName = "Narrative Component", meta=(BlueprintSpawnableComponent) )
class NARRATIVE_API UNarrativeComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

	friend class UQuest;
	friend class UNarrativePartyComponent;

	// Sets default values for this component's properties
	UNarrativeComponent();

	bool HasAuthority() const;
	bool IsPartyComponent() const;

protected:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:

	/**Called when a narrative data task is completed by the player */
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnNarrativeTaskCompleted OnNarrativeDataTaskCompleted;

	/**Called when a quest objective has been completed.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestBranchCompleted OnQuestBranchCompleted;

	/**Called when a quest objective is updated and we've received a new objective*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestNewState OnQuestNewState;

	/**Called when a quest task in a quest step has made progress. ie 6 out of 10 wolves killed*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestTaskProgressChanged OnQuestTaskProgressChanged;

	/**Called when a quest task in a step is completed*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestTaskCompleted OnQuestTaskCompleted;

	/**Called when a quest is completed.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestSucceeded OnQuestSucceeded;

	/**Called when a quest is failed.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestFailed OnQuestFailed;

	/**Called when a quest is started.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestStarted OnQuestStarted;

	/**Called when a quest is forgotten.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestForgotten OnQuestForgotten;

	/**Called when a quest is restarted.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestRestarted OnQuestRestarted;

	/**Called when a save has begun*/
	UPROPERTY(BlueprintAssignable, Category = "Saving/Loading")
	FOnBeginSave OnBeginSave;

	/**Called when a save has completed*/
	UPROPERTY(BlueprintAssignable, Category = "Saving/Loading")
	FOnSaveComplete OnSaveComplete;

	/**Called when a load has begun*/
	UPROPERTY(BlueprintAssignable, Category = "Saving/Loading")
	FOnBeginLoad OnBeginLoad;

	/**Called when a load has completed*/
	UPROPERTY(BlueprintAssignable, Category = "Saving/Loading")
	FOnLoadComplete OnLoadComplete;

	/**Called when we've joined a party*/
	UPROPERTY(BlueprintAssignable, Category = "Saving/Loading")
	FOnJoinedParty OnJoinedParty;

	/**Called when we've left a party*/
	UPROPERTY(BlueprintAssignable, Category = "Saving/Loading")
	FOnLeaveParty OnLeaveParty;

	/**Called when a dialogue starts*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FOnDialogueBegan OnDialogueBegan;

	/**Called when a dialogue has been finished for any reason*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FOnDialogueFinished OnDialogueFinished;

	/**Called when a dialogue option is selected*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FDialogueOptionSelected OnDialogueOptionSelected;

	/**Called when the NPC(s) have finished talking and the players replies are available*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FDialogueRepliesAvailable OnDialogueRepliesAvailable;

	/**Called when a dialogue line starts*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FNPCDialogueLineStarted OnNPCDialogueLineStarted;

	/**Called when a dialogue line finishes*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FNPCDialogueLineFinished OnNPCDialogueLineFinished;

	/**Called when a dialogue line starts*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FPlayerDialogueLineStarted OnPlayerDialogueLineStarted;

	/**Called when a dialogue line finishes*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FPlayerDialogueLineFinished OnPlayerDialogueLineFinished;

	//Server replicates these back to client so client can keep its state machine in sync with the servers
	UPROPERTY(ReplicatedUsing = OnRep_PendingUpdateList)
	TArray<FNarrativeUpdate> PendingUpdateList;

	//A list of all the quests the player is involved in
	UPROPERTY(VisibleAnywhere, Category = "Quests")
	TArray<class UQuest*> QuestList;

	//Current dialogue the player is in 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quests")
	class UDialogue* CurrentDialogue;

	/*A map of every narrative task the player has ever completed, where the key is the amount of times the action has been completed
	a TMap means we can very efficiently track large numbers of actions, such as shooting where the player may shoot a gun thousands of times
	
	*/
	UPROPERTY(EditAnywhere, Category = "Quests")
	TMap<FString, int32> MasterTaskList;

	//We set this flag to true during loading so we don't broadcast any quest update delegates as we load quests back in
	bool bIsLoading;

protected:

	/** The party we're in, if any. */
	UPROPERTY(ReplicatedUsing=OnRep_PartyComponent, BlueprintReadOnly, Category = "Narrative")
	class UNarrativePartyComponent* PartyComponent;

	//We cache the OwningController, we won't cache pawn as this might change
	UPROPERTY()
	class APlayerController* OwnerPC;

	void SendNarrativeUpdate(const FNarrativeUpdate& Update);

	UFUNCTION()
	void OnRep_PartyComponent(class UNarrativePartyComponent* OldPartyComponent);

	UFUNCTION()
	void OnRep_PendingUpdateList();

	/**Used internally when a quest is started - Ensures that a quest asset doesn't have any errors,
	for example a quest that has no ending, the start node has no connection, no loose nodes, etc.
	@param OutError passed by ref, if an error is found OutError will contain the error message*/
	bool IsQuestValid(const UQuest* QuestAsset, FString& OutError);

	UFUNCTION()
	virtual void NarrativeDataTaskCompleted(const UNarrativeDataTask* NarrativeTask, const FString& Name);

	UFUNCTION()
	virtual void QuestStarted(const UQuest* Quest);

	UFUNCTION()
	virtual void QuestForgotten(const UQuest* Quest);

	UFUNCTION()
	virtual void QuestFailed(const UQuest* Quest, const FText& QuestFailedMessage);

	UFUNCTION()
	virtual void QuestSucceeded(const UQuest* Quest, const FText& QuestSucceededMessage);

	UFUNCTION()
	virtual void QuestNewState(UQuest* Quest, const UQuestState* NewState);

	UFUNCTION()
	virtual void QuestTaskProgressMade(const UQuest* Quest, const UNarrativeTask* Task, const class UQuestBranch* Branch, int32 OldProgress, int32 NewProgress);

	UFUNCTION()
	virtual void QuestTaskCompleted(const UQuest* Quest, const UNarrativeTask* Task, const class UQuestBranch* Branch);

	UFUNCTION()
	virtual void QuestBranchCompleted(const UQuest* Quest, const class UQuestBranch* Branch);

	UFUNCTION()
	virtual void BeginSave(FString SaveName);

	UFUNCTION()
	virtual void BeginLoad(FString SaveName);

	UFUNCTION()
	virtual void SaveComplete(FString SaveName);

	UFUNCTION()
	virtual void LoadComplete(FString SaveName);

	UFUNCTION()
	virtual void DialogueRepliesAvailable(class UDialogue* Dialogue, const TArray<UDialogueNode_Player*>& PlayerReplies);

	UFUNCTION()
	virtual void DialogueLineStarted(class UDialogue* Dialogue, UDialogueNode* Node, const FDialogueLine& DialogueLine);

	UFUNCTION()
	virtual void DialogueLineFinished(class UDialogue* Dialogue, UDialogueNode* Node, const FDialogueLine& DialogueLine);

	UFUNCTION()
	virtual void DialogueBegan(class UDialogue* Dialogue);

	UFUNCTION()
	virtual void DialogueFinished(class UDialogue* Dialogue, const bool bStartingNewDialogue);


public:

	UFUNCTION(BlueprintPure, Category = "Narrative")
	virtual APawn* GetOwningPawn() const;

	UFUNCTION(BlueprintPure, Category = "Narrative")
	virtual APlayerController* GetOwningController() const;

	UFUNCTION(BlueprintPure, Category = "Narrative")
	class UDialogue* GetCurrentDialogue() const;

public:

	//BP exposed functions for scripters/designers 

	/**
	Return true if a given quest is started or finished
	@param QuestAsset The quest to check
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	virtual bool IsQuestStartedOrFinished(TSubclassOf<class UQuest> QuestClass) const;

	/**
	Return true if a given quest is in progress but false if the quest is finished
	@param QuestAsset The quest to check
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	virtual bool IsQuestInProgress(TSubclassOf<class UQuest> QuestClass) const;

	/**
	Return true if a given quest has been completed and was succeeded
	@param QuestAsset The quest to check
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	virtual bool IsQuestSucceeded(TSubclassOf<class UQuest> QuestClass) const;

	/**
	Return true if a given quest has been completed and was failed
	@param QuestAsset The quest to check
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	virtual bool IsQuestFailed(TSubclassOf<class UQuest> QuestClass) const;

	/**
	Return true if a given quest has been completed, regardless of whether we failed or succeeded the quest
	@param QuestAsset The quest to use
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	virtual bool IsQuestFinished(TSubclassOf<class UQuest> QuestClass) const;

	/**
	Begin a given quest. Return quest if success. 
	
	@param QuestAsset The quest to use
	@param StartFromID If this is set to a valid ID in the quest, we'll skip to this quest state instead of playing the quest from the start

	@return The created UQuest instance
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quests", meta = (AdvancedDisplay = "1"))
	virtual class UQuest* BeginQuest(TSubclassOf<class UQuest> QuestClass, FName StartFromID = NAME_None);


	/**
	Restart a given quest. Will only actually restart the quest if it has been started.
	If the quest hasn't started,  this will do nothing.

	@param QuestAsset The quest to use
	@param StartFromID If this is set to a valid ID in the quest, we'll skip to this quest state instead of playing the quest its default start state

	@return Whether or not the quest was restarted or not
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quests", meta = (AdvancedDisplay = "1"))
	virtual bool RestartQuest(TSubclassOf<class UQuest> QuestClass, FName StartFromID = NAME_None);

	/**
	Forget a given quest. The quest will be removed from the players quest list, 
	and the quest can be started again later using BeginQuest() if desired.

	@param QuestAsset The quest to use
	@return Whether or not the quest was forgotten or not
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quests")
	virtual bool ForgetQuest(TSubclassOf<class UQuest> QuestClass);

	/**
	* 
	Check if calling BeginDialogue on a given dialogue asset would actually play any dialogue.
	
	THIS WILL ONLY WORK ON THE AUTHORITY. Server should use this function then replicate any needed stuff to clients, this will always 
	return false on clients as they do not have the authority to begin dialogues. 

	Essentially the same as BeginDialogue, just doesn't actually start the dialogue, just gives you
	the bool return value to check if any dialogue would have started. 

	Same as BeginDialogue, however doesn't actually begin the dialogue.*/
	UFUNCTION(BlueprintPure, Category = "Dialogues")
	virtual bool HasDialogueAvailable(TSubclassOf<class UDialogue> Dialogue, const FDialoguePlayParams PlayParams = FDialoguePlayParams());

	/**Sets CurrentDialogue to the given dialogue class, cleaning up our existing dialogue if one is going. Won't actually begin playing the dialogue. */
	virtual bool SetCurrentDialogue(TSubclassOf<class UDialogue> Dialogue, const FDialoguePlayParams PlayParams = FDialoguePlayParams());

	/**
	* Only callable on the server. Server grabs root dialogue node, validates its conditions, and sends it to the client via ClientRecieveDialogueOptions
	*
	*  NOTE: If using multi-NPC dialogues you'll need to ensure each speaker has their avatar correctly assigned. To do this, either set a SpeakerAvatarClass
	* in your speaker info and let narrative spawn your speaker avatars in and it will correctly have them assigned, or if your avatars already exist in the 
	* level just add the Speakers ID to them as a tag and narrative will automatically find them for you. If neither of those work you can override the FindSpeakerAvatar function. 
	* 
	@param Dialogue The dialogue to begin 
	@param DefaultNPCAvatar The NPC Avatar to use. For multi-NPC dialogues each speaker obviously needs their own avatar assigned - see the functions comment on how to do this.  
	@param StartFromID The ID of the node you want to jump to. Can be left empty and the dialogue will begin from the root node.

	@return Whether the dialogue was successfully started 
	*/
	UFUNCTION(BlueprintCallable, Category = "Dialogues", BlueprintAuthorityOnly, meta=(AdvancedDisplay = "1"))
	virtual bool BeginDialogue(TSubclassOf<class UDialogue> Dialogue, const FDialoguePlayParams PlayParams = FDialoguePlayParams());

	/**Used by the server to tell client to start dialogue. Also sends the initial chunk*/
	UFUNCTION(Client, Reliable, Category = "Dialogues")
	virtual void ClientBeginDialogue(TSubclassOf<class UDialogue> Dialogue, const TArray<FName>& NPCReplyChainIDs, const TArray<FName>& AvailableResponseIDs);

	/**[server] Begin a party dialogue for the player. */
	virtual void BeginPartyDialogue(TSubclassOf<class UDialogue> Dialogue, const TArray<FName>& NPCReplyChainIDs, const TArray<FName>& AvailableResponseIDs);

	/**Used by the server to inform client to start party dialogue. Also sends the initial chunk*/
	UFUNCTION(Client, Reliable, Category = "Dialogues")
	virtual void ClientBeginPartyDialogue(TSubclassOf<class UDialogue> Dialogue, const TArray<FName>& NPCReplyChainIDs, const TArray<FName>& AvailableResponseIDs);

	/**Used by the server to tell client to end dialogue*/
	UFUNCTION(Client, Reliable, Category = "Dialogues")
	virtual void ClientExitDialogue();

	/**Used by the server to tell client to end dialogue*/
	UFUNCTION(Client, Reliable, Category = "Dialogues")
	virtual void ClientExitPartyDialogue();

	/**Used by the server to send valid dialogue chunks to the client*/
	UFUNCTION(Client, Reliable, Category = "Dialogues")
	virtual void ClientRecieveDialogueChunk(const TArray<FName>& NPCReplyChainIDs, const TArray<FName>& AvailableResponseIDs);

	/**Called by the client when it tries selecting a dialogue option - the server ultimately decides if this goes through or not,
	though the server validates replies before it sends them to you, so this should never fail */
	UFUNCTION(BlueprintCallable, Category = "Dialogues")
	virtual void TrySelectDialogueOption(class UDialogueNode_Player* Option);

	/**[server] Selects a dialogue option. Will update the dialogue and automatically start playing the next bit of dialogue*/
	virtual void SelectDialogueOption(class UDialogueNode_Player* Option, class APlayerState* Selector = nullptr);

	/**Allows the server to inform a client to select a dialogue option. Used by party dialogues */
	UFUNCTION(Client, Reliable, Category = "Dialogues")
	virtual void ClientSelectDialogueOption(const FName& OptionID, class APlayerState* Selector=nullptr);

	/**Tell the server we've selected a dialogue option*/
	UFUNCTION(Server, Reliable, Category = "Dialogues")
	virtual void ServerSelectDialogueOption(const FName& OptionID);



	/**Attempt to skip the current dialogue line
	Return true if we skipped the line, or if called on a client return true if we were able to ask server to skip the line (isn't guaranteed, server still needs to auth it)
	*/
	UFUNCTION(BlueprintCallable, Category = "Dialogues")
	virtual bool TrySkipCurrentDialogueLine();

	/**[server only] Skip the current dialogue line */
	virtual bool SkipCurrentDialogueLine();

	/**Attempt to skip the current dialogue line */
	UFUNCTION(Server, Reliable, Category = "Dialogues")
	virtual void ServerTrySkipCurrentDialogueLine();


	/**
	* Exit the dialogue, but authoritatively check we're allowed to before doing so. 
	* 
	@return Whether the dialogue was successfully exited
	*/
	UFUNCTION(BlueprintCallable, Category = "Dialogues")
	virtual void TryExitDialogue();

	/**Exit the dialogue, will never fail*/
	virtual void ExitDialogue();

	UFUNCTION(Server, Reliable, Category = "Dialogues")
	virtual void ServerTryExitDialogue();

	/**Return true if we're in a dialogue 

	@return Whether true if we're in a dialogue, false otherwise 
	*/
	UFUNCTION(BlueprintPure, Category = "Dialogues")
	virtual bool IsInDialogue();

	/**Complete a narrative task.

	If the task updates a quest, the input will be replicated back to the client so it can 
	then update its own state machines and keep them in sync with the server. This means we can use very little network
	bandwidth as we're only sending state machine FName inputs through instead of trying to replicate the entire UQuest object.

	Not marked blueprint callable since CompleteNarrativeTask() is exposed via a custom K2Node.

	@param Task the task that the player has completed
	@param Argument the tasks argument, ie if the task is ObtainItem this would be the name of the item we obtained
	@param Quantity the amount of times the task was completed

	@return Whether the data task successfully added itself */
	virtual bool CompleteNarrativeDataTask(const UNarrativeDataTask* Task, const FString& Argument, const int32 Quantity = 1);
	virtual bool CompleteNarrativeDataTask(const FString& TaskName, const FString& Argument, const int32 Quantity = 1);

protected:
	
	/**
	Creates a new quest and initializes it from a given quest class. 
	*/
	virtual class UQuest* MakeQuestInstance(TSubclassOf<class UQuest> QuestClass);

	/**Called after CompleteNarrativeTask() has converted the Task into a raw string version of our Task */
	virtual bool CompleteNarrativeTask_Internal(const FString& TaskString, const bool bFromReplication, const int32 Quantity);

	/**
	Create a dialogue object from the supplied dialogue class and params
	*/
	virtual class UDialogue* MakeDialogueInstance(TSubclassOf<class UDialogue> DialogueClass, const FDialoguePlayParams PlayParams = FDialoguePlayParams());


public:

	/**
	Check if the player has ever completed a given data task. For example you could check if the player has ever talked to a given NPC, taken a certain item, etc
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	bool HasCompletedTask(const UNarrativeDataTask* Task, const FString& Name, const int32 Quantity = 1 );

	/**
	* 
	IN MULTIPLAYER GAMES, THIS FUNCTION WILL ONLY WORK ON THE SERVER. Please see MasterTaskList comment for more info. 

	Check how many times the player has ever completed a narrative Task. Very efficient.
	
	Very powerful for scripting, for example we could check if the player has talked to an NPC at least 10 times and then change the dialogue
	text to a more personalized greeting since the NPC would know the player better. 

	@return The number of times the Task has been completed by this narrative component. 

	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	int32 GetNumberOfTimesTaskWasCompleted(const UNarrativeDataTask* Task, const FString& Name);

	/**Returns a list of all failed quests, in chronological order.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	TArray<UQuest*> GetFailedQuests() const;

	/**Returns a list of all succeeded quests, in chronological order.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	TArray<UQuest*> GetSucceededQuests() const;

	/**Returns a list of all quests that are in progress, in chronological order.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	TArray<UQuest*> GetInProgressQuests() const;

	/**Returns a list of all quests that are started, failed, or succeeded, in chronological order.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	TArray<UQuest*> GetAllQuests() const;

	/**Given a Quest class return its active quest object if we've started this quest */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	class UQuest* GetQuestInstance(TSubclassOf<class UQuest> QuestClass) const;

	/**Return our parties component, if we have one*/
	UFUNCTION(BlueprintPure, Category = "Parties")
	FORCEINLINE class UNarrativePartyComponent* GetParty() const {return PartyComponent;};

	/**
	* 
	* Save every quest we've done, and every legacy task the player has ever completed
	* 
	* BE CAREFUL: If you ship your game and players save their game, and you then modify a quest and ship that update to 
	* people, it can break their save file, especially if they were at a state that you later remove or change the ID of. It is recommended to not touch
	* quests as much as possible once they are shipped for this reason, especially do not remove/rename states the player can be at.
	* 
	@param SaveName the name of the save game. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Saving")
	virtual bool Save(const FString& SaveName = "NarrativeSaveData", const int32 Slot = 0);

	/**Load narratives state back in from disk
	@param SaveName the name of the save game. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Saving")
	virtual bool Load(const FString& SaveName = "NarrativeSaveData", const int32 Slot = 0);

	/**Deletes a saved game from disk. USE THIS WITH CAUTION. Return true if save file deleted, false if delete failed or file didn't exist.*/
	UFUNCTION(BlueprintCallable, Category = "Saving")
	virtual bool DeleteSave(const FString& SaveName = "NarrativeSaveData", const int32 Slot = 0);

	/**
	Once server loads a save in, it sends all the info to the client via this RPC so it can load it in. We do this so loading save
	games can be done by the server and flow down to the client.
	*/
	UFUNCTION(Client, Reliable, Category = "Saving")
	virtual void ClientReceiveSave(const TArray<FNarrativeSavedQuest>& SavedQuests, const TArray<FString>& Tasks, const TArray<int32>& Quantities);

protected:

	//Internal load function that actually does the work.
	virtual bool Load_Internal(const TArray<FNarrativeSavedQuest>& SavedQuests, const TMap<FString, int32>& NewMasterList);



};