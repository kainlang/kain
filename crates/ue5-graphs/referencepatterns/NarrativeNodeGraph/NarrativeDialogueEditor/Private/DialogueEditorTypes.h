// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "EdGraph/EdGraphPin.h"
#include "DialogueEditorTypes.generated.h"


UCLASS()
class UDialogueEditorTypes : public UObject
{
	GENERATED_BODY()

public:

	UDialogueEditorTypes(const FObjectInitializer& ObjectInitializer);

	static const FName PinCategory_SingleNode;
};
