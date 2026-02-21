// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/RootNodes/SMGraphK2Node_RootNode.h"

#include "SMGraphK2Node_RuntimeNodeContainer.generated.h"

class FCompilerResultsLog;
class FSMKismetCompilerContext;
class UK2Node_CallFunction;
class USMGraphK2Node_FunctionNode_NodeInstance;
enum class ESMExposedFunctionExecutionType : uint8;
struct FSMNode_Base;

UCLASS(MinimalAPI, Abstract)
class USMGraphK2Node_RuntimeNode_Base : public USMGraphK2Node_RootNode
{
	GENERATED_BODY()

public:
	/**
	 * Return the runtime node given a container object. When called from a container pass nullptr.
	 * When called from a reference the container must be valid or it will fail.
	 */
	virtual FSMNode_Base* GetRunTimeNodeFromContainer(USMGraphK2Node_RuntimeNodeContainer* InContainer) { return nullptr; }

	/**
	 * If this root node is compatible with an instance function graph node.
	 * Such as OnStateBegin (USMGraphK2Node_StateEntryNode) is compatible with USMGraphK2Node_StateInstance_Begin.
	 * Called from GetGraphExecutionType.
	 */
	virtual bool IsCompatibleWithInstanceGraphNodeClass(TSubclassOf<USMGraphK2Node_FunctionNode_NodeInstance> InGraphNodeClass) const { return false; }

	/** Return the immediate connected node instance function if present. */
	SMSYSTEMEDITOR_API USMGraphK2Node_FunctionNode_NodeInstance* GetConnectedNodeInstanceFunction() const;
	
	/** Return the immediate connected node instance function only if it is completely valid for an optimization pass. */
	SMSYSTEMEDITOR_API USMGraphK2Node_FunctionNode_NodeInstance* GetConnectedNodeInstanceFunctionIfValidForOptimization() const;
	
	/** Check the connected pins to the 'GetCorrectEntryPin' pin and determine the execution type.  */
	SMSYSTEMEDITOR_API ESMExposedFunctionExecutionType GetGraphExecutionType() const;

protected:
	/** Find the correct initial pin. Default implementation finds the 'Then' pin. */
	SMSYSTEMEDITOR_API virtual UEdGraphPin* GetCorrectEntryPin() const;

	/** Find the expected output pin of the connected instance. Default returns 'Then' pin. */
	SMSYSTEMEDITOR_API virtual UEdGraphPin* GetCorrectNodeInstanceOutputPin(USMGraphK2Node_FunctionNode_NodeInstance* InInstance) const;

public:
	/** If this node counts for USMGraphK2::HasAnyLogicConnections(). */
	virtual bool IsConsideredForEntryConnection() const { return false; }

	/** Reset any cached values. Called by owning graph by default. */
	virtual void ResetCachedValues() { bFastPathEnabledCached.Reset(); }
	
	/** If this node avoids the BP graph. */
	SMSYSTEMEDITOR_API bool IsFastPathEnabled() const;

private:
	mutable TOptional<bool> bFastPathEnabledCached;
};

UCLASS(MinimalAPI)
class USMGraphK2Node_RuntimeNodeContainer : public USMGraphK2Node_RuntimeNode_Base
{
	GENERATED_UCLASS_BODY()
	
	// UEdGraphNode
	virtual void PrepareForCopying() override;
	virtual void PostPasteNode() override;
	virtual bool HasExternalDependencies(TArray<UStruct*>* OptionalOutput) const override;
	// ~UEdGraphNode

	// USMGraphK2Node_RuntimeNode_Base
	virtual FSMNode_Base* GetRunTimeNodeFromContainer(USMGraphK2Node_RuntimeNodeContainer* InContainer = nullptr) override
	{
		return GetRunTimeNodeChecked();
	}
	virtual bool IsConsideredForEntryConnection() const override { return true; }
	// ~USMGraphK2Node_RuntimeNode_Base
	
	virtual FSMNode_Base* GetRunTimeNode() { return nullptr; }
	FORCEINLINE FSMNode_Base* GetRunTimeNodeChecked() { FSMNode_Base* Node = GetRunTimeNode(); check(Node); return Node; }

	/**
	 * Helper to determine which run time node this graph node represents.
	 * Requires that the GraphNode contain a node derived from USMNode_Base.
	 */
	UScriptStruct* GetRunTimeNodeType() const;
	FStructProperty* GetRuntimeNodeProperty() const;

	// Assign a new runtime node Guid.
	void ForceGenerateNodeGuid();
	// Checks if a new Guid has been generated. This resets on every copy.
	bool HasNewNodeGuidGenerated() const { return bHasNodeGuidGeneratedForCopy; }

	/** Generated during compile so this container can be found by references when placed on the consolidated event graph. */
	UPROPERTY()
	FGuid ContainerOwnerGuid;
	
private:
	bool bHasNodeGuidGeneratedForCopy;
};

UCLASS(MinimalAPI)
class USMGraphK2Node_RuntimeNodeReference : public USMGraphK2Node_RuntimeNode_Base
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	SMSYSTEMEDITOR_API virtual void PostPasteNode() override;
	// ~UEdGraphNode

	// USMGraphK2Node_Base
	SMSYSTEMEDITOR_API virtual void PreConsolidatedEventGraphValidate(FCompilerResultsLog& MessageLog) override;
	// ~USMGraphK2Node_Base

	// USMGraphK2Node_RuntimeNode_Base
	virtual FSMNode_Base* GetRunTimeNodeFromContainer(USMGraphK2Node_RuntimeNodeContainer* InContainer) override
	{
		check(InContainer); return InContainer->GetRunTimeNodeChecked();
	}
	// ~USMGraphK2Node_RuntimeNode_Base
	
	void SyncWithContainer();

	/**
	 * Locates the runtime container node. This assumes the graph the reference node belongs to
	 * also has the container node nested at some level. Result should not be null except when processing
	 * StateMachineState nodes, as their container may not be generated yet.
	 */
	SMSYSTEMEDITOR_API virtual USMGraphK2Node_RuntimeNodeContainer* GetRuntimeContainer() const;
	SMSYSTEMEDITOR_API USMGraphK2Node_RuntimeNodeContainer* GetRuntimeContainerChecked() const;

	/** When true the state machine compiler won't automatically expand this node and will instead call CustomExpandNode or allow the engine to do it. */
	virtual bool HandlesOwnExpansion() const { return false; }
	
	/** Custom node expand. This occurs at an earlier stage than when the engine normally calls ExpandNode. */
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) {}
	
	UPROPERTY()
	FGuid RuntimeNodeGuid;

	/** Set during compile to match id generated in this reference's owning container. */
	UPROPERTY()
	FGuid ContainerOwnerGuid;
	
protected:
	/** Creates a function call and wires a guid struct member get to the function input. */
	SMSYSTEMEDITOR_API UK2Node_CallFunction* CreateFunctionCallWithGuidInput(UFunction* Function, FSMKismetCompilerContext& CompilerContext,
	                                                      const USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, const FProperty* NodeProperty, FName PinName = "Guid");

	SMSYSTEMEDITOR_API void GetMenuActions_Internal(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const;
};
