// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SMTextGraphEditorSettings.generated.h"

UCLASS(MinimalAPI, config = Editor, defaultconfig)
class USMTextGraphEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	USMTextGraphEditorSettings();

	/**
	 * When an object is placed in a text graph this function will be dynamically found from the object and executed.
	 * The function should be pure and return only text.
	 *
	 * This is dynamically looked up during run-time. If empty no function is looked up.
	 *
	 * ToText serialization options on a text graph will always overwrite this global setting.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Text Conversion")
	FName ToTextDynamicFunctionName;
};
