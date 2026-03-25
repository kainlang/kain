/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "ElevenLabsEnums.h"
#include "LineDurationEnum.h"
#include "UObject/Object.h"
#include "DialogAssetEditorSettings.generated.h"

/**
 * 
 */
UCLASS(config = Engine, defaultconfig)
class DIALOGASSETEDITOR_API UDialogAssetEditorSettings : public UObject
{
	   GENERATED_BODY()
public:
    UDialogAssetEditorSettings();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category= "Dialogue editor options")
	ELineDurationType DefaultDurationType;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category= "Dialogue editor options")
	bool bMissingVoiceSoundWarnings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category= "Dialogue editor options")
	bool bMissingBodyAnimationWarnings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category= "Dialogue editor options")
	bool bMissingFacialAnimationWarnings;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category="Text to speech options")
	FString ElevenLabsAPIKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category="Text to speech options")
	TEnumAsByte<EElevenLabsOutputFormatEnum> ElevenLabsOutputFormat = pcm_24000;
};
