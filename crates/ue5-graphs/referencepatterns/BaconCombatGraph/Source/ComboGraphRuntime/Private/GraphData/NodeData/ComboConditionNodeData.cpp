// © 2025 mrbaconvn. All Rights Reserved.


#include "GraphData/NodeData/ComboConditionNodeData.h" 
#include "GraphInstance/ComboGraphInstance.h"
#include "GraphData/ComboGraphData.h"
#include "Action/ComboConditionPass.h"

#define TRUE_PIN_INDEX 0
#define FALSE_PIN_INDEX 1 

EComboConditionType UComboConditionNodeData::CheckCondition(UComboGraphInstance* GraphInstance) const
{
	int MatchedCount = 0;
	for (UComboConditionPass* ConditionPass : ConditionPasses)
	{
		if (!IsValid(ConditionPass))
		{
			continue;
		}

		bool bPassResult = ConditionPass->ExecutePass(GraphInstance);
		if (ConditionPass->bNegate)
		{
			bPassResult = !bPassResult;
		}

		if (bPassResult)
		{
			MatchedCount++;
		}
	}

	if (MatchedCount == ConditionPasses.Num())
	{
		return EComboConditionType::ESCCT_MATCH_ALL;
	}
	if (MatchedCount > 0)
	{
		return EComboConditionType::ESCCT_MATCH_ANY;
	}
	return EComboConditionType::ESCCT_MATCH_NONE;
}

bool UComboConditionNodeData::IsNodeMeetCondition(UComboGraphInstance* GraphInstance) const
{
	return CheckCondition(GraphInstance) == ConditionType;
}

const UComboGraphNodeData* UComboConditionNodeData::ExecuteNode(UComboGraphInstance* GraphInstance) const
{
	if (!IsValid(GraphInstance))
	{
		return nullptr;
	}

	if (IsNodeMeetCondition(GraphInstance))
	{
		return GetNextOutputNodeByPinIndex(TRUE_PIN_INDEX);
	}
	
	return GetNextOutputNodeByPinIndex(FALSE_PIN_INDEX);
} 