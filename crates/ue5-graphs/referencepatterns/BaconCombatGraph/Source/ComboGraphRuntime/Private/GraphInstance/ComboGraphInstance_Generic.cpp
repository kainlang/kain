// © 2025 mrbaconvn. All Rights Reserved.

#include "GraphInstance/ComboGraphInstance_Generic.h"

UComboGraphInstance_Generic::UComboGraphInstance_Generic()
{
	
}

void UComboGraphInstance_Generic::SetInstanceActive(bool bActive)
{
	Super::SetInstanceActive(bActive);

	SetBlockProceedRequestCount(0);
	SetInputWindowRequestCount(0);
}

void UComboGraphInstance_Generic::RequestInputWindow(bool bToggle)
{
	int32 NewCount = bToggle ? 1 : -1;
	NewCount = FMath::Max(0, NewCount + GetInputWindowRequestCount());
	SetInputWindowRequestCount(NewCount);
}

void UComboGraphInstance_Generic::RequestBlockProceedGraph(bool bToggle)
{
	int32 NewCount = bToggle ? 1 : -1;
	NewCount = FMath::Max(0, NewCount + GetBlockProceedRequestCount());
	SetBlockProceedRequestCount(NewCount);
}
