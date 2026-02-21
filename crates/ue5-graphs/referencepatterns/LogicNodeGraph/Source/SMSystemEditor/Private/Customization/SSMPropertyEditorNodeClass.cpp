// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMPropertyEditorNodeClass.h"

#include "Graph/Nodes/SMGraphNode_Base.h"
#include "Utilities/SMNodeInstanceUtils.h"
#include "Utilities/SMNodeRuleUtils.h"

#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragAndDrop/ClassDragDropOp.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "SMUnrealTypeDefs.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "SMPropertyEditorNodeCustomization"

/**
 * Filter what node classes are available in the class selection.
 */
class FSMNodeClassFilter : public IClassViewerFilter
{
public:
	FSMNodeClassFilter()
		: DisallowedClassFlags(CLASS_None)
	{}
	
	/** All children of these classes will be included unless filtered out by another setting. */
	TSet<const UClass*> AllowedChildrenOfClasses;

	/** Disallowed class flags. */
	EClassFlags DisallowedClassFlags;

	/** The graph node owning this node class. */
	TWeakObjectPtr<USMGraphNode_Base> OwningGraphNode;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		const bool bAllowed = !InClass->HasAnyClassFlags(DisallowedClassFlags)
			&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed;

		if (bAllowed)
		{
			LD::NodeRuleUtils::FNodeClassAllowedResult Result;
			if (OwningGraphNode.IsValid()
				&& !LD::NodeRuleUtils::IsNodeClassAllowed(InClass, OwningGraphNode.Get(), Result)
				&& (Result.bFailedFromOwningStateMachineRules
				|| FSMNodeInstanceUtils::ShouldHideFromDropDownIfRulesFail(InClass)))
			{
				return false;
			}
		}

		return bAllowed;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		const bool bAllowed = !InUnloadedClassData->HasAnyClassFlags(DisallowedClassFlags)
			&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;

		if (bAllowed)
		{
			const FString GeneratedClassPathString = InUnloadedClassData->GetClassPathName().ToString();
			const TSoftClassPtr<USMNodeInstance> NodeSoftClass(GeneratedClassPathString);

			LD::NodeRuleUtils::FNodeClassAllowedResult Result;
			if (OwningGraphNode.IsValid()
				&& !LD::NodeRuleUtils::IsNodeClassAllowed(NodeSoftClass, OwningGraphNode.Get(), Result)
				&& (Result.bFailedFromOwningStateMachineRules
				|| FSMNodeInstanceUtils::ShouldHideFromDropDownIfRulesFail(NodeSoftClass)))
			{
				return false;
			}
		}

		return bAllowed;
	}
};


void SSMPropertyEditorNodeClass::Construct(const FArguments& InArgs,
                                           const TSharedPtr<IPropertyHandle>& InPropertyHandle,
                                           const TWeakObjectPtr<USMGraphNode_Base>& InGraphNode)
{
	NodeClassPropertyHandle = InPropertyHandle;
	GraphNode = InGraphNode;
	ClassFilter = MakeShared<FSMNodeClassFilter>();
	
	// Fill in options
	ClassViewerOptions.Mode = EClassViewerMode::ClassPicker;
	ClassViewerOptions.NameTypeToDisplay = EClassViewerNameTypeToDisplay::ClassName;
	ClassViewerOptions.DisplayMode = EClassViewerDisplayMode::ListView;
	ClassViewerOptions.InitiallySelectedClass = nullptr;
	
	ClassFilter->OwningGraphNode = GraphNode;
	ClassFilter->AllowedChildrenOfClasses.Add(GraphNode->GetDefaultNodeClass());
	ClassFilter->DisallowedClassFlags = CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown | CLASS_Abstract | CLASS_Transient;
	ClassViewerOptions.ClassFilters.Add(ClassFilter.ToSharedRef());

	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");
	ClassFilterFuncs = ClassViewerModule.CreateFilterFuncs();
	
	ChildSlot
	[
		SAssignNew(NodeClassComboButton, SComboButton)
		.OnGetMenuContent(this, &SSMPropertyEditorNodeClass::GenerateNodeClassSelector)
		.ContentPadding(0)
		.ToolTipText(this, &SSMPropertyEditorNodeClass::GetNodeClassDisplayValue)
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SSMPropertyEditorNodeClass::GetNodeClassDisplayValue)
			]
		]
	];
}

UObject* SSMPropertyEditorNodeClass::LoadDragDropObject(const TSharedPtr<FAssetDragDropOp>& UnloadedClassOp)
{
	FString AssetPath;

	// Find the class/blueprint path
	if (UnloadedClassOp->HasAssets())
	{
		AssetPath = UnloadedClassOp->GetAssets()[0].GetObjectPathString();
	}
	else if (UnloadedClassOp->HasAssetPaths())
	{
		AssetPath = UnloadedClassOp->GetAssetPaths()[0];
	}

	// Check to see if the asset can be found, otherwise load it.
	UObject* Object = FindObject<UObject>(nullptr, *AssetPath);
	if (Object == nullptr)
	{
		// Load the package.
		GWarn->BeginSlowTask(LOCTEXT("OnDrop_LoadPackage", "Fully Loading Package For Drop"), true, false);

		Object = LoadObject<UObject>(nullptr, *AssetPath);

		GWarn->EndSlowTask();
	}

	return Object;
}

