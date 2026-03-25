/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "StepTypeEnum.h"
#include "EdGraph/EdGraph.h"
#include "QuestGraphSchema.generated.h"

UCLASS()
class UQuestGraphSchema : public UEdGraphSchema {
    GENERATED_BODY()

private:
    void ListDerivedClasses(UClass* BaseClass, TArray<UClass*> &SubClasses) const;
    
public:
    virtual void GetGraphContextActions(FGraphContextMenuBuilder& contextMenuBuilder) const override;
    virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* a, const UEdGraphPin* b) const override;
	virtual void CreateDefaultNodesForGraph(UEdGraph& graph) const override;

    void DeleteSelectedNodes(UEdGraph * Graph);
};

USTRUCT()
struct FNewQuestNodeAction : public FEdGraphSchemaAction {
    GENERATED_BODY()

    UClass * AutoFillTaskClass = nullptr;
    EStepType stepType;
public:
    FNewQuestNodeAction() {}
    FNewQuestNodeAction(UClass* classTemplate, FText inNodeCategory, FText inMenuDesc, FText inToolTip, const int32 inGrouping, UClass *autoFillTaskClass = nullptr, EStepType stepType = NORMAL)
        : FEdGraphSchemaAction(inNodeCategory, inMenuDesc, inToolTip, inGrouping), _classTemplate(classTemplate)
    {
        AutoFillTaskClass = autoFillTaskClass;
        this->stepType = stepType;
    }

    virtual UEdGraphNode* PerformAction(UEdGraph* parentGraph, UEdGraphPin* fromPin, const FVector2D location, bool bSelectNewNode = true);

protected:
    UClass* _classTemplate = nullptr;
};

USTRUCT()
struct FNewCommentNodeAction : public FEdGraphSchemaAction
{
    GENERATED_BODY()

    // Default constructor
    FNewCommentNodeAction()
        : FEdGraphSchemaAction()
    {
        
    }

    FNewCommentNodeAction(FText inMenuDesc, FText inToolTip)
        : FEdGraphSchemaAction(FText(), inMenuDesc, inToolTip, 0)
    {
        
    }

    virtual UEdGraphNode* PerformAction(UEdGraph* parentGraph, UEdGraphPin* fromPin, const FVector2D location, bool bSelectNewNode = true);
};