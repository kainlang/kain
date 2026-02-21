// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMEditorCustomization.h"

#include "Configuration/SMProjectEditorSettings.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_RuntimeNodeContainer.h"
#include "Graph/Nodes/SMGraphNode_AnyStateNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "ISMSystemEditorModule.h"
#include "SMSystemEditorLog.h"
#include "SSMPropertyEditorNodeClass.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMNodeInstanceUtils.h"
#include "Utilities/SMNodeRuleUtils.h"
#include "Utilities/SMPropertyUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "SMUtils.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "SMEditorCustomization"

void FSMBaseCustomization::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
	DetailBuilderPtr = DetailBuilder;
	CustomizeDetails(*DetailBuilder);
}

void FSMBaseCustomization::HideNestedCategoryHandles(const TSharedPtr<IPropertyHandle>& InHandle)
{
	if (InHandle.IsValid())
	{
		InHandle->MarkHiddenByCustomization();
		uint32 HandleNumChildren;
		InHandle->GetNumChildren(HandleNumChildren);

		for (uint32 CIdx = 0; CIdx < HandleNumChildren; ++CIdx)
		{
			TSharedPtr<IPropertyHandle> ChildProperty = InHandle->GetChildHandle(CIdx);
			HideNestedCategoryHandles(ChildProperty);
		}
	}
}

void FSMBaseCustomization::ForceUpdate()
{
	if (IDetailLayoutBuilder* DetailBuilder = DetailBuilderPtr.Pin().Get())
	{
		DetailBuilder->ForceRefreshDetails();
	}
}

void FSMBaseCustomization::HideAnyStateTags(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& AnyStateCategory = DetailBuilder.EditCategory(TEXT("Any State"));
	TArray<TSharedRef<IPropertyHandle>> AnyStateProperties;
	AnyStateCategory.GetDefaultProperties(AnyStateProperties);

	for (const TSharedRef<IPropertyHandle>& Handle : AnyStateProperties)
	{
		check(Handle->IsValidHandle() && Handle->GetProperty());
		if (Handle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(USMGraphNode_StateNodeBase, AnyStateTags))
		{
			// Because AnyStateTags has special unreal customization we have to manually find the category property
			// and hide. DetailBuilder.GetProperty() will not work.
			DetailBuilder.HideProperty(Handle);
		}
	}
}

void FSMNodeCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	SelectedGraphNode.Reset();
	USMGraphNode_Base* GraphNode = GetObjectBeingCustomized<USMGraphNode_Base>(DetailBuilder);
	if (!GraphNode)
	{
		return;
	}

	SelectedGraphNode = GraphNode;

	if (USMGraphNode_AnyStateNode* AnyState = Cast<USMGraphNode_AnyStateNode>(GraphNode))
	{
		IDetailCategoryBuilder& StateCategory = DetailBuilder.EditCategory(TEXT("State"));
		StateCategory.SetCategoryVisibility(false);

		IDetailCategoryBuilder& ClassCategory = DetailBuilder.EditCategory(TEXT("Class"));
		ClassCategory.SetCategoryVisibility(false);

		IDetailCategoryBuilder& DisplayCategory = DetailBuilder.EditCategory(TEXT("Display"));
		DisplayCategory.SetCategoryVisibility(false);

		HideAnyStateTags(DetailBuilder);
	}
	else if (USMGraphNode_StateMachineEntryNode* EntryNode = Cast<USMGraphNode_StateMachineEntryNode>(GraphNode))
	{
		HideAnyStateTags(DetailBuilder);
	}

	// Hide parallel categories from nodes that don't support them.
	{
		if (USMGraphNode_ConduitNode* Conduit = Cast<USMGraphNode_ConduitNode>(GraphNode))
		{
			IDetailCategoryBuilder& ParallelCategory = DetailBuilder.EditCategory("Parallel States");
			ParallelCategory.SetCategoryVisibility(false);
		}

		if (USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(GraphNode))
		{
			if (USMGraphNode_StateNodeBase* PrevNode = Transition->GetFromState())
			{
				if (PrevNode->IsA<USMGraphNode_ConduitNode>())
				{
					IDetailCategoryBuilder& ParallelCategory = DetailBuilder.EditCategory("Parallel States");
					ParallelCategory.SetCategoryVisibility(false);
				}
			}
		}
	}

	// Link to node guid.
	if (GraphNode->GetClass()->IsChildOf(USMGraphNode_StateNodeBase::StaticClass()))
	{
		if (const FSMNode_Base* RuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(GraphNode->GetBoundGraph()))
		{
			FGuid& GuidStructure = const_cast<FGuid&>(RuntimeNode->GetNodeGuid());
			const TSharedPtr<FStructOnScope> StructToDisplay = MakeShareable(new FStructOnScope(TBaseStructure<FGuid>::Get(),
			                                                                                    reinterpret_cast<uint8*>(&GuidStructure)));

			IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("GraphNodeDetail", /* From BlueprintDetailsCustomization */
				LOCTEXT("GraphNodeDetailsCategory", "Graph Node"), ECategoryPriority::Important);
			IDetailPropertyRow* GuidRow = Category.AddExternalStructure(StructToDisplay, EPropertyLocation::Advanced);
			check(GuidRow);
			GuidRow->DisplayName(LOCTEXT("NodeGuidDisplayName", "Node Guid"));
			GuidRow->ToolTip(LOCTEXT("NodeGuidTooltip", "NodeGuid must always be unique. Do not duplicate the guid in any other node in any blueprint.\
\n\
\nThis is not the same guid that is used at run-time. At run-time all NodeGuids in a path to a node\
\nare hashed to form the PathGuid. This is done to account for multiple references and parent graph calls.\
\n\
\nIf you need to change the path of a node (such as collapse it to a nested state machine) and you need to maintain\
\nthe old guid for run-time saves to work, you should use the GuidRedirectMap on the primary state machine instance\
\nwhich accepts PathGuids."));

			GuidRow->GetPropertyHandle()->SetOnChildPropertyValuePreChange(FSimpleDelegate::CreateLambda([&]()
			{
				if (SelectedGraphNode.IsValid())
				{
					if (USMGraphK2Node_RuntimeNodeContainer* ContainerNode =
						FSMBlueprintEditorUtils::GetRuntimeContainerFromGraph(SelectedGraphNode->GetBoundGraph()))
					{
						ContainerNode->Modify();
					}
				}
			}));
			
			GuidRow->GetPropertyHandle()->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateLambda([&]()
			{
				if (SelectedGraphNode.IsValid())
				{
					if (UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintFromObject(SelectedGraphNode.Get()))
					{
						FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
					}
				}
			}));
		}
	}
	const FName NodeClassPropertyName = GraphNode->GetNodeClassPropertyName();
	if (NodeClassPropertyName != NAME_None)
	{
		const TSharedRef<IPropertyHandle> NodeClassPropertyHandle = DetailBuilder.GetProperty(NodeClassPropertyName);
		
		IDetailPropertyRow* PropertyRow = DetailBuilder.EditDefaultProperty(NodeClassPropertyHandle);
		check(PropertyRow);
		
		PropertyRow->CustomWidget()
		.NameContent()
		[
			NodeClassPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SSMPropertyEditorNodeClass, NodeClassPropertyHandle, GraphNode)
		]
		.OverrideResetToDefault(FResetToDefaultOverride::Create(
		FIsResetToDefaultVisible::CreateLambda([this](const TSharedPtr<IPropertyHandle>& InPropertyHandle)
		{
			if (DetailBuilderPtr.IsValid() && InPropertyHandle.IsValid())
			{
				if (const USMGraphNode_Base* ThisGraphNode = GetObjectBeingCustomized<USMGraphNode_Base>(*DetailBuilderPtr.Pin()))
				{
					const UClass* DefaultNodeClass = ThisGraphNode->GetDefaultNodeClass();
					
					UObject* CurrentSelection = nullptr;
					// Make sure it isn't already default.
					if (FPropertyAccess::Success == InPropertyHandle->GetValue(CurrentSelection))
					{
						if (CurrentSelection == DefaultNodeClass)
						{
							return false;
						}
					}
					
					LD::NodeRuleUtils::FNodeClassAllowedResult Result;
					return !(!LD::NodeRuleUtils::IsNodeClassAllowed(DefaultNodeClass, ThisGraphNode, Result)
						&& (Result.bFailedFromOwningStateMachineRules
						|| FSMNodeInstanceUtils::ShouldHideFromDropDownIfRulesFail(DefaultNodeClass)));
				}
			}
			
			return false;
		}),
		FResetToDefaultHandler::CreateLambda([](const TSharedPtr<IPropertyHandle>& InPropertyHandle)
		{
			if (InPropertyHandle.IsValid())
			{
				InPropertyHandle->ResetToDefault();
			}
		})));
	}
}

