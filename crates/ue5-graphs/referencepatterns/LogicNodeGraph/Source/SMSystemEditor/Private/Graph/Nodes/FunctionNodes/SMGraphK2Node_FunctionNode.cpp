// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNode.h"

#include "Compilers/SMKismetCompiler.h"
#include "Graph/Nodes/SMGraphNode_Base.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMConduitGraph.h"
#include "Graph/SMStateGraph.h"
#include "Graph/SMTransitionGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "K2Node_CallFunction.h"

#include "Blueprints/SMBlueprint.h"
#include "SMNode_Base.h"

#define LOCTEXT_NAMESPACE "SMStateMachineFunctionNode"

USMGraphK2Node_FunctionNode::USMGraphK2Node_FunctionNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_FunctionNode::GetMenuCategory() const
{
	return FText::FromString(STATE_MACHINE_HELPER_CATEGORY);
}

bool USMGraphK2Node_FunctionNode::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
{
	for (UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		if (!Cast<USMBlueprint>(Blueprint))
		{
			return true;
		}
	}

	for (UEdGraph* Graph : Filter.Context.Graphs)
	{
		if (!Graph->IsA<USMTransitionGraph>() && !Graph->IsA<USMStateGraph>() && !Graph->IsA<USMConduitGraph>())
		{
			return true;
		}
	}

	return false;
}

void USMGraphK2Node_FunctionNode::PostPlacedNewNode()
{
	if (USMGraphK2Node_RuntimeNodeContainer* Container = GetRuntimeContainer())
	{
		RuntimeNodeGuid = Container->GetRunTimeNodeChecked()->GetNodeGuid();
	}
}

void USMGraphK2Node_FunctionNode::PostPasteNode()
{
	// Skip parent handling all together. Duplicating this type of node is fine.
	UK2Node::PostPasteNode();
	if (USMGraphK2Node_RuntimeNodeContainer* Container = GetRuntimeContainer())
	{
		RuntimeNodeGuid = Container->GetRunTimeNodeChecked()->GetNodeGuid();
	}
}

bool USMGraphK2Node_FunctionNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMTransitionGraph>() || Graph->IsA<USMStateGraph>();
}

UEdGraphPin* USMGraphK2Node_FunctionNode::GetInputPin() const
{
	const int32 VarInputPin = INDEX_PIN_INPUT + 1;

	if (Pins.Num() <= VarInputPin || Pins[VarInputPin]->Direction == EGPD_Output)
	{
		return nullptr;
	}

	return Pins[VarInputPin];
}

bool USMGraphK2Node_FunctionNode::ExpandAndWireStandardFunction(UFunction* Function, UEdGraphPin* SelfPin,
	FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer,
	FProperty* NodeProperty)
{
	// Call end function.
	UK2Node_CallFunction* StartFunctionNode = FSMBlueprintEditorUtils::CreateFunctionCall(CompilerContext.ConsolidatedEventGraph, Function);
	CompilerContext.MessageLog.NotifyIntermediateObjectCreation(StartFunctionNode, this);
	
	UEdGraphPin* SelfPinNew = StartFunctionNode->FindPinChecked(FName(USMGraphK2Schema::PN_Self));
	UEdGraphPin* ExecutePinNew = StartFunctionNode->FindPinChecked(USMGraphK2Schema::PN_Execute);
	UEdGraphPin* ThenPinNew = StartFunctionNode->FindPinChecked(USMGraphK2Schema::PN_Then);

	UEdGraphPin* ExecutePinOld = FindPinChecked(USMGraphK2Schema::PN_Execute);
	UEdGraphPin* ThenPinOld = FindPinChecked(USMGraphK2Schema::PN_Then);

	// Wire the reference pin to the self pin so we are calling start on the reference.
	CompilerContext.ConsolidatedEventGraph->GetSchema()->TryCreateConnection(SelfPin, SelfPinNew);

	// Wire old pins to new pins.
	ExecutePinNew->CopyPersistentDataFromOldPin(*ExecutePinOld);
	CompilerContext.MessageLog.NotifyIntermediatePinCreation(ExecutePinNew, ExecutePinOld);
	ThenPinNew->CopyPersistentDataFromOldPin(*ThenPinOld);
	CompilerContext.MessageLog.NotifyIntermediatePinCreation(ThenPinNew, ThenPinOld);

	BreakAllNodeLinks();
	return true;
}

#undef LOCTEXT_NAMESPACE
