// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "SMInstalledContentAsset.generated.h"

/**
 * An asset which may be created when content is installed and is used to determine installed content versioning.
 */
UCLASS(NotBlueprintable, NotBlueprintType, HideDropdown)
class USMInstalledContentAsset : public UObject
{
	GENERATED_BODY()

public:
	// UObject
	virtual void Serialize(FArchive& Ar) override;
	virtual bool IsEditorOnly() const override final { return true; }
	// ~UObject
	
public:
	FMD5Hash PAKFileHash;
};
