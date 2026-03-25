/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "QuestNodeInfoBase.generated.h"

/**
 * 
 */
UCLASS()
class DIALOGASSETEDITORRUNTIME_API UQuestNodeInfoBase : public UObject
{
	GENERATED_BODY()

public:
	virtual FName GetID() const { return NAME_None; }
};
