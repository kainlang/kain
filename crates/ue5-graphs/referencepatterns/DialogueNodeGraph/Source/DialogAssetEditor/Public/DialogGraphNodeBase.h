/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "DialogAsset.h"
#include "DialogAssetEditorApp.h"
#include "EdGraph/EdGraphNode.h"
#include "DialogNodeType.h"
#include "DialogNodeInfoBase.h"
#include "DialogGraphNodeBase.generated.h"

UCLASS()
class UDialogGraphNodeBase : public UEdGraphNode {
    GENERATED_BODY()
    
public: // Our interface
    virtual UEdGraphPin* CreateDialogPin(EEdGraphPinDirection direction, FName name) { /* Must be overidden */ return nullptr; }
    virtual UEdGraphPin* CreateDefaultInputPin() { return nullptr; }
    virtual void CreateDefaultOutputPins() { /* Nothing to do by default */ }

    virtual void InitNodeInfo(UObject* outer) { /* None by default */ }
    virtual void SetNodeInfo(UDialogNodeInfoBase* nodeInfo) { /* None by default */ }
    virtual UDialogNodeInfoBase* GetNodeInfo() const { /* None by default */ return nullptr; }

    virtual EDialogNodeType GetDialogNodeType() const { return EDialogNodeType::Unknown; }

    virtual void OnPropertiesChanged() { /* Nothing to do by default */ }
};
