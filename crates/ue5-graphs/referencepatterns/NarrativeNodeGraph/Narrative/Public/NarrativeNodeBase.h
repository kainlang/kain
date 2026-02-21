// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeEvent.h"
#include "NarrativeNodeBase.generated.h"

/**
 * The base class for all narrative nodes in eiher a quest state machine, or a dialogue tree 
 */
UCLASS()
class NARRATIVE_API UNarrativeNodeBase : public UObject
{
	GENERATED_BODY()

public:

	UNarrativeNodeBase();

#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

#endif 

	//This probably isn't needed anymore because we use node IDs to reference nodes over the network. TODO look at removing 
	bool IsNameStableForNetworking() const override {return true;};
	bool IsSupportedForNetworking() const override{return true;};

	/**The position the graph node is at, we need this because narrative does lots of sorting depending on the nodes Y position*/
	UPROPERTY()
	FVector2D NodePos; 

	//Execute all the events on this quest/dialogue node - this is blueprint callable so dialogue UI can call this
	//when a piece of dialogue is spoken
	UFUNCTION(BlueprintCallable, Category = "Events & Conditions")
	void ProcessEvents(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent, const EEventRuntime Runtime);

	//Check if all the conditions are met on this quest/dialogue node
	UFUNCTION(BlueprintCallable, Category = "Events & Conditions")
	bool AreConditionsMet(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent);

	/**
	This node only appears if the following conditions are met. Note that currently only dialogues support conditions, they won't do anything in quests!
	
	In multiplayer games the server will check the conditions to ensure your game is server authoritative.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Events & Conditions")
	TArray<class UNarrativeCondition*> Conditions;

	/**Events that should fire when this is reached. These are supported by both quests and dialogues, and will fire on both client and server. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Events & Conditions")
	TArray<class UNarrativeEvent*> Events;
	
	void SetID(const FName& NewID) 
	{
		ID = NewID;
		EnsureUniqueID();
	};

	FORCEINLINE FName GetID() const {return ID;};

protected:

	virtual void EnsureUniqueID(){};

	/**An optional ID for this node, can be left empty*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details", meta = (DisplayPriority = 0))
	FName ID;

};
