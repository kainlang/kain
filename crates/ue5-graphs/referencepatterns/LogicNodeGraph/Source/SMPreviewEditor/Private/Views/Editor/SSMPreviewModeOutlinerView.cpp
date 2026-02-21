// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMPreviewModeOutlinerView.h"
#include "Views/Viewport/SMPreviewModeViewportClient.h"
#include "SMPreviewObject.h"
#include "Utilities/SMPreviewUtils.h"
#include "ISMPreviewEditorModule.h"

#include "Blueprints/SMBlueprintEditor.h"
#include "Blueprints/SMBlueprint.h"

#include "ActorTreeItem.h"
#include "ISceneOutlinerColumn.h"
#include "SceneOutlinerModule.h"
#include "SSceneOutliner.h"
#include "ToolMenuContext.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "SSMPreviewModeOutlinerView"

class FPreviewModeOutlinerContextColumn : public ISceneOutlinerColumn
{
public:
	FPreviewModeOutlinerContextColumn(ISceneOutliner& Outliner, USMPreviewObject* InPreviewObject)
	{
		WeakPreviewObject = InPreviewObject;
		WeakOutliner = StaticCastSharedRef<ISceneOutliner>(Outliner.AsShared());
	}

	virtual ~FPreviewModeOutlinerContextColumn() {}

	static FName GetID() { return FName("Context"); }

	// ISceneOutlinerColumn
	virtual FName GetColumnID() override { return GetID(); }
	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override;
	virtual const TSharedRef<SWidget> ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row) override;
	virtual bool SupportsSorting() const override { return true; }
	virtual void SortItems(TArray<FSceneOutlinerTreeItemPtr>& RootItems, const EColumnSortMode::Type SortMode) const override;
	// ~ISceneOutlinerColumn

protected:
	bool IsColumnEnabled() const
	{
		if (WeakPreviewObject.IsValid())
		{
			return !WeakPreviewObject.Get()->IsSimulationRunning();
		}

		return false;
	}

	bool IsTreeItemContext(const FSceneOutlinerTreeItemPtr& TreeItem) const
	{
		const TWeakPtr<FActorTreeItem> ActorItem = StaticCastSharedRef<FActorTreeItem>(TreeItem.ToSharedRef());
		if (ActorItem.IsValid() && ActorItem.Pin()->Actor.IsValid() && WeakPreviewObject.IsValid())
		{
			return WeakPreviewObject.Get()->GetContextActor() == ActorItem.Pin()->Actor.Get();
		}

		return false;
	}

private:
	TWeakPtr<ISceneOutliner> WeakOutliner;
	TWeakObjectPtr<USMPreviewObject> WeakPreviewObject;
};

SHeaderRow::FColumn::FArguments FPreviewModeOutlinerContextColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnID())
	       .FillWidth(1.1f)
	       .DefaultLabel(LOCTEXT("ItemLabel_HeaderText", "Context"))
	       .DefaultTooltip(LOCTEXT("ItemLabel_TooltipText", "Set the actor as the state machine context."));
}

const TSharedRef<SWidget> FPreviewModeOutlinerContextColumn::ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row)
{
	const TWeakPtr<FActorTreeItem> ActorItem = StaticCastSharedRef<FActorTreeItem>(TreeItem);
	if (!ActorItem.IsValid() || !ActorItem.Pin()->Actor.IsValid() || ActorItem.Pin()->Actor->IsA<UWorld>())
	{
		return SNullWidget::NullWidget;
	}
	
	auto IsChecked = [this, ActorItem]() -> ECheckBoxState
	{
		if (WeakPreviewObject.IsValid() && ActorItem.IsValid())
		{
			return WeakPreviewObject.Get()->GetContextActor() == ActorItem.Pin()->Actor.Get() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		
		return ECheckBoxState::Unchecked;
	};
	
	auto OnCheckChanged = [this, ActorItem](ECheckBoxState NewState)
	{
		if (IsColumnEnabled())
		{
			if (WeakPreviewObject.IsValid() && ActorItem.IsValid())
			{
				AActor* ActorToSet = NewState == ECheckBoxState::Checked ? ActorItem.Pin()->Actor.Get() : nullptr;
				WeakPreviewObject.Get()->SetContextActor(ActorToSet);
			}
		}
	};
	
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SCheckBox)
			.IsEnabled(this, &FPreviewModeOutlinerContextColumn::IsColumnEnabled)
			.IsChecked(MakeAttributeLambda(IsChecked))
			.OnCheckStateChanged(FOnCheckStateChanged::CreateLambda(OnCheckChanged))
		];
}

