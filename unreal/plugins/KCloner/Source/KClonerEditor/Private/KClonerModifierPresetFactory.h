// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "KClonerModifierPresetFactory.generated.h"

UCLASS()
class UKClonerModifierPresetFactory : public UFactory {
  GENERATED_BODY()

public:
  UKClonerModifierPresetFactory();

  // UFactory interface
  virtual UObject *FactoryCreateNew(UClass *InClass, UObject *InParent,
                                    FName InName, EObjectFlags Flags,
                                    UObject *Context,
                                    FFeedbackContext *Warn) override;
  // End of UFactory interface
};