TSharedRef<IDetailCustomization> FSMNodeCustomization::MakeInstance()
{
	return MakeShareable(new FSMNodeCustomization);
}

void FSMNodeInstanceCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	SelectedGraphNode.Reset();
	if (USMNodeInstance* NodeInstance = GetObjectBeingCustomized<USMNodeInstance>(DetailBuilder))
	{
		if (USMGraphNode_Base* GraphNode = Cast<USMGraphNode_Base>(NodeInstance->GetOuter()))
		{
			SelectedGraphNode = GraphNode;
		}
	
		if (!SelectedGraphNode.IsValid())
		{
			if (const USMTransitionInstance* TransitionInstance = Cast<USMTransitionInstance>(NodeInstance))
			{
				// Special handling for Transition CDO that shouldn't have exposed property configuration.
				if (TransitionInstance->IsTemplate(RF_ClassDefaultObject))
				{
					DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(USMTransitionInstance, bEvalDefaultProperties));
					DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(USMTransitionInstance, bAutoEvalExposedProperties));
					DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(USMTransitionInstance, ExposedPropertyOverrides));
				}
			}
			else if (NodeInstance->IsTemplate(RF_ClassDefaultObject))
			{
				if (FSMBlueprintEditorUtils::GetProjectEditorSettings()->bEnableVariableCustomization)
				{
					// Variable customization will handle this.
					DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(USMNodeInstance, ExposedPropertyOverrides));
				}
			}
		
			// Should only be invalid when editing in the node class editor, in which case everything should be displayed.
			return;
		}
	
		TArray<TSharedRef<IPropertyHandle>> PropertyHandles;
		for (TFieldIterator<FProperty> PropIt(NodeInstance->GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
		{
			const FName PropertyName = PropIt->GetFName();
			TSharedRef<IPropertyHandle> PropertyHandle = DetailBuilder.GetProperty(PropertyName, PropIt->GetOwnerClass());
			if (PropertyHandle->IsValidHandle())
			{
				PropertyHandles.Add(PropertyHandle);
			}
		}

		ProcessNodeInstance(SelectedGraphNode, PropertyHandles, NodeInstance, DetailBuilder);

		// Allow users to further customize the details panel.
		FModuleManager::GetModuleChecked<ISMSystemEditorModule>(LOGICDRIVER_EDITOR_MODULE_NAME)
		.GetExtendNodeInstanceDetails().Broadcast(DetailBuilder);

		// Don't enable alphabetical sorting yet, some categories should be first like GraphNode and Class.
		//DetailBuilder.SortCategories(SortCategories);
	}
}

