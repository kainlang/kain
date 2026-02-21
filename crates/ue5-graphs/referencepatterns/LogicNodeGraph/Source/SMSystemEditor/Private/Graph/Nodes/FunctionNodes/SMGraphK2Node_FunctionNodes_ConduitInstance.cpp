// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_FunctionNodes_ConduitInstance.h"

#include "Compilers/SMKismetCompiler.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "SMGraphK2Node_StateReadNodes.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "SMConduitInstance.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"

#define LOCTEXT_NAMESPACE "SMFunctionConduitInstances"

USMGraphK2Node_ConduitInstance_Base::USMGraphK2Node_ConduitInstance_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_ConduitInstance_Base::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	if (GetClass() != USMGraphK2Node_ConduitInstance_Base::StaticClass())
	{
		return Super::GetMenuActions(ActionRegistrar);
	}
}

bool USMGraphK2Node_ConduitInstance_Base::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	if (!Super::IsCompatibleWithGraph(Graph))
	{
		return false;
	}

	return FSMBlueprintEditorUtils::GetNodeTemplateClass(Graph)->IsChildOf(USMConduitInstance::StaticClass());
}


USMGraphK2Node_ConduitInstance_CanEnterTransition::USMGraphK2Node_ConduitInstance_CanEnterTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_ConduitInstance_CanEnterTransition::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Boolean, USMGraphK2Schema::PN_ReturnValue);
}

FText USMGraphK2Node_ConduitInstance_CanEnterTransition::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("ConduitInstanceCanEnterTransition", "Can Enter Transition (Instance)");
}

void USMGraphK2Node_ConduitInstance_CanEnterTransition::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	if (!NodeInstanceClass)
	{
		CompilerContext.MessageLog.Error(TEXT("Can't expand node @@, instance template not set."), this);
		return;
	}

	if (FSMNodeClassRule::IsBaseClass(NodeInstanceClass))
	{
		const bool bResult = ExpandAndWireStandardFunction(USMConduitInstance::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName()),
		nullptr, CompilerContext, RuntimeNodeContainer, NodeProperty);
		ensure(!bResult);
		return;
	}
	
	// Retrieve the getter for the node instance.
	UK2Node_DynamicCast* CastNode = nullptr;
	USMGraphK2Node_StateReadNode_GetNodeInstance::CreateAndWireExpandedNodes(this, NodeInstanceClass, CompilerContext, RuntimeNodeContainer, NodeProperty, &CastNode);
	check(CastNode);

	UEdGraphPin* GetInstanceOutputPin = CastNode->GetCastResultPin();

	// Call end function.
	UFunction* Function = USMConduitInstance::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName());
	UK2Node_CallFunction* EvalFunctionNode = FSMBlueprintEditorUtils::CreateFunctionCall(CompilerContext.ConsolidatedEventGraph, Function);

	UEdGraphPin* SelfPinIn = EvalFunctionNode->FindPinChecked(FName(USMGraphK2Schema::PN_Self));
	UEdGraphPin* ResultPinOut = EvalFunctionNode->FindPinChecked(USMGraphK2Schema::PN_ReturnValue);

	UEdGraphPin* OldResultPinIn = FindPinChecked(USMGraphK2Schema::PN_ReturnValue);

	// Wire the reference pin to the self pin so we are calling start on the reference.
	CompilerContext.ConsolidatedEventGraph->GetSchema()->TryCreateConnection(GetInstanceOutputPin, SelfPinIn);

	// Wire old pins to new pins.
	ResultPinOut->CopyPersistentDataFromOldPin(*OldResultPinIn);

	BreakAllNodeLinks();
}

FName USMGraphK2Node_ConduitInstance_CanEnterTransition::GetInstanceRuntimeFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(USMConduitInstance, CanEnterTransition);
}


USMGraphK2Node_ConduitInstance_OnConduitEntered::USMGraphK2Node_ConduitInstance_OnConduitEntered(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_ConduitInstance_OnConduitEntered::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("InstanceConduitEntered", "Call On Conduit Entered (Instance)");
}

void USMGraphK2Node_ConduitInstance_OnConduitEntered::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	ExpandAndWireStandardFunction(USMConduitInstance::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName()),
		nullptr, CompilerContext, RuntimeNodeContainer, NodeProperty);
}

FName USMGraphK2Node_ConduitInstance_OnConduitEntered::GetInstanceRuntimeFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(USMConduitInstance, OnConduitEntered);
}


USMGraphK2Node_ConduitInstance_OnConduitInitialized::USMGraphK2Node_ConduitInstance_OnConduitInitialized(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_ConduitInstance_OnConduitInitialized::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("InstanceConduitInitialized", "Call On Conduit Initialized (Instance)");
}

void USMGraphK2Node_ConduitInstance_OnConduitInitialized::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	ExpandAndWireStandardFunction(USMConduitInstance::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName()),
		nullptr, CompilerContext, RuntimeNodeContainer, NodeProperty);
}

FName USMGraphK2Node_ConduitInstance_OnConduitInitialized::GetInstanceRuntimeFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(USMConduitInstance, OnConduitInitialized);
}


USMGraphK2Node_ConduitInstance_OnConduitShutdown::USMGraphK2Node_ConduitInstance_OnConduitShutdown(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_ConduitInstance_OnConduitShutdown::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("InstanceConduitShutdown", "Call On Conduit Shutdown (Instance)");
}

void USMGraphK2Node_ConduitInstance_OnConduitShutdown::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	ExpandAndWireStandardFunction(USMConduitInstance::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName()),
		nullptr, CompilerContext, RuntimeNodeContainer, NodeProperty);
}

FName USMGraphK2Node_ConduitInstance_OnConduitShutdown::GetInstanceRuntimeFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(USMConduitInstance, OnConduitShutdown);
}

#undef LOCTEXT_NAMESPACE
