// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMNodeStackCustomization.h"

#include "Graph/Nodes/SMGraphNode_RerouteNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "SMSystemEditorLog.h"
#include "Utilities/SMNodeInstanceUtils.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IPropertyTypeCustomization> FSMStateStackCustomization::MakeInstance()
{
	return MakeShared<FSMStateStackCustomization>();
}

void FSMStateStackCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
                                                 FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FSMStructCustomization::CustomizeHeader(StructPropertyHandle, HeaderRow, StructCustomizationUtils);

	const USMGraphNode_StateNode* GraphNode = Cast<USMGraphNode_StateNode>(GetGraphNodeBeingCustomized(StructCustomizationUtils, true));
	// Don't show children if we are on state machine graph.
	if (!GraphNode)
	{
		return;
	}
	
	const int32 IndexInArray = StructPropertyHandle->GetIndexInArray();
	const USMNodeInstance* NodeInstance = GraphNode->GetTemplateFromIndex(IndexInArray);

	const FText HeaderName = FNodeStackContainer::FormatStackInstanceName(NodeInstance);
	
	HeaderRow
		.CopyAction(FUIAction(FExecuteAction::CreateLambda([]()
		{
			// Disable for now.. stack arrays don't copy paste rows well.
			LDEDITOR_LOG_WARNING(TEXT("Copy and pasting node stack rows is not supported. Duplicate and move the row instead."))
		})))
		.PasteAction(FUIAction(FExecuteAction::CreateLambda([]()
		{
			// Disable for now.. stack arrays don't copy paste rows well.
			LDEDITOR_LOG_WARNING(TEXT("Copy and pasting node stack rows is not supported. Duplicate and move the row instead."))
		})))
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1) // Fill the entire width if possible
			.VAlign(VAlign_Center)	
			[
				SNew(STextBlock)
				.Text(HeaderName)
				.ToolTipText(HeaderName)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
}

void FSMStateStackCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	USMGraphNode_StateNode* GraphNode = Cast<USMGraphNode_StateNode>(GetGraphNodeBeingCustomized(StructCustomizationUtils, true));
	// Don't show children if we are on state machine graph.
	if (!GraphNode)
	{
		return;
	}
	
	// Build out default properties as if this wasn't being customized.
	
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		// Add the property.
		const TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		StructBuilder.AddProperty(ChildHandle);

		// Check if this is the template instance.
		if (ChildHandle->GetProperty() && ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FStateStackContainer, NodeStackInstanceTemplate))
		{
			const int32 IndexInArray = StructPropertyHandle->GetIndexInArray();
			USMNodeInstance* Template = GraphNode->GetTemplateFromIndex(IndexInArray);

			if (!Template)
			{
				continue;
			}
	
			TArray<TSharedRef<IPropertyHandle>> TemplateProperties;
			for (TFieldIterator<FProperty> PropIt(Template->GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
			{
				const FName PropertyName = PropIt->GetFName();
				TSharedPtr<IPropertyHandle> Handle = StructPropertyHandle->GetChildHandle(PropertyName);
				if (Handle.IsValid() && Handle->IsValidHandle())
				{
					TemplateProperties.Add(Handle.ToSharedRef());
				}
			}

			for (const TSharedRef<IPropertyHandle>& TemplateProperty : TemplateProperties)
			{
				FSMNodeInstanceCustomization::DisplayExposedPropertyWidget(GraphNode, TemplateProperty, Template, nullptr, &StructBuilder);
			}
			
			uint32 NumTemplateCategories;

			TSharedPtr<IPropertyHandle> TemplateHandle = ChildHandle->GetChildHandle(0);
			if (!TemplateHandle.IsValid())
			{
				continue;
			}

			// Check if the entire category should be hidden.
			TemplateHandle->GetNumChildren(NumTemplateCategories);
			for (uint32 CatIdx = 0; CatIdx < NumTemplateCategories; ++CatIdx)
			{
				TSharedPtr<IPropertyHandle> TemplateChildHandle = TemplateHandle->GetChildHandle(CatIdx);
				FSMNodeInstanceCustomization::HideEmptyCategoryHandles(TemplateChildHandle, FSMNodeInstanceCustomization::ENodeStackType::StateStack);
			}
		}
	}
}

