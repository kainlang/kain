// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "QuestAssetFactory.generated.h"

/**
 * Factory for creating new quest assets 
 */
UCLASS()
class UQuestAssetFactory : public UFactory
{
	GENERATED_BODY()

	UQuestAssetFactory();

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

	virtual bool ConfigureProperties() override;
	virtual bool CanCreateNew() const override;
	virtual FString GetDefaultNewAssetName() const override;
	virtual FText GetDisplayName() const override;
	// End of UFactory interface
	
	public:

	UPROPERTY(EditAnywhere, Category = "Quest Asset")
	TSubclassOf<class UQuestBlueprint> ParentClass;
};
