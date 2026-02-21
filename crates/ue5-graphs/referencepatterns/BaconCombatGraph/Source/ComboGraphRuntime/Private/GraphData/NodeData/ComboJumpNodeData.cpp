// © 2025 mrbaconvn. All Rights Reserved.

#include "GraphData/NodeData/ComboJumpNodeData.h"
#include "ComboGraphAsset.h" 
#include "GraphInstance/ComboGraphInstance.h"
#include "GraphData/ComboGraphData.h"
#include "GraphData/NodeData/ComboPortalNodeData.h"

const UComboGraphNodeData* UComboJumpNodeData::ExecuteNode(UComboGraphInstance* GraphInstance) const
{
	if (!IsValid(GraphInstance) || !GraphInstance->IsValidInstance())
	{
		return nullptr;
	} 

	TArray<UComboPortalNodeData*> PortalNodes = GraphInstance->GetComboGraphAsset()->GraphData->GetListNodeData<UComboPortalNodeData>(); 
	for (UComboPortalNodeData* PortalNode : PortalNodes)
	{
		if (PortalNode->PortalTag == PortalTag)
		{
			return PortalNode;
		}
	} 

	// We can't find the portal node so it will stuck here
	return nullptr;
}
