#include "CombatGraphInstance.h"
#include "CombatGraphNodeData.h"
#include "CombatGraphAsset.h"

bool UCombatGraphInstance::ResetInstance(ECombatGraphResetReason ResetReason)
{
	if (!IsValidInstance())
	{
		return false;
	}

	// Reset to root node
	CurrentNode = nullptr;
	bNeedProceed = false;

	// Broadcast reset event
	OnInstanceReset.Broadcast(ResetReason);

	return true;
}

bool UCombatGraphInstance::IsValidInstance() const
{
	return GraphAsset != nullptr && bInstanceActive;
}

const UCombatGraphAsset* UCombatGraphInstance::GetGraphAsset() const
{
	return GraphAsset;
}

const UCombatGraphNodeData* UCombatGraphInstance::GetCurrentNode() const
{
	return CurrentNode.Get();
}

bool UCombatGraphInstance::ConstructInstance(UCombatGraphAsset* InGraphAsset)
{
	if (InGraphAsset == nullptr)
	{
		return false;
	}

	GraphAsset = InGraphAsset;
	bInstanceActive = false;
	bNeedProceed = false;

	return true;
}

void UCombatGraphInstance::BeginDestroy()
{
	Super::BeginDestroy();

	// Clean up references
	CurrentNode = nullptr;
	GraphAsset = nullptr;
}

void UCombatGraphInstance::SetInstanceActive(bool bActive)
{
	bInstanceActive = bActive;

	if (!bActive)
	{
		// Reset state when deactivating
		CurrentNode = nullptr;
		bNeedProceed = false;
	}
}

bool UCombatGraphInstance::TryProceedGraph()
{
	if (!IsValidInstance())
	{
		return false;
	}

	// Check if proceed is blocked
	if (IsProceedBlocked())
	{
		return false;
	}

	// TODO: Implement graph traversal logic
	// This should find the next node based on current node and connections

	bNeedProceed = false;
	return true;
}

bool UCombatGraphInstance::SetCurrentNode(const UCombatGraphNodeData* ToNode)
{
	if (!IsValidInstance())
	{
		return false;
	}

	CurrentNode = ToNode;
	return true;
}

bool UCombatGraphInstance::IsProceedBlocked_Implementation() const
{
	// Default: never blocked
	// Override in Blueprint or C++ subclass for custom logic
	return false;
}