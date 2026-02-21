// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_FunctionNodes_StateInstance.h"

#include "Compilers/SMKismetCompiler.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "SMGraphK2Node_StateReadNodes.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "SMStateInstance.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"

#define LOCTEXT_NAMESPACE "SMFunctionNodeStateInstances"

USMGraphK2Node_StateInstance_Base::USMGraphK2Node_StateInstance_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_StateInstance_Base::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	if (GetClass() != USMGraphK2Node_StateInstance_Base::StaticClass())
	{
		return Super::GetMenuActions(ActionRegistrar);
	}
}

bool USMGraphK2Node_StateInstance_Base::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	if (!Super::IsCompatibleWithGraph(Graph))
	{
		return false;
	}

	return FSMBlueprintEditorUtils::GetNodeTemplateClass(Graph)->IsChildOf(USMStateInstance_Base::StaticClass());
}


USMGraphK2Node_StateInstance_Begin::USMGraphK2Node_StateInstance_Begin(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_StateInstance_Begin::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("StartStateNode", "Call On State Begin (Instance)");
}

void USMGraphK2Node_StateInstance_Begin::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	ExpandAndWireStandardFunction(USMStateInstance_Base::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName()),
		nullptr, CompilerContext, RuntimeNodeContainer, NodeProperty);
}

FName USMGraphK2Node_StateInstance_Begin::GetInstanceRuntimeFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(USMStateInstance_Base, OnStateBegin);
}


USMGraphK2Node_StateInstance_Update::USMGraphK2Node_StateInstance_Update(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_StateInstance_Update::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, USMGraphK2Schema::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Input, USMGraphK2Schema::PC_Real, USMGraphK2Schema::PC_Float, TEXT("DeltaSeconds"));
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Exec, UEdGraphSchema_K2::PN_Then);
}

FText USMGraphK2Node_StateInstance_Update::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UpdateStateNode", "Call On State Update (Instance)");
}

void USMGraphK2Node_StateInstance_Update::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	if (!NodeInstanceClass)
	{
		CompilerContext.MessageLog.Error(TEXT("Can't expand node @@, instance template not set."), this);
		return;
	}

	if (FSMNodeClassRule::IsBaseClass(NodeInstanceClass))
	{
		const bool bResult = ExpandAndWireStandardFunction(USMStateInstance_Base::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName()),
		nullptr, CompilerContext, RuntimeNodeContainer, NodeProperty);
		ensure(!bResult);
		return;
	}

	// Retrieve the getter for the node instance.
	UK2Node_DynamicCast* CastNode = nullptr;
	USMGraphK2Node_StateReadNode_GetNodeInstance::CreateAndWireExpandedNodes(this, NodeInstanceClass, CompilerContext, RuntimeNodeContainer, NodeProperty, &CastNode);
	check(CastNode);

	UEdGraphPin* GetInstanceOutputPin = CastNode->GetCastResultPin();
	
	// Call update.
	UFunction* Function = USMStateInstance_Base::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName());
	UK2Node_CallFunction* StartFunctionNode = FSMBlueprintEditorUtils::CreateFunctionCall(CompilerContext.ConsolidatedEventGraph, Function);
	CompilerContext.MessageLog.NotifyIntermediateObjectCreation(StartFunctionNode, this);
	
	UEdGraphPin* SelfPinIn = StartFunctionNode->FindPinChecked(UEdGraphSchema_K2::PN_Self);
	UEdGraphPin* SecondsPinIn = StartFunctionNode->FindPinChecked(FName("DeltaSeconds"));
	UEdGraphPin* ExecutePinIn = StartFunctionNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute);
	UEdGraphPin* ThenPinIn = StartFunctionNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);

	UEdGraphPin* SecondsPinOut = FindPinChecked(FName("DeltaSeconds"));
	UEdGraphPin* ExecutePinOut = FindPinChecked(UEdGraphSchema_K2::PN_Execute);
	UEdGraphPin* ThenPinOut = FindPinChecked(UEdGraphSchema_K2::PN_Then);

	// Wire the reference pin to the self pin so we are calling start on the reference.
	CompilerContext.ConsolidatedEventGraph->GetSchema()->TryCreateConnection(GetInstanceOutputPin, SelfPinIn);

	// Wire old pins to new pins.
	SecondsPinIn->CopyPersistentDataFromOldPin(*SecondsPinOut);
	ExecutePinIn->CopyPersistentDataFromOldPin(*ExecutePinOut);
	ThenPinIn->CopyPersistentDataFromOldPin(*ThenPinOut);

	BreakAllNodeLinks();
}

