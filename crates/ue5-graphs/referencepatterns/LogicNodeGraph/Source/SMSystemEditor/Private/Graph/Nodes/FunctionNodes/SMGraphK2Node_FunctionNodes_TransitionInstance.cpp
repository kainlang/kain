// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_FunctionNodes_TransitionInstance.h"

#include "Compilers/SMKismetCompiler.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "SMGraphK2Node_StateReadNodes.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "SMTransitionInstance.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"

#define LOCTEXT_NAMESPACE "SMFunctionNodeInstances"

USMGraphK2Node_TransitionInstance_Base::USMGraphK2Node_TransitionInstance_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_TransitionInstance_Base::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	if (GetClass() != USMGraphK2Node_TransitionInstance_Base::StaticClass())
	{
		return Super::GetMenuActions(ActionRegistrar);
	}
}

bool USMGraphK2Node_TransitionInstance_Base::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	if (!Super::IsCompatibleWithGraph(Graph))
	{
		return false;
	}

	return FSMBlueprintEditorUtils::GetNodeTemplateClass(Graph)->IsChildOf(USMTransitionInstance::StaticClass());
}


USMGraphK2Node_TransitionInstance_CanEnterTransition::USMGraphK2Node_TransitionInstance_CanEnterTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_TransitionInstance_CanEnterTransition::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Boolean, USMGraphK2Schema::PN_ReturnValue);
}

FText USMGraphK2Node_TransitionInstance_CanEnterTransition::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("InstanceCanEnterTransition", "Can Enter Transition (Instance)");
}

void USMGraphK2Node_TransitionInstance_CanEnterTransition::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	if (!NodeInstanceClass)
	{
		CompilerContext.MessageLog.Error(TEXT("Can't expand node @@, instance template not set."), this);
		return;
	}

	if (FSMNodeClassRule::IsBaseClass(NodeInstanceClass))
	{
		const bool bResult = ExpandAndWireStandardFunction(USMTransitionInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTransitionInstance, CanEnterTransition)),
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
	UFunction* Function = USMTransitionInstance::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName());
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

UEdGraphPin* USMGraphK2Node_TransitionInstance_CanEnterTransition::GetReturnValuePinChecked() const
{
	return FindPinChecked(USMGraphK2Schema::PN_ReturnValue, EGPD_Output);
}

void USMGraphK2Node_TransitionStackInstance_CanEnterTransition::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	
}

void USMGraphK2Node_TransitionStackInstance_CanEnterTransition::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();
}

FText USMGraphK2Node_TransitionStackInstance_CanEnterTransition::GetNodeTitle(ENodeTitleType::Type Title) const
{
	if (TransitionStackTemplateGuid.IsValid())
	{
		if (const USMGraphNode_TransitionEdge* TransitionEdge = GetTransitionEdge())
		{
			const USMNodeInstance* NodeInstance = TransitionEdge->GetTemplateFromGuid(TransitionStackTemplateGuid);
			const int32 Index = TransitionEdge->GetIndexOfTemplate(TransitionStackTemplateGuid);
			const FText StackInstanceName = FNodeStackContainer::FormatStackInstanceName(NodeInstance, Index);
			const FText FullTitle = FText::Format(LOCTEXT("StackInstanceNodeTitle", "Can Enter Transition (Stack {0})"), StackInstanceName);
			return FullTitle;
		}
	}
	
	return LOCTEXT("StackInstanceCanEnterTransition", "Can Enter Transition (Stack Instance)");
}

FLinearColor USMGraphK2Node_TransitionStackInstance_CanEnterTransition::GetNodeTitleColor() const
{
	if (TransitionStackTemplateGuid.IsValid())
	{
		if (const USMGraphNode_TransitionEdge* TransitionEdge = GetTransitionEdge())
		{
			if (const USMNodeInstance* NodeInstance = TransitionEdge->GetTemplateFromGuid(TransitionStackTemplateGuid))
			{
				return NodeInstance->GetNodeColor();
			}
		}
	}

	return Super::GetNodeTitleColor();
}