void FSMNodeInstanceCustomization::ProcessNodeInstance(TWeakObjectPtr<USMGraphNode_Base> GraphNode, const TArray<TSharedRef<IPropertyHandle>>& TemplateProperties,
	class USMNodeInstance* NodeInstance, IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TSharedRef<IPropertyHandle>> ContainerPropertyHandles;
	for (const TSharedRef<IPropertyHandle>& TemplatePropertyHandle : TemplateProperties)
	{
		if (const FProperty* Property = TemplatePropertyHandle->GetProperty())
		{
			if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(USMGraphNode_StateNode, StateStack))
			{
				// Don't hide, struct customization will handle this.
				continue;
			}

			// Check for and hide properties which are designed to be edited from class defaults only.
			if (Property->HasMetaData("InstancedTemplate") || (NodeInstance && NodeInstance->GetTemplateGuid().IsValid() && Property->HasMetaData("NodeBaseOnly")))
			{
				HideNestedCategoryHandles(TemplatePropertyHandle);
				continue;
			}

			// Process non-containers first so their customizations are applied before the container edits the category.
			if (!LD::PropertyUtils::IsPropertyHandleExposedContainer(TemplatePropertyHandle))
			{
				DisplayExposedPropertyWidget(GraphNode, TemplatePropertyHandle, NodeInstance, &DetailBuilder);
			}
			else
			{
				ContainerPropertyHandles.Add(TemplatePropertyHandle);
			}
		}
	}

	// Containers need to be generated last as they edit categories which prevents other customizations from applying after.
	for (const TSharedRef<IPropertyHandle>& TemplatePropertyHandle : ContainerPropertyHandles)
	{
		DisplayExposedPropertyWidget(GraphNode, TemplatePropertyHandle, NodeInstance, &DetailBuilder);
	}
}

void FSMNodeInstanceCustomization::DisplayExposedPropertyWidget(TWeakObjectPtr<USMGraphNode_Base> GraphNode, const TSharedRef<IPropertyHandle>& PropertyHandle, USMNodeInstance* NodeInstance,
	IDetailLayoutBuilder* DetailBuilder, IDetailChildrenBuilder* ChildrenBuilder)
{
	if (FProperty* Property = PropertyHandle->GetProperty())
	{
		if (LD::PropertyUtils::IsPropertyExposedToGraphNode(Property) && GraphNode->SupportsPropertyGraphs())
		{
			// Array properties will rely on custom array builders to generate their elements.
			if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				PropertyHandle->MarkHiddenByCustomization();

				// EditCategory won't work with nested categories. CustomBuilders require EditCategory at this stage.
				TArray<FString> Categories;
				FSMBlueprintEditorUtils::SplitCategories(PropertyHandle->GetDefaultCategoryName().ToString(), Categories);
			
				const FName ExposedArrayCategoryName = Categories.Num() > 0 ? *Categories[0] : FName("Default");

				const TSharedRef<FDetailArrayBuilder> ArrayBuilder = MakeShareable(new FDetailArrayBuilder(PropertyHandle));
				ArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateStatic(&FSMNodeInstanceCustomization::GenerateGraphArrayWidget,
					GraphNode, NodeInstance, FText::FromName(ExposedArrayCategoryName)));
		
				if (ChildrenBuilder)
				{
					// State stack builder.
					ChildrenBuilder->AddCustomBuilder(ArrayBuilder);
				}
				else if (DetailBuilder)
				{
					// Normal display such as for a node template.
					IDetailCategoryBuilder& CategoryBuilder = DetailBuilder->EditCategory(ExposedArrayCategoryName);
					CategoryBuilder.AddCustomBuilder(ArrayBuilder);

					if (Categories.Num() > 1)
					{
						// Nested categories may still be present under this grouping but will have
						// no property present. Clean them up.
						
						TArray<TSharedRef<IPropertyHandle>> ChildProperties;
						CategoryBuilder.GetDefaultProperties(ChildProperties);

						for (const TSharedRef<IPropertyHandle>& ChildProperty : ChildProperties)
						{
							HideEmptyCategoryHandles(ChildProperty, ENodeStackType::None);
						}
					}
				}
		
				return;
			}

			// Single element processing.
			FSMGraphProperty_Base PropertyLookup;
			const FGuid& PropertyGuid = LD::PropertyUtils::SetGraphPropertyFromProperty(PropertyLookup, Property, NodeInstance);
			if (!PropertyGuid.IsValid())
			{
				return;
			}

			if (USMGraphK2Node_PropertyNode_Base* GraphPropertyNode = GraphNode->GetGraphPropertyNode(PropertyGuid))
			{
				const TWeakObjectPtr<USMGraphK2Node_PropertyNode_Base> GraphPropertyWeakPtr = MakeWeakObjectPtr(
					GraphPropertyNode);

				const FResetToDefaultOverride ResetToDefaultOverride = FResetToDefaultOverride::Create(
					FIsResetToDefaultVisible::CreateStatic(&FSMNodeInstanceCustomization::IsResetToDefaultVisible, GraphPropertyWeakPtr),
					FResetToDefaultHandler::CreateStatic(&FSMNodeInstanceCustomization::OnResetToDefaultClicked, GraphPropertyWeakPtr));

				if (ChildrenBuilder)
				{
					// State stack builder.
					PropertyHandle->MarkHiddenByCustomization();
					IDetailPropertyRow& PropertyRow = ChildrenBuilder->AddProperty(PropertyHandle);
					PropertyRow.ShowPropertyButtons(false);
					PropertyRow.OverrideResetToDefault(ResetToDefaultOverride);
					PropertyRow.CustomWidget()
					.NameContent()
					[
						PropertyHandle->CreatePropertyNameWidget()
					]
					.ValueContent()
					[
						GraphPropertyNode->GetGraphDetailWidget().ToSharedRef()
					];
				}
				else if (DetailBuilder)
				{
					// Normal display such as for a node template.
					if (IDetailPropertyRow* PropertyRow = DetailBuilder->EditDefaultProperty(PropertyHandle))
					{
						PropertyRow->ShowPropertyButtons(false);
						PropertyRow->OverrideResetToDefault(ResetToDefaultOverride);
						PropertyRow->CustomWidget()
						.NameContent()
						[
							PropertyHandle->CreatePropertyNameWidget()
						]
						.ValueContent()
						[
							GraphPropertyNode->GetGraphDetailWidget().ToSharedRef()
						];
					}
				}
			}
		}
	}
}