void FPreviewModeOutlinerContextColumn::SortItems(TArray<FSceneOutlinerTreeItemPtr>& RootItems,
	const EColumnSortMode::Type SortMode) const
{
	RootItems.Sort([this, SortMode](const FSceneOutlinerTreeItemPtr& lhs, const FSceneOutlinerTreeItemPtr& rhs)
	{
		return IsTreeItemContext(SortMode == EColumnSortMode::Ascending ? lhs : rhs);
	});
}

SSMPreviewModeOutlinerView::~SSMPreviewModeOutlinerView()
{
	if (SceneOutliner.IsValid() && SceneOutlinerSelectionChanged.IsValid())
	{
		SceneOutliner->GetOnItemSelectionChanged().Remove(SceneOutlinerSelectionChanged);
	}

	if (OnSimEndHandle.IsValid())
	{
		if (BlueprintEditor.IsValid())
		{
			USMPreviewObject* PreviewObject = BlueprintEditor.Pin()->GetStateMachineBlueprint()->GetPreviewObject();
			PreviewObject->OnSimulationEndedEvent.Remove(OnSimEndHandle);
		}
	}
}

void SSMPreviewModeOutlinerView::Construct(const FArguments& InArgs, TSharedPtr<FSMBlueprintEditor> InStateMachineEditor, UWorld* InWorld)
{
	check(InStateMachineEditor.IsValid());
	BlueprintEditor = InStateMachineEditor;

	CreateWorldOutliner(InWorld);
}

