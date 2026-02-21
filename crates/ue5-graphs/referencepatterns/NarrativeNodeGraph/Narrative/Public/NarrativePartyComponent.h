// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "NarrativeComponent.h"
#include "NarrativePartyComponent.generated.h"

/**Defines how a party goes about selecting replies*/
UENUM(BlueprintType)
enum class EPartyDialogueControlPolicy : uint8 
{
	/**Only the party leader can select dialogue options, the rest of the players will be essentially spectating the conversation*/
	PartyLeaderControlled UMETA(DisplayName="Party Leader Controlled"),

	/**Anyone in the party can select dialogue options. Everyones camera will cut to whoever selected the line*/
	AllPlayers UMETA(DisplayName = "All Party Members Controlled")
};

/**
 * A Narrative component intended to be shared by multiple clients. This allows for some very cool functionality, teammates
 * can play quests and dialogues together with each other. Use AddPartyMember and RemovePartyMember to setup your party. 
 * 
 * QUESTS: Quests began on the party component will be shown on all party members UI, and any player in the party can complete quest tasks.
 * 
 * DIALOGUE: Dialogues began on the party component will begin for all players, and all players will see the dialogue in sync - if a player selects
 * a dialogue option all party members will see that player say the line - this behaviour can be modified in the components settings. 
 * 
 * You should put this component on an actor that replicates to all of your team members. The Game State is a great place for this,
 * however if your game requires multiple different parties you'll want to make a ASquad etc that derives AInfo to hold all your team members
 * and manage them, and put a party component there instead of the game state. 
 */
UCLASS( ClassGroup=(Narrative), DisplayName = "Narrative Party Component", meta=(BlueprintSpawnableComponent) )
class NARRATIVE_API UNarrativePartyComponent : public UNarrativeComponent
{
	GENERATED_BODY()
	

protected:

	UNarrativePartyComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsPartyComponent() const {return true;}

public:

	virtual bool BeginDialogue(TSubclassOf<class UDialogue> Dialogue, const FDialoguePlayParams PlayParams = FDialoguePlayParams()) override;
	virtual void SelectDialogueOption(class UDialogueNode_Player* Option, class APlayerState* Selector = nullptr) override;
	virtual void ExitDialogue() override;

	/**Parties dont have owning controllers or pawns so we iterate our party members and find them that way*/
	virtual APawn* GetOwningPawn() const override;
	virtual APlayerController* GetOwningController() const override;

	//[server] Add a member to the party.  Return true if successful.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parties")
	virtual bool AddPartyMember(class UNarrativeComponent* Member);

	//[server] Remove a member from the party. Return true if successful, false if player wasn't in party etc
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parties")
	virtual bool RemovePartyMember(class UNarrativeComponent* Member);

	//Return the members in the party 
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parties")
	TArray<class UNarrativeComponent*> GetPartyMembers() const {return PartyMembers;};

	//Return the members PlayerStates in the party 
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parties")
	TArray<class APlayerState*> GetPartyMemberStates() const {return PartyMemberStates;};

	//Return the party leader (only works on server) 
	UFUNCTION(BlueprintPure, Category = "Parties")
	class UNarrativeComponent* GetPartyLeader() const { return PartyMembers.IsValidIndex(0) ? PartyMembers[0] : nullptr; };

	/**Return whether or not we're the leader of our party. Return true if we're not in a party as we're essentially the leader in that case*/
	UFUNCTION(BlueprintPure, Category = "Parties")
	bool IsPartyLeader(class APlayerState* Member);

	/**Defines how a party goes about selecting replies - currently just enforced by the UI, isn't actually authed by server */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parties")
	EPartyDialogueControlPolicy PartyDialogueControlPolicy;

protected:

	/** All of the players in the party */
	UPROPERTY(BlueprintReadOnly, Category = "Parties")
	TArray<class UNarrativeComponent*> PartyMembers;

	/** Narrative Components exist on peoples player controllers, and so there isn't a nice way for people in the party to access
	each others pawns/playerstates via PartyMembers array, and so this array exists to solve that. We store PStates because pawns can change
	possession but PState->GetPawn() will always give us the current valid pawn */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Parties")
	TArray<class APlayerState*> PartyMemberStates;

};