USMNodeInstance* FSMNodeInstanceCustomization::GetCorrectNodeInstanceFromPropertyHandle(
	TWeakObjectPtr<USMGraphNode_Base> GraphNode, TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	USMNodeInstance* NodeTemplate = nullptr;

	check(GraphNode.IsValid());
	check(InPropertyHandle.IsValid());

	// Find the owning stack property handle to determine the stack template index to use.
	TSharedPtr<IPropertyHandle> NodeStackPropertyHandle = InPropertyHandle->GetParentHandle();

	const FName NodeStatePropertyName = GraphNode->GetNodeStackPropertyName();
	if (!NodeStatePropertyName.IsNone())
	{
		while (NodeStackPropertyHandle.IsValid() && (!NodeStackPropertyHandle->GetProperty() ||
			NodeStackPropertyHandle->GetProperty()->GetFName() != NodeStatePropertyName))
		{
			NodeStackPropertyHandle = NodeStackPropertyHandle->GetParentHandle();
		}
	}

	if (NodeStackPropertyHandle.IsValid())
	{
		const int32 Idx = NodeStackPropertyHandle->GetIndexInArray();
		NodeTemplate = GraphNode->GetTemplateFromIndex(Idx);
	}

	if (!NodeTemplate)
	{
		NodeTemplate = GraphNode->GetNodeTemplate();
	}

	return NodeTemplate;
}

bool FSMNodeInstanceCustomization::IsResetToDefaultVisible(TSharedPtr<IPropertyHandle> PropertyHandle,
	TWeakObjectPtr<USMGraphK2Node_PropertyNode_Base> GraphPropertyNode)
{
	if (GraphPropertyNode.IsValid())
	{
		return GraphPropertyNode->IsValueModifiedOrWired();
	}

	return false;
}

