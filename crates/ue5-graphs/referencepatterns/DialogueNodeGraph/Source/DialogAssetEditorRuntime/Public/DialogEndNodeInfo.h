/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "DialogNodeInfoBase.h"
#include "DialogEndNodeInfo.generated.h"

UENUM(BlueprintType)
enum class EDialogNodeAction : uint8 {
    None,
    StartQuest // ActionData is the QuestId
};

UCLASS(BlueprintType)
class DIALOGASSETEDITORRUNTIME_API UDialogEndNodeInfo : public UDialogNodeInfoBase {
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category="End Node")
    EDialogNodeAction Action = EDialogNodeAction::None;

    UPROPERTY(EditAnywhere, Category="End Node")
    FString ActionData = TEXT("");
};