void SSMPreviewModeOutlinerView::CreateWorldOutliner(UWorld* World)
{
	if (!BlueprintEditor.IsValid())
	{
		// This could be called during a bp editor shutdown sequence.
		return;
	}
	
	USMPreviewObject* PreviewObject = BlueprintEditor.Pin()->GetStateMachineBlueprint()->GetPreviewObject();

	if (!OnSimEndHandle.IsValid())
	{
		OnSimEndHandle = PreviewObject->OnSimulationEndedEvent.AddRaw(this, &SSMPreviewModeOutlinerView::OnSimulationEnded);
	}
	
	auto OutlinerFilterPredicate = [PreviewObject](const AActor* InActor)
	{
		// HACK: Only check preview world actors. Other actors spawned in aren't needed and can crash when selected (such as network manager)
		// This unfortunately prevents user spawned in actors from showing up in the outliner.
		UWorld* PreviewWorld = PreviewObject->GetPreviewWorld();
		return FSMPreviewUtils::DoesWorldContainActor(PreviewWorld, InActor, true);
	};

	const FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::Get().LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");

	FSceneOutlinerInitializationOptions SceneOutlinerOptions;
	SceneOutlinerOptions.Filters->AddFilterPredicate<FActorTreeItem>(FActorTreeItem::FFilterPredicate::CreateLambda(OutlinerFilterPredicate));
	SceneOutlinerOptions.CustomDelete = FCustomSceneOutlinerDeleteDelegate::CreateRaw(this, &SSMPreviewModeOutlinerView::OnDelete);
	SceneOutlinerOptions.ModifyContextMenu = FSceneOutlinerModifyContextMenu::CreateLambda([](FName& InName, FToolMenuContext& InContext)
	{
		// Hide context menu so we don't allow adding folders.
		InContext = FToolMenuContext();
	});

	// Default columns.
	SceneOutlinerOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(),
		FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 10, FCreateSceneOutlinerColumn(), false, TOptional<float>(),
			LOCTEXT("ActorInfoLabel", "Actor")));
	SceneOutlinerOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::ActorInfo(),
		FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 20, FCreateSceneOutlinerColumn(), true, TOptional<float>(),
			FSceneOutlinerBuiltInColumnTypes::ActorInfo_Localized()));
	
	SceneOutlinerOptions.bShowCreateNewFolder = false;
	SceneOutlinerOptions.OutlinerIdentifier = TEXT("LogicDriverPreviewOutliner");
	
	if (SceneOutliner.IsValid() && SceneOutlinerSelectionChanged.IsValid())
	{
		SceneOutliner->GetOnItemSelectionChanged().Remove(SceneOutlinerSelectionChanged);
	}
	
	SceneOutliner.Reset();
	SceneOutlinerSelectionChanged.Reset();

	SceneOutliner = StaticCastSharedRef<SSceneOutliner>(SceneOutlinerModule.CreateActorBrowser(SceneOutlinerOptions, World));
	SceneOutlinerSelectionChanged = SceneOutliner->GetOnItemSelectionChanged().AddRaw(this, &SSMPreviewModeOutlinerView::OnOutlinerSelectionChanged);

	FSceneOutlinerColumnInfo ColumnInfo;
	ColumnInfo.Visibility = ESceneOutlinerColumnVisibility::Visible;
	ColumnInfo.PriorityIndex = 0; // 10, 20
	ColumnInfo.Factory.BindLambda([PreviewObject](ISceneOutliner& Outliner)
	{
		return TSharedRef<ISceneOutlinerColumn>(MakeShared<FPreviewModeOutlinerContextColumn>(Outliner, PreviewObject));
	});

	SceneOutliner->AddColumn(FPreviewModeOutlinerContextColumn::GetID(), ColumnInfo);
	
	UpdateWidget();
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility | EInvalidateWidgetReason::ChildOrder);
}

void SSMPreviewModeOutlinerView::UpdateWidget()
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		[
			SceneOutliner.ToSharedRef()
		]
	];
}

void SSMPreviewModeOutlinerView::OnOutlinerSelectionChanged(FSceneOutlinerTreeItemPtr TreeItem,
                                                            ESelectInfo::Type Type)
{
	FSMPreviewUtils::DeselectEngineLevelEditor();
	
	check(BlueprintEditor.IsValid());

	FSMBlueprintEditor* BPEditor = BlueprintEditor.Pin().Get();

	const TWeakPtr<FSMPreviewModeViewportClient> PreviewClient = StaticCastSharedPtr<FSMPreviewModeViewportClient>(BPEditor->GetPreviewClient().Pin());
	if (!PreviewClient.IsValid())
	{
		return;
	}

	TSharedPtr<FActorTreeItem> ActorItem = StaticCastSharedPtr<FActorTreeItem>(TreeItem);
	AActor* ActorSelected = nullptr;
	if (ActorItem.IsValid())
	{
		ActorSelected = ActorItem->Actor.Get();
	}

	PreviewClient.Pin()->SelectActor(ActorSelected);
}

void SSMPreviewModeOutlinerView::OnSimulationEnded(USMPreviewObject* PreviewObject)
{
	if (SceneOutliner.IsValid())
	{
		SceneOutliner->ClearSelection();
	}
}

void SSMPreviewModeOutlinerView::OnDelete(const TArray<TWeakPtr<ISceneOutlinerTreeItem>>& InSelectedItem)
{
	if (BlueprintEditor.IsValid())
	{
		ISMPreviewEditorModule& PreviewModule = FModuleManager::LoadModuleChecked<ISMPreviewEditorModule>(LOGICDRIVER_PREVIEW_MODULE_NAME);
		PreviewModule.DeleteSelection(BlueprintEditor);
	}
}

#undef LOCTEXT_NAMESPACE
