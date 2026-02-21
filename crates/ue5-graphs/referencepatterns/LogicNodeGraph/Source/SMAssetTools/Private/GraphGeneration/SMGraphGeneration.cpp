// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphGeneration.h"

#include "SMAssetToolsLog.h"

#include "Blueprints/SMBlueprint.h"
#include "SMConduitInstance.h"
#include "SMStateMachineInstance.h"
#include "SMUtils.h"

#include "Configuration/SMProjectEditorSettings.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionResultNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Schema/SMGraphSchema.h"
#include "Graph/SMGraph.h"
#include "Graph/SMTransitionGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMNodeInstanceUtils.h"
#include "Utilities/SMPropertyUtils.h"

USMGraphNode_StateNodeBase* FSMGraphGeneration::CreateStateNode(USMBlueprint* InBlueprint,
                                                                const FCreateStateNodeArgs& InStateArgs)
{
	check(InBlueprint);

	USMGraph* StateMachineGraph = InStateArgs.GraphOwner
		                              ? InStateArgs.GraphOwner
		                              : FSMBlueprintEditorUtils::GetRootStateMachineGraph(InBlueprint);
	check(StateMachineGraph);

	// Determine the type of graph node to place.
	const UClass* GraphNodeClass = InStateArgs.GraphNodeClass.Get()
		                               ? InStateArgs.GraphNodeClass.Get()
		                               : GetGraphNodeClassFromInstanceType(InStateArgs.StateInstanceClass);

	if (!GraphNodeClass)
	{
		LDASSETTOOLS_LOG_ERROR(TEXT("Could not determine graph node class to use in blueprint %s"),
		                       *InBlueprint->GetName());
		return nullptr;
	}

	FSMGraphSchemaAction_NewNode AddNodeAction;
	AddNodeAction.GraphNodeTemplate = NewObject<USMGraphNode_StateNodeBase>(GetTransientPackage(), GraphNodeClass);
	AddNodeAction.NodeSoftClassPath = InStateArgs.StateInstanceClass;

	USMGraphNode_StateNodeBase* CreatedGraphNode = Cast<USMGraphNode_StateNodeBase>(AddNodeAction.PerformAction(
		StateMachineGraph,
		InStateArgs.FromPin, InStateArgs.NodePosition, false));

	if (!CreatedGraphNode)
	{
		LDASSETTOOLS_LOG_ERROR(TEXT("Could not create state node in blueprint %s"), *InBlueprint->GetName());
		return nullptr;
	}

	// Set a custom node guid.
	if (InStateArgs.NodeGuid.IsValid())
	{
		if (CreatedGraphNode->CanExistAtRuntime())
		{
			FSMNode_Base* RuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(CreatedGraphNode->GetBoundGraph());
			check(RuntimeNode);
			RuntimeNode->SetNodeGuid(InStateArgs.NodeGuid);
			// Update reference nodes with the new guid.
			FSMBlueprintEditorUtils::UpdateRuntimeNodeForNestedGraphs(RuntimeNode->GetNodeGuid(), RuntimeNode, CreatedGraphNode->GetBoundGraph());
		}
		else
		{
			CreatedGraphNode->NodeGuid = InStateArgs.NodeGuid;
		}
	}

	// Set a custom name.
	if (!InStateArgs.StateName.IsEmpty())
	{
		const FString DefaultNodeName = FSMBlueprintEditorUtils::GetProjectEditorSettings()->bRestrictInvalidCharacters
			                                ? FSMBlueprintEditorUtils::GetSafeStateName(InStateArgs.StateName)
			                                : InStateArgs.StateName;
		if (!DefaultNodeName.IsEmpty())
		{
			CreatedGraphNode->SetNodeName_Direct(DefaultNodeName);
		}
	}

	// Wire to entry.
	if (!InStateArgs.FromPin && InStateArgs.bIsEntryState)
	{
		const USMGraphNode_StateMachineEntryNode* EntryNode = StateMachineGraph->GetEntryNode();
		check(EntryNode);
		CreatedGraphNode->GetSchema()->TryCreateConnection(EntryNode->GetOutputPin(), CreatedGraphNode->GetInputPin());
	}

	return CreatedGraphNode;
}

