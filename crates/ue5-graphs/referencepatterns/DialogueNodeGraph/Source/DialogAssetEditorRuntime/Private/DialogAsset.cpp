/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "DialogAsset.h"

#include "DialogNodeInfo.h"
#include "UObject/ObjectSaveContext.h"

void UDialogAsset::PreSave(FObjectPreSaveContext saveContext) { 
	Super::PreSave(saveContext);
	if (_onPreSaveListener) {
        _onPreSaveListener();
    }
}

void UDialogAsset::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	UObject::PostDuplicate(DuplicateMode);

	// Update GUIDs & UserIDs
	if(DuplicateMode == EDuplicateMode::Normal)
	{
		AssetId = FGuid::NewGuid();

		if(Graph != nullptr)
		{
			for(UDialogRuntimeNode* node : Graph->Nodes)
			{
				if(node != nullptr)
				{
					if(node->NodeInfo != nullptr)
					{
						UDialogNodeInfo * info = Cast<UDialogNodeInfo>(node->NodeInfo);
						if(info != nullptr)
						{
							info->NodeId = FGuid::NewGuid();
							info->UserId = NAME_None;
						}
					}
				}
			}
		}
	}
}

#if WITH_EDITOR
void UDialogAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if(PropertyName.IsEqual("UniqueIdentifierOverride"))
	{
		PlayerActor.UniqueIdentifierOverride = FText::AsCultureInvariant(PlayerActor.UniqueIdentifierOverride);

		for(FActorInfo& info : DialogActors)
		{
			info.UniqueIdentifierOverride = FText::AsCultureInvariant(info.UniqueIdentifierOverride);
		}
	}
}
#endif
