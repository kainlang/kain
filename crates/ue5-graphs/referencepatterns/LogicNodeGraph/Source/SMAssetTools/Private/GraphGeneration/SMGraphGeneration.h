// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ISMGraphGeneration.h"

class USMNodeInstance;

class FSMGraphGeneration final : public ISMGraphGeneration
{
public:
	virtual ~FSMGraphGeneration() override {}

	virtual USMGraphNode_StateNodeBase* CreateStateNode(USMBlueprint* InBlueprint, const FCreateStateNodeArgs& InStateArgs) override;
	virtual USMGraphNode_TransitionEdge* CreateTransitionEdge(USMBlueprint* InBlueprint, const FCreateTransitionEdgeArgs& InTransitionArgs) override;
	virtual USMStateInstance* CreateStateStackInstance(USMGraphNode_StateNode* InStateNode, const FCreateStateStackArgs& InStateStackArgs) override;
	virtual bool SetNodePropertyValue(USMGraphNode_Base* InGraphNode, const FSetNodePropertyArgs& InPropertyArgs) override;

private:
	static UClass* GetGraphNodeClassFromInstanceType(TSubclassOf<USMNodeInstance> InNodeClass);
};