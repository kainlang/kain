// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeCondition.generated.h"

//How do we handle running this condition on a party dialogue?
UENUM()
enum class EPartyConditionPolicy
{
	/**The condition is run on all of your party members - if anyone in the party passes the condition, it passes. */
	AnyPlayerPasses UMETA(DisplayName = "Any Party Member Passes"),

	/**The condition is run on your party itself - your parties narrative component. If it passes, the condition passes. */
	PartyPasses UMETA(DisplayName="Party Passes"),

	/**The condition is run on all of your party members - everyone needs to pass for the condition to pass. */
	AllPlayersPass UMETA(DisplayName = "All Party Members Pass"),

	/**The condition is run on the party leader - if they pass the condition, the condition passes. */
	PartyLeaderPasses UMETA(DisplayName = "Party Leader Passes")
};

/**
 * Narrative Conditions allow you to make conditions that dialogues and quests can then use to conditionally include/exclude nodes.
 * 
 * For example, you could make a condition "HasItem" that checks if the player has a certain amount of an item. Then, you could use that
 * condition on a dialogue node so that the player can only say "I'll buy it!" if they actually have 500 coins in their inventory. 
 * 
 * Currently quests do not support NarrativeConditions, only NarrativeEvents. Dialogues support both. 
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, AutoExpandCategories = ("Default"))
class NARRATIVE_API UNarrativeCondition : public UObject
{
	GENERATED_BODY()

public:

	// Allows the Object to get a valid UWorld from it's outer.
	virtual UWorld* GetWorld() const override
	{
		if (HasAllFlags(RF_ClassDefaultObject))
		{
			// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
			return nullptr;
		}

		UObject* Outer = GetOuter();

		while (Outer)
		{
			UWorld* World = Outer->GetWorld();
			if (World)
			{
				return World;
			}

			Outer = Outer->GetOuter();
		}

		return nullptr;
	}

	/** Check whether this condition is true or false
	
	@param Pawn The pawn of the player we're running the condition on - will be null if running the condition against a party
	@param Controller The controller of the player we're running the condition on - will be null if running the condition against a party
	@param NarrativeComponent The narrative component we're running the condition on 
	*/
	UFUNCTION(BlueprintNativeEvent, Category = "Conditions")
	bool CheckCondition(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent);
	virtual bool CheckCondition_Implementation(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent);

	/**Define the text that will show up on a node if this condition is added to it */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Conditions")
	FString GetGraphDisplayText();
	virtual FString GetGraphDisplayText_Implementation();

	//Set this to true to flip the result of this condition
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conditions")
	bool bNot = false;

	/**
	Defines how the condition should be ran against a party that is doing this dialogue. Ignored by non-party dialogues. 

	Imagine if you want to check if your party has completed a quest. By default narrative will check if anyone in the party had completed 
	the quest on their own narrative component, but if you wanted to check if the party itself had completed the quest before you'd check this box.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parties")
	EPartyConditionPolicy PartyConditionPolicy = EPartyConditionPolicy::AnyPlayerPasses;
};
