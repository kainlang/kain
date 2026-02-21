// Copyright Narrative Tools 2022. 


#include "QuestBlueprintGeneratedClass.h"
#include "Quest.h"
#include "QuestSM.h"

void UQuestBlueprintGeneratedClass::InitializeQuest(class UQuest* Quest)
{
	if (Quest)
	{
		//Do what UUserWidget uses to initialize quest from bgclass 
		Quest->DuplicateAndInitializeFromQuest(QuestTemplate);
	}
}

void UQuestBlueprintGeneratedClass::PostLoad()
{
	Super::PostLoad();

	if (QuestTemplate)
	{
		// We don't want any of these flags to carry over from the WidgetBlueprint
		QuestTemplate->ClearFlags(RF_Public | RF_ArchetypeObject | RF_DefaultSubObject);
	}
}

void UQuestBlueprintGeneratedClass::SetQuestTemplate(UQuest* InQuestTemplate)
{
	QuestTemplate = InQuestTemplate;

	//These flags will be on the blueprints quest template, need to clear them 
	if (QuestTemplate)
	{
		QuestTemplate->ClearFlags(RF_Public | RF_ArchetypeObject | RF_DefaultSubObject);
	}
}
