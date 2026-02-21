// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Helpers/SMDragDropHelpers.h"

#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/SMPropertyGraph.h"
#include "SMGraphProperty_Base.h"

#include "BPVariableDragDropAction.h"
#include "EditorStyleSet.h"
#include "SMUnrealTypeDefs.h"

#define LOCTEXT_NAMESPACE "SMDragDropHelpers"

bool FSMDragDropHelpers::IsDragDropValidForPropertyNode(const USMGraphK2Node_PropertyNode_Base* PropertyNode,
	const FDragDropEvent& DragDropEvent, bool bIsEditModeAllowed)
{
	if (PropertyNode)
	{
		if (FSMGraphProperty_Base* GraphNode = PropertyNode->GetPropertyNodeConst())
		{
			if (GraphNode->IsVariableReadOnly())
			{
				return false;
			}
		}
		
		TSharedPtr<FKismetVariableDragDropAction> VariableDragDrop = DragDropEvent.GetOperationAs<FKismetVariableDragDropAction>();
		if (VariableDragDrop.IsValid())
		{
			return bIsEditModeAllowed || !PropertyNode->GetPropertyGraph()->IsGraphBeingUsedToEdit();
		}

		TSharedPtr<FKismetFunctionDragDropAction> FunctionDragDrop = DragDropEvent.GetOperationAs<FKismetFunctionDragDropAction>();
		if (FunctionDragDrop.IsValid())
		{
			if (!bIsEditModeAllowed && PropertyNode->GetPropertyGraph()->IsGraphBeingUsedToEdit())
			{
				return false;
			}

			UFunction const* Function = FSMDragDropAction_Function::GetFunction(FunctionDragDrop.Get());
			if (!Function->HasAnyFunctionFlags(FUNC_BlueprintPure))
			{
				return false;
			}

			return true;
		}
	}

	return false;
}

void FSMDragDropHelpers::SetDragDropMessage(const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FKismetVariableDragDropAction> VariableDragDrop = DragDropEvent.GetOperationAs<FKismetVariableDragDropAction>();
	if (VariableDragDrop.IsValid())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("DraggedVariableName"), FText::FromString(VariableDragDrop->GetVariableProperty()->GetName()));

		const FSlateBrush* StatusSymbol = FSMUnrealAppStyle::Get().GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
		VariableDragDrop->SetSimpleFeedbackMessage(StatusSymbol, FLinearColor::White,
			FText::Format(LOCTEXT("PlaceVariableOK", "Place Variable {DraggedVariableName}"), Args));
	}
	else
	{
		TSharedPtr<FKismetFunctionDragDropAction> FunctionDragDrop = DragDropEvent.GetOperationAs<FKismetFunctionDragDropAction>();
		if (FunctionDragDrop.IsValid())
		{
			const FSlateBrush* StatusSymbol = FSMUnrealAppStyle::Get().GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
			FunctionDragDrop->SetSimpleFeedbackMessage(StatusSymbol, FLinearColor::White,
				LOCTEXT("PlaceFunctionOK", "Place Function"));
		}
	}
}

FMyBlueprintItemDragDropAction_DEFINITION

#undef LOCTEXT_NAMESPACE
