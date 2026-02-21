// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_StateMachineRef.h"

#include "Compilers/SMKismetCompiler.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_StateReadNodes.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMIntermediateGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "SMInstance.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_CallFunction.h"

#define LOCTEXT_NAMESPACE "SMStateMachineFunctionNodeStateMachineRef"

void USMGraphK2Node_FunctionNode_StateMachineRef::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
}

USMGraphK2Node_FunctionNode_StateMachineRef::USMGraphK2Node_FunctionNode_StateMachineRef(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USMGraphK2Node_FunctionNode_StateMachineRef::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMIntermediateGraph>();
}

FText USMGraphK2Node_FunctionNode_StateMachineRef::GetMenuCategory() const
{
	return FText::FromString(STATE_MACHINE_INSTANCE_CALL_CATEGORY);
}

USMGraphK2Node_StateMachineRef_Start::USMGraphK2Node_StateMachineRef_Start(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_FunctionNode_StateMachineRef::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
}

void USMGraphK2Node_StateMachineRef_Start::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}

void USMGraphK2Node_StateMachineRef_Start::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, USMGraphK2Schema::PC_Exec, USMGraphK2Schema::PN_Execute);
	CreatePin(EGPD_Input, USMGraphK2Schema::PC_Object, UObject::StaticClass(), TEXT("Context"));
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Exec, USMGraphK2Schema::PN_Then);
}

FText USMGraphK2Node_StateMachineRef_Start::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::MenuTitle || TitleType == ENodeTitleType::ListView)
	{
		return LOCTEXT("StartStateMachineReference", "Start State Machine Reference");
	}

	return FText::FromString(TEXT("Start State Machine Reference"));
}

FText USMGraphK2Node_StateMachineRef_Start::GetTooltipText() const
{
	return LOCTEXT("StateMachineRefStart", "Start the state machine reference.");
}

void USMGraphK2Node_StateMachineRef_Start::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	// Retrieve state machine reference.
	UFunction* Function = USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, GetReferencedInstanceByGuid));
	UK2Node_CallFunction* GetReferenceFunctionNode = CreateFunctionCallWithGuidInput(Function, CompilerContext, RuntimeNodeContainer, NodeProperty);

	UEdGraphPin* GetReferenceOutputPin = GetReferenceFunctionNode->GetReturnValuePin();

	// Call state machine start.

	UFunction* StartFunction = USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, StartWithNewContext));
	UK2Node_CallFunction* StartFunctionNode = FSMBlueprintEditorUtils::CreateFunctionCall(CompilerContext.ConsolidatedEventGraph, StartFunction);
	CompilerContext.MessageLog.NotifyIntermediateObjectCreation(StartFunctionNode, this);

	UEdGraphPin* SelfPinIn = StartFunctionNode->FindPinChecked(FName("self"));
	UEdGraphPin* ContextPinIn = StartFunctionNode->FindPinChecked(FName("Context"));
	UEdGraphPin* ExecutePinIn = StartFunctionNode->FindPinChecked(FName("execute"));
	UEdGraphPin* ThenPinIn = StartFunctionNode->FindPinChecked(FName("then"));

	UEdGraphPin* ContextPinOut = FindPinChecked(FName("Context"));
	UEdGraphPin* ExecutePinOut = FindPinChecked(FName("execute"));
	UEdGraphPin* ThenPinOut = FindPinChecked(FName("then"));

	// Wire the reference pin to the self pin so we are calling start on the reference.
	CompilerContext.ConsolidatedEventGraph->GetSchema()->TryCreateConnection(GetReferenceOutputPin, SelfPinIn);

	// Wire old pins to new pins.
	ContextPinIn->CopyPersistentDataFromOldPin(*ContextPinOut);
	ExecutePinIn->CopyPersistentDataFromOldPin(*ExecutePinOut);
	ThenPinIn->CopyPersistentDataFromOldPin(*ThenPinOut);

	BreakAllNodeLinks();
}



USMGraphK2Node_StateMachineRef_Update::USMGraphK2Node_StateMachineRef_Update(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_StateMachineRef_Update::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}

void USMGraphK2Node_StateMachineRef_Update::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, USMGraphK2Schema::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Input, USMGraphK2Schema::PC_Real, USMGraphK2Schema::PC_Float, TEXT("DeltaSeconds"));
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Exec, UEdGraphSchema_K2::PN_Then);
}

FText USMGraphK2Node_StateMachineRef_Update::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::MenuTitle || TitleType == ENodeTitleType::ListView)
	{
		return LOCTEXT("UpdateStateMachineReference", "Update State Machine Reference");
	}

	return FText::FromString(TEXT("Update State Machine Reference"));
}

FText USMGraphK2Node_StateMachineRef_Update::GetTooltipText() const
{
	return LOCTEXT("StateMachineRefUpdate", "Update the state machine reference.");
}

