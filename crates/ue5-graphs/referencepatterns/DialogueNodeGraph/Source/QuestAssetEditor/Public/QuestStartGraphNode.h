/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "QuestGraphNodeBase.h"
#include "QuestStartGraphNode.generated.h"

UCLASS()
class UQuestStartGraphNode : public UQuestGraphNodeBase {
    GENERATED_BODY()

public: // UDialogStartGraphNode interface
    virtual FText GetNodeTitle(ENodeTitleType::Type titalType) const override { return FText::FromString("Quest Start"); }
    virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(FColor::Red); }
    virtual bool CanUserDeleteNode() const override { return false; }

public: // UDialogGraphNodeBase interface
    virtual UEdGraphPin* CreateQuestPin(EEdGraphPinDirection direction, FName name) override;

    virtual EQuestNodeType GetQuestNodeType() const override { return EQuestNodeType::QuestStartNode; }
};