void SSMPropertyEditorNodeClass::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const TSharedPtr<FAssetDragDropOp> UnloadedClassOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (UnloadedClassOp.IsValid())
	{
		UObject* Object = LoadDragDropObject(UnloadedClassOp);

		bool bOK = false;

		if (const UClass* Class = Cast<UClass>(Object))
		{
			bOK = ClassFilter->IsClassAllowed(ClassViewerOptions, Class, ClassFilterFuncs.ToSharedRef());
		}
		else if (const UBlueprint* Blueprint = Cast<UBlueprint>(Object))
		{
			if (Blueprint->GeneratedClass)
			{
				bOK = ClassFilter->IsClassAllowed(ClassViewerOptions, Blueprint->GeneratedClass, ClassFilterFuncs.ToSharedRef());
			}
		}
		
		if (bOK)
		{
			UnloadedClassOp->SetToolTip(FText::GetEmpty(), FSMUnrealAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")));
		}
		else
		{
			UnloadedClassOp->SetToolTip(FText::GetEmpty(), FSMUnrealAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")));
		}
	}
}

void SSMPropertyEditorNodeClass::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	const TSharedPtr<FAssetDragDropOp> UnloadedClassOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (UnloadedClassOp.IsValid())
	{
		UnloadedClassOp->ResetToDefaultToolTip();
	}
}

FReply SSMPropertyEditorNodeClass::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const TSharedPtr<FClassDragDropOp> ClassOperation = DragDropEvent.GetOperationAs<FClassDragDropOp>();
	if (ClassOperation.IsValid())
	{
		// We can only drop one item into the combo box, so drop the first one.
		const FString ClassPath = ClassOperation->ClassesToDrop[0]->GetPathName();

		// Set the property, it will be verified as valid.
		SendToObjects(ClassPath);

		return FReply::Handled();
	}

	const TSharedPtr<FAssetDragDropOp> UnloadedClassOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (UnloadedClassOp.IsValid())
	{
		FString AssetPath;

		// Find the class/blueprint path
		if (UnloadedClassOp->HasAssets())
		{
			AssetPath = UnloadedClassOp->GetAssets()[0].GetObjectPathString();
		}
		else if (UnloadedClassOp->HasAssetPaths())
		{
			AssetPath = UnloadedClassOp->GetAssetPaths()[0];
		}

		// Check to see if the asset can be found, otherwise load it.
		UObject* Object = FindObject<UObject>(nullptr, *AssetPath);
		if(Object == nullptr)
		{
			// Load the package.
			GWarn->BeginSlowTask(LOCTEXT("OnDrop_LoadPackage", "Fully Loading Package For Drop"), true, false);

			Object = LoadObject<UObject>(nullptr, *AssetPath);

			GWarn->EndSlowTask();
		}

		if (const UClass* Class = Cast<UClass>(Object))
		{
			if (ClassFilter->IsClassAllowed(ClassViewerOptions, Class, ClassFilterFuncs.ToSharedRef()))
			{
				// This was pointing to a class directly
				SendToObjects(Class->GetPathName());
			}
		}
		else if (const UBlueprint* Blueprint = Cast<UBlueprint>(Object))
		{
			if (Blueprint->GeneratedClass)
			{
				if (ClassFilter->IsClassAllowed(ClassViewerOptions, Blueprint->GeneratedClass, ClassFilterFuncs.ToSharedRef()))
				{
					// This was pointing to a blueprint, get generated class
					SendToObjects(Blueprint->GeneratedClass->GetPathName());
				}
			}
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget> SSMPropertyEditorNodeClass::GenerateNodeClassSelector()
{
	if (!GraphNode.IsValid())
	{
		return SNullWidget::NullWidget;
	}
	
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	return ClassViewerModule.CreateClassViewer(ClassViewerOptions, FOnClassPicked::CreateRaw(this, &SSMPropertyEditorNodeClass::OnNodeClassSelected));
}

void SSMPropertyEditorNodeClass::OnNodeClassSelected(UClass* InClass)
{
	const FPropertyAccess::Result Result = NodeClassPropertyHandle->SetValue(InClass);
	ensure(Result != FPropertyAccess::Result::Fail);

	NodeClassComboButton->SetIsOpen(false);
}

const FSlateBrush* SSMPropertyEditorNodeClass::GetNodeClassIcon() const
{
	UObject* CurrentValue = nullptr;
	const FPropertyAccess::Result Result = NodeClassPropertyHandle->GetValue(CurrentValue);
	if(Result == FPropertyAccess::Success && CurrentValue != nullptr)
	{
		const UClass* Class = CastChecked<UClass>(CurrentValue);
		return FSlateIconFinder::FindIconBrushForClass(Class);
	}
						
	return nullptr;
}

FText SSMPropertyEditorNodeClass::GetNodeClassDisplayValue() const
{
	// Similar to SPropertyEditor::GetClassDisplayName but we never use display name.
	
	UObject* CurrentValue = nullptr;
	const FPropertyAccess::Result Result = NodeClassPropertyHandle->GetValue(CurrentValue);
	if(Result == FPropertyAccess::Success && CurrentValue != nullptr)
	{
		const UClass* Class = Cast<UClass>(CurrentValue);
		if (Class != nullptr)
		{
			if(const UBlueprint* BP = UBlueprint::GetBlueprintFromClass(Class))
			{
				return FText::FromString(BP->GetName());
			}
		}
		
		const FString Name = Class ? Class->GetName() : "None";
		return FText::FromString(Name);
	}

	return FText::GetEmpty();
}

void SSMPropertyEditorNodeClass::SendToObjects(const FString& NewValue)
{
	if(NodeClassPropertyHandle.IsValid())
	{
		NodeClassPropertyHandle->SetValueFromFormattedString(NewValue);
	}
}

#undef LOCTEXT_NAMESPACE
