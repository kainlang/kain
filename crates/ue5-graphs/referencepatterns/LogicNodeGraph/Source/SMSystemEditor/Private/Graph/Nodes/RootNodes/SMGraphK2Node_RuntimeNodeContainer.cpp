// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/RootNodes/SMGraphK2Node_RuntimeNodeContainer.h"

#include "Compilers/SMKismetCompiler.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_ConduitInstance.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "SMNode_Base.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "K2Node_CallFunction.h"
#include "K2Node_StructMemberGet.h"
#include "Kismet2/CompilerResultsLog.h"

#define LOCTEXT_NAMESPACE "SMContainerNode"

USMGraphK2Node_FunctionNode_NodeInstance* USMGraphK2Node_RuntimeNode_Base::GetConnectedNodeInstanceFunction() const
{
	if (UEdGraphPin* ThenPin = GetCorrectEntryPin())
	{
		if (ThenPin->LinkedTo.Num() > 0)
		{
			if (USMGraphK2Node_FunctionNode_NodeInstance* NextNode = Cast<USMGraphK2Node_FunctionNode_NodeInstance>(ThenPin->LinkedTo[0]->GetOwningNode()))
			{
				if (IsCompatibleWithInstanceGraphNodeClass(NextNode->GetClass()))
				{
					return NextNode;
				}
			}
		}
	}
	
	return nullptr;
}

USMGraphK2Node_FunctionNode_NodeInstance* USMGraphK2Node_RuntimeNode_Base::
GetConnectedNodeInstanceFunctionIfValidForOptimization() const
{
	if (USMGraphK2Node_FunctionNode_NodeInstance* NextNode = GetConnectedNodeInstanceFunction())
	{
		if (const UEdGraphPin* NextNodeThenPin = GetCorrectNodeInstanceOutputPin(NextNode))
		{
			// Either there are no nodes, or were linking back to this node (in the case of CanEnterTransition minor hack!).
			if (NextNodeThenPin->LinkedTo.Num() == 0 ||
				(NextNodeThenPin->LinkedTo.Num() == 1 && NextNodeThenPin->LinkedTo[0]->GetOwningNode() == this))
			{
				return NextNode;
			}
		}
	}
	
	return nullptr;
}

ESMExposedFunctionExecutionType USMGraphK2Node_RuntimeNode_Base::GetGraphExecutionType() const
{
	if (const UEdGraphPin* ThenPin = GetCorrectEntryPin())
	{
		if (ThenPin->LinkedTo.Num() == 0)
		{
			return ESMExposedFunctionExecutionType::SM_None;
		}
		
		if (const USMGraphK2Node_FunctionNode_NodeInstance* NodeInstance = GetConnectedNodeInstanceFunctionIfValidForOptimization())
		{
			UClass* InstanceClass = NodeInstance->GetNodeInstanceClass();
			if (FSMNodeClassRule::IsBaseClass(InstanceClass))
			{
				// Don't bother running any execution if this is just to a default class.
				return ESMExposedFunctionExecutionType::SM_None;
			}
			return ESMExposedFunctionExecutionType::SM_NodeInstance;
		}
	}
	
	return ESMExposedFunctionExecutionType::SM_Graph;
}

UEdGraphPin* USMGraphK2Node_RuntimeNode_Base::GetCorrectEntryPin() const
{
	return GetThenPin();
}

UEdGraphPin* USMGraphK2Node_RuntimeNode_Base::GetCorrectNodeInstanceOutputPin(USMGraphK2Node_FunctionNode_NodeInstance* InInstance) const
{
	return InInstance->FindPin(UEdGraphSchema_K2::PN_Then);
}


USMGraphK2Node_RuntimeNodeContainer::USMGraphK2Node_RuntimeNodeContainer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), bHasNodeGuidGeneratedForCopy(false)
{
}

