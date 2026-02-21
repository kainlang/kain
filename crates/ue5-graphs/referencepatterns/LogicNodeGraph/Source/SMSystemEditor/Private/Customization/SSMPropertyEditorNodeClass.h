// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ClassViewerModule.h"
#include "Widgets/SCompoundWidget.h"

class FAssetDragDropOp;
class IPropertyHandle;
class USMGraphNode_Base;
class SComboButton;

/**
 * A widget used to edit node class properties which can respect node class rules.
 */
class SSMPropertyEditorNodeClass : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSMPropertyEditorNodeClass)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<IPropertyHandle>& InPropertyHandle, const TWeakObjectPtr<USMGraphNode_Base>& InGraphNode);

protected:
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	
private:
	TSharedRef<SWidget> GenerateNodeClassSelector();
	void OnNodeClassSelected(UClass* InClass);

	const FSlateBrush* GetNodeClassIcon() const;
	FText GetNodeClassDisplayValue() const;

	void SendToObjects(const FString& NewValue);

	static UObject* LoadDragDropObject(const TSharedPtr<FAssetDragDropOp>& UnloadedClassOp);

private:
	TSharedPtr<IPropertyHandle> NodeClassPropertyHandle;
	TSharedPtr<SComboButton> NodeClassComboButton;

	TWeakObjectPtr<USMGraphNode_Base> GraphNode;
	
	/** Class filter that the class viewer is using. */
	TSharedPtr<class FSMNodeClassFilter> ClassFilter;

	/** Class initialization options the class viewer is using. */
	FClassViewerInitializationOptions ClassViewerOptions;

	/** Filter functions for class viewer. */
	TSharedPtr<FClassViewerFilterFuncs> ClassFilterFuncs;
};