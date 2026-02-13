// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "CoreMinimal.h"
#include "KClonerModifierPreset.h"


class FAssetTypeActions_KClonerModifierPreset : public FAssetTypeActions_Base {
public:
  FAssetTypeActions_KClonerModifierPreset(
      EAssetTypeCategories::Type InAssetCategory)
      : MyAssetCategory(InAssetCategory) {}

  // IAssetTypeActions interface
  virtual FText GetName() const override {
    return NSLOCTEXT("AssetTypeActions",
                     "AssetTypeActions_KClonerModifierPreset",
                     "K-Cloner Modifier Preset");
  }
  virtual FColor GetTypeColor() const override {
    return FColor(255, 165, 0);
  } // Orange
  virtual UClass *GetSupportedClass() const override {
    return UKClonerModifierPreset::StaticClass();
  }
  virtual uint32 GetCategories() override { return MyAssetCategory; }
  // End of IAssetTypeActions interface

private:
  EAssetTypeCategories::Type MyAssetCategory;
};