bool USMGraphK2Node_RuntimeNode_Base::IsFastPathEnabled() const
{
	if (bFastPathEnabledCached.IsSet())
	{
		return *bFastPathEnabledCached;
	}
	
	bool bUseFastPath = false;
	if (IsConsideredForEntryConnection())
	{
		const ESMExposedFunctionExecutionType ExecutionType = GetGraphExecutionType();
		switch(ExecutionType)
		{
		case ESMExposedFunctionExecutionType::SM_None:
			{
				bUseFastPath = true;
				break;
			}
		case ESMExposedFunctionExecutionType::SM_Graph:
			{
				break;
			}
		case ESMExposedFunctionExecutionType::SM_NodeInstance:
			{
				if (const USMGraphNode_Base* OwningNode = GetTypedOuter<USMGraphNode_Base>())
				{
					bUseFastPath = OwningNode->IsNodeClassNative();
				}
				break;
			}
		}
	}
	
	bFastPathEnabledCached = bUseFastPath;

	return bUseFastPath;
}

void USMGraphK2Node_RuntimeNodeContainer::PrepareForCopying()
{
	Super::PrepareForCopying();
	// So referenced nodes know that this node is not ready.
	bHasNodeGuidGeneratedForCopy = false;
}

void USMGraphK2Node_RuntimeNodeContainer::PostPasteNode()
{
	Super::PostPasteNode();

	if (bIsBeingDestroyed)
	{
		return;
	}

	ForceGenerateNodeGuid();
}

bool USMGraphK2Node_RuntimeNodeContainer::HasExternalDependencies(TArray<UStruct*>* OptionalOutput) const
{
	UClass* OwningClass = nullptr;
	UClass* OwningReferenceClass = nullptr;
	if (USMGraphNode_Base* GraphNode = Cast<USMGraphNode_Base>(GetGraph()->GetOuter()))
	{
		OwningClass = GraphNode->GetNodeClass();

		if (const USMGraphNode_StateMachineStateNode* StateMachineNode = Cast<USMGraphNode_StateMachineStateNode>(GraphNode))
		{
			if (const USMBlueprint* ReferenceBlueprint = StateMachineNode->GetStateMachineReference())
			{
				OwningReferenceClass = ReferenceBlueprint->GeneratedClass;
			}
		}
	}

	if (OptionalOutput)
	{
		if (OwningClass)
		{
			// Add the owning node class here since the USMGraphNode that really owns it doesn't have a HasExternalDependencies method.
			OptionalOutput->AddUnique(OwningClass);
		}
		if (OwningReferenceClass)
		{
			// Same as the owning node class, references need to be listed as dependencies.
			OptionalOutput->AddUnique(OwningReferenceClass);
		}
	}
	
	const bool bSuperResult = Super::HasExternalDependencies(OptionalOutput);
	return bSuperResult || OwningClass != nullptr;
}

