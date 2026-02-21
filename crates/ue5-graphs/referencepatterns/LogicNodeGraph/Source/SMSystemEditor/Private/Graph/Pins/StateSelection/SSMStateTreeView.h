// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Widgets/Views/STreeView.h"

class USMBlueprintGeneratedClass;
class USMStateInstance_Base;
class USMStateMachineInstance;

/**
 * Represents a state item in the tree.
 */
struct FSMStateTreeItem
{
	FString StateName;
	
	TArray<TSharedPtr<FSMStateTreeItem>> ChildItems;
	TWeakPtr<FSMStateTreeItem> Parent;

	TWeakObjectPtr<USMStateInstance_Base> NodeInstance;
	
	FSMStateTreeItem()
	{
	}
	
	/** Checks if this particular item should be expanded based on its contents. */
	bool ShouldItemBeExpanded() const;
	
	/** Search up parents building a complete qualified name. */
	FString BuildQualifiedNameString() const;

	bool operator==(const FSMStateTreeItem& Other) const
	{
		return StateName == Other.StateName && Parent == Other.Parent;
	}
};

typedef TSharedPtr<FSMStateTreeItem> FSMStateTreeItemPtr;
typedef TSharedRef<FSMStateTreeItem> FSMStateTreeItemRef;

/**
 * A tree view for selecting states in a state machine.
 */
class SSMStateTreeView : public STreeView<FSMStateTreeItemPtr>
{
public:
	void Construct(const FArguments& InArgs);

	/** Recursively set expansion state of tree view to match items. */
	void SetExpansionStateFromItems(const TArray<FSMStateTreeItemPtr>& InTreeItems);
};

/**
 * Each row of the tree view.
 */
class SSMStateTreeViewRow : public SMultiColumnTableRow<FSMStateTreeItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SSMStateTreeViewRow) {}

		/** The list item for this row */
		SLATE_ARGUMENT(FSMStateTreeItemPtr, Item)

	SLATE_END_ARGS()

	/** Construct function for this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<SSMStateTreeView>& TreeView);

	/** Overridden from SMultiColumnTableRow. Generates a widget for this column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	/** The item associated with this row of data */
	TWeakPtr<FSMStateTreeItem> Item;
};

DECLARE_DELEGATE_OneParam(FOnStateTreeItemSelected, FSMStateTreeItemPtr)

/**
 * Visual representation of all available states.
 */
class SSMStateTreeSelectionView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSMStateTreeSelectionView) {}
	SLATE_EVENT(FOnStateTreeItemSelected, OnSelectionChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, USMBlueprintGeneratedClass* GeneratedClass);

	/** Adds a state item to the root tree. */
	void AddItemToRootTree(const FSMStateTreeItemRef& InItem);

	/** The name of the primary state column. */
	static FName StateColumnName() { return "State"; }

	/** The currently selected item. */
	FSMStateTreeItemPtr GetSelectedStateItem() const { return SelectedStateItem; }

private:
	/** Called by STreeView for each row being generated. */
	TSharedRef<ITableRow> OnGenerateRowForTree(FSMStateTreeItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	
	/** Called by STreeView to get child items for the specified parent item. */
	void OnGetChildrenForTree(FSMStateTreeItemPtr InParent, TArray<FSMStateTreeItemPtr>& OutChildren);

	/** Call when item in tree was selected. */
	void OnSelectedItemChanged(FSMStateTreeItemPtr InSelectedItem, ESelectInfo::Type InSelectInfo);
	
	/** Generate the RootTreeItems and all children. */
	void PopulateTreeItems(FSMStateTreeItemPtr InInitialItem);

private:
	TSharedPtr<SSMStateTreeView> StateTreeView;
	TSharedPtr<SScrollBar> VerticalScrollBar;
	TArray<FSMStateTreeItemPtr> RootTreeItems;
	TSharedPtr<SHeaderRow> HeaderRow;
	FSMStateTreeItemPtr SelectedStateItem;
	FOnStateTreeItemSelected OnItemSelectedDelegate;

	bool bInitialized = false;
};
