// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMStateInstance.h"
#include "SMTransitionInstance.h"

class USMGraphNode_Base;
class USMGraphNode_StateNodeBase;
class USMGraphNode_StateNode;
class USMGraphNode_TransitionEdge;
class USMGraph;
class USMBlueprint;
class UEdGraphPin;

class ISMGraphGeneration
{
public:
	virtual ~ISMGraphGeneration() {}

	/**
	 * Arguments for creating a new base state graph node.
	 */
	struct FCreateStateNodeArgs
	{
		/** [Optional] The node instance class to use. */
		TSubclassOf<USMStateInstance_Base> StateInstanceClass = USMStateInstance::StaticClass();

		/** [Optional] The graph node class to use. Leave null to determine the class from the instance. */
		TSubclassOf<USMGraphNode_StateNodeBase> GraphNodeClass;

		/** [Optional] The graph the node should be placed in. When null the blueprint root graph is used. */
		USMGraph* GraphOwner = nullptr;

		/** [Optional] The name to apply to the state. */
		FString StateName;

		/** [Optional] The graph pin leading to this state. */
		UEdGraphPin* FromPin = nullptr;

		/** [Optional] The position of the node in the graph. */
		FVector2D NodePosition = FVector2D(128.f, 0.f);

		/**
		 * [Optional] The node guid to assign. Generally best to leave invalid so it can be auto assigned.
		 * This also can serve as the UEdGraphNode NodeGuid if this is for a state that doesn't exist at runtime.
		 */
		FGuid NodeGuid;

		/** [Optional] If this node should be wired to the graph entry. Ignored if FromPin set. */
		bool bIsEntryState = false;
	};

	/**
	 * Arguments for creating a transition between two states.
	 */
	struct FCreateTransitionEdgeArgs
	{
		/** [Optional] The node instance class to use. */
		TSubclassOf<USMTransitionInstance> TransitionInstanceClass = USMTransitionInstance::StaticClass();

		/** [Required] The from state for this transition. */
		USMGraphNode_StateNodeBase* FromStateNode = nullptr;

		/** [Required] The destination state for this transition. */
		USMGraphNode_StateNodeBase* ToStateNode = nullptr;

		/** [Optional] The node guid to assign. Generally best to leave invalid so it can be auto assigned. */
		FGuid NodeGuid;

		/** [Optional] Default the transition to true. Only works if no node class is assigned. */
		bool bDefaultToTrue = false;
	};

	/**
	 * Arguments for creating a state stack state.
	 */
	struct FCreateStateStackArgs
	{
		/** [Required] The state instance class to use. */
		TSubclassOf<USMStateInstance> StateStackInstanceClass;

		/** [Optional] The index the instance should be inserted at. Leave INDEX_NONE to add to the end. */
		int32 StateStackIndex = INDEX_NONE;
	};

	/**
	 * Handle array processing.
	 */
	enum EArrayChangeType
	{
		/** Set the value of the provided index, resizing the array to match if necessary. */
		SetElement,

		/** Remove the element from the array at the provided index. */
		RemoveElement,

		/** Clear all elements from the array. */
		Clear
	};

	/**
	 * Arguments for setting a node property.
	 */
	struct FSetNodePropertyArgs
	{
		/** [Required] The name of the property. */
		FName PropertyName;

		/** [Optional] The default value to assign the property. */
		FString PropertyDefaultValue;

		/** [Optional] Index when setting an array element. */
		int32 PropertyIndex = 0;

		/** [Optional] How to handle modifying an array. */
		EArrayChangeType ArrayChangeType = SetElement;

		/**
		 * [Optional] The node instance which contains the property, such as a node stack instance.
		 * When null the default node template instance is used.
		 */
		USMNodeInstance* NodeInstance = nullptr;
	};

	/**
	 * Create a new state graph node in a blueprint.
	 *
	 * @param InBlueprint The blueprint to place the node.
	 * @param InStateArgs Arguments to use when creating the state.
	 *
	 * @return The newly created state node if successful.
	 */
	virtual USMGraphNode_StateNodeBase* CreateStateNode(USMBlueprint* InBlueprint, const FCreateStateNodeArgs& InStateArgs) = 0;
	template<typename T>
	T* CreateStateNode(USMBlueprint* InBlueprint, const FCreateStateNodeArgs& InStateArgs)
	{
		return Cast<T>(CreateStateNode(InBlueprint, InStateArgs));
	}

	/**
	 * Create a new transition between two states.
	 *
	 * @param InBlueprint The blueprint to place the edge.
	 * @param InTransitionArgs Arguments to use to create the transition.
	 *
	 * @return The newly created transition edge if successful.
	 */
	virtual USMGraphNode_TransitionEdge* CreateTransitionEdge(USMBlueprint* InBlueprint, const FCreateTransitionEdgeArgs& InTransitionArgs) = 0;
	template<typename T>
	T* CreateTransitionEdge(USMBlueprint* InBlueprint, const FCreateTransitionEdgeArgs& InTransitionArgs)
	{
		return Cast<T>(CreateTransitionEdge(InBlueprint, InTransitionArgs));
	}

	/**
	 * Create and add a state instance to a state stack.
	 *
	 * @param InStateNode The state node which should contain the state stack.
	 * @param InStateStackArgs The arguments to use when adding the state instance to the stack.
	 *
	 * @return The created state stack instance.
	 */
	virtual USMStateInstance* CreateStateStackInstance(USMGraphNode_StateNode* InStateNode, const FCreateStateStackArgs& InStateStackArgs) = 0;

	/**
	 * Set the value of a property on a graph node. This can be a state or transition.
	 * The property can be public, non-public, or a custom graph property (Such as a TextGraph).
	 *
	 * @param InGraphNode The graph node containing the property to modify.
	 * @param InPropertyArgs The property arguments.
	 *
	 * @return True if the value was set successfully.
	 */
	virtual bool SetNodePropertyValue(USMGraphNode_Base* InGraphNode, const FSetNodePropertyArgs& InPropertyArgs) = 0;
};
