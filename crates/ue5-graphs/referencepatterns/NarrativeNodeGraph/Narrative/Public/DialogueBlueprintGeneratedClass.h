// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/Package.h"
#include "DialogueBlueprintGeneratedClass.generated.h"

class UDialogueState;
class UDialogue;

/**
 * Blueprint generated class for Dialogue blueprints. The Dialogue compiler compiles the Dialogue and stores it in
 * the DialogueTemplate object ready for use at runtime. Good explanation at https://heapcleaner.wordpress.com/2016/06/12/inside-of-unreal-engine-blueprint/
 */
UCLASS()
class NARRATIVE_API UDialogueBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_BODY()
	
public:

	virtual void InitializeDialogue(class UDialogue* Dialogue);


	// UStruct interface
	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	// End of UStruct interface
	
	virtual void PostLoad() override;

	// UClass interface
	virtual void PurgeClass(bool bRecompilingOnLoad) override;
	// End of UClass interface

	UDialogue* GetDialogueTemplate() const {return DialogueTemplate;}
	void SetDialogueTemplate(UDialogue* InDialogueTemplate);

private:

	//The Dialogue template to be created 
	UPROPERTY()
	class UDialogue* DialogueTemplate;


};
