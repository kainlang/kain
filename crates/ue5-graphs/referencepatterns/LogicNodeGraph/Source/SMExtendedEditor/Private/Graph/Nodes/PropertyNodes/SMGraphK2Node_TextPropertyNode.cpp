// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_TextPropertyNode.h"

#include "Commands/SMExtendedEditorCommands.h"
#include "Graph/SMTextPropertyGraph.h"
#include "Widgets/Text/SSMTextProperty.h"

#include "SMTextGraphPropertyVersion.h"

#include "Graph/Nodes/SMGraphNode_Base.h"
#include "Utilities/SMTextUtils.h"

#include "SMNodeInstance.h"

#include "FindInBlueprintManager.h"
#include "ToolMenuSection.h"
#include "Components/RichTextBlock.h"
#include "Components/RichTextBlockDecorator.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"

#define LOCTEXT_NAMESPACE "SMTextPropertyNode"

USMGraphK2Node_TextPropertyNode::USMGraphK2Node_TextPropertyNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), RichTextBlockDummyOwner(nullptr), bUpgradingToSupportDefaults(false)
{
}

void USMGraphK2Node_TextPropertyNode::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FSMTextGraphPropertyCustomVersion::GUID);
	Super::Serialize(Ar);

	if (Ar.IsLoading() && Ar.CustomVer(FSMTextGraphPropertyCustomVersion::GUID) < FSMTextGraphPropertyCustomVersion::DefaultsSupported)
	{
		bUpgradingToSupportDefaults = true;
	}
}

void USMGraphK2Node_TextPropertyNode::PostLoad()
{
	Super::PostLoad();
	if (bUpgradingToSupportDefaults)
	{
		SetPropertyDefaultsFromPin();
		bUpgradingToSupportDefaults = false;
	}
}

void USMGraphK2Node_TextPropertyNode::AllocateDefaultPins()
{
	UEdGraphPin* GraphPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Text, TEXT("Result"));

	FSMGraphProperty_Base* Prop = GetPropertyNodeChecked();
	
	const bool bIsReadOnly = Prop->IsVariableReadOnly();
	GraphPin->bNotConnectable = bIsReadOnly;
	GraphPin->bDefaultValueIsReadOnly = bIsReadOnly;
}

bool USMGraphK2Node_TextPropertyNode::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	UEdGraphPin* ResultPin = GetResultPin();
	if (ResultPin && (ResultPin == MyPin || ResultPin == OtherPin))
	{
		if (const FSMGraphProperty_Base* Prop = GetPropertyNodeConst())
		{
			if (Prop->IsVariableReadOnly())
			{
				// We probably want to allow this if the graph is being reformated.
				const USMTextPropertyGraph* TextGraph = CastChecked<USMTextPropertyGraph>(GetPropertyGraph());
				if (TextGraph->IsUpdatingGraph())
				{
					return false;
				}
			}
		}
	}
	
	return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}

void USMGraphK2Node_TextPropertyNode::PostReconstructNode()
{
	Super::PostReconstructNode();

	if (USMTextPropertyGraph* TextGraph = Cast<USMTextPropertyGraph>(GetPropertyGraph()))
	{
		if (!TextGraph->DoesPlainStringMatchFormatTextString())
		{
			TextGraph->SetTextFromFormatTextNode(true, true);
		}
	}
}

void USMGraphK2Node_TextPropertyNode::AddPinSearchMetaDataInfo(const UEdGraphPin* Pin,
	TArray<FSearchTagDataPair>& OutTaggedMetaData) const
{
	AddSharedPinSearchMetaDataInfo(OutTaggedMetaData);

	const USMTextPropertyGraph* TextGraph = CastChecked<USMTextPropertyGraph>(GetPropertyGraph());
	const UEdGraphPin* FormatPin = TextGraph->GetFormatTextNodePinChecked();
	USMGraphK2Node_RuntimeNodeReference::AddPinSearchMetaDataInfo(FormatPin, OutTaggedMetaData);
}

