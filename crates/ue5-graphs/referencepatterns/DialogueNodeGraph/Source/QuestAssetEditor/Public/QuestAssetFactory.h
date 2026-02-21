/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "QuestAssetFactory.generated.h"

UCLASS()
class UQuestAssetFactory : public UFactory {
    GENERATED_BODY()

public:
    UQuestAssetFactory(const FObjectInitializer& objectInitializer);

public: // UFactory interface
    virtual UObject* FactoryCreateNew(UClass* uclass, UObject* inParent, FName name, EObjectFlags flags, UObject* context, FFeedbackContext* warn) override;
    virtual bool CanCreateNew() const override;
};
