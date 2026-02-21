// © 2025 mrbaconvn. All Rights Reserved.


#include "AbilityContext.h"   

TArray<FSoftObjectPath> FAbilityContext::GetPreviewAssets() const
{
	return {AbilityClass.GetUniqueID()};
}

FString FAbilityContext::GetPreviewText() const
{
	return FString::Printf(TEXT("Ability: %s"), *AbilityClass.GetAssetName());
}