UScriptStruct* USMGraphK2Node_RuntimeNodeContainer::GetRunTimeNodeType() const
{
	UScriptStruct* BaseClass = FSMNode_Base::StaticStruct();

	for (TFieldIterator<FProperty> PropIt(GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		if (FStructProperty* StructProp = CastField<FStructProperty>(*PropIt))
		{
			if (StructProp->Struct->IsChildOf(BaseClass))
			{
				return StructProp->Struct;
			}
		}
	}

	return nullptr;
}

FStructProperty* USMGraphK2Node_RuntimeNodeContainer::GetRuntimeNodeProperty() const
{
	UScriptStruct* BaseFStruct = FSMNode_Base::StaticStruct();

	for (TFieldIterator<FProperty> PropIt(GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		if (FStructProperty* StructProp = CastField<FStructProperty>(*PropIt))
		{
			if (StructProp->Struct->IsChildOf(BaseFStruct))
			{
				return StructProp;
			}
		}
	}

	return nullptr;
}

void USMGraphK2Node_RuntimeNodeContainer::ForceGenerateNodeGuid()
{
	// This has already been called for this copy.
	if (bHasNodeGuidGeneratedForCopy)
	{
		return;
	}
	GetRunTimeNodeChecked()->GenerateNewNodeGuid();
	bHasNodeGuidGeneratedForCopy = true;
}


USMGraphK2Node_RuntimeNodeReference::USMGraphK2Node_RuntimeNodeReference(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_RuntimeNodeReference::PostPasteNode()
{
	Super::PostPasteNode();

	if (bIsBeingDestroyed)
	{
		return;
	}

	USMGraphK2Node_RuntimeNodeContainer* ContainerNode = GetRuntimeContainer();

	// Check that the paste operation has completed for this node.
	if (ContainerNode && !ContainerNode->HasNewNodeGuidGenerated())
	{
		ContainerNode->ForceGenerateNodeGuid();
	}

	SyncWithContainer();
}

void USMGraphK2Node_RuntimeNodeReference::PreConsolidatedEventGraphValidate(FCompilerResultsLog& MessageLog)
{
	Super::PreConsolidatedEventGraphValidate(MessageLog);
	
	USMGraphK2Node_RuntimeNodeContainer* Container = GetRuntimeContainer();
	if (Container && Container->GetRunTimeNodeChecked()->GetNodeGuid() != RuntimeNodeGuid)
	{
		MessageLog.Error(TEXT("Runtime node mismatch on reference node @@ with container node @@"), this, Container);
	}
}

void USMGraphK2Node_RuntimeNodeReference::SyncWithContainer()

{	// Restore reference to container.
	if (USMGraphK2Node_RuntimeNodeContainer* Container = GetRuntimeContainer())
	{
		RuntimeNodeGuid = Container->GetRunTimeNodeChecked()->GetNodeGuid();
	}
}

USMGraphK2Node_RuntimeNodeContainer* USMGraphK2Node_RuntimeNodeReference::GetRuntimeContainer() const
{
	return FSMBlueprintEditorUtils::GetRuntimeContainerFromGraph(GetGraph());
}

USMGraphK2Node_RuntimeNodeContainer* USMGraphK2Node_RuntimeNodeReference::GetRuntimeContainerChecked() const
{
	USMGraphK2Node_RuntimeNodeContainer* Container = GetRuntimeContainer();
	check(Container);
	return Container;
}

UK2Node_CallFunction* USMGraphK2Node_RuntimeNodeReference::CreateFunctionCallWithGuidInput(UFunction* Function,
                                                                                           FSMKismetCompilerContext& CompilerContext, const USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer,
                                                                                           const FProperty* NodeProperty, FName PinName)
{
	UK2Node_CallFunction* GetReferenceFunctionNode = FSMBlueprintEditorUtils::CreateFunctionCall(CompilerContext.ConsolidatedEventGraph, Function);

	UK2Node_StructMemberGet* GuidGetNode = CompilerContext.SpawnIntermediateNode<UK2Node_StructMemberGet>(this, CompilerContext.ConsolidatedEventGraph);
	GuidGetNode->VariableReference.SetSelfMember(NodeProperty->GetFName());
	GuidGetNode->StructType = RuntimeNodeContainer->GetRunTimeNodeType();
	GuidGetNode->AllocateDefaultPins();

	UEdGraphPin* GetReferenceInputPin = GetReferenceFunctionNode->FindPinChecked(PinName, EGPD_Input);
	// Find the property on FSMNode. Can't use member name since it's protected.
	UEdGraphPin* ValuePin = GuidGetNode->FindPinChecked(FName("PathGuid"));

	CompilerContext.ConsolidatedEventGraph->GetSchema()->TryCreateConnection(ValuePin, GetReferenceInputPin);

	return GetReferenceFunctionNode;
}

void USMGraphK2Node_RuntimeNodeReference::GetMenuActions_Internal(
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

#undef LOCTEXT_NAMESPACE
