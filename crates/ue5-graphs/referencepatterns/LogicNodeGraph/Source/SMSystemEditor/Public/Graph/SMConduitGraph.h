// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMGraphK2.h"

#include "SMConduitGraph.generated.h"

class USMGraphK2Node_ConduitResultNode;
enum class ESMConditionalEvaluationType : uint8;

UCLASS(MinimalAPI)
class USMConduitGraph : public USMGraphK2
{
	GENERATED_UCLASS_BODY()

public:
	// USMGraphK2
	virtual bool HasAnyLogicConnections() const override;
	virtual FSMNode_Base* GetRuntimeNode() const override;
	// ~USMGraphK2
	
	/** Determine if the graph should be evaluated at runtime or can be statically known. */
	SMSYSTEMEDITOR_API ESMConditionalEvaluationType GetConditionalEvaluationType() const;

public:
	UPROPERTY()
	TObjectPtr<USMGraphK2Node_ConduitResultNode> ResultNode;
};