void USMGraphK2Node_TextPropertyNode::PreConsolidatedEventGraphValidate(FCompilerResultsLog& MessageLog)
{
	Super::PreConsolidatedEventGraphValidate(MessageLog);

	if (const USMTextPropertyGraph* Graph = Cast<USMTextPropertyGraph>(GetPropertyGraph()))
	{
		if (Graph->HasVariableParsingError())
		{
			MessageLog.Warning(TEXT("Node @@ text graph @@ has a variable parsing error. Are you missing a '}'?"), GetOwningGraphNode(), this);
		}
	}
}

void USMGraphK2Node_TextPropertyNode::ConfigureRuntimePropertyNode()
{
	RuntimeTextProperty.GraphEvaluator = TextProperty.GraphEvaluator;
	RuntimeTextProperty.SetGuid(TextProperty.GetGuid());
	RuntimeTextProperty.SetOwnerGuid(TextProperty.GetOwnerGuid());
	RuntimeTextProperty.TextSerializer = TextProperty.TextSerializer;
	RuntimeTextProperty.Result = TextProperty.Result;
}

FSMGraphProperty_Base_Runtime* USMGraphK2Node_TextPropertyNode::GetRuntimePropertyNode()
{
	return &RuntimeTextProperty;
}

TSharedPtr<SSMGraphProperty_Base> USMGraphK2Node_TextPropertyNode::GetGraphNodeWidget() const
{
	return SNew(SSMTextProperty)
		.GraphNode(const_cast<USMGraphK2Node_TextPropertyNode*>(this))
		.WidgetInfo(&TextProperty.WidgetInfo)
		.RichTextInfo(&TextProperty.RichTextInfo);
}

void USMGraphK2Node_TextPropertyNode::DefaultPropertyActionWhenPlaced(TSharedPtr<SWidget> Widget)
{
	const TSharedPtr<SSMTextProperty> TextWidget = StaticCastSharedPtr<SSMTextProperty>(Widget);
	if (TextWidget.IsValid())
	{
		TextWidget->ToggleTextEdit(true);
	}
}

void USMGraphK2Node_TextPropertyNode::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	const bool bDefaultValueChangedBeforeParentCall = bDefaultValueChanged;
	Super::PinDefaultValueChanged(Pin);
	if (!bDefaultValueChangedBeforeParentCall && bDefaultValueChanged)
	{
		bDefaultValueChanged = bDefaultValueChangedBeforeParentCall;
	}
}

void USMGraphK2Node_TextPropertyNode::SetPropertyDefaultsFromPin()
{
	if (bSettingPropertyDefaultsFromPin || HasAnyFlags(RF_NeedPostLoad | RF_NeedPostLoadSubobjects))
	{
		return;
	}

	bSettingPropertyDefaultsFromPin = true;

	if (USMNodeInstance* Template = GetOwningTemplate())
	{
		// Ignore REINST classes.
		const UClass* Class = Template->GetClass();
		if (Class->GetName().StartsWith(TEXT("REINST_")))
		{
			// Super rare to hit this branch. Likely a problem has already occurred and this is
			// an undo/redo operation.
			return;
		}
		
		const USMTextPropertyGraph* TextGraph = CastChecked<USMTextPropertyGraph>(GetPropertyGraph());
		if (const UEdGraphPin* FormatPin = TextGraph->GetFormatTextNodePinChecked())
		{
			if (FSMTextGraphProperty* TextGraphProperty = GetTextGraphProperty(Template))
			{
				Modify();
				Template->Modify();

				TextGraphProperty->Result = FormatPin->DefaultTextValue;

				if (const USMGraphNode_Base* OwningNode = GetOwningGraphNode())
				{
					if (OwningNode->IsBeingPasted() || OwningNode->IsPreCompiling())
					{
						// Pasting and recompiling will run all construction scripts.
						bSettingPropertyDefaultsFromPin = false;
						return;
					}
				}
			}
		}
	}

	bSettingPropertyDefaultsFromPin = false;
}

