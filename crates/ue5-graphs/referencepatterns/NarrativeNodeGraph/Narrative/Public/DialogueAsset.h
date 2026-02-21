// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DialogueAsset.generated.h"

class UDialogue;

/**
 * Legacy dialogue asset, still included so users of previous versions can right click and convert these into DialogueBlueprints 
 */
UCLASS(BlueprintType)
class NARRATIVE_API UDialogueAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:

	UDialogueAsset();

	UPROPERTY(EditAnywhere, Instanced, Category = "Dialogue")
	UDialogue* Dialogue;

#if WITH_EDITORONLY_DATA

		/** Graph for quest asset */
		UPROPERTY()
		class UEdGraph*	DialogueGraph;

	/** Info about the graphs we last edited */
	UPROPERTY()
	TArray<FEditedDocumentInfo> LastEditedDocuments;

#endif


};
