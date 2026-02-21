// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMGraphK2.h"

#include "SMPropertyGraph.generated.h"

class UEdGraphPin;
class USMGraphK2Node_PropertyNode_Base;
struct FSMGraphProperty_Base;

UCLASS()
class SMSYSTEMEDITOR_API USMPropertyGraph : public USMGraphK2
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TObjectPtr<USMGraphK2Node_PropertyNode_Base> ResultNode;

	// UObject
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostEditUndo() override;
	// ~UObject
	
	/** Temporarily set during graph initialization. */
	FSMGraphProperty_Base* TempGraphProperty;

	/**
	 * Called from owning state machine graph node.
	 *
	 * @param bModify Conditionally recompile blueprint.
	 * @param bSetFromPinFirst If true will call SetPropertyDefaultsFromPin prior to SetPinValueFromPropertyDefaults.
	 * This is needed largely for undo support. When creating initially it should not be called so property defaults can be read.
	 */
	virtual void RefreshProperty(bool bModify = true, bool bSetFromPinFirst = true);

	/** Delete all nodes and recreate default nodes. */
	virtual void ResetGraph();

	/** Configure if the graph is editable and update the slate node. */
	virtual void SetUsingGraphToEdit(bool bValue, bool bModify = true);

	/** If the graph is the primary source of data. */
	virtual bool IsGraphBeingUsedToEdit() const { return bEditable; }

	/** Called before setting the edit status during RefreshProperty. */
	virtual bool CanSetEditStatusFromReadOnlyVariable() const { return true; }
	
	/** Toggles the property edit value and updates the blueprint. */
	virtual void ToggleGraphPropertyEdit();

	virtual void SetPropertyOnGraph(FProperty* Property);
	virtual void SetFunctionOnGraph(UFunction* Function);

	/**
	 *	Called after we manually clone nodes into this graph.
	 *	@param OldGraph The original graph used in the clone, available in case there are properties that need to be copied.
	 */
	virtual void OnGraphManuallyCloned(USMPropertyGraph* OldGraph);

	/** Called when this graph is being deleted. */
	virtual void OnGraphDeleted() {}
	
	/** Remove any nodes that aren't connected to the ResultNode. */
	void PruneDisconnectedNodes();

	/** If the variable property is configured for read only. */
	bool IsVariableReadOnly() const { return bVariableIsReadOnly; }

	/**
	 * If the graph is supposed to be editable. This could be what the user wants, but bEditable could still be false such
	 * as when the variable is set to read only.
	 */
	bool IsGraphEditDesired() const { return bUsingGraphToEdit; }

	/** True when this graph is responsible for the property placement, such as if the user dragged a property to the title,
	 * and the title is asking the graph to place the property. */
	bool IsPropertyBeingManuallyPlacedOnGraph() const { return bIsManuallyPlacingPropertyOnGraph; }

	/** Allow the graph to be duplicated. */
	void SetAllowDuplication(bool bNewValue) { bAllowDuplication = bNewValue; }

	/** If the graph is currently set to allow duplication. */
	bool AllowsDuplication() const { return bAllowDuplication; }

	TSet<UEdGraphPin*> PreventConnections;

protected:
	/** When ResetGraph is called re-init property nodes. */
	uint8 bInitPropertyNodesOnReset: 1;
	
private:
	UPROPERTY()
	uint8 bUsingGraphToEdit: 1;

	UPROPERTY()
	uint8 bVariableIsReadOnly: 1;

	uint8 bIsManuallyPlacingPropertyOnGraph: 1;

	uint8 bAllowDuplication: 1;
};
