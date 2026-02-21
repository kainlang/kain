//  Copyright Narrative Tools 2022.


#include "QuestTaskBlueprintFactory.h"
#include "QuestTask.h"
#include "NarrativeCondition.h"
#include "NarrativeEvent.h"
#include "NarrativeQuestTaskBlueprint.h"
#include <Kismet2/KismetEditorUtilities.h>

UQuestTaskBlueprintFactory::UQuestTaskBlueprintFactory()
{
	SupportedClass = UNarrativeTaskBlueprint::StaticClass();
	ParentClass = UNarrativeTask::StaticClass();
	bSkipClassPicker = true;
}

UObject* UQuestTaskBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a blueprint, then create and init one
	check(Class->IsChildOf(UBlueprint::StaticClass()));

	return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UNarrativeTaskBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
}

UObject* UQuestTaskBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

UNarrativeEventBlueprintFactory::UNarrativeEventBlueprintFactory()
{
	SupportedClass = UNarrativeEvent::StaticClass();//Parent class will actually be UBlueprint, we need to follow Factory selector
	ParentClass = UNarrativeEvent::StaticClass();
	bSkipClassPicker = true;
}

UObject* UNarrativeEventBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a blueprint, then create and init one
	//check(Class->IsChildOf(UBlueprint::StaticClass()));

	return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
}

UObject* UNarrativeEventBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

UNarrativeConditionFactory::UNarrativeConditionFactory()
{
	SupportedClass = UNarrativeCondition::StaticClass();//Parent class will actually be UBlueprint, we need to follow Factory selector
	ParentClass = UNarrativeCondition::StaticClass();
	bSkipClassPicker = true;
}

UObject* UNarrativeConditionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a blueprint, then create and init one
	//check(Class->IsChildOf(UBlueprint::StaticClass()));

	return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
}

UObject* UNarrativeConditionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}
