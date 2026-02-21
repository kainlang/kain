// © 2025 mrbaconvn. All Rights Reserved.
 
#include "GraphInstance/ComboGraphInstance.h"
#include "ComboGraphAsset.h"
#include "ComboManager/ComboManagerComponent.h"
#include "GraphData/ComboGraphData.h"   
#include "GraphData/NodeData/ComboExecutionNodeData.h"
#include "GraphData/NodeData/ComboRootNodeData.h" 

#define MAX_INPUT_HISTORY_STACK 3

bool UComboGraphInstance::ConstructInstance(UComboGraphAsset* InComboGraphAsset)
{
	ManagerComponent = Cast<UComboManagerComponent>(GetOuter());
	ComboGraphAsset = InComboGraphAsset; 
	ResetInstance();

	if(IsValid(ComboGraphAsset))
	{
		ComboGraphAsset->PostSaveDelegate.AddUObject(this, &UComboGraphInstance::OnGraphChanged);
	}
	
	return IsValidInstance();
}

void UComboGraphInstance::BeginDestroy()
{
	UObject::BeginDestroy();
	
	SetInstanceActive(false);
	if(IsValid(ComboGraphAsset))
	{
		ComboGraphAsset->PreSaveDelegate.RemoveAll(this);
	}
}

void UComboGraphInstance::SetInstanceActive(bool bActive)
{
	ResetInstance();
	bInstanceActive = bActive;
}

void UComboGraphInstance::OnGraphChanged(UComboGraphAsset* InComboGraphAsset)
{
	ResetInstance();
}

const UComboGraphAsset* UComboGraphInstance::GetComboGraphAsset() const
{
	return ComboGraphAsset;
}

const UComboGraphNodeData* UComboGraphInstance::GetCurrentNode() const
{
	return CurrentNode.Get();
}

bool UComboGraphInstance::IsValidInstance() const
{
	return IsValid(ComboGraphAsset) && IsValid(ComboGraphAsset->GraphData) && IsValid(GetManagerComponent());
} 

bool UComboGraphInstance::ResetInstance(EComboResetReason ResetReason)
{
	CurrentInput = FGameplayTag();
	InputHistory.Init(FGameplayTag(), MAX_INPUT_HISTORY_STACK);
	
	bHasPendingInputWindow = false;
	bSkipWindowConstraint = true; 
	bNeedProceed = false;

	OnInstanceReset.Broadcast(ResetReason);
	
	if(!IsValidInstance())
	{
		return false;
	}  
	
	return SetCurrentNode(ComboGraphAsset->GraphData->GetNodeData<UComboRootNodeData>());
}

bool UComboGraphInstance::TryProceedGraph()
{
	if (!IsValidInstance() || IsProceedBlocked())
	{
		return false;
	}

	UComboGraphData* GraphData = ComboGraphAsset->GraphData;
	if (!IsValid(GraphData))
	{
		return false;
	}

	bool bReset = false;
	bool bTravelSuccess = false;

	SetCurrentNode(CurrentNode.Get());

	while (CurrentNode.IsValid())
	{
		const UComboGraphNodeData* NextNode = CurrentNode->ExecuteNode(this);
		if (NextNode == CurrentNode)
		{
			// The node wanted to stay in the same node, we should not proceed
			break;
		}
		
		if (!IsValid(NextNode))
		{
			if (!ComboGraphAsset->bResetWhenGraphBlocked)
			{
				break;
			}

			FGameplayTag LastInput;
			GetInputHistory(LastInput);

			if (CurrentInput.IsValid())
			{
				LastInput = CurrentInput;
			}

			if (!bReset && LastInput.IsValid() && !bTravelSuccess)
			{
				ResetInstance(EComboResetReason::RETRY);
				CurrentInput = LastInput;
				bReset = true;
			}
			else
			{
				ResetInstance(bTravelSuccess ? EComboResetReason::END_COMBO : EComboResetReason::RESET);
				break;
			}
		}
		else
		{
			// TODO: For now we only count a successful travel if we pass through a combo node
			// This can be expanded to include other types of nodes but not for now
			bTravelSuccess |= NextNode->IsA<UComboExecutionNodeData>();
			SetCurrentNode(NextNode);
		}
	}

	if(bTravelSuccess)
	{
		bHasPendingInputWindow = false;
	}

	return bTravelSuccess;
}

bool UComboGraphInstance::SetCurrentNode(const UComboGraphNodeData* ToNode)
{ 
	if(!IsValidInstance())
	{
		return false;
	} 
	
	CurrentNode = ToNode;

	UComboManagerComponent* Manager = GetManagerComponent();
	if (IsValid(Manager) && Manager->OnComboNodeExecuted.IsBound())
	{
		Manager->OnComboNodeExecuted.Broadcast(CurrentNode.Get());
	}
	
	return true;
}

void UComboGraphInstance::SetInputWindowRequestCount(int32 InCount)
{
	if (InputWindowRequestCount == InCount)
	{
		return;
	}
	
	int32 OldCount = InputWindowRequestCount;
	InputWindowRequestCount = InCount;
	
	if(InputWindowRequestCount <= 0 && bHasPendingInputWindow)
	{
		ResetInstance(); 
	}

	bHasPendingInputWindow = (InputWindowRequestCount > 0);
	OnInputWindowUpdate.Broadcast(InputWindowRequestCount > 0);
}

void UComboGraphInstance::SetBlockProceedRequestCount(int32 InCount)
{
	if (BlockProceedRequestCount == InCount)
	{
		return;
	}
	
	int32 OldCount = BlockProceedRequestCount;
	BlockProceedRequestCount = InCount;

	if(BlockProceedRequestCount <= 0)
	{ 
		if(CurrentInput.IsValid())
		{
			bNeedProceed = true;
		}
		if(!bHasPendingInputWindow)
		{
			ResetInstance();
		}
	}
}

bool UComboGraphInstance::TriggerInput(FGameplayTag InInputTag)
{
	if(!IsValidInstance())
	{
		return false;
	}

	if(!IsInInputWindow() && !IsProceedBlocked())
	{
		// This prevents stuck when input window was not introduced
		ResetInstance();
	}
	
	if(IsInInputWindow())
	{
		CurrentInput = InInputTag;
		bNeedProceed = true;
	} 

	return true;
}

bool UComboGraphInstance::ConsumeCurrentInput(FGameplayTag& OutInputTag)
{
	OutInputTag = CurrentInput;
	CurrentInput = FGameplayTag(); 
	
	for (int i = InputHistory.Num() - 1; i >= 1; i--)
	{
		InputHistory[i] = InputHistory[i-1]; 
	}
	InputHistory[0] = OutInputTag;
	return OutInputTag.IsValid();
}

bool UComboGraphInstance::GetInputHistory(FGameplayTag& OutInputTag, int InputHistoryIndex) const
{
	if(!InputHistory.IsValidIndex(InputHistoryIndex))
	{
		return false;
	}
	
	OutInputTag = InputHistory[InputHistoryIndex];
	return OutInputTag.IsValid();
}

void UComboGraphInstance::SetSkipWindowConstraint(bool bSkip)
{
	bSkipWindowConstraint = bSkip;
}

bool UComboGraphInstance::IsProceedBlocked_Implementation() const
{
	return GetBlockProceedRequestCount() > 0;
}

bool UComboGraphInstance::IsInInputWindow_Implementation() const
{
	return bSkipWindowConstraint || GetInputWindowRequestCount() > 0;
}

UComboManagerComponent* UComboGraphInstance::GetManagerComponent() const
{
	return ManagerComponent;
} 