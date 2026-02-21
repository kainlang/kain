// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SGraphPin_StatePin.h"

#include "Editor/Kismet/Private/BPFunctionDragDropAction.h"

FReply SSMGraphPin_StatePin::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	// Function drag drops will attempt to wire through a k2 schema which will crash since this isn't for a k2 schema.
	TSharedPtr<FKismetFunctionDragDropAction> FunctionDragDrop = DragDropEvent.GetOperationAs<FKismetFunctionDragDropAction>();
	if (FunctionDragDrop.IsValid())
	{
		return FReply::Handled();
	}

	return SGraphPin::OnDrop(MyGeometry, DragDropEvent);
}
