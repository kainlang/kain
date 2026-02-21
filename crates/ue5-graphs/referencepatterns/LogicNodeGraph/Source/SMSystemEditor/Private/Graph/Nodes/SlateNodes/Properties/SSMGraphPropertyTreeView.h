// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Widgets/Views/STreeView.h"

class SSMGraphProperty_Base;
class USMGraphK2Node_PropertyNode_Base;
class UEdGraphNode;
class USMNodeInstance;
class USMGraphNode_Base;

/**
 * Represents either a category or property in a tree view.
 */
struct FSMGraphPropertyTreeItem
{
	FString TreeId;
	FString CategoryName;
	TWeakObjectPtr<USMGraphK2Node_PropertyNode_Base> PropertyNode;
	TWeakPtr<SSMGraphProperty_Base> PropertyWidget;
	
	TArray<TSharedPtr<FSMGraphPropertyTreeItem>> ChildItems;
	TWeakPtr<FSMGraphPropertyTreeItem> Parent;
	TWeakObjectPtr<USMGraphNode_Base> NodeOwner;
	TWeakObjectPtr<USMNodeInstance> OwningTemplate;
	
	FSMGraphPropertyTreeItem(): PropertyNode(nullptr)
	{
		TreeId = FGuid::NewGuid().ToString();
	}
	
	/** Checks if this particular item should be expanded based on its contents. */
	bool ShouldItemBeExpanded() const;

	/** Saves the expansion state to the owning node. */
	void SaveExpansionState(bool bValue, bool bRecursive);

	/** Sets the template of all parents. */
	void ApplyOwningTemplateToParents(USMNodeInstance* InTemplate);
	
	/** If this item represents a category. */
	bool IsCategory() const { return !PropertyNode.IsValid(); }

	/** If this is the top most default category. */
	bool IsDefaultCategory() const { return CategoryName.Equals("default", ESearchCase::IgnoreCase) && !Parent.IsValid(); }
	
	/** Categories use category name, property items have a unique id. */
	FName GetIdToUse() const
	{
		const FString NameString = PropertyNode.IsValid() ? TreeId : CategoryName;
		return *SlugStringForValidName(NameString);
	}

	/** Search up parents building a complete category string. */
	FString BuildFullCategoryString() const;
	
	/** Find the number of parents. */
	int32 GetParentCount() const;

	bool operator==(const FSMGraphPropertyTreeItem& Other) const
	{
		return CategoryName == Other.CategoryName && PropertyNode.Get() == Other.PropertyNode.Get();
	}
};

typedef TSharedPtr<FSMGraphPropertyTreeItem> FPropertyTreeItemPtr;
typedef TSharedRef<FSMGraphPropertyTreeItem> FPropertyTreeItemRef;

/**
 * A tree view containing all categories and properties for a given node instance.
 */
class SSMPropertyTreeView : public STreeView<FPropertyTreeItemPtr>
{
public:
	void Construct(const FArguments& InArgs);

	/** Recursively set expansion state of tree view to match items. */
	void SetExpansionStateFromItems(const TArray<FPropertyTreeItemPtr>& InTreeItems);
};

/**
 * Each row of the tree view, per each node instance.
 */
class SSMPropertyTreeViewRow : public SMultiColumnTableRow<FPropertyTreeItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SSMPropertyTreeViewRow) {}

		/** The list item for this row */
		SLATE_ARGUMENT(FPropertyTreeItemPtr, Item)

	SLATE_END_ARGS()

	/** Construct function for this widget */
	void Construct(const FArguments& InArgs, const TSharedPtr<SSMPropertyTreeView>& TreeView);

	/** Overridden from SMultiColumnTableRow. Generates a widget for this column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	/** The item associated with this row of data. */
	TWeakPtr<FSMGraphPropertyTreeItem> Item;
};

/**
 * Visual representation of all properties within a node instance.
 */
class SSMNodeInstancePropertyView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSMNodeInstancePropertyView) {}
		/** Graph node containing the property. */
		SLATE_ARGUMENT(USMGraphNode_Base*, GraphNode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, USMNodeInstance* InTemplate, const TArray<USMGraphK2Node_PropertyNode_Base*>& InGraphPropertyNodes);

	/** Calls finalize on all embedded property widgets. */
	void FinalizePropertyWidgets();

	/** Populates and refreshes all tree items. */
	void RefreshPropertyWidgets(const TArray<USMGraphK2Node_PropertyNode_Base*>& InGraphPropertyNodes);
	
	/** Adds a property item to the root tree. */
	void AddItemToRootTree(const FPropertyTreeItemRef& InItem);

	/** Find a tree item by category. */
	FPropertyTreeItemPtr GetOrCreateTreeItemByFullCategoryName(const FString& InFullCategoryName);

	/** Return the map of properties to property widgets. */
	const TMap<USMGraphK2Node_PropertyNode_Base*, TSharedPtr<SSMGraphProperty_Base>>& GetPropertyWidgets() const { return PropertyWidgets; }

	/** The name of the primary property column. */
	static FName PropertyColumnName() { return "Property"; }

private:
	/** Called by STreeView for each row being generated. */
	TSharedRef<ITableRow> OnGenerateRowForPropertyTree(FPropertyTreeItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	
	/** Called by STreeView to get child items for the specified parent item. */
	void OnGetChildrenForPropertyTree(FPropertyTreeItemPtr InParent, TArray<FPropertyTreeItemPtr>& OutChildren);
	
	/** Called when an item in the tree has been collapsed or expanded. */
	void OnItemExpansionChanged(FPropertyTreeItemPtr TreeItem, bool bIsExpanded) const;

	/** Generate the RootTreeItems and all children. */
	void PopulateTreeItems(const TArray<USMGraphK2Node_PropertyNode_Base*>& InGraphPropertyNodes);
	
	/** Recursively sort all tree items. */
	static void SortTreeItems(TArray<FPropertyTreeItemPtr>& InTreeItems, TMap<FName, int32>& InCategoryOrder);

private:
	TSharedPtr<SSMPropertyTreeView> PropertyTreeView;
	TArray<FPropertyTreeItemPtr> RootTreeItems;

	TMap<USMGraphK2Node_PropertyNode_Base*, TSharedPtr<SSMGraphProperty_Base>> PropertyWidgets;
	TWeakObjectPtr<USMGraphNode_Base> GraphNode;
	TWeakObjectPtr<USMNodeInstance> NodeTemplate;

	bool bInitialized = false;
};
