// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_NodeInstance.h"

#include "Compilers/SMKismetCompiler.h"
#include "Graph/Nodes/SMGraphNode_Base.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "SMGraphK2Node_StateReadNodes.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "SMNodeRules.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_DynamicCast.h"

#define LOCTEXT_NAMESPACE "SMFunctionNodeInstances"

USMGraphK2Node_FunctionNode_NodeInstance::USMGraphK2Node_FunctionNode_NodeInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_FunctionNode_NodeInstance::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	if (GetClass() != USMGraphK2Node_FunctionNode_NodeInstance::StaticClass())
	{
		GetMenuActions_Internal(ActionRegistrar);
	}
}

FText USMGraphK2Node_FunctionNode_NodeInstance::GetMenuCategory() const
{
	return FText::FromString(STATE_MACHINE_INSTANCE_CALL_CATEGORY);
}

bool USMGraphK2Node_FunctionNode_NodeInstance::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->GetSchema()->GetClass()->IsChildOf<USMGraphK2Schema>() && FSMBlueprintEditorUtils::GetNodeTemplateClass(Graph) != nullptr;
}

FText USMGraphK2Node_FunctionNode_NodeInstance::GetTooltipText() const
{
	return LOCTEXT("NodeInstanceTooltip", "Call the instance method.");
}

void USMGraphK2Node_FunctionNode_NodeInstance::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, USMGraphK2Schema::PC_Exec, USMGraphK2Schema::PN_Execute);
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Exec, USMGraphK2Schema::PN_Then);
}

UObject* USMGraphK2Node_FunctionNode_NodeInstance::GetJumpTargetForDoubleClick() const
{
	if (const UClass* NodeClassToUse = GetNodeInstanceClass())
	{
		if (!NodeClassToUse->IsNative())
		{
			if (UBlueprint* NodeBlueprint = FSMBlueprintEditorUtils::GetNodeBlueprintFromClassAndSetDebugObject(NodeClassToUse,
				Cast<USMGraphNode_Base>(GetTypedOuter(USMGraphNode_Base::StaticClass()))))
			{
				return NodeBlueprint;
			}
		}
	}

	return Super::GetJumpTargetForDoubleClick();
}

void USMGraphK2Node_FunctionNode_NodeInstance::PreConsolidatedEventGraphValidate(FCompilerResultsLog& MessageLog)
{
	Super::PreConsolidatedEventGraphValidate(MessageLog);

	Modify();
	NodeInstanceClass = FSMBlueprintEditorUtils::GetNodeTemplateClass(GetGraph(), true);
}

void USMGraphK2Node_FunctionNode_NodeInstance::CustomExpandNode(FSMKismetCompilerContext& CompilerContext,
                                                                USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
}

bool USMGraphK2Node_FunctionNode_NodeInstance::ExpandAndWireStandardFunction(UFunction* Function, UEdGraphPin* SelfPin,
	FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer,
	FProperty* NodeProperty)
{
	if (!NodeInstanceClass)
	{
		CompilerContext.MessageLog.Error(TEXT("Can't expand node @@, instance template not set."), this);
		return false;
	}

	// No point in wiring up functions to the base class. Skip this node all together.
	if (FSMNodeClassRule::IsBaseClass(NodeInstanceClass))
	{
		UEdGraphPin* ThenPin = GetThenPin();
		if (ThenPin->HasAnyConnections())
		{
			if (const UEdGraphPin* ExecPin = GetExecPin())
			{
				TArray<UEdGraphPin*> FromPins = ExecPin->LinkedTo;
				UEdGraphPin* DestinationPin = ThenPin->LinkedTo[0];
				BreakAllNodeLinks();

				for (UEdGraphPin* FromPin : FromPins)
				{
					FromPin->MakeLinkTo(DestinationPin);
				}
			}
		}
		else
		{
			BreakAllNodeLinks();
		}
		
		return false;
	}
	
	// Retrieve the getter for the node instance.
	UK2Node_DynamicCast* CastNode = nullptr;
	if (!SelfPin)
	{
		USMGraphK2Node_StateReadNode_GetNodeInstance::CreateAndWireExpandedNodes(this, NodeInstanceClass, CompilerContext, RuntimeNodeContainer, NodeProperty, &CastNode);
		check(CastNode);
	}
	
	return Super::ExpandAndWireStandardFunction(Function, SelfPin ? SelfPin : CastNode->GetCastResultPin(), CompilerContext, RuntimeNodeContainer, NodeProperty);
}

UClass* USMGraphK2Node_FunctionNode_NodeInstance::GetNodeInstanceClass() const
{
	const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this);
	return (Blueprint && Blueprint->bBeingCompiled) ? NodeInstanceClass :
			FSMBlueprintEditorUtils::GetNodeTemplateClass(GetGraph(), true);
}

#undef LOCTEXT_NAMESPACE
