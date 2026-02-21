// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Utilities/SMTextGraphUtils.h"

#include "Configuration/SMTextGraphEditorSettings.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_TextPropertyNode.h"
#include "Graph/SMTextPropertyGraph.h"
#include "SMExtendedPropertyHelpers.h"

#include "Blueprints/SMBlueprint.h"
#include "SMNodeInstance.h"

#include "Graph/Nodes/SMGraphNode_Base.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "K2Node_CallFunction.h"
#include "Kismet/KismetTextLibrary.h"

void FSMTextGraphUtils::GetAllNodesWithTextPropertiesNested(UBlueprint* Blueprint, TArray<USMGraphNode_Base*>& NodesOut, const FName& ParsedNameFilter)
{
	TArray<USMGraphNode_Base*> AllNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_Base>(FSMBlueprintEditorUtils::GetTopLevelStateMachineGraph(Blueprint), AllNodes);

	for (USMGraphNode_Base* Node : AllNodes)
	{
		for (const TTuple<FGuid, TObjectPtr<USMGraphK2Node_PropertyNode_Base>>& GraphPropertyNode : Node->GetAllPropertyGraphNodes())
		{
			if (const USMGraphK2Node_TextPropertyNode* TextProperty = Cast<USMGraphK2Node_TextPropertyNode>(GraphPropertyNode.Value))
			{
				if (const USMTextPropertyGraph* TextPropertyGraph = Cast<USMTextPropertyGraph>(TextProperty->GetPropertyGraph()))
				{
					if (!ParsedNameFilter.IsNone() && !TextPropertyGraph->ContainsFunction(ParsedNameFilter) && !TextPropertyGraph->ContainsProperty(ParsedNameFilter))
					{
						// Check filter before adding nodes.
						continue;
					}
				}
				
				NodesOut.Add(Node);
			}
		}
	}
}

void FSMTextGraphUtils::GetAllTextPropertiesNested(UBlueprint* Blueprint,
	TArray<USMGraphK2Node_TextPropertyNode*>& TextPropertyNodesOut, const FName& ParsedNameFilter)
{
	TArray<USMGraphNode_Base*> AllNodes;
	GetAllNodesWithTextPropertiesNested(Blueprint, AllNodes, ParsedNameFilter);

	for (const USMGraphNode_Base* Node : AllNodes)
	{
		for (const TTuple<FGuid, TObjectPtr<USMGraphK2Node_PropertyNode_Base>>& GraphPropertyNode : Node->GetAllPropertyGraphNodes())
		{
			if (USMGraphK2Node_TextPropertyNode* PropertyNode = Cast<USMGraphK2Node_TextPropertyNode>(GraphPropertyNode.Value))
			{
				TextPropertyNodesOut.Add(PropertyNode);
			}
		}
	}
}

void FSMTextGraphUtils::RefreshTextProperties(UBlueprint* InBlueprint, const FName& ContainingParsedName)
{
	if (USMBlueprint* SMBlueprint = Cast<USMBlueprint>(InBlueprint))
	{
		SMBlueprint->bPreventConditionalCompile = true;
		
		TArray<USMGraphK2Node_TextPropertyNode*> TextPropertyNodes;
		GetAllTextPropertiesNested(InBlueprint, TextPropertyNodes, ContainingParsedName);

		for (USMGraphK2Node_TextPropertyNode* Node : TextPropertyNodes)
		{
			if (USMTextPropertyGraph* TextPropertyGraph = Cast<USMTextPropertyGraph>(Node->GetPropertyGraph()))
			{
				const bool bResetGraph = false; // Don't clear, we need existing variables.
				const bool bOnlyIfChanged = true;
				TextPropertyGraph->RefreshTextBody(true, bResetGraph, bOnlyIfChanged);
			}
		}

		SMBlueprint->bPreventConditionalCompile = false;

		if (TextPropertyNodes.Num() > 0)
		{
			FSMBlueprintEditorUtils::ConditionallyCompileBlueprint(SMBlueprint);
		}
	}
}

void FSMTextGraphUtils::HandleRenameVariableReferencesEvent(UBlueprint* InBlueprint, UClass* InVariableClass,
	const FName& InOldVarName, const FName& InNewVarName)
{
	if (InOldVarName != InNewVarName)
	{
		RefreshTextProperties(InBlueprint, InOldVarName);
	}
}

void FSMTextGraphUtils::HandleRenameGraphEvent(UBlueprint* InBlueprint, UEdGraph* InVariableClass,
	const FName& InOldVarName, const FName& InNewVarName)
{
	if (InOldVarName != InNewVarName && InVariableClass && !InVariableClass->GetClass()->IsChildOf(USMGraphK2::StaticClass()))
	{
		// Renaming our custom graphs won't impact text graph properties, but renaming function graphs will.
		RefreshTextProperties(InBlueprint, InOldVarName);
	}
}

