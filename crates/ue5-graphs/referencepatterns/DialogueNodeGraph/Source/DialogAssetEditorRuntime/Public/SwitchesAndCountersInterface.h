/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once
#include <Runtime/CoreUObject/Public/UObject/Interface.h>
#include "SwitchesAndCountersInterface.generated.h"

UINTERFACE(BlueprintType)
class USwitchAndCounterPrerequisitesInterface : public UInterface
{
	GENERATED_BODY()
};

class DIALOGASSETEDITORRUNTIME_API ISwitchAndCounterPrerequisitesInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Switch and Counter prerequisites interface")
	bool getSwitchValue(FName switchName);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Switch and Counter prerequisites interface")
	int getCounterValue(FName counterName);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Switch and Counter prerequisites interface")
	void setSwitchValue(FName switchName, bool value);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Switch and Counter prerequisites interface")
	void setCounterValue(FName counterName, int value);
};