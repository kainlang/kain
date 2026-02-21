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
#include "QuestAssetEditorSettings.generated.h"

/**
 * 
 */
UCLASS(config = Engine, defaultconfig)
class QUESTASSETEDITOR_API UQuestAssetEditorSettings : public UObject
{
	   GENERATED_BODY()
public:
    UQuestAssetEditorSettings();

	//UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category= "Quest editor options")
	//bool bForFutureUse;
	
};
