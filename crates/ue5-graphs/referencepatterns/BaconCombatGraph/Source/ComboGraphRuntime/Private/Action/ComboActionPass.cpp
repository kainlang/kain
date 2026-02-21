// © 2025 mrbaconvn. All Rights Reserved.


#include "Action/ComboActionPass.h" 

bool UComboActionPass::ExecutePass_Implementation(UComboGraphInstance* GraphInstance) const
{
	return false;
}

FString UComboActionPass::GetPassInformation_Implementation() const
{
	if(IsValid(GetClass()))
	{
		return GetClass()->GetName();
	}
	return "";
}

TArray<FSoftObjectPath> UComboActionPass::GetPreviewAssets() const
{
	return K2_GetPreviewAssets();
} 