USMGraphNode_TransitionEdge* FSMGraphGeneration::CreateTransitionEdge(USMBlueprint* InBlueprint,
                                                                      const FCreateTransitionEdgeArgs& InTransitionArgs)
{
	check(InBlueprint);
	check(InTransitionArgs.FromStateNode);
	check(InTransitionArgs.ToStateNode);
	check(InTransitionArgs.FromStateNode->GetGraph() == InTransitionArgs.ToStateNode->GetGraph());

	const TSet<UEdGraphPin*> FromPins(InTransitionArgs.FromStateNode->GetOutputPin()->LinkedTo);

	// Allow self transitions.
	if (InTransitionArgs.FromStateNode == InTransitionArgs.ToStateNode)
	{
		InTransitionArgs.FromStateNode->bCanTransitionToSelf = true;
	}
	
	const bool bSuccess = InTransitionArgs.FromStateNode->GetSchema()->TryCreateConnection(
		InTransitionArgs.FromStateNode->GetOutputPin(),
		InTransitionArgs.ToStateNode->GetInputPin());

	// Turn off self transitions.
	if (InTransitionArgs.FromStateNode == InTransitionArgs.ToStateNode)
	{
		InTransitionArgs.FromStateNode->bCanTransitionToSelf = false;
	}
	
	if (!ensureMsgf(bSuccess, TEXT("Could not create a connection between %s and %s."),
	                *InTransitionArgs.FromStateNode->GetStateName(), *InTransitionArgs.ToStateNode->GetStateName()))
	{
		return nullptr;
	}

	USMGraphNode_TransitionEdge* CreatedTransitionEdge = nullptr;

	for (const UEdGraphPin* Pin : InTransitionArgs.FromStateNode->GetOutputPin()->LinkedTo)
	{
		if (!FromPins.Contains(Pin))
		{
			CreatedTransitionEdge = Cast<USMGraphNode_TransitionEdge>(Pin->GetOwningNode());
		}
	}

	if (!ensureMsgf(CreatedTransitionEdge, TEXT("Could not locate created transition edge.")))
	{
		return nullptr;
	}

	if (!FSMNodeClassRule::IsBaseClass(InTransitionArgs.TransitionInstanceClass))
	{
		CreatedTransitionEdge->SetNodeClass(InTransitionArgs.TransitionInstanceClass);
	}
	else if (InTransitionArgs.bDefaultToTrue)
	{
		// Set default transition value to true if applicable.
		const USMTransitionGraph* TransitionGraph = CreatedTransitionEdge->GetTransitionGraph();
		if (TransitionGraph->ResultNode)
		{
			const UEdGraphSchema* Schema = TransitionGraph->GetSchema();
			check(Schema);
			Schema->TrySetDefaultValue(*TransitionGraph->ResultNode->GetTransitionEvaluationPin(), TEXT("True"));
		}
	}

	// Set a custom node guid.
	if (InTransitionArgs.NodeGuid.IsValid())
	{
		FSMNode_Base* RuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(CreatedTransitionEdge->GetBoundGraph());
		check(RuntimeNode);
		RuntimeNode->SetNodeGuid(InTransitionArgs.NodeGuid);
		// Update reference nodes with the new guid.
		FSMBlueprintEditorUtils::UpdateRuntimeNodeForNestedGraphs(RuntimeNode->GetNodeGuid(), RuntimeNode, CreatedTransitionEdge->GetBoundGraph());
	}

	return CreatedTransitionEdge;
}

USMStateInstance* FSMGraphGeneration::CreateStateStackInstance(USMGraphNode_StateNode* InStateNode,
                                                               const FCreateStateStackArgs& InStateStackArgs)
{
	check(InStateNode);
	check(InStateStackArgs.StateStackInstanceClass.Get());

	return Cast<USMStateInstance>(InStateNode->AddStackNode(InStateStackArgs.StateStackInstanceClass, InStateStackArgs.StateStackIndex));
}

