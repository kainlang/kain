// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMGraphPropertyTreeView.h"

#include "Configuration/SMEditorSettings.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/SlateNodes/Properties/SSMGraphProperty_Base.h"
#include "Graph/Nodes/SMGraphNode_Base.h"
#include "SMSystemEditorLog.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "SMNodeInstance.h"

#include "EditorCategoryUtils.h"
#include "ObjectEditorUtils.h"
#include "SMUnrealTypeDefs.h"

#define LOCTEXT_NAMESPACE "SSMGraphPropertyTreeView"

bool FSMGraphPropertyTreeItem::ShouldItemBeExpanded() const
{
	const bool bIsDefaultCategory = IsDefaultCategory();
	if (IsCategory() && !bIsDefaultCategory && NodeOwner.IsValid())
	{
		const FString FullCategory = BuildFullCategoryString();
		if (const bool* ShouldExpand = NodeOwner->PropertyCategoriesExpanded.Find(FullCategory))
		{
			return *ShouldExpand;
		}
	}

	return bIsDefaultCategory || !FSMBlueprintEditorUtils::GetEditorSettings()->bCollapseCategoriesByDefault;
}

void FSMGraphPropertyTreeItem::SaveExpansionState(bool bValue, bool bRecursive)
{
	if (IsCategory() && !IsDefaultCategory() && NodeOwner.IsValid())
	{
		const FString FullCategory = BuildFullCategoryString();
		NodeOwner->PropertyCategoriesExpanded.Add(FullCategory, bValue);
	}

	if (bRecursive)
	{
		for (const TSharedPtr<FSMGraphPropertyTreeItem>& Child : ChildItems)
		{
			Child->SaveExpansionState(bValue, bRecursive);
		}
	}
}

void FSMGraphPropertyTreeItem::ApplyOwningTemplateToParents(USMNodeInstance* InTemplate)
{
	TWeakPtr<FSMGraphPropertyTreeItem> ParentToCheck = Parent;
	while (ParentToCheck.IsValid())
	{
		ParentToCheck.Pin()->OwningTemplate = InTemplate;
		ParentToCheck = ParentToCheck.Pin()->Parent;
	}
}

FString FSMGraphPropertyTreeItem::BuildFullCategoryString() const
{
	FString FullCategory = CategoryName;
	TWeakPtr<FSMGraphPropertyTreeItem> ParentToCheck = Parent;
	while (ParentToCheck.IsValid())
	{
		FullCategory = ParentToCheck.Pin()->CategoryName + "|" + FullCategory;
		ParentToCheck = ParentToCheck.Pin()->Parent;
	}

	// Save node template to help with state stacks using the same category.
	if (OwningTemplate.IsValid())
	{
		FullCategory = OwningTemplate->GetTemplateGuid().ToString() + "_" + FullCategory;
	}

	return FullCategory;
}

int32 FSMGraphPropertyTreeItem::GetParentCount() const
{
	int32 Count = 0;
	TWeakPtr<FSMGraphPropertyTreeItem> ParentToCheck = Parent;
	while (ParentToCheck.IsValid())
	{
		Count++;
		ParentToCheck = ParentToCheck.Pin()->Parent;
	}

	return Count;
}

void SSMPropertyTreeView::Construct(const FArguments& InArgs)
{
	STreeView::Construct(InArgs);
}

void SSMPropertyTreeView::SetExpansionStateFromItems(const TArray<FPropertyTreeItemPtr>& InTreeItems)
{
	for (const FPropertyTreeItemPtr& TreeItem : InTreeItems)
	{
		SetItemExpansion(TreeItem, TreeItem->ShouldItemBeExpanded());
		SetExpansionStateFromItems(TreeItem->ChildItems);
	}
}

void SSMPropertyTreeViewRow::Construct(const FArguments& InArgs, const TSharedPtr<SSMPropertyTreeView>& TreeView)
{
	Item = InArgs._Item;

	const FSuperRowType::FArguments SuperArgs;
	SMultiColumnTableRow<FPropertyTreeItemPtr>::Construct(SuperArgs, TreeView.ToSharedRef());
}

