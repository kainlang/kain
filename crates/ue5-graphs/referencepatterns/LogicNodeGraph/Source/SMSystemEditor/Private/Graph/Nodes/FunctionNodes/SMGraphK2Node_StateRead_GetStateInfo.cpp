// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_StateReadNodes.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMStateGraph.h"
#include "Graph/SMTransitionGraph.h"

#include "SMUtils.h"

#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_CallFunction.h"

#define LOCTEXT_NAMESPACE "SMStateMachineReadNodeGetInfo"

USMGraphK2Node_StateReadNode_GetStateInformation::USMGraphK2Node_StateReadNode_GetStateInformation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_StateReadNode_GetStateInformation::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Struct, FSMStateInfo::StaticStruct(), TEXT("StateInfo"));
}

bool USMGraphK2Node_StateReadNode_GetStateInformation::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMStateGraph>();
}

FText USMGraphK2Node_StateReadNode_GetStateInformation::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType != ENodeTitleType::MenuTitle)
	{
		const FString StateName = GetMostRecentStateName();
		if (!StateName.IsEmpty())
		{
			return FText::FromString(FString::Printf(TEXT("Get State '%s' Info"), *StateName));
		}
	}
	
	return FText::FromString(TEXT("Get State Info"));
}

FText USMGraphK2Node_StateReadNode_GetStateInformation::GetTooltipText() const
{
	return LOCTEXT("StateInfoTooltip", "Read only information about this state.");
}

void USMGraphK2Node_StateReadNode_GetStateInformation::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}

void USMGraphK2Node_StateReadNode_GetStateInformation::CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	UFunction* Function = USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, TryGetStateInfo));
	UK2Node_CallFunction* GetInfoFunctionNode = CreateFunctionCallWithGuidInput(Function, CompilerContext, RuntimeNodeContainer, NodeProperty);
	
	UEdGraphPin* GetInfoOutputPin = GetInfoFunctionNode->FindPinChecked(FName("StateInfo"), EGPD_Output);
	GetInfoOutputPin->CopyPersistentDataFromOldPin(*GetOutputPin());
	BreakAllNodeLinks();
}



USMGraphK2Node_StateReadNode_GetTransitionInformation::USMGraphK2Node_StateReadNode_GetTransitionInformation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_StateReadNode_GetTransitionInformation::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Struct, FSMTransitionInfo::StaticStruct(), TEXT("TransitionInfo"));
}

bool USMGraphK2Node_StateReadNode_GetTransitionInformation::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMTransitionGraph>();
}

FText USMGraphK2Node_StateReadNode_GetTransitionInformation::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType != ENodeTitleType::MenuTitle)
	{
		const FString TransitionName = GetTransitionName();
		if (!TransitionName.IsEmpty())
		{
			return FText::FromString(FString::Printf(TEXT("Get Transition '%s' Info"), *TransitionName));
		}
	}
	
	return FText::FromString(TEXT("Get Transition Info"));
}

FText USMGraphK2Node_StateReadNode_GetTransitionInformation::GetTooltipText() const
{
	return LOCTEXT("TransitionInfoTooltip", "Read only information about this transition.");
}

void USMGraphK2Node_StateReadNode_GetTransitionInformation::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}

void USMGraphK2Node_StateReadNode_GetTransitionInformation::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	UFunction* Function = USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, TryGetTransitionInfo));
	UK2Node_CallFunction* GetInfoFunctionNode = CreateFunctionCallWithGuidInput(Function, CompilerContext, RuntimeNodeContainer, NodeProperty);

	UEdGraphPin* GetInfoOutputPin = GetInfoFunctionNode->FindPin(FName("TransitionInfo"), EGPD_Output);
	GetInfoOutputPin->CopyPersistentDataFromOldPin(*GetOutputPin());
	BreakAllNodeLinks();
}

#undef LOCTEXT_NAMESPACE