void USMGraphK2Node_TextPropertyNode::SetPinValueFromPropertyDefaults(bool bUpdateTemplateDefaults, bool bUseArchetype, bool bForce)
{
	if (bGeneratedDefaultValueBeingSet || bUpgradingToSupportDefaults)
	{
		return;
	}

	if (bResettingProperty)
	{
		// Never reset from the instance.
		bUseArchetype = true;
	}
	
	if (bDefaultValueChanged && !bForce)
	{
		/*
		 * Assume the pin is accurate and update the default value of the archetype. Pasting nodes doesn't grab the updated value
		 * when a variable name has a special character like `[` or `(` character in it. We think this is a problem with CopyPropertiesForUnrelatedObjects.
		 * Without this code default values may not be set and will require graph evaluation.
		 */
		if (bUpdateTemplateDefaults)
		{
			SetPropertyDefaultsFromPin();
		}
		return;
	}
	
	if (USMNodeInstance* Template = GetOwningTemplate())
	{
		// Ignore REINST classes.
		const UClass* Class = Template->GetClass();
		if (Class->GetName().StartsWith(TEXT("REINST_")))
		{
			return;
		}

		// Switch to the CDO so we can get the real defaults.
		Template = bUseArchetype ? CastChecked<USMNodeInstance>(Class->GetDefaultObject()) : Template;
		USMTextPropertyGraph* TextGraph = CastChecked<USMTextPropertyGraph>(GetPropertyGraph());
		
		if (UEdGraphPin* FormatPin = TextGraph->GetFormatTextNodePinChecked())
		{
			// Only reset if the user hasn't changed the default value.
			if (bForce || bResettingProperty || IsValueSetToDefault())
			{
				if (const FSMTextGraphProperty* TextGraphProperty = GetTextGraphProperty(Template))
				{
					FTextProperty* ResultProperty =
						CastFieldChecked<FTextProperty>(FSMTextGraphProperty::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FSMTextGraphProperty, Result)));
					check(ResultProperty);

					const uint8* ResultContainer = ResultProperty->ContainerPtrToValuePtr<uint8>(TextGraphProperty);
					
					FString TextStringBuffer;
					FBlueprintEditorUtils::PropertyValueToString_Direct(ResultProperty, ResultContainer, TextStringBuffer, Template);
					
					const UEdGraphSchema_K2* Schema = CastChecked<UEdGraphSchema_K2>(GetSchema());

					// Text needs special handling to preserve localization key from CDO. bPreserveTextIdentity is false when using TrySetDefaultValue.
					FString UseDefaultValue;
					TObjectPtr<UObject> UseDefaultObject = nullptr;
					FText UseDefaultText;
					Schema->GetPinDefaultValuesFromString(FormatPin->PinType, FormatPin->GetOwningNodeUnchecked(),
						TextStringBuffer, UseDefaultValue, UseDefaultObject, UseDefaultText, true);

					// The package may still be set to the CDO's package. If we don't change it here when it eventually gets
					// set to the property TextHistory.cpp will fail and generate a new key guid.
					// Always use the instance for the correct package, even if we're loading from an archetype.
					const USMNodeInstance* InstanceTemplate = GetOwningTemplate();
					check(InstanceTemplate);
					ConformLocalizationPackage(FormatPin->PinType, TextStringBuffer, UseDefaultText, InstanceTemplate->GetPackage());

					// Only update if the value is different.
					const FString CurrentValueAsString = LD::TextUtils::TextToStringBuffer(FormatPin->DefaultTextValue);
					if (bResettingProperty || !CurrentValueAsString.Equals(TextStringBuffer) || !LastAutoGeneratedDefaultValue.Equals(TextStringBuffer))
					{
						Modify();
						FormatPin->Modify();
						
						bGeneratedDefaultValueBeingSet = true;
						LastAutoGeneratedDefaultValue = TextStringBuffer;
						
						const FText AsText = LD::TextUtils::StringBufferToText(TextStringBuffer);
						
						// Set the pin value first so SetTextBody has the correct property data to compare. Very useful
						// for localization changes.
						Schema->TrySetDefaultText(*FormatPin, AsText);
						TextGraph->SetTextBody(AsText);
						
						bGeneratedDefaultValueBeingSet = false;
					}
				}
			}
		}
	}
}