void FSMNodeInstanceCustomization::OnResetToDefaultClicked(TSharedPtr<IPropertyHandle> PropertyHandle,
	TWeakObjectPtr<USMGraphK2Node_PropertyNode_Base> GraphPropertyNode)
{
	if (GraphPropertyNode.IsValid())
	{
		GraphPropertyNode->ResetProperty();
	}
}

bool FSMNodeInstanceCustomization::HideEmptyCategoryHandles(const TSharedPtr<IPropertyHandle>& InHandle,
	ENodeStackType NodeStackType)
{
	if (InHandle.IsValid())
	{
		if (const FProperty* Property = InHandle->GetProperty())
		{
			bool bHidden = false;
			if (NodeStackType != ENodeStackType::None)
			{
				// Stacks should always hide if this property is exposed since a child builder displays that.
				bHidden = (Property->HasMetaData("InstancedTemplate") || Property->HasMetaData("NodeBaseOnly")) ||
					(NodeStackType == ENodeStackType::StateStack && LD::PropertyUtils::IsPropertyExposedToGraphNode(Property));

				if (!bHidden && NodeStackType == ENodeStackType::TransitionStack)
				{
					const FName PropertyName = Property->GetFName();
					if (PropertyName == TEXT("bUseCustomColors") || PropertyName == TEXT("NodeColor"))
					{
						// State stack allows these to be customized, but they aren't relevant to the transition stack.
						InHandle->MarkHiddenByCustomization();
						bHidden = true;
					}
				}
			}
			else
			{
				// Base states display the properties in their normal categories unless they are containers.
				bHidden = LD::PropertyUtils::IsPropertyHandleExposedContainer(InHandle);
			}
			return bHidden;
		}

		uint32 HandleNumChildren;
		InHandle->GetNumChildren(HandleNumChildren);

		bool bAreAllChildrenEmpty = true;
		for (uint32 CIdx = 0; CIdx < HandleNumChildren; ++CIdx)
		{
			TSharedPtr<IPropertyHandle> ChildProperty = InHandle->GetChildHandle(CIdx);
			const bool bIsChildEmpty = HideEmptyCategoryHandles(ChildProperty, NodeStackType);
			if (!bIsChildEmpty)
			{
				bAreAllChildrenEmpty = false;
				continue;
			}

			ChildProperty->MarkHiddenByCustomization();
		}

		if (bAreAllChildrenEmpty)
		{
			InHandle->MarkHiddenByCustomization();
		}

		return bAreAllChildrenEmpty;
	}

	return true;
}

TSharedRef<IDetailCustomization> FSMNodeInstanceCustomization::MakeInstance()
{
	return MakeShareable(new FSMNodeInstanceCustomization);
}

