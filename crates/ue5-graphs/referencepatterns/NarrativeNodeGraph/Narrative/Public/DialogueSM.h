// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "NarrativeNodeBase.h"
#include "MovieSceneSequencePlayer.h"
#include "DialogueSM.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueNodeFinishedPlaying);

/**Convinience struct with a details customization that allows the speaker ID to be selected from a combobox
rather than inputted as an FName */
USTRUCT(BlueprintType)
struct FSpeakerSelector
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Speakers")
	FName SpeakerID;

	FSpeakerSelector()
	{}

	FSpeakerSelector(const FName& InSpeakerID) :
		SpeakerID(InSpeakerID)
	{}
};

//Defines when the line is finished and we should play the next one 
UENUM(BlueprintType)
enum class ELineDuration : uint8
{
	/*The default option is generally the best and won't often need changed. Default will use When Audio Ends if 
	audio is set, then when sequence ends if that is set and no text is set, then after default reading time if text is set. Empty lines are instantly finished.  */
	LD_Default UMETA(DisplayName="Default"),
	//The line finishes when the sound ends
	LD_WhenAudioEnds UMETA(DisplayName = "When Audio Ends"),
	//The line finishes when the sequence ends
	LD_WhenSequenceEnds UMETA(DisplayName = "When Sequence Ends"),
	//The line finishes when the player has finished reading the line, given the letters per second reading rate set in Project Settings. 
	LD_AfterReadingTime UMETA(DisplayName = "After Reading Time"),
	//The line finishes when the duration is up
	LD_AfterDuration UMETA(DisplayName = "After X Seconds"),

	//The line never ends, and the only way to end the line is by skipping it with the enter key 
	LD_Never UMETA(DisplayName = "Never")
};

USTRUCT(BlueprintType)
struct FDialogueLine
{
	GENERATED_BODY()

public:

	FDialogueLine()
	{
		Text = FText::GetEmpty();
		FacialAnimation = nullptr;
		DialogueSound = nullptr;
		DialogueMontage = nullptr;
		Shot = nullptr;
		Duration = ELineDuration::LD_Default;
		DurationSecondsOverride = 0.f;
	}

	/**
	The text for this dialogue node. Narrative will automatically display this on the NarrativeDefaultUI if you're using that, otherwise you can simply grab this
	yourself if you're making your own dialogue UI - it is readable from Blueprints.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Line", meta = (MultiLine = true))
	FText Text;

	/**
	The duration the line should play for 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Line")
	ELineDuration Duration;

	/**
	The overridden seconds the line should play for 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Line", meta = (EditCondition = "Duration == ELineDuration::LD_AfterDuration", EditConditionHides))
	float DurationSecondsOverride;

	/**
	* If a dialogue sound is selected, narrative will automatically play the sound for you in 3D space, at the location of the speaker.  
	* If narrative can't find a speaker actor (for example if you were getting a phone call where there isn't an physical speaker) it will be played in 2D. 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Line")
	class USoundBase* DialogueSound;

	/**
	Narrative will play this montage on the first skeletalmeshcomponent found on your speaker with the tag "Body" added to it.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Line", meta = (DisplayName = "Body Animation"))
	class UAnimMontage* DialogueMontage;

	/**
	Narrative will play this montage on the first skeletalmeshcomponent found on your speaker with the tag "Face" added to it. 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Line")
	class UAnimMontage* FacialAnimation;

	/**
	* Shot to play for this line. Overrides speaker shot if one is set 
	*/
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Dialogue Line")
	class UNarrativeDialogueSequence* Shot;
};

