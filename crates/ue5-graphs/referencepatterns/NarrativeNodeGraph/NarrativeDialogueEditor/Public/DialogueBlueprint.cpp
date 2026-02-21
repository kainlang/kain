// Copyright Narrative Tools 2022. 


#include "DialogueBlueprint.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "DialogueBlueprintGeneratedClass.h"

UDialogueBlueprint::UDialogueBlueprint()
{
	DialogueTemplate = CreateDefaultSubobject<UDialogue>(TEXT("DialogueTemplate"));
	DialogueTemplate->SetFlags(RF_Transactional | RF_ArchetypeObject);
}

UClass* UDialogueBlueprint::GetBlueprintClass() const
{
	return UDialogueBlueprintGeneratedClass::StaticClass();
}

void UDialogueBlueprint::PostLoad()
{
	Super::PostLoad();

	DialogueTemplate->SetFlags(RF_Transactional | RF_ArchetypeObject);

	//Any dialogues created prior to 2.6 have their speakers stored in the Dialogue Template - this is a mistake, it needs to be in the CDO.
	if (DialogueTemplate->Speakers.Num())
	{
		if (UDialogue* DialogueCDO = Cast<UDialogue>(GeneratedClass->GetDefaultObject()))
		{
			DialogueCDO->Speakers = DialogueTemplate->Speakers;
			DialogueTemplate->Speakers.Empty();
		}
	}
}

bool UDialogueBlueprint::ValidateGeneratedClass(const UClass* InClass)
{
	return true;
}
