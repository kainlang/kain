// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "UObject/TextProperty.h" // Required for plugin packaging to work.
#include "SMTextGraphProperty.h"

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "SMExtendedPropertyHelpers.generated.h"

UCLASS(MinimalAPI)
class USMExtendedGraphPropertyHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Evaluate a text graph property. */ 
	UFUNCTION(BlueprintPure, Category = "Graph Property", meta = (NativeBreakFunc))
	static SMEXTENDEDRUNTIME_API void BreakTextGraphProperty(const FSMTextGraphProperty& GraphProperty, FText& Result);

	/** Convert an object to text by dynamically looking up a `ToText` function on the object during run-time. */
	UFUNCTION(BlueprintPure, Category = "Graph Property", BlueprintInternalUseOnly)
	static SMEXTENDEDRUNTIME_API FText ObjectToText(UObject* InObject, const FName InFunctionName);
};