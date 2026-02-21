#include "SMLinkStateCustomization.h"

#include "Graph/Nodes/SMGraphNode_LinkStateNode.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "DetailWidgetRow.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "SSearchableComboBox.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "SMLinkStateCustomization"

void FSMLinkStateCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const USMGraphNode_LinkStateNode* LinkStateNode = GetObjectBeingCustomized<USMGraphNode_LinkStateNode>(DetailBuilder);
	if (!LinkStateNode)
	{
		return;
	}

	const UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNode(LinkStateNode);
	if (!Blueprint)
	{
		return;
	}

	HideAnyStateTags(DetailBuilder);

	TArray<USMGraphNode_StateNodeBase*> States;
	LinkStateNode->GetAvailableStatesToLink(States);

	AvailableStateNames.Reset(States.Num());

	for (const USMGraphNode_StateNodeBase* StateNode : States)
	{
		AvailableStateNames.Add(MakeShareable(new FString(StateNode->GetNodeName())));
	}

	const TSharedPtr<IPropertyHandle> LinkedStatePropertyHandle =
		DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USMGraphNode_LinkStateNode, LinkedStateName), USMGraphNode_LinkStateNode::StaticClass());

	LinkedStatePropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FSMLinkStateCustomization::ForceUpdate));

	if (IDetailPropertyRow* Row = DetailBuilder.EditDefaultProperty(LinkedStatePropertyHandle))
	{
		TSharedPtr<SHorizontalBox> StateButtonsRow;

		Row->CustomWidget()
		.NameContent()
		[
			LinkedStatePropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(125.f)
		.MaxDesiredWidth(600.f)
		[
			SAssignNew(StateButtonsRow, SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SSearchableComboBox)
				.OptionsSource(&AvailableStateNames)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem)
				{
					return SNew(STextBlock)
					// The combo box selection text.
					.Text(FText::FromString(*InItem));
				})
				.OnSelectionChanged_Lambda([LinkedStatePropertyHandle, this](TSharedPtr<FString> Selection, ESelectInfo::Type)
				{
					if (LinkedStatePropertyHandle->IsValidHandle())
					{
						LinkedStatePropertyHandle->SetValue(*Selection);
						ForceUpdate();
					}
				})
				.ContentPadding(FMargin(2, 2))
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text_Lambda([=]() -> FText
					{
						if (LinkedStatePropertyHandle->IsValidHandle())
						{
							FString Value;
							const FPropertyAccess::Result Result = LinkedStatePropertyHandle->GetValue(Value);
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

		if (LinkStateNode->GetLinkedState() != nullptr)
		{
			StateButtonsRow->AddSlot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("GoToState", "Go to State"))
				.HAlign(HAlign_Fill)
				.OnClicked_Lambda([=]
				{
					FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(LinkStateNode->GetLinkedState());
					return FReply::Handled();
				})
			];
		}
	}

	FSMNodeCustomization::CustomizeDetails(DetailBuilder);
}

TSharedRef<IDetailCustomization> FSMLinkStateCustomization::MakeInstance()
{
	return MakeShared<FSMLinkStateCustomization>();
}

#undef LOCTEXT_NAMESPACE