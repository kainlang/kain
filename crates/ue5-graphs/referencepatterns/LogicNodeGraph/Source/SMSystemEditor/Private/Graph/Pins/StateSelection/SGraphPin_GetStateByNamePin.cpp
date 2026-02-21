// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SGraphPin_GetStateByNamePin.h"

#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Pins/SGraphPin_SMDefaults.h"
#include "K2Node_CallFunction.h"
#include "SSMStateTreeView.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "SMInstance.h"

#include "ScopedTransaction.h"
#include "SMUnrealTypeDefs.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "SMCreateStateByNamePin"

TSharedPtr<SGraphPin> FSMGetStateByNamePinFactory::CreatePin(UEdGraphPin* InPin) const
{
	if (!InPin)
	{
		return nullptr;
	}

	if (InPin->PinType.PinCategory != UEdGraphSchema_K2::PC_String)
	{
		return nullptr;
	}

	const FString MetaDataString = TEXT("UseLogicDriverStatePicker");
	UEdGraphNode* OwningNode = InPin->GetOwningNodeUnchecked();
	if (const UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(OwningNode))
	{
		if (const UFunction* Function = CallFunctionNode->GetTargetFunction())
		{
			if (Function->HasMetaData(*MetaDataString) && SGraphPin_GetStateByNamePin::GetBlueprintGeneratedClass(InPin))
			{
				if (Function->GetMetaData(*MetaDataString) == InPin->GetName())
				{
					return SNew(SGraphPin_GetStateByNamePin, InPin);
				}
			}
		}
	}
	else if (const USMGraphK2Node_PropertyNode_Base* GraphPropertyNode = Cast<USMGraphK2Node_PropertyNode_Base>(OwningNode))
	{
		if (const FProperty* Property = GraphPropertyNode->GetProperty())
		{
			if (Property->HasMetaData(*MetaDataString) && SGraphPin_GetStateByNamePin::GetBlueprintGeneratedClass(InPin))
			{
				return SNew(SGraphPin_GetStateByNamePin, InPin);
			}
		}
	}

	return nullptr;
}

void FSMGetStateByNamePinFactory::RegisterFactory()
{
	FEdGraphUtilities::RegisterVisualPinFactory(MakeShared<FSMGetStateByNamePinFactory>());
}

void SGraphPin_GetStateByNamePin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

USMBlueprintGeneratedClass* SGraphPin_GetStateByNamePin::GetBlueprintGeneratedClass(const UEdGraphPin* InGraphPin)
{
	if (InGraphPin == nullptr)
	{
		return nullptr;
	}
	
	const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(InGraphPin->GetSchema());
	if (Schema == nullptr)
	{
		return nullptr;
	}

	UEdGraphNode* OwningNode = InGraphPin->GetOwningNodeUnchecked();

	// Our graph property nodes can lookup the blueprint generated class easily.
	if (const USMGraphK2Node_PropertyNode_Base* GraphPropertyNode = Cast<USMGraphK2Node_PropertyNode_Base>(OwningNode))
	{
		if (const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(GraphPropertyNode))
		{
			return Cast<USMBlueprintGeneratedClass>(Blueprint->GeneratedClass);
		}
	}

	const UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(OwningNode);
	if (CallFunctionNode == nullptr)
	{
		return nullptr;
	}

	// Call functions need to identify the self pin.
	UEdGraphPin* SelfPin = Schema->FindSelfPin(*OwningNode, EGPD_Input);
	if (SelfPin == nullptr)
	{
		return nullptr;
	}

	UClass* SelfPinClass = nullptr;
	if (SelfPin->PinType.PinSubCategoryObject.IsValid())
	{
		SelfPinClass =
			Cast<UClass>(SelfPin->PinType.PinSubCategoryObject.Get());
	}

	if (SelfPinClass == nullptr || !SelfPinClass->IsChildOf(USMInstance::StaticClass()))
	{
		return nullptr;
	}

	USMBlueprintGeneratedClass* SMBlueprintClass;
	if (SelfPin->LinkedTo.Num() == 0)
	{
		SMBlueprintClass = Cast<USMBlueprintGeneratedClass>(CallFunctionNode->GetBlueprint()->GeneratedClass);
	}
	else
	{
		const FEdGraphPinType ConnectedPinType = SelfPin->LinkedTo[0]->PinType;
		SMBlueprintClass = Cast<USMBlueprintGeneratedClass>(ConnectedPinType.PinSubCategoryObject);
	}

	return SMBlueprintClass;
}

