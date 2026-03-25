/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "DialogueEditorBlueprintFactories.h"

#include "DialogNodeInfo.h"
#include "Kismet2/KismetEditorUtilities.h"

// -----------------------------------------------------------
// Dialogue Condition
// -----------------------------------------------------------
UDialogueConditionFactory::UDialogueConditionFactory()
{
	SupportedClass = UAdvancedPrerequisiteBase::StaticClass();
	ParentClass = UAdvancedPrerequisiteBase::StaticClass();
	bSkipClassPicker = true;
}

UObject* UDialogueConditionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
}

UObject* UDialogueConditionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}


// -----------------------------------------------------------
// Dialogue Action
// -----------------------------------------------------------
UDialogueActionFactory::UDialogueActionFactory()
{
	SupportedClass = UDialogueMasterAction::StaticClass();
	ParentClass = UDialogueMasterAction::StaticClass();
	bSkipClassPicker = true;
}

UObject* UDialogueActionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
}

UObject* UDialogueActionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}


// -----------------------------------------------------------
// Custom Camera shot
// -----------------------------------------------------------
UDialogueCameraShotFactory::UDialogueCameraShotFactory()
{
	SupportedClass = ADialogCameraActor::StaticClass();
	ParentClass = ADialogCameraActor::StaticClass();
	bSkipClassPicker = true;
}

UObject* UDialogueCameraShotFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
}

UObject* UDialogueCameraShotFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}