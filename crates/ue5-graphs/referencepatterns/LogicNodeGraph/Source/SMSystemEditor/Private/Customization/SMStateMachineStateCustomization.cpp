// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMStateMachineStateCustomization.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineParentNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "SMUtils.h"

#include "PropertyCustomizationHelpers.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "SMUnrealTypeDefs.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Kismet2/KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "SMStateMachineStateCustomization"

void FSMStateMachineStateCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	USMGraphNode_StateMachineStateNode* StateNode = GetObjectBeingCustomized<USMGraphNode_StateMachineStateNode>(DetailBuilder);
	if (!StateNode)
	{
		return;
	}

	const bool bIsParent = StateNode->IsA<USMGraphNode_StateMachineParentNode>();
	if (bIsParent)
	{
		CustomizeParentSelection(DetailBuilder);
	}
	
	const bool bIsReference = StateNode->IsStateMachineReference();
	if (bIsReference)
	{
		CustomizeReferenceDynamicClassSelection(DetailBuilder);
	}

	// Use template -- toggles template visibility.
	if (const TSharedPtr<IPropertyHandle> Property = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USMGraphNode_StateMachineStateNode, bUseTemplate)))
	{
		// Detect when value changes.
		Property->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FSMStateMachineStateCustomization::OnUseTemplateChange));
	}
	
	// Template visibility
	if (const TSharedPtr<IPropertyHandle> Property = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USMGraphNode_StateMachineStateNode, ReferencedInstanceTemplate)))
	{
		if (IDetailPropertyRow* PropertyRow = DetailBuilder.EditDefaultProperty(Property))
		{
			PropertyRow->ShouldAutoExpand(true);
			PropertyRow->Visibility(VisibilityConverter(bIsReference && StateNode->bUseTemplate));
		}
	}

	// Misc reference visibility
	{
		if (const TSharedPtr<IPropertyHandle> Property = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USMGraphNode_StateMachineStateNode, bAllowIndependentTick)))
		{
			if (IDetailPropertyRow* PropertyRow = DetailBuilder.EditDefaultProperty(Property))
			{
				PropertyRow->Visibility(VisibilityConverter(bIsReference));
			}
		}
		if (const TSharedPtr<IPropertyHandle> Property = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USMGraphNode_StateMachineStateNode, bCallTickOnManualUpdate)))
		{
			if (IDetailPropertyRow* PropertyRow = DetailBuilder.EditDefaultProperty(Property))
			{
				PropertyRow->Visibility(VisibilityConverter(bIsReference));
			}
		}
		// Class template only valid for nested static state machines.
		if (const TSharedPtr<IPropertyHandle> Property = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USMGraphNode_StateMachineStateNode, StateMachineClass)))
		{
			if (IDetailPropertyRow* PropertyRow = DetailBuilder.EditDefaultProperty(Property))
			{
				PropertyRow->Visibility(VisibilityConverter(!bIsReference && !bIsParent));
			}
		}
	}

	// Set overall category visibility last as this will consider it detailed and editing properties past this point won't work.
	IDetailCategoryBuilder& ReferenceCategory = DetailBuilder.EditCategory("State Machine Reference");
	ReferenceCategory.SetCategoryVisibility(bIsReference);

	if (bIsParent || bIsReference)
	{
		IDetailCategoryBuilder& DisplayCategory = DetailBuilder.EditCategory("Display");
		DisplayCategory.SetCategoryVisibility(false);

		IDetailCategoryBuilder& ColorCategory = DetailBuilder.EditCategory("Color");
		ColorCategory.SetCategoryVisibility(false);
	}

	FSMNodeCustomization::CustomizeDetails(DetailBuilder);
}

TSharedRef<IDetailCustomization> FSMStateMachineStateCustomization::MakeInstance()
{
	return MakeShareable(new FSMStateMachineStateCustomization);
}

