/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "QuestStartGraphNode.h"

UEdGraphPin* UQuestStartGraphNode::CreateQuestPin(EEdGraphPinDirection direction, FName name) {
    FName category = TEXT("Outputs");
    FName subcategory = TEXT(QUEST_START_PIN_CATEGORY);

    UEdGraphPin* pin = CreatePin(
        EEdGraphPinDirection::EGPD_Output,
        category,
        name
    );
    pin->PinType.PinSubCategory = subcategory;

    return pin;
}
