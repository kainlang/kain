/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "QuestEditorBlueprintFactories.h"

#include "DialogueMasterTask.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "DialogueMasterStatistic.h"


// -----------------------------------------------------------
// Quest Task
// -----------------------------------------------------------
UQuestTaskFactory::UQuestTaskFactory()
{
	SupportedClass = UDialogueMasterTask::StaticClass();
	ParentClass = UDialogueMasterTask::StaticClass();
	bSkipClassPicker = true;
}

UObject* UQuestTaskFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
}

UObject* UQuestTaskFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}


// -----------------------------------------------------------
// Statistics DA
// -----------------------------------------------------------
UDialogueMasterStatisticActionFactory::UDialogueMasterStatisticActionFactory()
{
	SupportedClass = UDialogueMasterStatistic::StaticClass();

	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UDialogueMasterStatisticActionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UDialogueMasterStatistic>(InParent, Class, Name, Flags);
}

bool UDialogueMasterStatisticActionFactory::CanCreateNew() const
{
	return true;
}

FString UDialogueMasterStatisticActionFactory::GetDefaultNewAssetName() const
{
	return FString(TEXT("NewStoredStatistic"));
}

FText UDialogueMasterStatisticActionFactory::GetDisplayName() const
{
	return FText::FromString("Statistic");
}

