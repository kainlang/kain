/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */
 
#include "DataLayerBlueprintUtils.h"

FName UDataLayerBlueprintUtils::GetDataLayerLabel(const UDataLayerAsset* DataLayerAsset)
{
	if(DataLayerAsset)
	{
		return DataLayerAsset->GetFName();
	}

	return "";
}
