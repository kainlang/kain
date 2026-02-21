/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once
#include "DialogueLine.generated.h"

USTRUCT(BlueprintType)
struct FDialogueLine
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Dialogue Line")
	FString Speaker;

	UPROPERTY(BlueprintReadWrite, Category="Dialogue Line")
	FString Text;
};
