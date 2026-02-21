// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "DialogueAssetFactory.generated.h"

/**
 * 
 */
UCLASS()
class UDialogueAssetFactory : public UFactory
{
	GENERATED_BODY()
	
	UDialogueAssetFactory();

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool CanCreateNew() const override;
	virtual FString GetDefaultNewAssetName() const override;
	virtual FText GetDisplayName() const override;
	// End of UFactory interface

	UPROPERTY(EditAnywhere, Category = "Dialogue Asset")
	TSubclassOf<class UDialogueBlueprint> ParentClass;

public:

	//If set, the factory will use this legacy asset as a template for the new asset
	UPROPERTY()
	TObjectPtr<class UDialogueAsset> LegacyAsset;

};
