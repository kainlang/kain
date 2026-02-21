// © 2025 mrbaconvn. All Rights Reserved.

#include "ComboGraphAsset.h"
#include "UObject/ObjectSaveContext.h"

void UComboGraphAsset::PreSave(FObjectPreSaveContext SaveContext)
{
	PreSaveDelegate.Broadcast(this);
	Super::PreSave(SaveContext);
	PostSaveDelegate.Broadcast(this);
}