void FSMStateMachineStateCustomization::CustomizeParentSelection(IDetailLayoutBuilder& DetailBuilder)
{
	USMGraphNode_StateMachineParentNode* StateNode = GetObjectBeingCustomized<USMGraphNode_StateMachineParentNode>(DetailBuilder);
	if (!StateNode)
	{
		return;
	}

	UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNode(StateNode);
	if (!Blueprint)
	{
		return;
	}

	AvailableParentClasses.Reset();
	MappedParentClasses.Reset();
	TArray<USMBlueprintGeneratedClass*> ParentClasses;
	if (FSMBlueprintEditorUtils::TryGetParentClasses(Blueprint, ParentClasses))
	{
		for (USMBlueprintGeneratedClass* ParentClass : ParentClasses)
		{
			AvailableParentClasses.Add(MakeShareable(new FName(ParentClass->GetFName())));
			MappedParentClasses.Add(ParentClass->GetFName(), ParentClass);
		}
	}

	const TSharedPtr<IPropertyHandle> ParentProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USMGraphNode_StateMachineParentNode, ParentClass), USMGraphNode_StateMachineParentNode::StaticClass());

	// Row could be null if multiple nodes selected -- Hide original property we will recreate it.
	if (IDetailPropertyRow* Row = DetailBuilder.EditDefaultProperty(ParentProperty))
	{
		Row->Visibility(EVisibility::Collapsed);
	}
	
	const TSharedPtr<IPropertyHandle> ClassProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USMGraphNode_StateMachineParentNode, StateMachineClass), USMGraphNode_StateMachineParentNode::StaticClass());

	// We don't want to edit the class property for a parent.
	if (IDetailPropertyRow* Row = DetailBuilder.EditDefaultProperty(ClassProperty))
	{
		Row->Visibility(EVisibility::Collapsed);
	}

	// May want to switch to FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &FSMStateMachineReferenceCustomization::OnClassPicked));

	// Add a new custom row so we don't have to deal with the automatic assigned buttons next to the drop down that using the CustomWidget of the PropertyRow gets us.
	DetailBuilder.EditCategory("Parent State Machine")
	.AddCustomRow(LOCTEXT("StateMachineParent", "State Machine Parent"))
	.NameContent()
	[
		ParentProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SComboBox<TSharedPtr<FName>>)
			.OptionsSource(&AvailableParentClasses)
			.OnGenerateWidget_Lambda([](TSharedPtr<FName> InItem)
			{
				return SNew(STextBlock)
				// The combo box selection text.
				.Text(FText::FromName(*InItem));
			})
			.OnSelectionChanged_Lambda([ParentProperty, this](TSharedPtr<FName> Selection, ESelectInfo::Type)
			{
				// When selecting a property from the drop down.
				if (ParentProperty->IsValidHandle())
				{
					USMBlueprintGeneratedClass* Result = MappedParentClasses.FindRef(*Selection);
					ParentProperty->SetValue(Result);
				}
			})
			.ContentPadding(FMargin(2, 2))
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text_Lambda([=]() -> FText
				{
					// Display selected property text.
					if (ParentProperty->IsValidHandle())
					{
						UObject* Value = nullptr;
						const FPropertyAccess::Result Result = ParentProperty->GetValue(Value);
						if (Result == FPropertyAccess::Result::Success)
						{
							return FText::FromName(Value ? Value->GetFName() : "None");
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
}

void FSMStateMachineStateCustomization::CustomizeReferenceDynamicClassSelection(IDetailLayoutBuilder& DetailBuilder)
{
	const TWeakObjectPtr<USMGraphNode_StateMachineStateNode> StateNode = GetObjectBeingCustomized<USMGraphNode_StateMachineStateNode>(DetailBuilder);
	if (!StateNode.IsValid())
	{
		return;
	}

	const TWeakObjectPtr<UBlueprint> Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNode(StateNode.Get());
	if (!Blueprint.IsValid())
	{
		return;
	}

	AvailableVariables.Reset();
	MappedNamesToDisplayNames.Reset();
	
	const TSharedPtr<FText> NoneOption = MakeShareable(new FText(FText::FromString(TEXT("None"))));
	AvailableVariables.Add(NoneOption);

	const TSharedPtr<IPropertyHandle> DynamicVariableProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USMGraphNode_StateMachineStateNode, DynamicClassVariable), USMGraphNode_StateMachineStateNode::StaticClass());
	check(DynamicVariableProperty.IsValid());

	FName InitialItemName;
	DynamicVariableProperty->GetValue(InitialItemName);
	
	const UClass* ClassToUse = Blueprint->SkeletonGeneratedClass ? Blueprint->SkeletonGeneratedClass : Blueprint->GeneratedClass;
	if (!ClassToUse)
	{
		return;
	}

	SelectedVariable = NoneOption;
	for (TFieldIterator<FProperty> It(ClassToUse); It; ++It)
	{
		if (const FClassProperty* ClassProperty = CastField<FClassProperty>(*It))
		{
			if (ClassProperty->MetaClass->IsChildOf(USMInstance::StaticClass()))
			{
				FText DisplayName = It->GetDisplayNameText();
				TSharedPtr<FText> VariableText = MakeShareable(new FText(DisplayName));
				AvailableVariables.Add(VariableText);
				MappedNamesToDisplayNames.Add(It->GetFName(), DisplayName);

				if (It->GetFName() == InitialItemName)
				{
					SelectedVariable = VariableText;
				}
			}
		}
	}
	
	if (IDetailPropertyRow* Row = DetailBuilder.EditDefaultProperty(DynamicVariableProperty))
	{
		Row->OverrideResetToDefault(
			FResetToDefaultOverride::Create(FIsResetToDefaultVisible::CreateLambda([&](const TSharedPtr<IPropertyHandle> PropertyHandle)
		{
			FName ValueName;
			PropertyHandle->GetValue(ValueName);
			return (!ValueName.IsNone());
		}),
		FResetToDefaultHandler::CreateLambda([&](const TSharedPtr<IPropertyHandle> PropertyHandle)
		{
			PropertyHandle->SetValue(FName(NAME_None));
			DetailBuilder.ForceRefreshDetails();
		})));
		
		Row->CustomWidget()
		.NameContent()
		[
			DynamicVariableProperty->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.MaxWidth(125.f)
			[
				SNew(SComboBox<TSharedPtr<FText>>)
				.OptionsSource(&AvailableVariables)
				.InitiallySelectedItem(SelectedVariable)
				.OnGenerateWidget_Lambda([](TSharedPtr<FText> InItem)
				{
					return SNew(STextBlock)
					// The combo box selection text.
					.Text(*InItem);
				})
				.OnSelectionChanged_Lambda([DynamicVariableProperty, this](TSharedPtr<FText> Selection, ESelectInfo::Type)
				{
					check(Selection);
					SelectedVariable = Selection;
					// When selecting a property from the drop down.
					if (DynamicVariableProperty->IsValidHandle())
					{
						FName VariableName = NAME_None;
						for (const TTuple<FName, FText>& KeyVal : MappedNamesToDisplayNames)
						{
							if (KeyVal.Value.EqualTo(*Selection))
							{
								VariableName = KeyVal.Key;
								break;
							}
						}
						
						DynamicVariableProperty->SetValue(VariableName);
					}
				})
				.ContentPadding(FMargin(2, 2))
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text_Lambda([DynamicVariableProperty, NoneOption, this]() -> FText
					{
						// Display selected property text.
						if (DynamicVariableProperty->IsValidHandle())
						{
							FName Value;
							const FPropertyAccess::Result Result = DynamicVariableProperty->GetValue(Value);
							if (FText* DisplayName = MappedNamesToDisplayNames.Find(Value))
							{
								if (Result == FPropertyAccess::Result::Success)
								{
									return *DisplayName;
								}
								if (Result == FPropertyAccess::Result::MultipleValues)
								{
									return FText::FromString("Multiple Values");
								}
							}
						}

						return NoneOption.IsValid() ? *NoneOption : FText::GetEmpty();
					})
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				[
					SNew(SImage)
					.Image(FSMUnrealAppStyle::Get().GetBrush("PListEditor.Button_AddToArray"))
				]
				.ButtonStyle(FSMUnrealAppStyle::Get(), "HoverHintOnly")
				.ToolTipText(LOCTEXT("AddDynamicClassVariableToolTip", "Create a new variable in the blueprint."))
				.OnClicked_Lambda([=]()
				{
					if (Blueprint.IsValid() && StateNode.IsValid() && StateNode->GetBoundGraph())
					{
						const FString BaseName = StateNode->GetBoundGraph()->GetName() + TEXT("DynamicClass");
						const FName VarName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint.Get(), BaseName);
						FEdGraphPinType PinType;
						PinType.PinCategory = UEdGraphSchema_K2::PC_Class;
						PinType.PinSubCategoryObject = USMInstance::StaticClass();
						FBlueprintEditorUtils::AddMemberVariable(Blueprint.Get(), VarName, PinType);
					}
					return FReply::Handled();
				})
			]
		];
	}
}

void FSMStateMachineStateCustomization::OnUseTemplateChange()
{
	ForceUpdate();
}

#undef LOCTEXT_NAMESPACE
