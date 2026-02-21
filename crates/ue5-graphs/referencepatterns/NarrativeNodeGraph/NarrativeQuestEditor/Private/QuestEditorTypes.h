// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "EdGraph/EdGraphPin.h"
#include "QuestEditorTypes.generated.h"


UCLASS()
class UQuestEditorTypes : public UObject
{
	GENERATED_BODY()

public:

	UQuestEditorTypes(const FObjectInitializer& ObjectInitializer);

	static const FName PinCategory_SingleNode;
};
