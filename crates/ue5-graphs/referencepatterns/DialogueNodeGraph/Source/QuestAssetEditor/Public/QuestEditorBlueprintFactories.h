/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */
#pragma once

#include "CoreMinimal.h"
#include "Factories/BlueprintFactory.h"
#include "QuestEditorBlueprintFactories.generated.h"

/**
 * 
 */

UCLASS()
class QUESTASSETEDITOR_API UQuestTaskFactory : public UBlueprintFactory
{
	GENERATED_BODY()

	UQuestTaskFactory();

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};


UCLASS()
class QUESTASSETEDITOR_API UDialogueMasterStatisticActionFactory : public UFactory
{
	GENERATED_BODY()
	
	UDialogueMasterStatisticActionFactory();

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
		FFeedbackContext* Warn) override;
	virtual bool CanCreateNew() const override;
	virtual FString GetDefaultNewAssetName() const override;
	virtual FText GetDisplayName() const override;
	
};