#include "SMTransitionEdgeCustomization.h"

#include "Graph/Nodes/FunctionNodes/SMTransitionEventTypes.h"
#include "Graph/Nodes/SMGraphNode_RerouteNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "SMUnrealTypeDefs.h"
#include "SSearchableComboBox.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "SMTransitionEdgeCustomization"

void FSMTransitionEdgeCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	USMGraphNode_Base* CustomizedNode = GetObjectBeingCustomized<USMGraphNode_Base>(DetailBuilder);
	if (!CustomizedNode)
	{
		return;
	}

	if (CustomizedNode->IsA<USMGraphNode_RerouteNode>())
	{
		HideAnyStateTags(DetailBuilder);
	}

	USMGraphNode_TransitionEdge* TransitionNode = CustomizedNode->IsA<USMGraphNode_RerouteNode>() ? CastChecked<USMGraphNode_RerouteNode>(CustomizedNode)->GetPrimaryTransition()
		: Cast<USMGraphNode_TransitionEdge>(CustomizedNode);
	if (!TransitionNode)
	{
		return;
	}

	if (CustomizedNode->IsA<USMGraphNode_RerouteNode>())
	{
		// The object being customized doesn't actually point to the object we should customize, such as a reroute node to a transition.
		IDetailPropertyRow* Row = DetailBuilder.EditCategory("Reroute Node").AddExternalObjects({TransitionNode}, EPropertyLocation::Common);
		check(Row);

		const TSharedPtr<IPropertyHandle> PropertyHandle = Row->GetPropertyHandle();
		check(PropertyHandle.IsValid());
		FDetailWidgetRow& WidgetRow = Row->CustomWidget();

		// Customize the object being displayed... setting the DisplayName override won't work for an external object.
		WidgetRow.NameWidget
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TransitionRerouteName", "Rerouted Transition"))
				.ToolTipText(LOCTEXT("TransitionRerouteTooltip", "The transition being rerouted."))
				.Font(FSMUnrealAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];

		WidgetRow.ValueContent()
		[
			PropertyHandle->CreatePropertyValueWidget()
		];

		Row->ShouldAutoExpand(true);

		// This editor customized category will already be shown through external object above.
		DetailBuilder.HideCategory("GraphNodeDetail");
	}
	else if (TransitionNode->IsUsingCustomName())
	{
		// Transitions using a custom name need special handling to reset the name to defaults. The GraphNodeDetail
		// widget is added in through editor customization, making it difficult to customize. Ideally we would add a
		// reset to default option to the editor customization, but doing so would be very hacky.
		
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("GraphNodeDetail", /* From BlueprintDetailsCustomization */
			LOCTEXT("GraphNodeDetailsCategory", "Graph Node"), ECategoryPriority::Important);

		const FText ResetNameText = LOCTEXT("GraphNodeDetail_ResetName", "Reset Name");

		TWeakObjectPtr<USMGraphNode_TransitionEdge> TransitionWeakPtr(TransitionNode);

		// Add to advanced so it shows up below name.
		Category.AddCustomRow(ResetNameText, true)
		.ValueContent()
		[
			SNew(SButton)
			.Text(ResetNameText)
			.ToolTipText(LOCTEXT("ResetName_Tooltip", "Reset the transition name to the default system generated name."))
			.HAlign(HAlign_Center)
			.OnClicked_Lambda([TransitionWeakPtr]()
			{
				if (TransitionWeakPtr.IsValid())
				{
					TransitionWeakPtr->ResetNodeName();
				}
				return FReply::Handled();
			})
		];

		Category.SetShowAdvanced(true);
	}

	const UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNode(TransitionNode);
	if (!Blueprint)
	{
		return;
	}

	AvailableDelegates.Reset();
	AvailableDelegates.Add(MakeShareable(new FString()));

	if (const UClass* DelegateOwnerClass = TransitionNode->GetSelectedDelegateOwnerClass())
	{
		for (TFieldIterator<FMulticastDelegateProperty> It(DelegateOwnerClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			if (const FMulticastDelegateProperty* Delegate = CastField<FMulticastDelegateProperty>(*It))
			{
				AvailableDelegates.Add(MakeShareable(new FString(Delegate->GetName())));
			}
		}
	}

	const TSharedPtr<IPropertyHandle> DelegatePropertyName = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, DelegatePropertyName), USMGraphNode_TransitionEdge::StaticClass());
	const TSharedPtr<IPropertyHandle> DelegatePropertyClass = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, DelegateOwnerClass), USMGraphNode_TransitionEdge::StaticClass());
	const TSharedPtr<IPropertyHandle> DelegatePropertyInstance = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, DelegateOwnerInstance), USMGraphNode_TransitionEdge::StaticClass());

	DelegatePropertyInstance->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FSMTransitionEdgeCustomization::ForceUpdate));
	DelegatePropertyClass->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FSMTransitionEdgeCustomization::ForceUpdate));
	DelegatePropertyName->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FSMTransitionEdgeCustomization::ForceUpdate));
	
	// Custom delegate name picker.
	if (IDetailPropertyRow* Row = DetailBuilder.EditDefaultProperty(DelegatePropertyName))
	{
		TSharedPtr<SHorizontalBox> DelegateButtonsRow;
		
		Row->CustomWidget()
		.NameContent()
		[
			DelegatePropertyName->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(125.f)
		.MaxDesiredWidth(400.f)
		[
			SAssignNew(DelegateButtonsRow, SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SSearchableComboBox)
				.OptionsSource(&AvailableDelegates)
				.ToolTipText(FText::FromString(TransitionNode->DelegatePropertyName.ToString()))
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem)
				{
					return SNew(STextBlock)
					// The combo box selection text.
					.Text(FText::FromString(*InItem));
				})
				.OnSelectionChanged_Lambda([DelegatePropertyName, this](TSharedPtr<FString> Selection, ESelectInfo::Type)
				{
					// When selecting a property from the drop down.
					if (DelegatePropertyName->IsValidHandle())
					{
						DelegatePropertyName->SetValue(*Selection);
						ForceUpdate();
					}
				})
				.ContentPadding(FMargin(2, 2))
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text_Lambda([=]() -> FText
					{
						// Display selected property text.
						if (DelegatePropertyName->IsValidHandle())
						{
							FString Value;
							const FPropertyAccess::Result Result = DelegatePropertyName->GetValue(Value);
							if (Result == FPropertyAccess::Result::Success)
							{
								return FText::FromString(Value);
							}
							if (Result == FPropertyAccess::Result::MultipleValues)
							{
								return FText::FromString("Multiple Values");
							}
						}

						return FText::GetEmpty();
					})
					]
				]
		.HAlign(HAlign_Fill)
		];

		if (TransitionNode->DelegatePropertyName != NAME_None)
		{
			DelegateButtonsRow->AddSlot()
			[
				SNew(SButton)
				.Text(LOCTEXT("GoToDelegate", "Open Graph"))
				.OnClicked_Lambda([=]
				{
					if (TransitionNode)
					{
						TransitionNode->GoToTransitionEventNode();
					}
					return FReply::Handled();
				})
			];
		}
	}

	// Only allow class selection when the class isn't inherently known.
	if (TransitionNode->DelegateOwnerInstance != SMDO_Context)
	{
		if (IDetailPropertyRow* Row = DetailBuilder.EditDefaultProperty(DelegatePropertyClass))
		{
			Row->Visibility(EVisibility::Collapsed);
		}
	}

	FSMNodeCustomization::CustomizeDetails(DetailBuilder);
}

TSharedRef<IDetailCustomization> FSMTransitionEdgeCustomization::MakeInstance()
{
	return MakeShared<FSMTransitionEdgeCustomization>();
}

#undef LOCTEXT_NAMESPACE