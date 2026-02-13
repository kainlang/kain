// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerTargetComponent.h"

UKClonerTargetComponent::UKClonerTargetComponent() {
  PrimaryComponentTick.bCanEverTick = false;
  bWantsInitializeComponent = false;

#if WITH_EDITORONLY_DATA
  bVisualizeComponent = true;
#endif
}
