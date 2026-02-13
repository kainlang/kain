// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerModifierPresetFactory.h"
#include "KClonerModifierPreset.h"

UKClonerModifierPresetFactory::UKClonerModifierPresetFactory() {
  bCreateNew = true;
  bEditAfterNew = true;
  SupportedClass = UKClonerModifierPreset::StaticClass();
}

UObject *UKClonerModifierPresetFactory::FactoryCreateNew(
    UClass *InClass, UObject *InParent, FName InName, EObjectFlags Flags,
    UObject *Context, FFeedbackContext *Warn) {
  return NewObject<UKClonerModifierPreset>(InParent, InClass, InName, Flags);
}
