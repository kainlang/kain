// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "QuestActionFactory.generated.h"

/**
 * 
 */
UCLASS()
class UQuestActionFactory : public UFactory
{
	GENERATED_BODY()

	UQuestActionFactory();

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool CanCreateNew() const override;
	virtual FString GetDefaultNewAssetName() const override;
	virtual FText GetDisplayName() const override;
	// End of UFactory interface


};