static TSet<USMBlueprint*> BlueprintsWithTextPropertiesToUpdate;

void FSMTextGraphUtils::HandleOnPropertyChangedEvent(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent)
{
	const FName PropertyName = InPropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FSMTextSerializer, ToTextFunctionNames) || PropertyName == GET_MEMBER_NAME_CHECKED(FSMTextSerializer, ToTextDynamicFunctionName))
	{
		// Text graph serialization changes requires a complete graph rebuild.
		// Track the blueprints impacted only so they can be read later during a compile.
	
		if (USMNodeInstance* NodeInstance = Cast<USMNodeInstance>(InObject))
		{
			if (USMNodeBlueprint* NodeBlueprint = Cast<USMNodeBlueprint>(UBlueprint::GetBlueprintFromClass(NodeInstance->GetClass())))
			{
				TArray<UBlueprint*> OtherBlueprints;
				FBlueprintEditorUtils::GetDependentBlueprints(NodeBlueprint, OtherBlueprints);

				for (UBlueprint* OtherBlueprint : OtherBlueprints)
				{
					if (USMBlueprint* SMBlueprint = Cast<USMBlueprint>(OtherBlueprint))
					{
						BlueprintsWithTextPropertiesToUpdate.Add(SMBlueprint);
					}
				}
			}
		}
	}
}

void FSMTextGraphUtils::HandlePostConditionallyCompileBlueprintEvent(UBlueprint* Blueprint, bool bUpdateDependencies, bool bRecreateGraphProperties)
{
	USMBlueprint* SMBlueprint = Cast<USMBlueprint>(Blueprint);
	if (SMBlueprint && bRecreateGraphProperties && BlueprintsWithTextPropertiesToUpdate.Contains(SMBlueprint))
	{
		// Look for text graph properties that require a full rebuild.
		
		TArray<USMGraphK2Node_TextPropertyNode*> TextNodes;
		GetAllTextPropertiesNested(SMBlueprint, TextNodes);

		for (USMGraphK2Node_TextPropertyNode* Node : TextNodes)
		{
			if (USMTextPropertyGraph* TextGraph = Cast<USMTextPropertyGraph>(Node->GetPropertyGraph()))
			{
				const bool bFullReset = true;
				TextGraph->RefreshTextBody(false, bFullReset);
			}
		}
		
		BlueprintsWithTextPropertiesToUpdate.Remove(SMBlueprint);
	}
}

UK2Node_CallFunction* FSMTextGraphUtils::CreateTextConversionNode(USMTextPropertyGraph* Graph, UEdGraphPin* FromPin,
                                                                       UEdGraphPin* ToPin, const FSMTextSerializer& TextSerializer, bool bWireConnection)
{
	const UEdGraphSchema_K2* K2_Schema = Cast<const UEdGraphSchema_K2>(Graph->GetSchema());

	if (!ensure(FromPin))
	{
		return nullptr;
	}
	
	bool bUsingCustomFunction = false;
	bool bIsOurStaticFunction = false;
	const FName DynamicFunctionName = GetCustomConversionFunctionName(TextSerializer);
	
	UFunction* MakeNodeFunction = nullptr;
	UObject* Object = FromPin->PinType.PinSubCategoryObject.Get();
	if (Object && TextSerializer.ToTextFunctionNames.Num() > 0)
	{
		// Attempt look up of custom function first. This takes priority.
		for (const FName& CustomFunctionName : TextSerializer.ToTextFunctionNames)
		{
			if (CustomFunctionName == NAME_None)
			{
				continue;
			}
			
			if (UClass* Class = Cast<UClass>(Object))
			{
				MakeNodeFunction = Class->FindFunctionByName(CustomFunctionName);
			}
			else
			{
				MakeNodeFunction = Object->FindFunction(CustomFunctionName);
			}

			if (MakeNodeFunction)
			{
				break;
			}
		}
	}

	// Check node functions.
	if (!MakeNodeFunction && DynamicFunctionName != NAME_None)
	{
		MakeNodeFunction = USMExtendedGraphPropertyHelpers::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMExtendedGraphPropertyHelpers, ObjectToText));
		if (MakeNodeFunction)
		{
			bIsOurStaticFunction = true;
		}
	}

	if (MakeNodeFunction == nullptr)
	{
		// No custom function found or provided, attempt default lookup.
		MakeNodeFunction = UKismetTextLibrary::StaticClass()->FindFunctionByName(FindTextConversionFunctionName(FromPin->PinType.PinCategory, Object));
	}
	else
	{
		bUsingCustomFunction = true;
	}
	
	if (MakeNodeFunction == nullptr)
	{
		return nullptr;
	}

	UK2Node_CallFunction* ConversionNode = NewObject<UK2Node_CallFunction>(Graph);

	ConversionNode->CreateNewGuid();
	ConversionNode->PostPlacedNewNode();
	ConversionNode->SetFromFunction(MakeNodeFunction);
	ConversionNode->SetFlags(RF_Transactional);
	ConversionNode->AllocateDefaultPins();
	ConversionNode->NodePosX = FromPin->GetOwningNode()->NodePosX;
	ConversionNode->NodePosY = FromPin->GetOwningNode()->NodePosY + 32;
	Graph->AddNode(ConversionNode, true);

	if (bWireConnection)
	{
		// CustomFunctions probably use a self pin. If the static function for dynamic lookup then there is no self pin.
		UEdGraphPin* ConversionInputPin = GetConversionInputPin(ConversionNode, bUsingCustomFunction && !bIsOurStaticFunction);

		if (ConversionInputPin == nullptr)
		{
			return ConversionNode;
		}

		UEdGraphPin* ConversionOutputPin = GetConversionOutputPin(ConversionNode);

		if (ConversionOutputPin == nullptr)
		{
			return ConversionNode;
		}

		if (bIsOurStaticFunction)
		{
			if (UEdGraphPin* FunctionNamePin = GetStaticFunctionPin(ConversionNode))
			{
				K2_Schema->TrySetDefaultValue(*FunctionNamePin, DynamicFunctionName.ToString());
			}
		}

		// Wire connections from argument node to the conversion node, and from the conversion node to the format node.
		K2_Schema->TryCreateConnection(FromPin, ConversionInputPin);
		K2_Schema->TryCreateConnection(ConversionOutputPin, ToPin);
	}

	return ConversionNode;
}