void FSMNodeInstanceCustomization::GenerateGraphArrayWidget(TSharedRef<IPropertyHandle> PropertyHandle,
                                                            int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder,
															TWeakObjectPtr<USMGraphNode_Base> SelectedNode, USMNodeInstance* NodeInstance,
															FText FilterString)
{
	if (!SelectedNode.IsValid())
	{
		return;
	}

	NodeInstance = GetCorrectNodeInstanceFromPropertyHandle(SelectedNode, PropertyHandle);

	IDetailPropertyRow& PropertyRow = ChildrenBuilder.AddProperty(PropertyHandle);

	FSMGraphProperty_Base PropertyLookup;
	const FGuid& PropertyGuid = LD::PropertyUtils::SetGraphPropertyFromProperty(PropertyLookup, PropertyHandle->GetProperty(), NodeInstance, ArrayIndex);
	if (!PropertyGuid.IsValid())
	{
		return;
	}

	PropertyRow.OverrideResetToDefault(FResetToDefaultOverride::Create(
		FIsResetToDefaultVisible::CreateLambda([=](const TSharedPtr<IPropertyHandle> InPropertyHandle)
		{
			if (SelectedNode.IsValid())
			{
				if (const USMGraphK2Node_PropertyNode_Base* PropertyNode = SelectedNode->GetGraphPropertyNode(PropertyLookup.VariableName, NodeInstance, ArrayIndex))
				{
					return PropertyNode->IsValueModifiedOrWired();
				}
			}

			return false;
		}),
		FResetToDefaultHandler::CreateLambda([=](TSharedPtr<IPropertyHandle> InPropertyHandle)
		{
			if (SelectedNode.IsValid())
			{
				if (USMGraphK2Node_PropertyNode_Base* PropertyNode = SelectedNode->GetGraphPropertyNode(PropertyLookup.VariableName, NodeInstance, ArrayIndex))
				{
					PropertyNode->ResetProperty();
				}
			}
		})));

	if (const USMGraphK2Node_PropertyNode_Base* GraphPropertyNode = SelectedNode->GetGraphPropertyNode(PropertyGuid))
	{
		PropertyRow.CustomWidget(false)
			.CopyAction(FUIAction(FExecuteAction::CreateLambda([]()
			{
				// Disable for now.. variable arrays don't copy paste rows well.
				LDEDITOR_LOG_WARNING(TEXT("Copy and pasting public variable array rows is not supported. Duplicate and move the row via context menu instead."))
			})))
			.PasteAction(FUIAction(FExecuteAction::CreateLambda([]()
			{
				// Disable for now.. variable arrays don't copy paste rows well.
				LDEDITOR_LOG_WARNING(TEXT("Copy and pasting public variable array rows is not supported. Duplicate and move the row via context menu instead."))
			})))
			.AddCustomContextMenuAction(FUIAction(FExecuteAction::CreateLambda([=]()
			{
				const TSharedPtr<IPropertyHandleArray> ArrayHandle = PropertyHandle->GetParentHandle()->AsArray();
				check(ArrayHandle.IsValid());

				uint32 NumElements;
				ArrayHandle->GetNumElements(NumElements);

				int32 DestinationIndex = ArrayIndex - 1;
				if (DestinationIndex < 0)
				{
					DestinationIndex = static_cast<int32>(NumElements) - 1;
				}

				SelectedNode->NotifySwapPropertyGraphArrayElements(PropertyHandle->GetParentHandle()->GetProperty()->GetFName(),
					DestinationIndex, ArrayIndex, NodeInstance);
				ArrayHandle->SwapItems(ArrayIndex, DestinationIndex);
			})), LOCTEXT("MoveArrayElementUp_Label", "Move Up"),
			LOCTEXT("MoveArrayElementUp_Tooltip", "Swap this index with the element above it"))
			.AddCustomContextMenuAction(FUIAction(FExecuteAction::CreateLambda([=]()
			{
				const TSharedPtr<IPropertyHandleArray> ArrayHandle = PropertyHandle->GetParentHandle()->AsArray();
				check(ArrayHandle.IsValid());

				uint32 NumElements;
				ArrayHandle->GetNumElements(NumElements);

				int32 DestinationIndex = ArrayIndex + 1;
				if (DestinationIndex >= static_cast<int32>(NumElements))
				{
					DestinationIndex = 0;
				}

				SelectedNode->NotifySwapPropertyGraphArrayElements(PropertyHandle->GetParentHandle()->GetProperty()->GetFName(),
					DestinationIndex, ArrayIndex, NodeInstance);
				ArrayHandle->SwapItems(ArrayIndex, DestinationIndex);
			})), LOCTEXT("MoveArrayElementDown_Label", "Move Down"),
			LOCTEXT("MoveArrayElementDown_Tooltip", "Swap this index with the element below it"))
			.NameContent()
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				GraphPropertyNode->GetGraphDetailWidget().ToSharedRef()
			]
			.FilterString(FilterString);
	}
}

