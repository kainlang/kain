// Copyright Narrative Tools 2022. 

#include "QuestBlueprint.h"
#include "Quest.h"
#include "QuestSM.h"
#include "QuestBlueprintGeneratedClass.h"

UQuestBlueprint::UQuestBlueprint(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	QuestTemplate = CreateDefaultSubobject<UQuest>(TEXT("QuestTemplate"));
	QuestTemplate->SetFlags(RF_Transactional | RF_ArchetypeObject);
}


UClass* UQuestBlueprint::GetBlueprintClass() const
{
	return UQuestBlueprintGeneratedClass::StaticClass();
}

void UQuestBlueprint::PostLoad()
{
	Super::PostLoad();

	QuestTemplate->SetFlags(RF_Transactional | RF_ArchetypeObject);
}

bool UQuestBlueprint::ValidateGeneratedClass(const UClass* InClass)
{
	return true;
}

bool UQuestBlueprint::DoesStateHaveDuplicate(const class UQuestState* State, FName& OutBetterName)
{
	return false;
}
