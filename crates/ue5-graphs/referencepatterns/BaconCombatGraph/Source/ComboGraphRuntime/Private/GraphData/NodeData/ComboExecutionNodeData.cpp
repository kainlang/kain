// © 2025 mrbaconvn. All Rights Reserved.

#include "GraphData/NodeData/ComboExecutionNodeData.h"
#include "Action/ComboActionPass.h"
#include "GraphData/ComboGraphData.h"
#include "GraphInstance/ComboGraphInstance.h" 

const UComboGraphNodeData* UComboExecutionNodeData::ExecuteNode(UComboGraphInstance* GraphInstance) const
{
	if (!IsValid(GraphInstance))
	{
		return nullptr;
	}
	
	bool bExecuteSuccess = true;
	for (UComboActionPass* ActionPass : ExecutionPasses)
	{
		if (!IsValid(ActionPass))
		{
			continue;
		}

		if (!ActionPass->ExecutePass(GraphInstance))
		{
			bExecuteSuccess = false;
			if (bBreakWhenPassFailed)
			{
				break;
			}
		}
	}

	return GetNextOutputNodeByPinIndex(bExecuteSuccess ? 0 : 1);
}