TSharedRef<SWidget> SGraphPin_GetStateByNamePin::GetDefaultValueWidget()
{
	return SAssignNew(AssetPickerAnchor, SComboButton)
	.ContentPadding(3.f)
	.ButtonStyle(FSMUnrealAppStyle::Get(), "PropertyEditor.AssetComboStyle")
	.ForegroundColor(this, &SGraphPin_GetStateByNamePin::OnGetComboForeground)
	.ButtonColorAndOpacity(this, &SGraphPin_GetStateByNamePin::OnGetWidgetBackground)
	.Visibility(this, &SGraphPin_GetStateByNamePin::OnGetWidgetVisibility)
	.MenuPlacement(MenuPlacement_BelowAnchor)
	.IsEnabled_Lambda([this]()
	{
		if (!IsEditingEnabled())
		{
			return false;
		}
		if (const UEdGraphPin* Pin = GetPinObj())
		{
			if (USMGraphK2Node_PropertyNode_Base* PropertyNode = Cast<USMGraphK2Node_PropertyNode_Base>(Pin->GetOwningNodeUnchecked()))
			{
				if (const FSMGraphProperty_Base* GraphProperty = PropertyNode->GetPropertyNode())
				{
					if (GraphProperty->bReadOnly)
					{
						return false;
					}
				}
			}

			return !Pin->bDefaultValueIsReadOnly;
		}

		return true;
	})
	.OnGetMenuContent(this, &SGraphPin_GetStateByNamePin::OnGetMenuContent)
	.ButtonContent()
	[
		SNew(STextBlock).Text(this, &SGraphPin_GetStateByNamePin::OnGetDefaultComboText)
	];
}

TSharedRef<SWidget> SGraphPin_GetStateByNamePin::OnGetMenuContent()
{
	USMBlueprintGeneratedClass* SMBlueprintClass = GetBlueprintGeneratedClass(GraphPinObj);
	if (SMBlueprintClass == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	return SAssignNew(StateTreeView, SSMStateTreeSelectionView, SMBlueprintClass)
	.OnSelectionChanged(this, &SGraphPin_GetStateByNamePin::OnStateSelected);
}

FText SGraphPin_GetStateByNamePin::OnGetDefaultComboText() const
{
	if (GraphPinObj)
	{
		FText CurrentValue = GraphPinObj->GetDefaultAsText();
		if (!CurrentValue.IsEmpty())
		{
			return CurrentValue;
		}
	}
	
	return LOCTEXT("DefaultComboText", "Select State");
}

FSlateColor SGraphPin_GetStateByNamePin::OnGetComboForeground() const
{
	const float Alpha = (IsHovered() || bOnlyShowDefaultValue)
		                    ? ActiveComboAlpha
		                    : InActiveComboAlpha;
	return FSlateColor(FLinearColor(1.f, 1.f, 1.f, Alpha));
}

FSlateColor SGraphPin_GetStateByNamePin::OnGetWidgetBackground() const
{
	const float Alpha = (IsHovered() || bOnlyShowDefaultValue)
		                    ? ActivePinBackgroundAlpha
		                    : InactivePinBackgroundAlpha;
	return FSlateColor(FLinearColor(1.f, 1.f, 1.f, Alpha));
}

EVisibility SGraphPin_GetStateByNamePin::OnGetWidgetVisibility() const
{
	return GraphPinObj && GraphPinObj->LinkedTo.Num() == 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

void SGraphPin_GetStateByNamePin::OnStateSelected(FSMStateTreeItemPtr SelectedState)
{
	if (!GraphPinObj || GraphPinObj->IsPendingKill())
	{
		return;
	}

	if (!SelectedState.IsValid())
	{
		const FScopedTransaction Transaction(NSLOCTEXT("StateNamePin", "ClearPinValue", "Clear State Name"));
		GraphPinObj->Modify();
		GraphPinObj->ResetDefaultValue();
		return;
	}
	
	const FString FullyQualifiedName = SelectedState.IsValid() ? SelectedState->BuildQualifiedNameString() : TEXT("");
	if (!GraphPinObj->GetDefaultAsString().Equals(FullyQualifiedName))
	{
		const FScopedTransaction Transaction(NSLOCTEXT("StateNamePin", "ChangePinValue", "Select State Name"));
		GraphPinObj->Modify();
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, FullyQualifiedName);
	}
	
	CloseComboButton();
}

void SGraphPin_GetStateByNamePin::CloseComboButton()
{
	AssetPickerAnchor->SetIsOpen(false);
}

#undef LOCTEXT_NAMESPACE
