// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"

#include "SMContentAssetFactory.generated.h"

UCLASS(HideCategories = Object, MinimalAPI)
class USMContentAssetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// UFactory
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
	                                  FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
	                                  FFeedbackContext* Warn) override;
	virtual bool DoesSupportClass(UClass* Class) override;
	// ~UFactory

};