TSharedRef<SWidget> SSMPropertyTreeViewRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	const TSharedPtr<FSMGraphPropertyTreeItem> ItemPtr = Item.Pin();
	if (ItemPtr.IsValid() && ItemPtr->NodeOwner.IsValid() && ColumnName == SSMNodeInstancePropertyView::PropertyColumnName())
	{
		const bool bIsDefaultCategory = ItemPtr->IsDefaultCategory();
		const int32 NestedLevel = ItemPtr->GetParentCount() - static_cast<int32>(bIsDefaultCategory);

		// Wrap everything in a border to help with different zoom levels not rendering. This seems to be
		// most noticeable on Mac OS.
		const TSharedPtr<SBorder> Border = SNew(SBorder)
		.BorderImage(FSMUnrealAppStyle::Get().GetBrush("NoBorder"));
		
		if (ItemPtr->PropertyWidget.IsValid())
		{
			Border->SetContent(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(NestedLevel * 8.f, 0.f, 0.f, 2.f)
				[
					ItemPtr->PropertyWidget.Pin().ToSharedRef()
				]);
		}
		else if (!bIsDefaultCategory)
		{
			// Display the category name if it isn't the default category.
			const FText CategoryDisplayName = FText::FromString(FEditorCategoryUtils::GetCategoryDisplayString(ItemPtr->CategoryName));
			
			Border->SetContent(SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 0.f, 0.f)
			.VAlign(VAlign_Center)
			[
				SNew(SExpanderArrow, SharedThis(this))
				.StyleSet(&FSMUnrealAppStyle::Get())
				.IndentAmount(5)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::Get().GetFontStyle("ExpandableArea.TitleFont"))
				.Text(CategoryDisplayName)
			]);
		}

		return Border.ToSharedRef();
	}

	return SNullWidget::NullWidget;
}

void SSMNodeInstancePropertyView::Construct(const FArguments& InArgs, USMNodeInstance* InTemplate, const TArray<USMGraphK2Node_PropertyNode_Base*>& InGraphPropertyNodes)
{
	check(InTemplate);
	GraphNode = InArgs._GraphNode;
	NodeTemplate = InTemplate;

	TSharedPtr<SHeaderRow> HeaderRow;
	// Setup the columns.
	{
		SAssignNew(HeaderRow, SHeaderRow);

		SHeaderRow::FColumn::FArguments ColumnArgs;
		ColumnArgs.ColumnId(PropertyColumnName());
		ColumnArgs.DefaultLabel(LOCTEXT("ItemLabel_HeaderText", "Property"));

		// Don't draw the column header.
		HeaderRow->SetVisibility(EVisibility::Collapsed);
		HeaderRow->AddColumn(ColumnArgs);
	}

	PopulateTreeItems(InGraphPropertyNodes);

	ChildSlot
	[
		SAssignNew(PropertyTreeView, SSMPropertyTreeView)
		.SelectionMode(ESelectionMode::None)
		.ItemHeight(12.f)
		.TreeItemsSource(&RootTreeItems)
		.HeaderRow(HeaderRow)
		.OnGenerateRow(this, &SSMNodeInstancePropertyView::OnGenerateRowForPropertyTree)
		.OnGetChildren(this, &SSMNodeInstancePropertyView::OnGetChildrenForPropertyTree)
		.OnExpansionChanged(this, &SSMNodeInstancePropertyView::OnItemExpansionChanged)
	];

	PropertyTreeView->SetExpansionStateFromItems(RootTreeItems);

	bInitialized = true;
}

void SSMNodeInstancePropertyView::FinalizePropertyWidgets()
{
	for (const auto& PropertyWidget : PropertyWidgets)
	{
		if (PropertyWidget.Value.IsValid())
		{
			PropertyWidget.Value->Finalize();
		}
	}
}

void SSMNodeInstancePropertyView::RefreshPropertyWidgets(const TArray<USMGraphK2Node_PropertyNode_Base*>& InGraphPropertyNodes)
{
	bInitialized = false;
	
	PopulateTreeItems(InGraphPropertyNodes);
	if (PropertyTreeView.IsValid())
	{
		PropertyTreeView->RequestListRefresh();
		PropertyTreeView->SetExpansionStateFromItems(RootTreeItems);
	}
	FinalizePropertyWidgets();
	
	bInitialized = true;
}

void SSMNodeInstancePropertyView::AddItemToRootTree(const FPropertyTreeItemRef& InItem)
{
	RootTreeItems.Add(InItem);
}