TSharedRef<IPropertyTypeCustomization> FSMTransitionStackCustomization::MakeInstance()
{
	return MakeShared<FSMTransitionStackCustomization>();
}

void FSMTransitionStackCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FSMStructCustomization::CustomizeHeader(StructPropertyHandle, HeaderRow, StructCustomizationUtils);

	const USMGraphNode_TransitionEdge* GraphNode = GetTransitionBeingCustomized(StructCustomizationUtils);
	// Don't show children if we are on state machine graph.
	if (!GraphNode)
	{
		return;
	}
	
	const int32 IndexInArray = StructPropertyHandle->GetIndexInArray();
	const USMNodeInstance* NodeInstance = GraphNode->GetTemplateFromIndex(IndexInArray);

	const FText HeaderName = FNodeStackContainer::FormatStackInstanceName(NodeInstance);
	
	HeaderRow
		.CopyAction(FUIAction(FExecuteAction::CreateLambda([]()
		{
			// Disable for now.. stack arrays don't copy paste rows well.
			LDEDITOR_LOG_WARNING(TEXT("Copy and pasting node stack rows is not supported. Duplicate and move the row instead."))
		})))
		.PasteAction(FUIAction(FExecuteAction::CreateLambda([]()
		{
			// Disable for now.. stack arrays don't copy paste rows well.
			LDEDITOR_LOG_WARNING(TEXT("Copy and pasting node stack rows is not supported. Duplicate and move the row instead."))
		})))
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1) // Fill the entire width if possible
			.VAlign(VAlign_Center)	
			[
				SNew(STextBlock)
				.Text(HeaderName)
				.ToolTipText(HeaderName)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
}

void FSMTransitionStackCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const USMGraphNode_TransitionEdge* GraphNode = GetTransitionBeingCustomized(StructCustomizationUtils);
	// Don't show children if we are on state machine graph.
	if (!GraphNode)
	{
		return;
	}
	
	// Build out default properties as if this wasn't being customized.
	
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		// Add the property.
		const TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		StructBuilder.AddProperty(ChildHandle);

		// Check if this is the template instance.
		if (ChildHandle->GetProperty() && ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FStateStackContainer, NodeStackInstanceTemplate))
		{
			const int32 IndexInArray = StructPropertyHandle->GetIndexInArray();
			const USMNodeInstance* Template = GraphNode->GetTemplateFromIndex(IndexInArray);

			if (!Template)
			{
				continue;
			}

			uint32 NumTemplateCategories;

			TSharedPtr<IPropertyHandle> TemplateHandle = ChildHandle->GetChildHandle(0);
			if (!TemplateHandle.IsValid())
			{
				continue;
			}

			// Check if the entire category should be hidden.
			TemplateHandle->GetNumChildren(NumTemplateCategories);
			for (uint32 CatIdx = 0; CatIdx < NumTemplateCategories; ++CatIdx)
			{
				TSharedPtr<IPropertyHandle> TemplateChildHandle = TemplateHandle->GetChildHandle(CatIdx);
				FSMNodeInstanceCustomization::HideEmptyCategoryHandles(TemplateChildHandle, FSMNodeInstanceCustomization::ENodeStackType::TransitionStack);
			}
		}
	}
}

USMGraphNode_TransitionEdge* FSMTransitionStackCustomization::GetTransitionBeingCustomized(
	IPropertyTypeCustomizationUtils& StructCustomizationUtils) const
{
	if (USMGraphNode_Base* GraphNode = GetGraphNodeBeingCustomized(StructCustomizationUtils, true))
	{
		if (const USMGraphNode_RerouteNode* Reroute = Cast<USMGraphNode_RerouteNode>(GraphNode))
		{
			return Reroute->GetPrimaryTransition();
		}

		return Cast<USMGraphNode_TransitionEdge>(GraphNode);
	}

	return nullptr;
}