UEdGraphPin* FSMTextGraphUtils::GetConversionInputPin(UEdGraphNode* Node, bool bCheckSelfPin)
{
	const UEdGraphSchema_K2* K2_Schema = Cast<const UEdGraphSchema_K2>(Node->GetSchema());

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->Direction == EGPD_Input && !UEdGraphSchema_K2::IsExecPin(*Pin))
		{
			if (!K2_Schema->IsSelfPin(*Pin) || bCheckSelfPin)
			{
				return Pin;
			}
		}
	}

	return nullptr;
}

UEdGraphPin* FSMTextGraphUtils::GetConversionOutputPin(UEdGraphNode* Node)
{
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->Direction == EGPD_Output && !UEdGraphSchema_K2::IsExecPin(*Pin))
		{
			return Pin;
		}
	}

	return nullptr;
}

UEdGraphPin* FSMTextGraphUtils::GetStaticFunctionPin(UEdGraphNode* Node)
{
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->Direction == EGPD_Input && !UEdGraphSchema_K2::IsExecPin(*Pin) && Pin->GetName() == TEXT("InFunctionName"))
		{
			return Pin;
		}
	}

	return nullptr;
}

FName FSMTextGraphUtils::FindTextConversionFunctionName(FName FromType, UObject* Object)
{
	if (FromType == UEdGraphSchema_K2::PC_Boolean)
	{
		return GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_BoolToText);
	}
	if (FromType == UEdGraphSchema_K2::PC_Byte)
	{
		return GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_ByteToText);
	}
	if (FromType == UEdGraphSchema_K2::PC_Int)
	{
		return GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_IntToText);
	}
	if (FromType == UEdGraphSchema_K2::PC_Float || FromType == UEdGraphSchema_K2::PC_Real)
	{
		return GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_DoubleToText);
	}
	if (FromType == UEdGraphSchema_K2::PC_Name)
	{
		return GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_NameToText);
	}
	if (FromType == UEdGraphSchema_K2::PC_String)
	{
		return GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_StringToText);
	}
	if (FromType == UEdGraphSchema_K2::PC_Object)
	{
		return GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_ObjectToText);
	}
	if (FromType == UEdGraphSchema_K2::PC_Struct)
	{
		if (Object)
		{
			if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Object))
			{
				const FName Name = ScriptStruct->GetFName();

				if (Name == TEXT("Vector"))
				{
					return GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_VectorToText);
				}
				if (Name == TEXT("Vector2d"))
				{
					return GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_Vector2dToText);
				}
				if (Name == TEXT("Rotator"))
				{
					return GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_RotatorToText);
				}
				if (Name == TEXT("Transform"))
				{
					return GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_TransformToText);
				}
				if (Name == TEXT("Color"))
				{
					return GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_ColorToText);
				}
			}
		}
	}

	return NAME_None;
}

FName FSMTextGraphUtils::GetCustomConversionFunctionName(const FSMTextSerializer& TextSerializer)
{
	return !TextSerializer.ToTextDynamicFunctionName.IsNone() ?
		TextSerializer.ToTextDynamicFunctionName : GetDefault<USMTextGraphEditorSettings>()->ToTextDynamicFunctionName;
}

const FGuid& FSMTextGraphUtils::GetNativePropertyGuid()
{
	static FGuid NativeGuid(0x8C40223F, 0x66954765, 0x9F4E2603, 0x835E7351);
	return NativeGuid;
}