void USMGraphK2Node_StateMachineRef_Update::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	// Retrieve state machine reference.
	UFunction* Function = USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, GetReferencedInstanceByGuid));
	UK2Node_CallFunction* GetReferenceFunctionNode = CreateFunctionCallWithGuidInput(Function, CompilerContext, RuntimeNodeContainer, NodeProperty);

	UEdGraphPin* GetReferenceOutputPin = GetReferenceFunctionNode->GetReturnValuePin();

	// Call state machine start.

	UFunction* StartFunction = USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, RunUpdateAsReference));
	UK2Node_CallFunction* StartFunctionNode = FSMBlueprintEditorUtils::CreateFunctionCall(CompilerContext.ConsolidatedEventGraph, StartFunction);
	CompilerContext.MessageLog.NotifyIntermediateObjectCreation(StartFunctionNode, this);
	
	UEdGraphPin* SelfPinIn = StartFunctionNode->FindPinChecked(FName("self"));
	UEdGraphPin* SecondsPinIn = StartFunctionNode->FindPinChecked(FName("DeltaSeconds"));
	UEdGraphPin* ExecutePinIn = StartFunctionNode->FindPinChecked(FName("execute"));
	UEdGraphPin* ThenPinIn = StartFunctionNode->FindPinChecked(FName("then"));

	UEdGraphPin* SecondsPinOut = FindPinChecked(FName("DeltaSeconds"));
	UEdGraphPin* ExecutePinOut = FindPinChecked(FName("execute"));
	UEdGraphPin* ThenPinOut = FindPinChecked(FName("then"));

	// Wire the reference pin to the self pin so we are calling start on the reference.
	CompilerContext.ConsolidatedEventGraph->GetSchema()->TryCreateConnection(GetReferenceOutputPin, SelfPinIn);

	// Wire old pins to new pins.
	SecondsPinIn->CopyPersistentDataFromOldPin(*SecondsPinOut);
	ExecutePinIn->CopyPersistentDataFromOldPin(*ExecutePinOut);
	ThenPinIn->CopyPersistentDataFromOldPin(*ThenPinOut);

	BreakAllNodeLinks();
}



USMGraphK2Node_StateMachineRef_Stop::USMGraphK2Node_StateMachineRef_Stop(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


void USMGraphK2Node_StateMachineRef_Stop::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}

void USMGraphK2Node_StateMachineRef_Stop::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, USMGraphK2Schema::PC_Exec, USMGraphK2Schema::PN_Execute);
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Exec, USMGraphK2Schema::PN_Then);
}

FText USMGraphK2Node_StateMachineRef_Stop::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::MenuTitle || TitleType == ENodeTitleType::ListView)
	{
		return LOCTEXT("StopStateMachineReference", "Stop State Machine Reference");
	}

	return FText::FromString(TEXT("Stop State Machine Reference"));
}

FText USMGraphK2Node_StateMachineRef_Stop::GetTooltipText() const
{
	return LOCTEXT("StateMachineRefStop", "Stop the state machine reference.");
}

void USMGraphK2Node_StateMachineRef_Stop::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	// Retrieve state machine reference.
	UFunction* Function = USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, GetReferencedInstanceByGuid));
	UK2Node_CallFunction* GetReferenceFunctionNode = CreateFunctionCallWithGuidInput(Function, CompilerContext, RuntimeNodeContainer, NodeProperty);

	UEdGraphPin* GetReferenceOutputPin = GetReferenceFunctionNode->GetReturnValuePin();

	// Call state machine start.

	UFunction* StartFunction = USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, Stop));
	UK2Node_CallFunction* StartFunctionNode = FSMBlueprintEditorUtils::CreateFunctionCall(CompilerContext.ConsolidatedEventGraph, StartFunction);
	CompilerContext.MessageLog.NotifyIntermediateObjectCreation(StartFunctionNode, this);
	
	UEdGraphPin* SelfPinIn = StartFunctionNode->FindPinChecked(FName("self"));
	UEdGraphPin* ExecutePinIn = StartFunctionNode->FindPinChecked(FName("execute"));
	UEdGraphPin* ThenPinIn = StartFunctionNode->FindPinChecked(FName("then"));

	UEdGraphPin* ExecutePinOut = FindPinChecked(FName("execute"));
	UEdGraphPin* ThenPinOut = FindPinChecked(FName("then"));

	// Wire the reference pin to the self pin so we are calling start on the reference.
	CompilerContext.ConsolidatedEventGraph->GetSchema()->TryCreateConnection(GetReferenceOutputPin, SelfPinIn);

	// Wire old pins to new pins.
	ExecutePinIn->CopyPersistentDataFromOldPin(*ExecutePinOut);
	ThenPinIn->CopyPersistentDataFromOldPin(*ThenPinOut);

	BreakAllNodeLinks();
}

#undef LOCTEXT_NAMESPACE
