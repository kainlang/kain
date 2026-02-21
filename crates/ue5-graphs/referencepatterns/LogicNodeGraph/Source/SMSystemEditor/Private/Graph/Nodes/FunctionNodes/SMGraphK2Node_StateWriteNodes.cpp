// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_StateWriteNodes.h"

#include "Compilers/SMKismetCompiler.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMConduitGraph.h"
#include "Graph/SMStateGraph.h"
#include "Graph/SMTransitionGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "SMInstance.h"
#include "SMTransition.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"
#include "K2Node_StructMemberSet.h"

#define LOCTEXT_NAMESPACE "SMStateMachineWriteNode"

USMGraphK2Node_StateWriteNode::USMGraphK2Node_StateWriteNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_StateWriteNode::GetMenuCategory() const
{
	return FText::FromString(STATE_MACHINE_HELPER_CATEGORY);
}

bool USMGraphK2Node_StateWriteNode::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
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
		if (!Graph->IsA<USMTransitionGraph>() && !Graph->IsA<USMStateGraph>())
		{
			return true;
		}
	}

	return false;
}

void USMGraphK2Node_StateWriteNode::PostPlacedNewNode()
{
	if (USMGraphK2Node_RuntimeNodeContainer* Container = GetRuntimeContainer())
	{
		RuntimeNodeGuid = Container->GetRunTimeNodeChecked()->GetNodeGuid();
	}
}

void USMGraphK2Node_StateWriteNode::PostPasteNode()
{
	// Skip parent handling all together. Duplicating this type of node is fine.
	UK2Node::PostPasteNode();
	if (USMGraphK2Node_RuntimeNodeContainer* Container = GetRuntimeContainer())
	{
		RuntimeNodeGuid = Container->GetRunTimeNodeChecked()->GetNodeGuid();
	}
}

bool USMGraphK2Node_StateWriteNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMTransitionGraph>() || Graph->IsA<USMStateGraph>();
}

UEdGraphPin* USMGraphK2Node_StateWriteNode::GetInputPin() const
{
	const int32 VarInputPin = INDEX_PIN_INPUT + 1;

	if (Pins.Num() <= VarInputPin || Pins[VarInputPin]->Direction == EGPD_Output)
	{
		return nullptr;
	}

	return Pins[VarInputPin];
}

USMGraphK2Node_StateWriteNode_CanEvaluate::USMGraphK2Node_StateWriteNode_CanEvaluate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_StateWriteNode_CanEvaluate::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, USMGraphK2Schema::PC_Exec, USMGraphK2Schema::PN_Execute);
	CreatePin(EGPD_Input, USMGraphK2Schema::PC_Boolean, TEXT("bCanEvaluate"));
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Exec, USMGraphK2Schema::PN_Then);
}

bool USMGraphK2Node_StateWriteNode_CanEvaluate::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMTransitionGraph>() || Graph->IsA<USMConduitGraph>();
}

bool USMGraphK2Node_StateWriteNode_CanEvaluate::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
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
		if (!Graph->IsA<USMTransitionGraph>() && !Graph->IsA<USMConduitGraph>())
		{
			return true;
		}
	}

	return false;
}

FText USMGraphK2Node_StateWriteNode_CanEvaluate::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("SetCanEvaluate", "Set Can Evaluate Conditionally");
}

FText USMGraphK2Node_StateWriteNode_CanEvaluate::GetTooltipText() const
{
	return LOCTEXT("CanEvaluateTooltip", "If the transition or conduit is allowed to evaluate. If false CanEnterTransition logic is never evaluated and this transition (or conduit) will never be taken.");
}

void USMGraphK2Node_StateWriteNode_CanEvaluate::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();

	// Only list option to create this node if it is not already placed.
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}


USMGraphK2Node_StateWriteNode_CanEvaluateFromEvent::USMGraphK2Node_StateWriteNode_CanEvaluateFromEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_StateWriteNode_CanEvaluateFromEvent::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, USMGraphK2Schema::PC_Exec, USMGraphK2Schema::PN_Execute);
	CreatePin(EGPD_Input, USMGraphK2Schema::PC_Boolean, GET_MEMBER_NAME_CHECKED(FSMTransition, bCanEvaluateFromEvent));
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Exec, USMGraphK2Schema::PN_Then);
}

bool USMGraphK2Node_StateWriteNode_CanEvaluateFromEvent::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMTransitionGraph>();
}

FText USMGraphK2Node_StateWriteNode_CanEvaluateFromEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("SetCanTransitionEvaluateFromEvent", "Set Can Transition Evaluate From Event");
}

FText USMGraphK2Node_StateWriteNode_CanEvaluateFromEvent::GetTooltipText() const
{
	return LOCTEXT("CanEvaluateTooltipFromEvent", "If the transition is allowed to evaluate when called from an auto-bound event.");
}

void USMGraphK2Node_StateWriteNode_CanEvaluateFromEvent::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();

	// Only list option to create this node if it is not already placed.
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}


USMGraphK2Node_StateWriteNode_TransitionEventReturn::USMGraphK2Node_StateWriteNode_TransitionEventReturn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), bEventTriggersUpdate_DEPRECATED(true), bUseOwningTransitionSettings(true),
	  bEventTriggersTargetedUpdate(true), bEventTriggersFullUpdate(false)
{
	bCanRenameNode = false;
}

