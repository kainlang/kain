// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Assets/SMInstalledContentAsset.h"

void USMInstalledContentAsset::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	Ar << PAKFileHash;
}
