// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SSMGraphPropertyTreeView.h"

/**
 * All properties for a given slate node. This includes the state stack properties and all categories.
 */
class SSMGraphNode_PropertyContent : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SSMGraphNode_PropertyContent)
	: _GraphNode(nullptr) {}
	
		/** Graph node containing the property. */
		SLATE_ARGUMENT(USMGraphNode_Base*, GraphNode)
	
	SLATE_END_ARGS()

	SSMGraphNode_PropertyContent();
	virtual ~SSMGraphNode_PropertyContent() override;

	void Construct(const FArguments& InArgs);

	/** Call finalize on all property widgets. */
	void Finalize();

	/**
	 * Refresh all properties within all node instances.
	 * @return True if the operation succeeded.
	 */
	bool RefreshAllProperties();
	
	/** Return the property widgets created within this property content. */
	const TMap<TSharedPtr<SSMGraphProperty_Base>, USMGraphK2Node_PropertyNode_Base*>& GetPropertyWidgets() const { return PropertyWidgets; }

protected:
	TSharedPtr<SWidget> CreateContent();

	/** Discover and sort properties per node instance. */
	void MapTemplatesToProperties(TMap<USMNodeInstance*, TArray<USMGraphK2Node_PropertyNode_Base*>>& OutTemplatePropertyMap);

private:
	TMap<TSharedPtr<SSMGraphProperty_Base>, USMGraphK2Node_PropertyNode_Base*> PropertyWidgets;
	TMap<USMNodeInstance*, TSharedPtr<SSMNodeInstancePropertyView>> NodeInstanceProperties;
	
	TWeakObjectPtr<USMGraphNode_Base> GraphNode;
};