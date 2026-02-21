// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h" 
#include "ComboGraphSchema.generated.h"

class UComboGraphNodeData;
class UComboNodeBase;

UCLASS()
class COMBOGRAPHEDITOR_API UComboGraphSchema : public UEdGraphSchema 
{
    GENERATED_BODY()

public:
    UComboGraphSchema();

    virtual bool SafeDeleteNodeFromGraph(UEdGraph* Graph, UEdGraphNode* Node) const override;
    virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const override;
    virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override;
    virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;
    virtual void GetGraphContextActions(FGraphContextMenuBuilder& contextMenuBuilder) const override;
    virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* a, const UEdGraphPin* b) const override;

    virtual void CreateDefaultNodesForGraph(UEdGraph& graph) const override;

    UComboNodeBase* CreateAndInitComboNode(TSubclassOf<UComboGraphNodeData> NodeDataClass, UEdGraph* graph) const;
    UComboNodeBase* CreateComboNode(TSubclassOf<UComboGraphNodeData> NodeDataClass, UEdGraph* Graph) const;

private:
	TMap<TSubclassOf<UComboGraphNodeData>, TSubclassOf<UComboNodeBase>> NodeDataToEditorNodeMap; 
    TArray<TSubclassOf<UComboGraphNodeData>> BlacklistNodeCreation;
};