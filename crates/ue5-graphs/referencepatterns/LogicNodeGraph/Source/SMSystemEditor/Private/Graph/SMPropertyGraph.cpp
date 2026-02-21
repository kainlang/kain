// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/SMPropertyGraph.h"

#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/SMGraphNode_Base.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "SMGraphProperty_Base.h"

#include "ScopedTransaction.h"
#include "Misc/Guid.h"

struct FSMPropertyGraphCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,

		// When tracking graph edit state was added.
		GraphEditStateSupported,
		
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FSMPropertyGraphCustomVersion() {}
};

const FGuid FSMPropertyGraphCustomVersion::GUID(0xD49C2618, 0x2737A8A8, 0xF9063E76, 0xBCCD549A);
FCustomVersionRegistration GRegisterPropertyGraphCustomVersion(FSMPropertyGraphCustomVersion::GUID, FSMPropertyGraphCustomVersion::LatestVersion, TEXT("PropertyGraph"));


USMPropertyGraph::USMPropertyGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), ResultNode(nullptr), TempGraphProperty(nullptr),
	  bInitPropertyNodesOnReset(true), bUsingGraphToEdit(false), bVariableIsReadOnly(false),
	  bIsManuallyPlacingPropertyOnGraph(false), bAllowDuplication(false)
{
	bAllowDeletion = false;
}

void USMPropertyGraph::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FSMPropertyGraphCustomVersion::GUID);
	UObject::Serialize(Ar);

	if (Ar.IsLoading() && Ar.CustomVer(FSMPropertyGraphCustomVersion::GUID) < FSMPropertyGraphCustomVersion::GraphEditStateSupported)
	{
		bUsingGraphToEdit = bEditable;
	}
}

void USMPropertyGraph::PostEditUndo()
{
	Super::PostEditUndo();
	ClearFlags(RF_Transient);
}

void USMPropertyGraph::RefreshProperty(bool bModify, bool bSetFromPinFirst)
{
	// This can be set if the graph was deleted then the action undone.
	ClearFlags(RF_Transient);

	if (ResultNode)
	{
		if (const FSMGraphProperty_Base* PropertyNode = ResultNode->GetPropertyNodeConst())
		{
			bVariableIsReadOnly = PropertyNode->IsVariableReadOnly();
			SetUsingGraphToEdit(bUsingGraphToEdit, bModify);
			if (CanSetEditStatusFromReadOnlyVariable())
			{
				bEditable = !bVariableIsReadOnly;
			}
		}
		if (bSetFromPinFirst)
		{
			ResultNode->SetPropertyDefaultsFromPin();
		}
		ResultNode->SetPinValueFromPropertyDefaults(false, false);
	}
	
	if (bModify)
	{
		UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForGraphChecked(this);
		FSMBlueprintEditorUtils::ConditionallyCompileBlueprint(Blueprint);
	}
	
	if (ResultNode && FSMBlueprintEditorUtils::FindBlueprintForNode(ResultNode))
	{
		// The blueprint could be null during undo and ReconstructNode performs a checked find.
		ResultNode->ReconstructNode();
	}
}

void USMPropertyGraph::ResetGraph()
{
	Modify();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(this);
	// Clear out existing nodes since this graph supports reconstruction without deletion.
	TArray<UEdGraphNode*> NodesToDelete = Nodes;
	for (UEdGraphNode* Node : NodesToDelete)
	{
		// Don't delete the main result node.
		if (Node == ResultNode)
		{
			continue;
		}

		FSMBlueprintEditorUtils::RemoveNodeSilently(Blueprint, Node);
	}

	// Recreate any existing default nodes except the main node saved above.
	GetSchema()->CreateDefaultNodesForGraph(*this);
	if (bInitPropertyNodesOnReset)
	{
		ResultNode->OwningGraphNode->InitPropertyGraphNodes(this, ResultNode->GetPropertyNode());
	}
}

void USMPropertyGraph::SetUsingGraphToEdit(bool bValue, bool bModify)
{
	if (IsVariableReadOnly())
	{
		return;
	}
	if (bModify)
	{
		Modify();
	}
	bEditable = bValue;
	bUsingGraphToEdit = bValue;
}

void USMPropertyGraph::ToggleGraphPropertyEdit()
{
	SetUsingGraphToEdit(!IsGraphBeingUsedToEdit());
	// Forces details panel to update.
	FSMBlueprintEditorUtils::ConditionallyCompileBlueprint(FSMBlueprintEditorUtils::FindBlueprintForGraphChecked(this));
}

void USMPropertyGraph::SetPropertyOnGraph(FProperty* Property)
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "SetObject", "Set Object"));

	UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForGraph(this);
	if (!Blueprint)
	{
		// It's possible this is null if the graph was deleted but the UI hasn't updated.
		// This could occur when the array was cleared but the node didn't update. This
		// shouldn't happen any more but is being added as a precaution.
		return;
	}
	
	Modify();
	ResetGraph();

	const FName VariableName = Property->GetFName();

	// TODO: Error message if variable doesn't exist.
	FBPVariableDescription Variable;
	
	if (FSMBlueprintEditorUtils::TryGetVariableByName(Blueprint, VariableName, Variable) ||
		FSMBlueprintEditorUtils::GetPropertyByName(Blueprint, VariableName))
	{
		UEdGraphPin* ResultPin = ResultNode->GetResultPinChecked();
		bIsManuallyPlacingPropertyOnGraph = true;
		FSMBlueprintEditorUtils::PlacePropertyOnGraph(this, Property, ResultPin, nullptr, 50);
		bIsManuallyPlacingPropertyOnGraph = false;
	}
}

void USMPropertyGraph::SetFunctionOnGraph(UFunction* Function)
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "SetObject", "Set Object"));

	Modify();
	ResetGraph();

	UEdGraphPin* ResultPin = ResultNode->GetResultPinChecked();
	FSMBlueprintEditorUtils::PlaceFunctionOnGraph(this, Function, ResultPin, nullptr, nullptr, 50);
}

void USMPropertyGraph::OnGraphManuallyCloned(USMPropertyGraph* OldGraph)
{
	bUsingGraphToEdit = OldGraph->bUsingGraphToEdit;
	bVariableIsReadOnly = OldGraph->bVariableIsReadOnly;
}

void USMPropertyGraph::PruneDisconnectedNodes()
{
	if (ResultNode)
	{
		UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForGraphChecked(this);
		bool bChanged = false;
		
		TSet<UEdGraphNode*> ConnectedNodes;
		FSMBlueprintEditorUtils::GetAllConnectedNodes(ResultNode, EGPD_Input, ConnectedNodes);

		TArray<UEdGraphNode*> AllNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<UEdGraphNode>(this, AllNodes);

		for (UEdGraphNode* Node : AllNodes)
		{
			if (!ConnectedNodes.Contains(Node))
			{
				RemoveNode(Node);
				bChanged = true;
			}
		}

		if (bChanged)
		{
			FSMBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
	}
}
