// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMGraphK2.h"

#include "SMTransitionGraph.generated.h"

class USMGraphNode_TransitionEdge;
class USMGraphK2Node_TransitionResultNode;
enum class ESMConditionalEvaluationType : uint8;

UCLASS(MinimalAPI)
class USMTransitionGraph : public USMGraphK2
{
	GENERATED_UCLASS_BODY()

public:
	// USMGraphK2
	virtual bool HasAnyLogicConnections() const override;
	virtual FSMNode_Base* GetRuntimeNode() const override;
	// ~USMGraphK2

	/** Determine if the graph should be evaluated at runtime or can be statically known. */
	SMSYSTEMEDITOR_API ESMConditionalEvaluationType GetConditionalEvaluationType() const;

	/** If there is non-const logic which executes on a successful transition. */
	bool HasTransitionEnteredLogic() const;

	/** If this has the pre eval node and logic executing. */
	bool HasPreEvalLogic() const;

	/** If this has the post eval node and logic executing. */
	bool HasPostEvalLogic() const;

	/** If this has the initialize node and logic executing. */
	bool HasInitLogic() const;

	/** If this has the shut down node and logic executing. */
	bool HasShutdownLogic() const;

	template<typename T>
	bool HasNodeWithExecutionLogic() const;

	USMGraphNode_TransitionEdge* GetOwningTransitionNode() const;
	USMGraphNode_TransitionEdge* GetOwningTransitionNodeChecked() const;

public:
	UPROPERTY()
	TObjectPtr<USMGraphK2Node_TransitionResultNode> ResultNode;
};
