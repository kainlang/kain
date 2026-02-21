// © 2025 mrbaconvn. All Rights Reserved.


#include "GraphData/NodeData/ComboResetNodeData.h"
#include "GraphInstance/ComboGraphInstance.h"
#include "GraphData/ComboGraphData.h"

const UComboGraphNodeData* UComboResetNodeData::ExecuteNode(UComboGraphInstance* GraphInstance) const
{
	GraphInstance->ResetInstance(EComboResetReason::END_COMBO);
	return nullptr;
}