void USMGraphK2Node_StateWriteNode_TransitionEventReturn::PostLoad()
{
	Super::PostLoad();

	if (!bEventTriggersUpdate_DEPRECATED)
	{
		bEventTriggersUpdate_DEPRECATED = true;
		bUseOwningTransitionSettings = false;
		bEventTriggersTargetedUpdate = false;
	}
}

void USMGraphK2Node_StateWriteNode_TransitionEventReturn::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USMGraphK2Node_StateWriteNode_TransitionEventReturn, bUseOwningTransitionSettings) && bUseOwningTransitionSettings)
	{
		UpdateEventSettingsFromTransition();
	}
}

void USMGraphK2Node_StateWriteNode_TransitionEventReturn::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, USMGraphK2Schema::PC_Exec, USMGraphK2Schema::PN_Execute);
	UEdGraphPin* EvalPin = CreatePin(EGPD_Input, USMGraphK2Schema::PC_Boolean, GET_MEMBER_NAME_CHECKED(FSMTransition, bCanEnterTransitionFromEvent));
	EvalPin->DefaultValue = TEXT("true");
	EvalPin->PinFriendlyName = FText::FromString(TEXT("CanEnterTransition"));

	if (bUseOwningTransitionSettings)
	{
		UpdateEventSettingsFromTransition();
	}
}

void USMGraphK2Node_StateWriteNode_TransitionEventReturn::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}

bool USMGraphK2Node_StateWriteNode_TransitionEventReturn::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMTransitionGraph>();
}

bool USMGraphK2Node_StateWriteNode_TransitionEventReturn::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
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
		if (!FSMBlueprintEditorUtils::IsGraphConfiguredForTransitionEvents(Graph))
		{
			return true;
		}
	}

	return false;
}

FText USMGraphK2Node_StateWriteNode_TransitionEventReturn::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromString(TEXT("Event Trigger Result Node"));
}

FText USMGraphK2Node_StateWriteNode_TransitionEventReturn::GetTooltipText() const
{
	return LOCTEXT("TransitionEventReturnToolTip", "This node can trigger transition evaluation from an event and switch to the next state.");
}

void USMGraphK2Node_StateWriteNode_TransitionEventReturn::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
                                                                           USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	// Manually add an evaluation pin to signal to the transition it is evaluating.
	UEdGraphPin* EvalPin = CreatePin(EGPD_Input, USMGraphK2Schema::PC_Boolean, GET_MEMBER_NAME_CHECKED(FSMTransition, bIsEvaluating));
	EvalPin->DefaultValue = TEXT("true");
	
	UK2Node_StructMemberSet* MemberSet = CompilerContext.CreateSetter(this, NodeProperty->GetFName(), RuntimeNodeContainer->GetRunTimeNodeType());

	UEdGraphPin* ThenPin = USMGraphK2Schema::GetThenPin(MemberSet);
	if (bEventTriggersTargetedUpdate)
	{
		UFunction* Function = USMInstance::StaticClass()->FindFunctionByName(USMInstance::GetInternalEvaluateAndTakeTransitionChainFunctionName());
		check(Function);
		const UK2Node_CallFunction* EvalTransitionFunctionNode =
			CreateFunctionCallWithGuidInput(Function, CompilerContext, RuntimeNodeContainer, NodeProperty, TEXT("PathGuid"));

		ensure(GetSchema()->TryCreateConnection(ThenPin, EvalTransitionFunctionNode->GetExecPin()));
		ThenPin = EvalTransitionFunctionNode->GetThenPin();
	}

	if (bEventTriggersFullUpdate)
	{
		UFunction* Function = USMInstance::StaticClass()->FindFunctionByName(USMInstance::GetInternalEventUpdateFunctionName());
		check(Function);
		const UK2Node_CallFunction* UpdateFunctionCall = FSMBlueprintEditorUtils::CreateFunctionCall(CompilerContext.ConsolidatedEventGraph, Function);

		ensure(GetSchema()->TryCreateConnection(ThenPin, UpdateFunctionCall->GetExecPin()));
		ThenPin = UpdateFunctionCall->GetThenPin();
	}

	// Add special cleanup handling.
	{
		UFunction* CleanupFunction = USMInstance::StaticClass()->FindFunctionByName(USMInstance::GetInternalEventCleanupFunctionName()); 
		check(CleanupFunction);
		const UK2Node_CallFunction* CleanupFunctionNode =
			CreateFunctionCallWithGuidInput(CleanupFunction, CompilerContext, RuntimeNodeContainer, NodeProperty, "PathGuid");

		ensure(GetSchema()->TryCreateConnection(ThenPin, CleanupFunctionNode->GetExecPin()));
	}
}

void USMGraphK2Node_StateWriteNode_TransitionEventReturn::UpdateEventSettingsFromTransition()
{
	if (!bUseOwningTransitionSettings)
	{
		return;
	}
	
	if (const USMGraphNode_TransitionEdge* TransitionOwner = Cast<USMGraphNode_TransitionEdge>(GetTypedOuter(USMGraphNode_TransitionEdge::StaticClass())))
	{
		bEventTriggersTargetedUpdate = TransitionOwner->bEventTriggersTargetedUpdate;
		bEventTriggersFullUpdate = TransitionOwner->bEventTriggersFullUpdate;
	}
}

#undef LOCTEXT_NAMESPACE
