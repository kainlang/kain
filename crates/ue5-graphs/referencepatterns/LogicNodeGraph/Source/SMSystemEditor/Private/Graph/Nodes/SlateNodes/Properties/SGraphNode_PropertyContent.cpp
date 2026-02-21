// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SGraphNode_PropertyContent.h"

#include "SMSystemEditorLog.h"
#include "SSMGraphPropertyTreeView.h"
#include "Configuration/SMEditorSettings.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/SlateNodes/Properties/SSMGraphProperty_Base.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMNodeInstanceUtils.h"
#include "Utilities/SMPropertyUtils.h"

#include "SMNodeInstance.h"

#include "SMUnrealTypeDefs.h"

#define LOCTEXT_NAMESPACE "SSMGraphNode_PropertyContent"

SSMGraphNode_PropertyContent::SSMGraphNode_PropertyContent(): GraphNode(nullptr)
{
}

SSMGraphNode_PropertyContent::~SSMGraphNode_PropertyContent()
{
	GraphNode.Reset();
	PropertyWidgets.Empty();
}

void SSMGraphNode_PropertyContent::Construct(const FArguments& InArgs)
{
	GraphNode = InArgs._GraphNode;
	ChildSlot
	[
		CreateContent().ToSharedRef()
	];
}

void SSMGraphNode_PropertyContent::Finalize()
{
	for (const auto& PropertyWidget : NodeInstanceProperties)
	{
		PropertyWidget.Value->FinalizePropertyWidgets();
	}	
}

bool SSMGraphNode_PropertyContent::RefreshAllProperties()
{
	TMap<USMNodeInstance*, TArray<USMGraphK2Node_PropertyNode_Base*>> TemplatePropertyMap;
	MapTemplatesToProperties(TemplatePropertyMap);
	
	if (NodeInstanceProperties.Num() != TemplatePropertyMap.Num())
	{
		return false;
	}
	
	for (const auto& KeyVal : TemplatePropertyMap)
	{
		const USMNodeInstance* CurrentTemplate = KeyVal.Key;
		const TArray<USMGraphK2Node_PropertyNode_Base*>& PropertyNodes = KeyVal.Value;

		if (const TSharedPtr<SSMNodeInstancePropertyView>* PropertyView = NodeInstanceProperties.Find(CurrentTemplate))
		{
			(*PropertyView)->RefreshPropertyWidgets(PropertyNodes);
			continue;
		}
		
		return false;
	}

	return true;
}

TSharedPtr<SWidget> SSMGraphNode_PropertyContent::CreateContent()
{
	TSharedPtr<SVerticalBox> Content;
	TSharedPtr<SVerticalBox> NodePropertiesBox;
	SAssignNew(Content, SVerticalBox)
	+SVerticalBox::Slot()
	.AutoHeight()
	[
		SAssignNew(NodePropertiesBox, SVerticalBox)
	];

	PropertyWidgets.Reset();

	const USMGraphNode_StateNodeBase* StateNodeBase = CastChecked<USMGraphNode_StateNodeBase>(GraphNode);
	const USMEditorSettings* EditorSettings = FSMBlueprintEditorUtils::GetEditorSettings();

	TMap<USMNodeInstance*, TArray<USMGraphK2Node_PropertyNode_Base*>> TemplatePropertyMap;
	MapTemplatesToProperties(TemplatePropertyMap);

	NodeInstanceProperties.Reset();
	NodeInstanceProperties.Reserve(TemplatePropertyMap.Num());
	
	for (const auto& KeyVal : TemplatePropertyMap)
	{
		USMNodeInstance* CurrentTemplate = KeyVal.Key;
		const TArray<USMGraphK2Node_PropertyNode_Base*>& PropertyNodes = KeyVal.Value;
		
		TSharedPtr<SSMNodeInstancePropertyView> PropertyView;
		SAssignNew(PropertyView, SSMNodeInstancePropertyView, CurrentTemplate, PropertyNodes).GraphNode(GraphNode.Get());
		
		NodeInstanceProperties.Add(CurrentTemplate, PropertyView);

		for (const TTuple<USMGraphK2Node_PropertyNode_Base*, TSharedPtr<SSMGraphProperty_Base>>&
		     PropertyKeyVal : PropertyView->GetPropertyWidgets())
		{
			PropertyWidgets.Add(PropertyKeyVal.Value, PropertyKeyVal.Key);
		}

		TSharedPtr<SVerticalBox> NodeStackBox;
		
		// State stack handling.
		if (CurrentTemplate->GetTemplateGuid().IsValid())
		{
			FLinearColor BackgroundColor = StateNodeBase->GetBackgroundColorForNodeInstance(CurrentTemplate);
			BackgroundColor.A *= 0.25f;

			const FString StateStackName = FSMNodeInstanceUtils::GetNodeDisplayName(CurrentTemplate);
				
			Content->AddSlot()
				.AutoHeight()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 1.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(StateStackName))
						.TextStyle(FSMUnrealAppStyle::Get(), "NormalText.Important")
						.Visibility(EditorSettings->bDisplayStateStackClassNames ? EVisibility::Visible : EVisibility::Collapsed)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(FSMUnrealAppStyle::Get().GetBrush("Graph.StateNode.Body"))
						.Padding(2)
						.BorderBackgroundColor(BackgroundColor)
						[
							SAssignNew(NodeStackBox, SVerticalBox)
						]
					]
				];
		}

		const TSharedPtr<SVerticalBox> ContentBoxToUse = NodeStackBox.IsValid() ? NodeStackBox : NodePropertiesBox;
		
		ContentBoxToUse->AddSlot()
		[
			PropertyView.ToSharedRef()
		];
	}

	return Content;
}

