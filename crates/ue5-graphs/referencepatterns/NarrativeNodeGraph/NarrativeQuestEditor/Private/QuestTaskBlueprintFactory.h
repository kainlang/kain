//  Copyright Narrative Tools 2022.

#pragma once

#include "CoreMinimal.h"
#include "Factories/BlueprintFactory.h"
#include "QuestTaskBlueprintFactory.generated.h"

/**
 * Factory for creating a new QuestTaskBlueprint 
 */
UCLASS()
class UQuestTaskBlueprintFactory : public UBlueprintFactory
{
	GENERATED_BODY()
	
	UQuestTaskBlueprintFactory();

	// UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory Interface

};

/**
 * Sticking events and conditions in here too because adding 4 code files for boilerplate factories feels overkill 
 */
UCLASS()
class UNarrativeEventBlueprintFactory : public UBlueprintFactory
{
	GENERATED_BODY()
	UNarrativeEventBlueprintFactory();

	// UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory Interface

};

UCLASS()
class UNarrativeConditionFactory : public UBlueprintFactory
{
	GENERATED_BODY()

	UNarrativeConditionFactory();

	// UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory Interface

};