void FSMNodeInstanceCustomization::SortCategories(const TMap<FName, IDetailCategoryBuilder*>& AllCategoryMap)
{
	// TODO: Not used yet. Commented out in FSMNodeInstanceCustomization::CustomizeDetails.
	
	TArray<FString> KeysString;
	KeysString.Reserve(AllCategoryMap.Num());

	for (const auto& KeyVal : AllCategoryMap)
	{
		KeysString.Add(KeyVal.Key.ToString());
	}
	
	KeysString.Sort();

	for (int32 Idx = 0; Idx < KeysString.Num(); ++Idx)
	{
		const FName& Key = *KeysString[Idx];

		if (IDetailCategoryBuilder* const* Value = AllCategoryMap.Find(Key))
		{
			(*Value)->SetSortOrder(Idx);
		}
	}
}

USMGraphNode_Base* FSMStructCustomization::GetGraphNodeBeingCustomized(
	IPropertyTypeCustomizationUtils& StructCustomizationUtils, bool bCheckParent) const
{
	return GetObjectBeingCustomized<USMGraphNode_Base>(StructCustomizationUtils, bCheckParent);
}

TSet<FName> FSMStructCustomization::RegisteredStructs;

TSharedRef<IPropertyTypeCustomization> FSMGraphPropertyCustomization::MakeInstance()
{
	return MakeShared<FSMGraphPropertyCustomization>();
}

void FSMGraphPropertyCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FSMStructCustomization::CustomizeHeader(StructPropertyHandle, HeaderRow, StructCustomizationUtils);
	
	USMGraphNode_Base* GraphNode = GetGraphNodeBeingCustomized(StructCustomizationUtils);

	// This isn't a graph node containing this property. Use the default display.
	if (!GraphNode)
	{
		HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			StructPropertyHandle->CreatePropertyValueWidget()
		];
		return;
	}

	// HACK to get around the reset to defaults button from showing up when using NameContent and ValueContent below.
	// NoResetToDefault is a property level metadata and since this property can be added by a user it won't be reliable.
	// EditFixedSize is checked in FPropertyHandleBase::CanResetToDefault() and will always be false if this is set.
	StructPropertyHandle->GetProperty()->SetPropertyFlags(CPF_EditFixedSize);

	FProperty* Property = CastField<FProperty>(StructPropertyHandle->GetProperty());
	if (!Property)
	{
		return;
	}

	USMNodeInstance* NodeTemplate = FSMNodeInstanceCustomization::GetCorrectNodeInstanceFromPropertyHandle(GraphNode, StructPropertyHandle);
	if (!NodeTemplate)
	{
		return;
	}
	
	TArray<FSMGraphProperty_Base*> GraphProperties;
	USMUtils::BlueprintPropertyToNativeProperty<FSMGraphProperty_Base>(Property, NodeTemplate, GraphProperties);

	const int32 Index = FMath::Max(StructPropertyHandle->GetIndexInArray(), 0);
	if (Index < GraphProperties.Num())
	{
		const FSMGraphProperty_Base* GraphProperty = GraphProperties[Index];
		check(GraphProperty);
		const USMGraphK2Node_PropertyNode_Base* GraphPropertyNode = GraphNode->GetGraphPropertyNode(GraphProperty->GetGuid());
		if (!GraphPropertyNode)
		{
			return;
		}

		HeaderRow
			.NameContent()
			[
				StructPropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					GraphPropertyNode->GetGraphDetailWidget().ToSharedRef()
				]
			];
	}
}

void FSMGraphPropertyCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const USMGraphNode_Base* GraphNode = GetGraphNodeBeingCustomized(StructCustomizationUtils);
	// Don't show children if we are on state machine graph.
	if (GraphNode)
	{
		return;
	}
	
	// Build out default properties as if this wasn't being customized.
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		const TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		if (!ChildHandle->GetProperty()->HasMetaData(TEXT("ExposeOverrideOnly")))
		{
			// Structs will have been registered unless part of expose override so hide any properties that shouldn't be displayed.
			// This customization won't be called for ExposedPropertyOverrides.
			
			StructBuilder.AddProperty(ChildHandle);
		}
	}
}

#undef LOCTEXT_NAMESPACE
