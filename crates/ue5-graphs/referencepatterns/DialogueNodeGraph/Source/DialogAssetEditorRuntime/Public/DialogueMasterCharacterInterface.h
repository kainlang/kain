/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "DialogueMasterCharacterInterface.generated.h"

UINTERFACE(BlueprintType)
class UDialogueMasterCharacterInterface : public UInterface
{
	GENERATED_BODY()
};

class DIALOGASSETEDITORRUNTIME_API IDialogueMasterCharacterInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Dialogue Master - Character Interface")
	bool canEnterDialogue(UObject * UserObject);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Dialogue Master - Character Interface", meta = (ReturnDisplayName = "Wait time"))
	float onBeforeEnterDialogue(UObject * UserObject);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Dialogue Master - Character Interface")
	void onDialogueEnd(UObject * UserObject);
};