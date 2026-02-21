// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMConduitInstance.h"

#include "SMConduit.h"

USMConduitInstance::USMConduitInstance() : Super(), bEvalGraphsOnInitialize(true), bEvalGraphsOnTransitionEval(true),
                                           bEvalWithTransitions(false), bCanEvaluate(true)
{
}

void USMConduitInstance::SetCanEvaluate(const bool bValue)
{
	SET_NODE_DEFAULT_VALUE(FSMConduit, bCanEvaluate, bValue);
}

bool USMConduitInstance::GetCanEvaluate() const
{
	GET_NODE_DEFAULT_VALUE(FSMConduit, bCanEvaluate);
}

bool USMConduitInstance::GetEvalWithTransitions() const
{
	GET_NODE_DEFAULT_VALUE(FSMConduit, bEvalWithTransitions);
}

void USMConduitInstance::SetEvalWithTransitions(const bool bValue)
{
	SET_NODE_DEFAULT_VALUE(FSMConduit, bEvalWithTransitions, bValue);
}
