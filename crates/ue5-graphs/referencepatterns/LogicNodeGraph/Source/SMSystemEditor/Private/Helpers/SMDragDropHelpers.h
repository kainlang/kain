// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Editor/Kismet/Private/BPFunctionDragDropAction.h"
#include "SMUnrealTypeDefs.h"

/**
 * Wrapper for KismetDragDropAction to expose protected method.
 */
class FSMDragDropAction_Function : public FKismetFunctionDragDropAction
{
public:
	FSMDragDropAction_Function() = default;
	virtual ~FSMDragDropAction_Function() override = default;
	
	// Necessary functions to implement because they aren't exported at the base level.
	virtual FReply DroppedOnAction(TSharedRef<FEdGraphSchemaAction> Action) override { return FReply::Handled(); }
	virtual FReply DroppedOnCategory(FText Category) override { return FReply::Handled(); }

	/** Create a wrapper for the kismet action, retrieve the now exposed function, then delete the wrapper. */
	static UFunction const* GetFunction(FKismetFunctionDragDropAction* RealAction)
	{
		FSMDragDropAction_Function* Wrapper = new FSMDragDropAction_Function(*(FSMDragDropAction_Function*)RealAction);
		UFunction const* Function = Wrapper->GetFunctionProperty();
		delete Wrapper;
		return Function;
	}
};

class SMSYSTEMEDITOR_API FSMDragDropHelpers
{
public:
	static bool IsDragDropValidForPropertyNode(const class USMGraphK2Node_PropertyNode_Base* PropertyNode, const FDragDropEvent& DragDropEvent, bool bIsEditModeAllowed = false);
	static void SetDragDropMessage(const FDragDropEvent& DragDropEvent);
};


/*
 * FMyBlueprintItemDragDropAction is the only dragdrop in the inheritance chain not exported. On Linux when the clang compiler performs linking the vtable for the constructor and destructor aren't found.
 * This is a redefinition from MyBlueprintItemDragDropAction.cpp.
 * TODO: UE4 check to see if engine updates require this to change. See if there's something being missed... all required modules are included so the real definition should be found.
 */

