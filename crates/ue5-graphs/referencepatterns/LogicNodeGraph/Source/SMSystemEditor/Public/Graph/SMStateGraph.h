// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMGraphK2.h"

#include "SMStateGraph.generated.h"

class USMGraphNode_StateNode;
class USMGraphNode_StateNodeBase;
class USMGraphK2Node_StateEntryNode;

UCLASS(MinimalAPI)
class USMStateGraph : public USMGraphK2
{
	GENERATED_UCLASS_BODY()

public:
	virtual USMGraphNode_StateNodeBase* GetOwningStateNode() const;

	// USMGraphK2
	virtual FSMNode_Base* GetRuntimeNode() const override;
	// ~USMGraphK2
	
public:
	UPROPERTY()
	TObjectPtr<USMGraphK2Node_StateEntryNode> EntryNode;
};
