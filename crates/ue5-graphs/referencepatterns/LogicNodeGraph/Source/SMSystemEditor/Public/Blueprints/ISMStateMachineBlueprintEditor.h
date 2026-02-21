// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "BlueprintEditor.h"

class UEdGraphNode;
class USMBlueprint;
class USMGraphK2Node_PropertyNode_Base;

class ISMStateMachineBlueprintEditor : public FBlueprintEditor
{
public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCreateGraphEditorCommands, ISMStateMachineBlueprintEditor*, TSharedPtr<FUICommandList>);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSelectedNodesChanged, TSharedPtr<ISMStateMachineBlueprintEditor>, const TSet<UObject*>& /* New Selection */);
	
	/** Return the loaded blueprint as a USMBlueprint. */
	virtual USMBlueprint* GetStateMachineBlueprint() const = 0;

	/** Return the currently selected property node. This isn't guaranteed to be valid unless used in a selected property command. */
	virtual const TWeakObjectPtr<USMGraphK2Node_PropertyNode_Base>& GetSelectedPropertyNode() const = 0;

	/** Checks if a single selected node is a valid property node. */
	virtual bool IsSelectedPropertyNodeValid(bool bCheckReadOnlyStatus = true) const = 0;

	/** Clear the current selection, select the new nodes, and optionally focus the selection which accounts for multiple nodes. */
	virtual void SelectNodes(const TSet<UEdGraphNode*>& InGraphNodes, bool bZoomToFit = false) = 0;
	
	/** Called when the selected nodes in the blueprint editor have changed. */
	virtual FOnSelectedNodesChanged& OnSelectedNodesChanged() = 0;
	
	/** Called when the state machine graph editor is creating the command list. */
	SMSYSTEMEDITOR_API static FOnCreateGraphEditorCommands OnCreateGraphEditorCommandsEvent;
};