FText USMGraphK2Node_TransitionStackInstance_CanEnterTransition::GetTooltipText() const
{
	return LOCTEXT("TransitionStackInstanceTooltip", "Calls CanEnterTransition of the transition stack node instance.");
}

bool USMGraphK2Node_TransitionStackInstance_CanEnterTransition::CanUserDeleteNode() const
{
	TArray<USMGraphK2Node_TransitionStackInstance_CanEnterTransition*> Nodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested(GetGraph(), Nodes);

	for (USMGraphK2Node_TransitionStackInstance_CanEnterTransition* Node : Nodes)
	{
		if (Node == this)
		{
			continue;
		}

		// Only allow deletion if this node is a duplicate.
		if (GetNodeStackGuid() == Node->GetNodeStackGuid())
		{
			return true;
		}
	}

	return false;
}

UObject* USMGraphK2Node_TransitionStackInstance_CanEnterTransition::GetJumpTargetForDoubleClick() const
{
	if (const UClass* NodeClassToUse = GetNodeInstanceClass())
	{
		if (!NodeClassToUse->IsNative())
		{
			if (UBlueprint* NodeBlueprint = FSMBlueprintEditorUtils::GetNodeBlueprintFromClassAndSetDebugObject(NodeClassToUse,
				Cast<USMGraphNode_Base>(GetTypedOuter(USMGraphNode_Base::StaticClass())), &TransitionStackTemplateGuid))
			{
				return NodeBlueprint;
			}
		}
	}
	
	return nullptr;
}

void USMGraphK2Node_TransitionStackInstance_CanEnterTransition::CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer,
                                                                                 FProperty* NodeProperty)
{
	if (!NodeInstanceClass)
	{
		CompilerContext.MessageLog.Error(TEXT("Can't expand node @@, instance template not set."), this);
		return;
	}

	if (FSMNodeClassRule::IsBaseClass(NodeInstanceClass))
	{
		CompilerContext.MessageLog.Error(TEXT("Can't expand node @@, instance class not set."), this);
		return;
	}

	if (!TransitionStackTemplateGuid.IsValid())
	{
		CompilerContext.MessageLog.Error(TEXT("Can't expand node @@, invalid stack template guid."), this);
		return;
	}

	// GetNodeInstance
	UK2Node_DynamicCast* BaseCastNode = nullptr;
	USMGraphK2Node_StateReadNode_GetNodeInstance::CreateAndWireExpandedNodes(this, USMTransitionInstance::StaticClass(), CompilerContext, RuntimeNodeContainer, NodeProperty, &BaseCastNode);
	check(BaseCastNode);

	// GetStackInstance
	UFunction* GetStackFunction = USMTransitionInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTransitionInstance, GetTransitionInStack));
	check(GetStackFunction);

	const UK2Node_CallFunction* GetStackFunctionNode = FSMBlueprintEditorUtils::CreateFunctionCall(CompilerContext.ConsolidatedEventGraph, GetStackFunction);
	
	UEdGraphPin* IndexPin = GetStackFunctionNode->FindPinChecked(TEXT("Index"), EGPD_Input);
	IndexPin->DefaultValue = FString::FromInt(StackIndex);

	UEdGraphPin* GetInstanceOutputPin = BaseCastNode->GetCastResultPin();
	UEdGraphPin* StackSelfPinIn = GetStackFunctionNode->FindPinChecked(FName(USMGraphK2Schema::PN_Self));
	
	check(CompilerContext.ConsolidatedEventGraph->GetSchema()->TryCreateConnection(GetInstanceOutputPin, StackSelfPinIn));
	
	UEdGraphPin* GetStackInstanceOutputPin = GetStackFunctionNode->GetReturnValuePin();
	
	// CanEnterTransition (Stack Instance)
	UFunction* CanEnterTransitionFunction = USMTransitionInstance::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName());
	const UK2Node_CallFunction* CanEnterTransitionFunctionNode = FSMBlueprintEditorUtils::CreateFunctionCall(CompilerContext.ConsolidatedEventGraph, CanEnterTransitionFunction);

	UEdGraphPin* CanEnterTransitionFunctionNodeSelfPinIn = CanEnterTransitionFunctionNode->FindPinChecked(FName(USMGraphK2Schema::PN_Self));

	// GetStackInstance -> CanEnterTransition
	check(CompilerContext.ConsolidatedEventGraph->GetSchema()->TryCreateConnection(GetStackInstanceOutputPin, CanEnterTransitionFunctionNodeSelfPinIn));

	UEdGraphPin* CanEnterTransitionFunctionNodeResultPinOut = CanEnterTransitionFunctionNode->FindPinChecked(USMGraphK2Schema::PN_ReturnValue);
	const UEdGraphPin* OldResultPinIn = FindPinChecked(USMGraphK2Schema::PN_ReturnValue);

	// Wire old pins to new pins.
	CanEnterTransitionFunctionNodeResultPinOut->CopyPersistentDataFromOldPin(*OldResultPinIn);

	BreakAllNodeLinks();
}