#if !PLATFORM_WINDOWS
#define FMyBlueprintItemDragDropAction_DEFINITION \
FMyBlueprintItemDragDropAction::FMyBlueprintItemDragDropAction(): bControlDrag(false), bAltDrag(false){} \
FReply FMyBlueprintItemDragDropAction::DroppedOnAction(TSharedRef<FEdGraphSchemaAction> Action) \
{ \
	if (SourceAction.IsValid() && (SourceAction->GetTypeId() == Action->GetTypeId()))\
	{ \
		if (SourceAction->GetPersistentItemDefiningObject() == Action->GetPersistentItemDefiningObject())\
		{ \
			SourceAction->ReorderToBeforeAction(Action);\
			return FReply::Handled();\
		} \
	} \
	return FReply::Unhandled(); \
} \
FReply FMyBlueprintItemDragDropAction::DroppedOnCategory(FText Category) \
{ \
	if (SourceAction.IsValid()) \
	{ \
		SourceAction->MovePersistentItemToCategory(Category); \
	} \
	return FReply::Handled(); \
} \
void FMyBlueprintItemDragDropAction::HoverTargetChanged() \
{ \
	if (SourceAction.IsValid()) \
	{ \
		if (!HoveredCategoryName.IsEmpty()) \
		{ \
			const bool bIsNative = !SourceAction->GetPersistentItemDefiningObject().IsPotentiallyEditable(); \
			FFormatNamedArguments Args; \
			Args.Add(TEXT("DisplayName"), SourceAction->GetMenuDescription()); \
			Args.Add(TEXT("HoveredCategoryName"), HoveredCategoryName); \
			if (bIsNative) \
			{ \
				SetFeedbackMessageError(FText::Format(LOCTEXT("ChangingCatagoryNotEditable", "Cannot change category for '{DisplayName}' because it is declared in C++"), Args)); \
			} \
			else if (HoveredCategoryName.EqualTo(SourceAction->GetCategory())) \
			{ \
				SetFeedbackMessageError(FText::Format(LOCTEXT("ChangingCatagoryAlreadyIn", "'{DisplayName}' is already in category '{HoveredCategoryName}'"), Args)); \
			} \
			else \
			{ \
				SetFeedbackMessageOK(FText::Format(LOCTEXT("ChangingCatagoryOk", "Move '{DisplayName}' to category '{HoveredCategoryName}'"), Args)); \
			} \
			return; \
		} \
		else if (HoveredAction.IsValid()) \
		{ \
			TSharedPtr<FEdGraphSchemaAction> HoveredActionPtr = HoveredAction.Pin(); \
			FFormatNamedArguments Args; \
			Args.Add(TEXT("DraggedDisplayName"), SourceAction->GetMenuDescription()); \
			Args.Add(TEXT("DropTargetDisplayName"), HoveredActionPtr->GetMenuDescription()); \
			if (HoveredActionPtr->GetTypeId() == SourceAction->GetTypeId()) \
			{ \
				if (SourceAction->GetPersistentItemDefiningObject() == HoveredActionPtr->GetPersistentItemDefiningObject()) \
				{ \
					const int32 MovingItemIndex = SourceAction->GetReorderIndexInContainer(); \
					const int32 TargetVarIndex = HoveredActionPtr->GetReorderIndexInContainer(); \
					 \
					if (MovingItemIndex == INDEX_NONE) \
					{ \
						SetFeedbackMessageError(FText::Format(LOCTEXT("ReorderNonOrderedItem", "Cannot reorder '{DraggedDisplayName}'."), Args)); \
					} \
					else if (TargetVarIndex == INDEX_NONE) \
					{ \
						SetFeedbackMessageError(FText::Format(LOCTEXT("ReorderOntoNonOrderedItem", "Cannot reorder '{DraggedDisplayName}' before '{DropTargetDisplayName}'."), Args)); \
					} \
					else if (HoveredActionPtr == SourceAction) \
					{ \
						SetFeedbackMessageError(FText::Format(LOCTEXT("ReorderOntoSameItem", "Cannot reorder '{DraggedDisplayName}' before itself."), Args)); \
					} \
					else \
					{ \
						SetFeedbackMessageOK(FText::Format(LOCTEXT("ReorderActionOK", "Reorder '{DraggedDisplayName}' before '{DropTargetDisplayName}'"), Args)); \
					} \
				} \
				else \
				{ \
					SetFeedbackMessageError(FText::Format(LOCTEXT("ReorderActionDifferentScope", "Cannot reorder '{DraggedDisplayName}' into a different scope."), Args)); \
				} \
			} \
			else \
			{ \
				SetFeedbackMessageError(FText::Format(LOCTEXT("ReorderActionDifferentAction", "Cannot reorder '{DraggedDisplayName}' into a different section."), Args)); \
			} \
			return; \
		} \
	} \
	FGraphSchemaActionDragDropAction::HoverTargetChanged(); \
} \
void FMyBlueprintItemDragDropAction::SetFeedbackMessageError(const FText& Message) \
{ \
	const FSlateBrush* StatusSymbol = FSMUnrealAppStyle::Get().GetBrush(TEXT("Graph.ConnectorFeedback.Error")); \
	SetSimpleFeedbackMessage(StatusSymbol, FLinearColor::White, Message); \
} \
void FMyBlueprintItemDragDropAction::SetFeedbackMessageOK(const FText& Message) \
{ \
	const FSlateBrush* StatusSymbol = FSMUnrealAppStyle::Get().GetBrush(TEXT("Graph.ConnectorFeedback.OK")); \
	SetSimpleFeedbackMessage(StatusSymbol, FLinearColor::White, Message); \
}
#else
#define FMyBlueprintItemDragDropAction_DEFINITION
#endif