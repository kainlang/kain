// © 2025 mrbaconvn. All Rights Reserved.

#include "GraphData/NodeData/ComboInputNodeData.h"   
#include "GraphData/ComboGraphData.h"
#include "GraphInstance/ComboGraphInstance.h"

const UComboGraphNodeData* UComboInputNodeData::ExecuteNode(UComboGraphInstance* GraphInstance) const
{ 
	if (!IsValid(GraphInstance) || !GraphInstance->IsValidInstance())
	{
		return nullptr;
	}

	FGameplayTag InputTag;
	bool bIsValidInput;
	if(UseInputHistory > 0)
	{
		bIsValidInput = GraphInstance->GetInputHistory(InputTag, UseInputHistory - 1);
	}
	else
	{
		bIsValidInput = GraphInstance->ConsumeCurrentInput(InputTag);
	}
	
	if (bIsValidInput == false)
	{
		return this;
	}

	for (int i = 0; i < InputTags.Num(); i++)
	{
		if (InputTag.MatchesTag(InputTags[i])) 
		{
			UComboGraphNodeData* NextNode = GetNextOutputNodeByPinIndex(i);
			if (IsValid(NextNode))
			{
				GraphInstance->SetSkipWindowConstraint(false);
				return NextNode;
			}
		}
	}

	// Go into default route if no input match 
	UComboGraphNodeData* DefaultNode = GetNextOutputNodeByPinIndex(InputTags.Num());
	if(IsValid(DefaultNode))
	{
		GraphInstance->SetSkipWindowConstraint(false);
	}
	
	return DefaultNode;
}