FName USMGraphK2Node_StateInstance_Update::GetInstanceRuntimeFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(USMStateInstance_Base, OnStateUpdate);
}


USMGraphK2Node_StateInstance_End::USMGraphK2Node_StateInstance_End(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_StateInstance_End::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("StopStateNode", "Call On State End (Instance)");
}

void USMGraphK2Node_StateInstance_End::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	ExpandAndWireStandardFunction(USMStateInstance_Base::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName()),
		nullptr, CompilerContext, RuntimeNodeContainer, NodeProperty);
}

FName USMGraphK2Node_StateInstance_End::GetInstanceRuntimeFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(USMStateInstance_Base, OnStateEnd);
}


USMGraphK2Node_StateInstance_StateMachineStart::USMGraphK2Node_StateInstance_StateMachineStart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_StateInstance_StateMachineStart::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("StateMachineStartNode", "Call On Root State Machine Start (Instance)");
}

void USMGraphK2Node_StateInstance_StateMachineStart::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	ExpandAndWireStandardFunction(USMStateInstance_Base::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName()),
		nullptr, CompilerContext, RuntimeNodeContainer, NodeProperty);
}

FName USMGraphK2Node_StateInstance_StateMachineStart::GetInstanceRuntimeFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(USMStateInstance_Base, OnRootStateMachineStart);
}


USMGraphK2Node_StateInstance_StateMachineStop::USMGraphK2Node_StateInstance_StateMachineStop(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_StateInstance_StateMachineStop::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("StateMachineStopNode", "Call On Root State Machine Stop (Instance)");
}

void USMGraphK2Node_StateInstance_StateMachineStop::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	ExpandAndWireStandardFunction(USMStateInstance_Base::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName()),
		nullptr, CompilerContext, RuntimeNodeContainer, NodeProperty);
}

FName USMGraphK2Node_StateInstance_StateMachineStop::GetInstanceRuntimeFunctionName() const
{
 return GET_FUNCTION_NAME_CHECKED(USMStateInstance_Base, OnRootStateMachineStop); }


USMGraphK2Node_StateInstance_OnStateInitialized::USMGraphK2Node_StateInstance_OnStateInitialized(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_StateInstance_OnStateInitialized::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("InstanceStateInitialized", "Call On State Initialized (Instance)");
}

void USMGraphK2Node_StateInstance_OnStateInitialized::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	ExpandAndWireStandardFunction(USMStateInstance::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName()),
		nullptr, CompilerContext, RuntimeNodeContainer, NodeProperty);
}

FName USMGraphK2Node_StateInstance_OnStateInitialized::GetInstanceRuntimeFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(USMStateInstance, OnStateInitialized);
}


USMGraphK2Node_StateInstance_OnStateShutdown::USMGraphK2Node_StateInstance_OnStateShutdown(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_StateInstance_OnStateShutdown::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("InstanceStateShutdown", "Call On State Shutdown (Instance)");
}

void USMGraphK2Node_StateInstance_OnStateShutdown::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	ExpandAndWireStandardFunction(USMStateInstance::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName()),
		nullptr, CompilerContext, RuntimeNodeContainer, NodeProperty);
}

FName USMGraphK2Node_StateInstance_OnStateShutdown::GetInstanceRuntimeFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(USMStateInstance, OnStateShutdown);
}

#undef LOCTEXT_NAMESPACE