FPropertyTreeItemPtr SSMNodeInstancePropertyView::GetOrCreateTreeItemByFullCategoryName(const FString& InFullCategoryName)
{
	// Split nested categories.
	TArray<FString> Categories;
	FSMBlueprintEditorUtils::SplitCategories(InFullCategoryName, Categories);
	
	auto FindCategoryNode = [&](const FString& InExactCategoryName, const TArray<FPropertyTreeItemPtr>& InTreeItems) -> FPropertyTreeItemPtr
	{
		for (const TSharedPtr<FSMGraphPropertyTreeItem>& Item : InTreeItems)
		{
			if (Item->CategoryName == InExactCategoryName)
			{
				return Item;
			}
		}

		return nullptr;
	};

	// All found categories in the order they should nest.
	TArray<FPropertyTreeItemPtr> FoundCategoryNodes;
	
	TArray<FPropertyTreeItemPtr> ItemsToSearch = RootTreeItems;
	for (const FString& CategoryName : Categories)
	{
		const FString SanitizedCategoryName = SlugStringForValidName(CategoryName);
		if (FoundCategoryNodes.Num() > 0)
		{
			ItemsToSearch = FoundCategoryNodes.Last()->ChildItems;
		}
		
		FPropertyTreeItemPtr FoundCategoryNode = FindCategoryNode(SanitizedCategoryName, ItemsToSearch);
		if (!FoundCategoryNode.IsValid())
		{
			// Create the category and add to the parent.
			FoundCategoryNode = MakeShared<FSMGraphPropertyTreeItem>();	
			FoundCategoryNode->CategoryName = SanitizedCategoryName;
			FoundCategoryNode->NodeOwner = CastChecked<USMGraphNode_Base>(GraphNode.Get());

			if (FoundCategoryNodes.Num() > 0)
			{
				const FPropertyTreeItemPtr Parent = FoundCategoryNodes.Last();
				check(Parent.IsValid());
				
				// Child node.
				FoundCategoryNode->Parent = Parent;
				Parent->ChildItems.Add(FoundCategoryNode);
			}
			else
			{
				// Root node.
				AddItemToRootTree(FoundCategoryNode.ToSharedRef());
			}
		}

		FoundCategoryNodes.Add(FoundCategoryNode);
	}

	if (FoundCategoryNodes.Num() > 0)
	{
		return FoundCategoryNodes.Last();
	}
	
	return nullptr;
}

TSharedRef<ITableRow> SSMNodeInstancePropertyView::OnGenerateRowForPropertyTree(FPropertyTreeItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SSMPropertyTreeViewRow, PropertyTreeView).Item(Item);
}

void SSMNodeInstancePropertyView::OnGetChildrenForPropertyTree(FPropertyTreeItemPtr InParent, TArray<FPropertyTreeItemPtr>& OutChildren)
{
	OutChildren.Append(InParent->ChildItems);
}

void SSMNodeInstancePropertyView::OnItemExpansionChanged(FPropertyTreeItemPtr TreeItem, bool bIsExpanded) const
{
	if (!bInitialized || !GraphNode.IsValid())
	{
		return;
	}
	
	TreeItem->SaveExpansionState(bIsExpanded, false);
	GraphNode->SaveConfig();

	if (PropertyTreeView.IsValid() && !bIsExpanded && TreeItem->IsCategory() && TreeItem->IsDefaultCategory())
	{
		// Double clicking on a default property can still collapse it, prevent that here.
		PropertyTreeView->SetItemExpansion(TreeItem, true);
	}
}