void USMGraphK2Node_TransitionStackInstance_CanEnterTransition::PreConsolidatedEventGraphValidate(FCompilerResultsLog& MessageLog)
{
	// Skip parent call as it calls GetNodeTemplateClass which won't work for this case.
	USMGraphK2Node_RuntimeNodeReference::PreConsolidatedEventGraphValidate(MessageLog);
	
	Modify();

	NodeInstanceClass = GetNodeInstanceClass();

	if (const USMGraphNode_TransitionEdge* TransitionEdge = GetTransitionEdge())
	{
		StackIndex = TransitionEdge->GetIndexOfTemplate(TransitionStackTemplateGuid);
	}
}

UClass* USMGraphK2Node_TransitionStackInstance_CanEnterTransition::GetNodeInstanceClass() const
{
	if (const USMGraphNode_TransitionEdge* TransitionEdge = GetTransitionEdge())
	{
		if (const USMNodeInstance* Template = TransitionEdge->GetTemplateFromGuid(TransitionStackTemplateGuid))
		{
			return Template->GetClass();
		}
	}
	
	return nullptr;
}

USMGraphNode_TransitionEdge* USMGraphK2Node_TransitionStackInstance_CanEnterTransition::GetTransitionEdge() const
{
	if (const UEdGraph* OwningGraph = GetGraph())
	{
		return Cast<USMGraphNode_TransitionEdge>(FSMBlueprintEditorUtils::FindTopLevelOwningNode(OwningGraph));
	}

	return nullptr;
}

USMGraphK2Node_TransitionStackInstance_CanEnterTransition::USMGraphK2Node_TransitionStackInstance_CanEnterTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


USMGraphK2Node_TransitionInstance_OnTransitionTaken::USMGraphK2Node_TransitionInstance_OnTransitionTaken(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_TransitionInstance_OnTransitionTaken::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("InstanceTransitionEntered", "Call On Transition Entered (Instance)");
}

void USMGraphK2Node_TransitionInstance_OnTransitionTaken::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	ExpandAndWireStandardFunction(USMTransitionInstance::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName()),
		nullptr, CompilerContext, RuntimeNodeContainer, NodeProperty);
}


USMGraphK2Node_TransitionInstance_OnTransitionInitialized::USMGraphK2Node_TransitionInstance_OnTransitionInitialized(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_TransitionInstance_OnTransitionInitialized::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("InstanceTransitionInitialized", "Call On Transition Initialized (Instance)");
}

void USMGraphK2Node_TransitionInstance_OnTransitionInitialized::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	ExpandAndWireStandardFunction(USMTransitionInstance::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName()),
		nullptr, CompilerContext, RuntimeNodeContainer, NodeProperty);
}


USMGraphK2Node_TransitionInstance_OnTransitionShutdown::USMGraphK2Node_TransitionInstance_OnTransitionShutdown(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_TransitionInstance_OnTransitionShutdown::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("InstanceTransitionShutdown", "Call On Transition Shutdown (Instance)");
}

void USMGraphK2Node_TransitionInstance_OnTransitionShutdown::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	ExpandAndWireStandardFunction(USMTransitionInstance::StaticClass()->FindFunctionByName(GetInstanceRuntimeFunctionName()),
		nullptr, CompilerContext, RuntimeNodeContainer, NodeProperty);
}

#undef LOCTEXT_NAMESPACE
