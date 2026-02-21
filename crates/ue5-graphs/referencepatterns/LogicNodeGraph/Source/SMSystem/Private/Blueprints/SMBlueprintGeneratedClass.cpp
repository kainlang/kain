// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Blueprints/SMBlueprintGeneratedClass.h"

USMBlueprintGeneratedClass::USMBlueprintGeneratedClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bHasStaleParentData = false;
#endif
}

void USMBlueprintGeneratedClass::PurgeClass(bool bRecompilingOnLoad)
{
	Super::PurgeClass(bRecompilingOnLoad);

	RootGuid.Invalidate();
#if WITH_EDITORONLY_DATA
	bHasStaleParentData = false;
#endif
}

void USMBlueprintGeneratedClass::SetRootGuid(const FGuid& Guid)
{
	RootGuid = Guid;
}


USMNodeBlueprintGeneratedClass::USMNodeBlueprintGeneratedClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}