bool FSMGraphGeneration::SetNodePropertyValue(USMGraphNode_Base* InGraphNode,
                                              const FSetNodePropertyArgs& InPropertyArgs)
{
	check(InGraphNode);
	if (InPropertyArgs.PropertyName.IsNone())
	{
		LDASSETTOOLS_LOG_ERROR(TEXT("No property name provided for node %s."), *InGraphNode->GetName());
		return false;
	}

	if (USMNodeInstance* NodeInstance = InPropertyArgs.NodeInstance ? InPropertyArgs.NodeInstance : InGraphNode->GetNodeTemplate())
	{
		FProperty* Property = NodeInstance->GetClass()->FindPropertyByName(InPropertyArgs.PropertyName);
		if (!Property)
		{
			LDASSETTOOLS_LOG_ERROR(TEXT("Could not locate property %s in node %s."),
			                       *InPropertyArgs.PropertyName.ToString(), *InGraphNode->GetName());
			return false;
		}

		// Handle array properties and add elements as needed.
		const int32 ArrayIndex = InPropertyArgs.PropertyIndex;
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<uint8>(NodeInstance));
			if (InPropertyArgs.ArrayChangeType == SetElement)
			{
				if (!Helper.IsValidIndex(ArrayIndex))
				{
					LDASSETTOOLS_LOG_INFO(TEXT("Resizing array property %s to support array index %s."),
										  *InPropertyArgs.PropertyName.ToString(), *FString::FromInt(InPropertyArgs.PropertyIndex));
					Helper.Resize(ArrayIndex + 1);

					// Create property graphs and assign guids.
					InGraphNode->ForceRecreateProperties();
				}
			}
			else if (InPropertyArgs.ArrayChangeType == RemoveElement)
			{
				if (!Helper.IsValidIndex(ArrayIndex))
				{
					LDASSETTOOLS_LOG_WARNING(TEXT("Could not remove index %s from array of property %s, index is invalid."),
										  *InPropertyArgs.PropertyName.ToString(), *FString::FromInt(InPropertyArgs.PropertyIndex));
					return false;
				}

				Helper.RemoveValues(ArrayIndex);
				InGraphNode->ForceRecreateProperties();
				return true;
			}
			else if (InPropertyArgs.ArrayChangeType == Clear)
			{
				Helper.EmptyValues();
				InGraphNode->ForceRecreateProperties();
				return true;
			}
		}

		// Discover if this is a public property with a graph on the node.
		USMGraphK2Node_PropertyNode_Base* ExposedPropertyNode = nullptr;
		FStructProperty* StructProperty = LD::PropertyUtils::GetGraphPropertyFromProperty(Property);
		if (StructProperty || LD::PropertyUtils::IsPropertyExposedToGraphNode(Property))
		{
			FGuid PropertyGuid;
			if (StructProperty)
			{
				// Custom graph property.
				TArray<FSMGraphProperty_Base*> GraphProperties;
				USMUtils::BlueprintPropertyToNativeProperty(StructProperty, NodeInstance, GraphProperties);

				if (ensure(GraphProperties.Num() >= ArrayIndex))
				{
					PropertyGuid = LD::PropertyUtils::SetGraphPropertyFromProperty(
						*GraphProperties[ArrayIndex], StructProperty, NodeInstance, ArrayIndex, false);
				}
			}
			else
			{
				// Variable property.
				FSMGraphProperty_Base PropertyLookup;
				PropertyGuid = LD::PropertyUtils::SetGraphPropertyFromProperty(
					PropertyLookup, Property, NodeInstance, ArrayIndex);
			}

			if (!ensureMsgf(PropertyGuid.IsValid(), TEXT("Could not locate GUID for property %s in node %s."),
			                *InPropertyArgs.PropertyName.ToString(), *InGraphNode->GetName()))
			{
				return false;
			}

			ExposedPropertyNode = InGraphNode->GetGraphPropertyNode(PropertyGuid);
		}

		// Standard property, could be exposed on the node or not.
		LD::PropertyUtils::SetPropertyValue(Property, InPropertyArgs.PropertyDefaultValue, NodeInstance, ArrayIndex);

		if (ExposedPropertyNode)
		{
			// Update graph pins with the new instance defaults.
			const bool bUseArchetype = false; // We've set the instance value.
			const bool bForce = true;
			ExposedPropertyNode->SetPinValueFromPropertyDefaults(false, bUseArchetype, bForce);
		}

		return true;
	}

	return false;
}

UClass* FSMGraphGeneration::GetGraphNodeClassFromInstanceType(TSubclassOf<USMNodeInstance> InNodeClass)
{
	if (InNodeClass)
	{
		if (InNodeClass->IsChildOf(USMStateInstance::StaticClass()))
		{
			return USMGraphNode_StateNode::StaticClass();
		}
		if (InNodeClass->IsChildOf(USMStateMachineInstance::StaticClass()))
		{
			return USMGraphNode_StateMachineStateNode::StaticClass();
		}
		if (InNodeClass->IsChildOf(USMConduitInstance::StaticClass()))
		{
			return USMGraphNode_ConduitNode::StaticClass();
		}
		if (InNodeClass->IsChildOf(USMTransitionInstance::StaticClass()))
		{
			return USMGraphNode_TransitionEdge::StaticClass();
		}
	}

	return nullptr;
}
