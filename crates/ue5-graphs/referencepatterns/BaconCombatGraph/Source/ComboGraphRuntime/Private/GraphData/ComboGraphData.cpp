// © 2025 mrbaconvn. All Rights Reserved.


#include "GraphData/ComboGraphData.h"
#include "GraphData/NodeData/ComboPortalNodeData.h"

UComboPinData* UComboGraphData::FindPinById(FGuid PinId)
{
	for (UComboGraphNodeData* Node : Nodes)
	{
		for (UComboPinData* Pin : Node->InputPins)
		{
			if (Pin->PinId == PinId)
			{
				return Pin;
			}
		}

		for (UComboPinData* Pin : Node->OutputPins)
		{
			if (Pin->PinId == PinId)
			{
				return Pin;
			}
		}
	}

	return nullptr;
}

UComboGraphNodeData* UComboGraphNodeData::GetNextOutputNodeByPinIndex(int OutputPinIndex) const
{
	if (!OutputPins.IsValidIndex(OutputPinIndex))
	{
		return nullptr;
	}

	UComboPinData* OutputPin = OutputPins[OutputPinIndex];
	if (!IsValid(OutputPin))
	{
		return nullptr;
	}

	if (OutputPin->ConnectToPins.Num() == 0)
	{
		return nullptr;
	}

	return OutputPin->ConnectToPins[0]->Parent;
} 

const UComboGraphNodeData* UComboGraphNodeData::ExecuteNode(UComboGraphInstance* GraphInstance) const
{
	return GetNextOutputNodeByPinIndex(0);
}
