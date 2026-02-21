// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraph.h"

#include "SMGraphK2.generated.h"

class USMBlueprint;
struct FSMNode_Base;

UCLASS()
class SMSYSTEMEDITOR_API USMGraphK2 : public UEdGraph
{
	GENERATED_UCLASS_BODY()
	
	virtual ~USMGraphK2() override;

	/** Checks if any of the root nodes of this graph are wired to a logic pin. */
	virtual bool HasAnyLogicConnections() const;

	/** Checks the graph node owning this graph and returns the runtime state. */
	virtual FSMNode_Base* GetRuntimeNode() const { return nullptr; }

	/** Allow graphs to reset cached values. */
	virtual void ResetCachedValues();
	
	// UObject
	virtual void PostRename(UObject* OldOuter, const FName OldName) override;
	// ~UObject
	
	// UEdGraph
	virtual void NotifyGraphChanged() override;
	virtual void NotifyGraphChanged(const FEdGraphEditAction& Action) override;
	// ~UEdGraph

private:
	void OnBlueprintCacheCleared(const USMBlueprint* Blueprint);

protected:
	mutable TOptional<bool> bHasLogicConnectionsCached;

private:
	/** Handle for when UI related cache is cleared. */
	FDelegateHandle OnCacheClearedHandle;
};
