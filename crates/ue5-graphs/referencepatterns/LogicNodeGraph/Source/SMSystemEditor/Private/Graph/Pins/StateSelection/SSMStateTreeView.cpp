// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMStateTreeView.h"

#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "Construction/ISMEditorConstructionManager.h"
#include "Construction/SMEditorInstance.h"
#include "Construction/SMEditorStateMachine.h"
#include "Utilities/SMNodeInstanceUtils.h"

#include "Blueprints/SMBlueprint.h"

#include "SMUnrealTypeDefs.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "SSMStateTreeView"

bool FSMStateTreeItem::ShouldItemBeExpanded() const
{
	return true;
}

FString FSMStateTreeItem::BuildQualifiedNameString() const
{
	FString FullName = StateName;
	TWeakPtr<FSMStateTreeItem> ParentToCheck = Parent;
	while (ParentToCheck.IsValid())
	{
		FullName = ParentToCheck.Pin()->StateName + "." + FullName;
		ParentToCheck = ParentToCheck.Pin()->Parent;
	}

	FullName.RemoveFromStart(TEXT("Root."));

	return FullName;
}

void SSMStateTreeView::Construct(const FArguments& InArgs)
{
	STreeView::Construct(InArgs);
}

void SSMStateTreeView::SetExpansionStateFromItems(const TArray<FSMStateTreeItemPtr>& InTreeItems)
{
	for (const FSMStateTreeItemPtr& TreeItem: InTreeItems)
	{
		SetItemExpansion(TreeItem, TreeItem->ShouldItemBeExpanded());
		SetExpansionStateFromItems(TreeItem->ChildItems);
	}
}

void SSMStateTreeViewRow::Construct(const FArguments& InArgs, const TSharedRef<SSMStateTreeView>& TreeView)
{
	Item = InArgs._Item;

	const FSuperRowType::FArguments SuperArgs;
	SMultiColumnTableRow<FSMStateTreeItemPtr>::Construct(SuperArgs, TreeView);
}

TSharedRef<SWidget> SSMStateTreeViewRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	const TSharedPtr<FSMStateTreeItem> ItemPtr = Item.Pin();
	if (ItemPtr.IsValid() && ColumnName == SSMStateTreeSelectionView::StateColumnName())
	{
		// Display the category name if it isn't the default category.
		const FText ItemDisplayName = FText::FromString(ItemPtr->StateName);
		
		return SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SExpanderArrow, SharedThis(this))
			.StyleSet(&FSMUnrealAppStyle::Get())
			.IndentAmount(5)
		]
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::Get().GetFontStyle("ExpandableArea.TitleFont"))
			.Text(ItemDisplayName)
		];
	}

	return SNullWidget::NullWidget;
}

void SSMStateTreeSelectionView::Construct(const FArguments& InArgs, USMBlueprintGeneratedClass* GeneratedClass)
{
	check(GeneratedClass);

	OnItemSelectedDelegate = InArgs._OnSelectionChanged;

	// Setup the columns.
	{
		SAssignNew(HeaderRow, SHeaderRow);

		SHeaderRow::FColumn::FArguments ColumnArgs;
		ColumnArgs.ColumnId(StateColumnName());
		ColumnArgs.DefaultLabel(LOCTEXT("ItemLabel_HeaderText", "State"));

		// Don't draw the column header.
		HeaderRow->SetVisibility(EVisibility::Collapsed);
		HeaderRow->AddColumn(ColumnArgs);
	}

	if (USMBlueprint* Blueprint = Cast<USMBlueprint>(UBlueprint::GetBlueprintFromClass(GeneratedClass)))
	{
		const FSMEditorStateMachine& EditorStateMachine = ISMEditorConstructionManager::Get().CreateEditorStateMachine(Blueprint);

		const FSMStateTreeItemPtr StateMachineItem = MakeShared<FSMStateTreeItem>();
		StateMachineItem->NodeInstance = EditorStateMachine.StateMachineEditorInstance->GetRootStateMachineNodeInstance();
		StateMachineItem->StateName = StateMachineItem->NodeInstance->GetNodeName();

		RootTreeItems.Reset();
		RootTreeItems.Add(StateMachineItem);
		
		PopulateTreeItems(StateMachineItem);

		ISMEditorConstructionManager::Get().CleanupEditorStateMachine(Blueprint);

		SAssignNew(VerticalScrollBar, SScrollBar)
		.AlwaysShowScrollbar(true);
		
		ChildSlot
		.Padding(5.f, 2.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.MaxHeight(400.f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)
				+SScrollBox::Slot()
				[
					SAssignNew(StateTreeView, SSMStateTreeView)
					.SelectionMode(ESelectionMode::Single)
					.TreeItemsSource(&RootTreeItems)
					.HeaderRow(HeaderRow)
					.OnSelectionChanged(this, &SSMStateTreeSelectionView::OnSelectedItemChanged)
					.OnGenerateRow(this, &SSMStateTreeSelectionView::OnGenerateRowForTree)
					.OnGetChildren(this, &SSMStateTreeSelectionView::OnGetChildrenForTree)
					.ItemHeight(12.0f)
					.ExternalScrollbar(VerticalScrollBar)
				]
			]
		];

		StateTreeView->SetExpansionStateFromItems(RootTreeItems);
	}
	bInitialized = true;
}

void SSMStateTreeSelectionView::AddItemToRootTree(const FSMStateTreeItemRef& InItem)
{
	RootTreeItems.Add(InItem);
}

TSharedRef<ITableRow> SSMStateTreeSelectionView::OnGenerateRowForTree(FSMStateTreeItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SSMStateTreeViewRow, StateTreeView.ToSharedRef()).Item(Item);
}

void SSMStateTreeSelectionView::OnGetChildrenForTree(FSMStateTreeItemPtr InParent, TArray<FSMStateTreeItemPtr>& OutChildren)
{
	OutChildren.Append(InParent->ChildItems);
}

void SSMStateTreeSelectionView::OnSelectedItemChanged(FSMStateTreeItemPtr InSelectedItem, ESelectInfo::Type InSelectInfo)
{
	SelectedStateItem = InSelectedItem;
	OnItemSelectedDelegate.ExecuteIfBound(SelectedStateItem);
}

void SSMStateTreeSelectionView::PopulateTreeItems(FSMStateTreeItemPtr InInitialItem)
{
	if (const USMStateMachineInstance* StateMachineInstance = Cast<USMStateMachineInstance>(InInitialItem->NodeInstance.Get()))
	{
		TArray<USMStateInstance_Base*> AllStateInstances;
		StateMachineInstance->GetAllStateInstances(AllStateInstances);

		for (USMStateInstance_Base* StateInstance : AllStateInstances)
		{
			FSMStateTreeItemPtr StateItem = MakeShared<FSMStateTreeItem>();

			StateItem->StateName = StateInstance->GetNodeName();
			StateItem->Parent = InInitialItem;
			StateItem->NodeInstance = StateInstance;
		
			InInitialItem->ChildItems.Add(StateItem);

			if (USMStateMachineInstance* NestedStateMachineInstance = Cast<USMStateMachineInstance>(StateInstance))
			{
				PopulateTreeItems(StateItem);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