void SSMGraphNode_PropertyContent::MapTemplatesToProperties(TMap<USMNodeInstance*, TArray<USMGraphK2Node_PropertyNode_Base*>>& OutTemplatePropertyMap)
{
	// Add custom property widgets sorted by user specification.
	USMGraphNode_StateNodeBase* StateNodeBase = CastChecked<USMGraphNode_StateNodeBase>(GraphNode);
	TArray<USMGraphK2Node_PropertyNode_Base*> GraphPropertyNodes = StateNodeBase->GetAllPropertyGraphNodesAsArray();
	
	TArray<USMGraphK2Node_PropertyNode_Base*> GraphPropertyBPVariablesOrdered;
	// Each property node mapped to its instance.
	TMap<USMGraphK2Node_PropertyNode_Base*, USMNodeInstance*> PropertiesToTemplates;
	
	// Populate all used state classes, in order.
	TArray<USMNodeInstance*> NodeTemplates;

	auto IsValidClass = [](USMGraphNode_Base* Node, UClass* NodeClass) {return NodeClass && NodeClass != Node->GetDefaultNodeClass(); };

	if (IsValidClass(StateNodeBase, StateNodeBase->GetNodeClass()))
	{
		if (USMNodeInstance* NodeTemplate = StateNodeBase->GetNodeTemplate())
		{
			NodeTemplates.Add(NodeTemplate);
		}
	}
	
	if (USMGraphNode_StateNode* StateNode = Cast<USMGraphNode_StateNode>(StateNodeBase))
	{
		for (const auto& StackTemplate : StateNode->StateStack)
		{
			if (StackTemplate.NodeStackInstanceTemplate)
			{
				if (IsValidClass(StateNodeBase, StackTemplate.NodeStackInstanceTemplate->GetClass()))
				{
					NodeTemplates.Add(StackTemplate.NodeStackInstanceTemplate);
				}
			}
		}
	}

	/*
	 * Look for array types and build out templates.
	 */
	auto ExpandAndSortProperty = [&GraphPropertyNodes, &GraphPropertyBPVariablesOrdered, &PropertiesToTemplates](USMGraphK2Node_PropertyNode_Base* GraphProperty, USMNodeInstance* NodeTemplate)
	{
		if (GraphProperty)
		{
			FSMGraphProperty_Base* RealPropertyNode = GraphProperty->GetPropertyNode();
			if (!RealPropertyNode)
			{
				return false;
			}

			// Look for array items that may belong to this property.
			TArray<USMGraphK2Node_PropertyNode_Base*> ArrayItems = GraphPropertyNodes.
				FilterByPredicate([&RealPropertyNode](const USMGraphK2Node_PropertyNode_Base* PropertyNode)
			{
				const FSMGraphProperty_Base* TestProperty = PropertyNode->GetPropertyNodeConst();
				if (!TestProperty)
				{
					return false;
				}
				return TestProperty->VariableName == RealPropertyNode->VariableName && TestProperty->GetTemplateGuid() == RealPropertyNode->GetTemplateGuid();
			});
			ArrayItems.Sort([&](USMGraphK2Node_PropertyNode_Base& lhs, USMGraphK2Node_PropertyNode_Base& rhs)
			{
				// Should never be null unless something was forcibly deleted or an underlying class removed.
				FSMGraphProperty_Base* PropertyLhs = lhs.GetPropertyNode();
				if (!PropertyLhs)
				{
					return false;
				}

				FSMGraphProperty_Base* PropertyRhs = rhs.GetPropertyNode();
				if (!PropertyRhs)
				{
					return false;
				}

				return PropertyLhs->ArrayIndex <= PropertyRhs->ArrayIndex;
			});

			// Add on array items in the correct order directly after the first element.
			for (USMGraphK2Node_PropertyNode_Base* ArrItem : ArrayItems)
			{
				GraphPropertyNodes.Remove(ArrItem);
				PropertiesToTemplates.Add(ArrItem, NodeTemplate);
			}
			
			GraphPropertyBPVariablesOrdered.Append(ArrayItems);
			PropertiesToTemplates.Add(GraphProperty, NodeTemplate);
			
			return true;
		}

		return false;
	};
	
	for (USMNodeInstance* NodeTemplate : NodeTemplates)
	{
		{
			// Check native properties first. Reference PropertyEditor/Private/PropertyEditorHelpers.cpp  ~ OrderPropertiesFromMetadata()
			
			TArray<FProperty*> NativeProperties;
			TArray<TTuple<FProperty*, int32>> NativePropertiesOrdered;
			TMap<FName, TArray<TTuple<FProperty*, int32>>> DisplayAfterPropertyMap;
			for (TFieldIterator<FProperty> It(NodeTemplate->GetClass()); It; ++It)
			{
				FProperty* NativeProperty = *It;
				if (!NativeProperty->GetOwnerUField()->HasAnyInternalFlags(EInternalObjectFlags::Native) ||
					(!LD::PropertyUtils::IsPropertyExposedToGraphNode(NativeProperty) && !LD::PropertyUtils::GetGraphPropertyFromProperty(NativeProperty)))
				{
					// Blueprint properties checked later.
					continue;
				}

				NativeProperties.Add(NativeProperty);
			}
			
			NativePropertiesOrdered.Reserve(NativeProperties.Num());
			for (FProperty* NativeProperty : NativeProperties)
			{
				// DisplayPriority and DisplayAfter handling should be almost identical to OrderPropertiesFromMetadata
				// under PropertyEditorHelpers.cpp since details panel ordering and node ordering should be consistent.

				// The overall sorting with blueprint variables won't handle the same as the details panel however,
				// since we handle blueprint variables separately after native processing. This means a native property
				// could potentially use DisplayAfter="BP_Child_Prop" and the engine would support that, but we do not.
				
				// Sort native properties based on their display order only.
				const FString& DisplayPriorityStr = NativeProperty->GetMetaData(TEXT("DisplayPriority"));
				int32 DisplayPriority = (DisplayPriorityStr.IsEmpty() ? MAX_int32 : FCString::Atoi(*DisplayPriorityStr));
				if (DisplayPriority == 0 && !FCString::IsNumeric(*DisplayPriorityStr))
				{
					// If there was a malformed display priority str Atoi will say it is 0, but we want to treat it as unset
					DisplayPriority = MAX_int32;
				}

				auto InsertProperty = [NativeProperty, DisplayPriority](TArray<TTuple<FProperty*, int32>>& InsertToArray)
				{
					bool bInserted = false;
					if (DisplayPriority != MAX_int32)
					{
						for (int32 InsertIndex = 0; InsertIndex < InsertToArray.Num(); ++InsertIndex)
						{
							const int32 PriorityAtIndex = InsertToArray[InsertIndex].Get<1>();
							if (DisplayPriority < PriorityAtIndex)
							{
								InsertToArray.Insert(MakeTuple(NativeProperty, DisplayPriority), InsertIndex);
								bInserted = true;
								break;
							}
						}
					}

					if (!bInserted)
					{
						InsertToArray.Emplace(MakeTuple(NativeProperty, DisplayPriority));
					}
				};

				const FString& DisplayAfterStr = NativeProperty->GetMetaData(TEXT("DisplayAfter"));
				if (DisplayAfterStr.IsEmpty())
				{
					InsertProperty(NativePropertiesOrdered);
				}
				else
				{
					TArray<TPair<FProperty*, int32>>& DisplayAfterProperties = DisplayAfterPropertyMap.FindOrAdd(FName(*DisplayAfterStr));
					InsertProperty(DisplayAfterProperties);
				}
			}

			// While there are still properties that need insertion seek out the property they should be listed after and insert them in their pre-display priority sorted order
			int32 RemainingDisplayAfterNodes = -1; // avoid infinite loop caused by cycles or missing dependencies by tracking that the map shrunk each iteration
			while (DisplayAfterPropertyMap.Num() > 0 && DisplayAfterPropertyMap.Num() != RemainingDisplayAfterNodes)
			{
				RemainingDisplayAfterNodes = DisplayAfterPropertyMap.Num();

				for (int32 InsertIndex = 0; InsertIndex < NativePropertiesOrdered.Num(); ++InsertIndex)
				{
					FProperty* Prop = NativePropertiesOrdered[InsertIndex].Get<0>();

					if (TArray<TTuple<FProperty*, int32>>* DisplayAfterProperties = DisplayAfterPropertyMap.Find(Prop->GetFName()))
					{
						NativePropertiesOrdered.Insert(MoveTemp(*DisplayAfterProperties), InsertIndex + 1);
						DisplayAfterPropertyMap.Remove(Prop->GetFName());
						if (DisplayAfterPropertyMap.Num() == 0)
						{
							break;
						}
					}
				}
			}

			// Reload back into array.
			NativeProperties.Reset();
			for (const TTuple<FProperty*, int32>& Property : NativePropertiesOrdered)
			{
				NativeProperties.Add(Property.Get<0>());
			}

			if (DisplayAfterPropertyMap.Num() != 0)
			{
				// If we hit this there is either a cycle or a dependency on something that doesn't exist, so just put them at the end of the list
				for (TPair<FName, TArray<TTuple<FProperty*, int32>>>& DisplayAfterProperties : DisplayAfterPropertyMap)
				{
					for (const TTuple<FProperty*, int32>& Property : DisplayAfterProperties.Value)
					{
						NativeProperties.Add(Property.Get<0>());
					}
				}
			}

			for (FProperty* NativeProperty : NativeProperties)
			{
				FSMGraphProperty_Base Property;
				Property.SetTemplateGuid(NodeTemplate->GetTemplateGuid());
				LD::PropertyUtils::SetGraphPropertyFromProperty(Property, NativeProperty, NodeTemplate);

				const FGuid& CalculatedGuid = Property.GetGuid();

				// First lookup by var guid. This is the standard lookup.
				USMGraphK2Node_PropertyNode_Base* GraphProperty = StateNodeBase->GetGraphPropertyNode(CalculatedGuid);

				// Attempt lookup by name -- Only can happen on extended graph properties.
				if (!GraphProperty)
				{
					// TODO: This may not be necessary, at least for native properties.
					GraphProperty = StateNodeBase->GetGraphPropertyNode(NativeProperty->GetFName(), NodeTemplate);
				}

				if (!ExpandAndSortProperty(GraphProperty, NodeTemplate))
				{
					continue;
				}
			}
		}

		// Blueprint variable sorting. Grab the blueprint and all parents.
		{
			TArray<UBlueprint*> BlueprintParents;
			UBlueprint::GetBlueprintHierarchyFromClass(NodeTemplate->GetClass(), BlueprintParents);

			TArray<FBPVariableDescription> Variables;
			for (UBlueprint* Blueprint : BlueprintParents)
			{
				Variables.Append(Blueprint->NewVariables);
			}

			// Check blueprint properties.
			for (const FBPVariableDescription& Variable : Variables)
			{
				FSMGraphProperty_Base Property;
				Property.SetTemplateGuid(NodeTemplate->GetTemplateGuid());
				const FGuid& CalculatedGuid = Property.SetGuid(Variable.VarGuid, 0);

				// First lookup by var guid. This is the standard lookup.
				USMGraphK2Node_PropertyNode_Base* GraphProperty = StateNodeBase->GetGraphPropertyNode(CalculatedGuid);

				// Attempt lookup by name -- Only can happen on extended graph properties.
				if (!GraphProperty)
				{
					GraphProperty = StateNodeBase->GetGraphPropertyNode(Variable.VarName, NodeTemplate);
				}

				if (!ExpandAndSortProperty(GraphProperty, NodeTemplate))
				{
					continue;
				}
			}
		}
	}

	// GraphPropertyNodes are just native / non-variable properties. Add sorted BP on after.
	GraphPropertyNodes.Append(MoveTemp(GraphPropertyBPVariablesOrdered));

	TMap<int32, TArray<USMGraphK2Node_PropertyNode_Base*>> CustomOrderMap;
	{
		// Perform custom sorting using widget vertical order override. Maintain the desired order accounting for combined states.
		// TODO deprecate: Vertical order attribute is now deprecated in favor of DisplayOrder (native) or variable order in BP. Remove this code block eventually.
		int32 BaseCount = 0;
		int32 TotalCount = 0;
		USMNodeInstance* LastTemplate = nullptr;
		for (USMGraphK2Node_PropertyNode_Base* PropertyNode : GraphPropertyNodes)
		{
			USMNodeInstance* CurrentTemplate = PropertiesToTemplates.FindRef(PropertyNode);
			if (CurrentTemplate && CurrentTemplate != LastTemplate && CurrentTemplate->GetTemplateGuid().IsValid())
			{
				BaseCount = TotalCount;
			} 
			TotalCount++;
			LastTemplate = CurrentTemplate;

			if (FSMGraphProperty_Base* Property = PropertyNode->GetPropertyNode())
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				const int32 Order = Property->GetVerticalDisplayOrder();
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				if (Order != 0)
				{
					// Look for all related elements since this could be an array that is being re-ordered.
					TArray<USMGraphK2Node_PropertyNode_Base*> PropertiesToMove = GraphPropertyNodes.
						FilterByPredicate([&Property](USMGraphK2Node_PropertyNode_Base* PropertyNode)
					{
						FSMGraphProperty_Base* TestProperty = PropertyNode->GetPropertyNodeConst();
						if (!TestProperty)
						{
							return false;
						}
						return TestProperty->VariableName == Property->VariableName && TestProperty->GetTemplateGuid() == Property->GetTemplateGuid();
					});

					CustomOrderMap.Add(BaseCount + Order, MoveTemp(PropertiesToMove));
				}
			}
		}
	}

	// Insert or add the elements to the array.
	for (auto& KeyVal : CustomOrderMap)
	{
		TArray<USMGraphK2Node_PropertyNode_Base*>& PropertyNodes = KeyVal.Value;
		for (USMGraphK2Node_PropertyNode_Base* PropertyNode : PropertyNodes)
		{
			GraphPropertyNodes.Remove(PropertyNode);
		}

		const int32 Index = FMath::Clamp(KeyVal.Key, 0, GraphPropertyNodes.Num());
		if (Index > GraphPropertyNodes.Num() - 1)
		{
			GraphPropertyNodes.Append(PropertyNodes);
		}
		else
		{
			GraphPropertyNodes.Insert(PropertyNodes, Index);
		}
	}

	for (USMGraphK2Node_PropertyNode_Base* PropertyNode : GraphPropertyNodes)
	{
		check(PropertyNode);
		USMNodeInstance* OwningTemplate = PropertyNode->GetOwningTemplate();
		if (OwningTemplate == nullptr)
		{
			LDEDITOR_LOG_ERROR(TEXT("Missing OwningTemplate for PropertyNode %s."), *PropertyNode->GetName())
			continue;
		}

		TArray<USMGraphK2Node_PropertyNode_Base*>& TemplateProperties = OutTemplatePropertyMap.FindOrAdd(OwningTemplate);
		TemplateProperties.Add(PropertyNode);
	}
}

#undef LOCTEXT_NAMESPACE
