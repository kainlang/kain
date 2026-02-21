// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMNodeRulesCustomization.h"

#include "SMNodeRules.h"
#include "Utilities/SMNodeInstanceUtils.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IPropertyTypeCustomization> FSMNodeRulesCustomization::MakeInstance()
{
	return MakeShared<FSMNodeRulesCustomization>();
}

void FSMNodeRulesCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
                                                 FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FSMStructCustomization::CustomizeHeader(StructPropertyHandle, HeaderRow, StructCustomizationUtils);
	
	HeaderRow
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
				.Text(this, &FSMNodeRulesCustomization::GetRuleDisplayName)
				.ToolTipText(this, &FSMNodeRulesCustomization::GetRuleDisplayName)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
}

void FSMNodeRulesCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildNum = 0; ChildNum < NumChildren; ++ChildNum)
	{
		ChildBuilder.AddProperty(StructPropertyHandle->GetChildHandle(ChildNum).ToSharedRef());
	}
}

FText FSMNodeRulesCustomization::GetRuleDisplayName() const
{
	if (PropertyHandle.IsValid())
	{
		TArray<const void*> RawStructData;
		PropertyHandle->AccessRawData(RawStructData);

		if (RawStructData.Num() > 0)
		{
			if (const FSMNodeClassRule* NodeClassRule = static_cast<const FSMNodeClassRule*>(RawStructData[0]))
			{
				const FString ClassName = GetFormattedClassName(NodeClassRule);
		
				const FText HeaderName = FText::FromString(ClassName);
				return HeaderName;
			}
		}
	}

	return FText::GetEmpty();
}

FString FSMNodeRulesCustomization::GetFormattedClassName(const FSMNodeClassRule* InNodeClassRule)
{
	check(InNodeClassRule);
	const TSoftClassPtr<USMNodeInstance> NodeClass = InNodeClassRule->GetClass();
		
	FString ClassName = NodeClass.IsNull() ? TEXT("None") : NodeClass.GetAssetName();
	ClassName.RemoveFromEnd(TEXT("_C"));
	return ClassName;
}

TSharedRef<IPropertyTypeCustomization> FSMConnectionRulesCustomization::MakeInstance()
{
	return MakeShared<FSMConnectionRulesCustomization>();
}

FText FSMConnectionRulesCustomization::GetRuleDisplayName() const
{
	if (PropertyHandle.IsValid())
	{
		TArray<const void*> RawStructData;
		PropertyHandle->AccessRawData(RawStructData);

		if (RawStructData.Num() > 0)
		{
			if (const FSMNodeConnectionRule* NodeConnectionRule = static_cast<const FSMNodeConnectionRule*>(RawStructData[0]))
			{
				const FString FromClassName = GetFormattedClassName(&NodeConnectionRule->FromState);
				const FString ToClassName = GetFormattedClassName(&NodeConnectionRule->ToState);
				const FString InStateMachineClassName = GetFormattedClassName(&NodeConnectionRule->InStateMachine);
		
				const FText DisplayName = FText::FromString(FString::Printf(TEXT("%s to %s in %s"), *FromClassName, *ToClassName, *InStateMachineClassName));
		
				return DisplayName;
			}
		}
	}

	return FText::GetEmpty();
}