bool USMGraphK2Node_TextPropertyNode::IsValueSetToDefault() const
{
	if (const USMTextPropertyGraph* TextGraph = Cast<USMTextPropertyGraph>(GetPropertyGraph()))
	{
		const FString CurrentValueAsString = LD::TextUtils::TextToStringBuffer(TextGraph->GetFormatTextNodeText());
		const bool bMatch = LD::TextUtils::DoesTextValueAndLocalizationMatch(CurrentValueAsString,LastAutoGeneratedDefaultValue);
		return bMatch;
	}
	return Super::IsValueSetToDefault();
}

void USMGraphK2Node_TextPropertyNode::Internal_GetContextMenuActionsForOwningNode(const UEdGraph* CurrentGraph,
                                                                                  const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, FToolMenuSection& MenuSection, bool bIsDebugging) const
{
	if (!GetPropertyGraph()->IsGraphBeingUsedToEdit() && !bIsDebugging)
	{
		MenuSection.AddMenuEntry(FSMExtendedEditorCommands::Get().StartTextPropertyEdit);
	}
	Super::Internal_GetContextMenuActionsForOwningNode(CurrentGraph, InGraphNode, InGraphPin, MenuSection, bIsDebugging);
}

FSMTextGraphProperty* USMGraphK2Node_TextPropertyNode::GetTextGraphProperty(USMNodeInstance* Template) const
{
	check(Template);
	FSMTextGraphProperty* TextGraphProperty = nullptr;

	const FSMGraphProperty_Base* GraphProperty = GetPropertyNodeConstChecked();
	if (FProperty* Property = GraphProperty->MemberReference.ResolveMember<FProperty>(Template->GetClass()))
	{
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<uint8>(Template));
			if (Helper.IsValidIndex(GraphProperty->ArrayIndex))
			{
				if (uint8* DefaultValue = Helper.GetRawPtr(GraphProperty->ArrayIndex))
				{
					TextGraphProperty = ArrayProperty->Inner->ContainerPtrToValuePtr<FSMTextGraphProperty>(DefaultValue);
				}
				check(TextGraphProperty);
			}
		}
		else
		{
			TextGraphProperty = Property->ContainerPtrToValuePtr<FSMTextGraphProperty>(Template);
		}
	}

	return TextGraphProperty;
}

void USMGraphK2Node_TextPropertyNode::NotifyFormatTextUpdated(const FText& NewText)
{
	const FText OldText = LD::TextUtils::StringBufferToText(LastAutoGeneratedDefaultValue);
	if (!LD::TextUtils::DoesTextValueAndLocalizationMatch(OldText, NewText))
	{
		bDefaultValueChanged = true;
	}
}

void USMGraphK2Node_TextPropertyNode::CreateDecorators(TArray<TSharedRef<ITextDecorator>>& OutDecorators)
{
	RichStyleInstanceDecorators.Reset();

	if (USMNodeInstance* Template = GetOwningTemplate())
	{
		if (FSMTextGraphProperty* TextGraphProperty = GetTextGraphProperty(Template))
		{
			if (TextGraphProperty->RichTextInfo.RichTextDecoratorClasses.Num() == 0)
			{
				return;
			}

			if (!RichTextBlockDummyOwner)
			{
				RichTextBlockDummyOwner = NewObject<URichTextBlock>(this);
			}

			// Setting the style is necessary to prevent an ensure later from the decorators.
			RichTextBlockDummyOwner->SetTextStyleSet(TextGraphProperty->RichTextInfo.RichTextStyleSet);

			for (TSubclassOf<URichTextBlockDecorator> DecoratorClass : TextGraphProperty->RichTextInfo.RichTextDecoratorClasses)
			{
				if (const UClass* ResolvedClass = DecoratorClass.Get())
				{
					if (!ResolvedClass->HasAnyClassFlags(CLASS_Abstract))
					{
						URichTextBlockDecorator* Decorator = NewObject<URichTextBlockDecorator>(RichTextBlockDummyOwner, ResolvedClass);
						RichStyleInstanceDecorators.Add(Decorator);

						TSharedPtr<ITextDecorator> TextDecorator = Decorator->CreateDecorator(RichTextBlockDummyOwner);
						if (TextDecorator.IsValid())
						{
							OutDecorators.Add(TextDecorator.ToSharedRef());
						}
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