/**Base class for states and branches in the Dialogues state machine*/
 UCLASS(BlueprintType, Blueprintable)
 class NARRATIVE_API UDialogueNode : public UNarrativeNodeBase
 {

	 GENERATED_BODY()

 public:

	UDialogueNode();

	 //The dialogue line associated with this node
	 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Details - Dialogue Node", meta = (ShowOnlyInnerProperties))
	 FDialogueLine Line;

	 /** If alternative lines are added in here, narrative will randomly select either the main line or one of the alternatives.
	 
	 This can make dialogues more random and believable. 
	 */
	 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue Line", meta=(AdvancedDisplay))
	 TArray<FDialogueLine> AlternativeLines;

	 virtual FDialogueLine GetRandomLine(const bool bStandalone) const;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnDialogueNodeFinishedPlaying OnDialogueFinished;

	//The last line the dialogue node played.
	UPROPERTY(BlueprintReadOnly, Category = "Details")
	FDialogueLine PlayedLine;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (AdvancedDisplay))
	TArray<class UDialogueNode_NPC*> NPCReplies;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (AdvancedDisplay))
	TArray<class UDialogueNode_Player*> PlayerReplies;
	
	UPROPERTY()
	class UDialogue* OwningDialogue;

	UPROPERTY()
	class UNarrativeComponent* OwningComponent;

	//Name of custom event to call when this is reached 
	UPROPERTY(BlueprintReadWrite, Category = "Details - Dialogue Node", meta = (AdvancedDisplay))
	FName OnPlayNodeFuncName;

	/**The ID of the speaker we are saying this line to. Can be left empty. */
	UPROPERTY(BlueprintReadWrite, Category = "Details - Dialogue Node")
	FName DirectedAtSpeakerID;
	
	/**Should pressing the enter key allow this line to be skipped?*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Details - Dialogue Node")
	bool bIsSkippable;

#if WITH_EDITORONLY_DATA

	/**If true, the dialogue editor will style this node in a compact form*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Details - Dialogue Editor")
	bool bCompactView;

#endif

	class UDialogueNode_NPC* GetFirstValidNPCReply(APlayerController* OwningController, APawn* OwningPawn, class UNarrativeComponent* NarrativeComponent);
	TArray<class UDialogueNode_Player*> GetPlayerReplies(APlayerController* OwningController, APawn* OwningPawn, class UNarrativeComponent* NarrativeComponent);

	virtual UWorld* GetWorld() const;

	//The text this dialogue should display on its Graph Node
	const bool IsMissingCues() const;

	//Node is just used for routing and doesn't contain any dialogue 
	bool IsRoutingNode() const;

private:

#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

public:
	virtual void EnsureUniqueID();
	void GenerateIDFromText();

	bool HasDefaultID() const;

#endif 

 };

UCLASS(BlueprintType)
class NARRATIVE_API UDialogueNode_NPC : public UDialogueNode
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "NPC Dialogue Node")
	FName GetSpeakerID() const {return SpeakerID;}; 

	void SetSpeakerID(const FName& NewID) { SpeakerID = NewID; };

	//Sequence to play when player is selecting their reply after this shot has played 
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Details - NPC Dialogue Node")
	class UNarrativeDialogueSequence* SelectingReplyShot;

	/**Grab this NPC node, appending all follow up responses to that node. Since multiple NPC replies can be linked together, 
	we need to grab the chain of replies the NPC has to say. */
	TArray<class UDialogueNode_NPC*> GetReplyChain(APlayerController* OwningController, APawn* OwningPawn, class UNarrativeComponent* NarrativeComponent);

protected:

	/**The ID of the speaker for this node */
	UPROPERTY(BlueprintReadWrite, Category = "Details - NPC Dialogue Node")
	FName SpeakerID;

};

UCLASS(BlueprintType)
class NARRATIVE_API UDialogueNode_Player : public UDialogueNode
{
	GENERATED_BODY()

public:

	//Runs a wildcard replace on a player reply 
	UFUNCTION(BlueprintPure, Category = "Details")
	virtual FText GetOptionText(class UDialogue* InDialogue) const;

	//Get any hint text that should be added to a reply ( ie (Lie, Start Quest, etc))
	UFUNCTION(BlueprintPure, Category = "Details")
	virtual FText GetHintText(class UDialogue* InDialogue) const;

	FORCEINLINE bool IsAutoSelect() const {return bAutoSelect || IsRoutingNode(); };

protected:

	/**The shortened text to display for dialogue option when it shows up in the list of available responses. If left empty narrative will just use the main text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details - Player Dialogue Node")
	FText OptionText;

	/**Optional hint text after the option text, ie (Lie, Persauade, Begin Quest) If left empty narrative will see if events have hint text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details - Player Dialogue Node")
	FText HintText;

	/**If true, this dialogue option will be automatically selected instead of the player having to select it from the UI*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details - Player Dialogue Node")
	bool bAutoSelect = false;
};