void SSMNodeInstancePropertyView::PopulateTreeItems(const TArray<USMGraphK2Node_PropertyNode_Base*>& InGraphPropertyNodes)
{
	check(NodeTemplate.IsValid());
	RootTreeItems.Reset();
	
	TMap<USMGraphK2Node_PropertyNode_Base*, TSharedPtr<SSMGraphProperty_Base>> UpdatedWidgets;
	
	for (USMGraphK2Node_PropertyNode_Base* PropertyNode : InGraphPropertyNodes)
	{
		const FSMGraphProperty_Base* GraphProperty = PropertyNode->GetPropertyNodeChecked();
		if (GraphProperty->IsVariableHidden())
		{
			continue;
		}
		
		TSharedPtr<SSMGraphProperty_Base> PropertyWidget;
		if (const TSharedPtr<SSMGraphProperty_Base>* CachedPropertyWidget = PropertyWidgets.Find(PropertyNode))
		{
			if (CachedPropertyWidget->IsValid())
			{
				PropertyWidget = *CachedPropertyWidget;
			}
		}
		else
		{
			PropertyWidget = PropertyNode->GetGraphNodeWidget();
		}
		
		if (PropertyWidget.IsValid())
		{
			// Retrieve the category widget to add the new widget to.

			const USMNodeInstance* OwningTemplate = PropertyNode->GetOwningTemplate();
			check(OwningTemplate);

			const FProperty* RealProperty = GraphProperty->MemberReference.ResolveMember<FProperty>(OwningTemplate->GetClass());
			if (RealProperty == nullptr)
			{
				LDEDITOR_LOG_ERROR(TEXT("Property %s missing. Does the node class %s need to be recompiled?"),
					*GraphProperty->GetDisplayName().ToString(), *OwningTemplate->GetClass()->GetName());
				continue;
			}
			
			FString PropertyCategory = FObjectEditorUtils::GetCategory(RealProperty);
			
			FPropertyTreeItemPtr CategoryItem = GetOrCreateTreeItemByFullCategoryName(PropertyCategory);
			check(CategoryItem.IsValid())
			
			// Add a child item containing the property.
			{
				FPropertyTreeItemPtr PropertyItem = MakeShared<FSMGraphPropertyTreeItem>();
				PropertyItem->PropertyNode = PropertyNode;
				PropertyItem->PropertyWidget = PropertyWidget;
				PropertyItem->CategoryName = SlugStringForValidName(PropertyCategory);
				PropertyItem->NodeOwner = CastChecked<USMGraphNode_Base>(GraphNode.Get());

				PropertyItem->Parent = CategoryItem;
				CategoryItem->ChildItems.Add(PropertyItem);

				PropertyItem->ApplyOwningTemplateToParents(PropertyNode->GetOwningTemplate());
			}
			
			// Make sure the parent is assigned so the owning state slate node can be found.
			PropertyWidget->AssignParentWidget(AsShared());

			// Must call finalize on these after the context box has been created and assigned.
			UpdatedWidgets.Add(PropertyNode, PropertyWidget);
		}
	}

	PropertyWidgets = MoveTemp(UpdatedWidgets);

	// Order category items.
	if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(NodeTemplate->GetClass()))
	{
		// Root category order based on user sort order from the blueprint.
		TMap<FName, int32> CategoryOrder;
		for (int32 CategoryIdx = 0; CategoryIdx < Blueprint->CategorySorting.Num(); ++CategoryIdx)
		{
			FName CategoryName = *SlugStringForValidName(Blueprint->CategorySorting[CategoryIdx].ToString());
			CategoryOrder.Add(MoveTemp(CategoryName), CategoryIdx);
		}
		
		SortTreeItems(RootTreeItems, CategoryOrder);
	}
}

void SSMNodeInstancePropertyView::SortTreeItems(TArray<FPropertyTreeItemPtr>& InTreeItems, TMap<FName, int32>& InCategoryOrder)
{
	// Always set default categories to first, track by unique ID so property order is preserved.
	{
		int32 TotalInDefault = 0;
		for (int32 PropertyIdx = 0; PropertyIdx < InTreeItems.Num(); ++PropertyIdx)
		{
			const FPropertyTreeItemPtr& Item = InTreeItems[PropertyIdx];
			const bool bIsDefaultInCategory = Item->PropertyNode != nullptr || Item->IsDefaultCategory();

			if (bIsDefaultInCategory)
			{
				InCategoryOrder.Add(Item->GetIdToUse(), TotalInDefault++);
			}
		}
	}
	
	InTreeItems.Sort([&](const FPropertyTreeItemPtr& InItemA, const FPropertyTreeItemPtr& InItemB)
	{
		if (InItemA.IsValid() && InItemB.IsValid())
		{
			int32* OrderA = InCategoryOrder.Find(InItemA->GetIdToUse());
			int32* OrderB = InCategoryOrder.Find(InItemB->GetIdToUse());

			if (OrderA && OrderB)
			{
				return *OrderA < *OrderB;
			}
		}

		return false;
	});

	// Sort all nested categories. The order is correct already except default categories should go first.
	for (const FPropertyTreeItemPtr& Item : InTreeItems)
	{
		// Offset by at least the amount so default properties can record their index.
		const int32 MinCount = Item->ChildItems.Num();
		TMap<FName, int32> ChildCategoryOrder;
		for (int32 PropertyIdx = 0; PropertyIdx < Item->ChildItems.Num(); ++PropertyIdx)
		{
			const FPropertyTreeItemPtr& ChildItem = Item->ChildItems[PropertyIdx];
			const bool bIsDefaultInCategory = Item->PropertyNode != nullptr;

			if (!bIsDefaultInCategory)
			{
				// Record sub category order.
				ChildCategoryOrder.Add(ChildItem->GetIdToUse(), PropertyIdx + MinCount);
			}
		}

		SortTreeItems(Item->ChildItems, ChildCategoryOrder);
	}
}

#undef LOCTEXT_NAMESPACE
