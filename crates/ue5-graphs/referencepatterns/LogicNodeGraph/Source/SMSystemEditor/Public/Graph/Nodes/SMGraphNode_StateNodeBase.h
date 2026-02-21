// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/SMGraphNode_Base.h"

#include "ISMEditorGraphNode_StateBaseInterface.h"

#include "GameplayTagContainer.h"

#include "SMGraphNode_StateNodeBase.generated.h"

class USMGraphNode_TransitionEdge;
class USMGraphNode_LinkStateNode;
struct FSMState_Base;

/** Base class required as states and conduits branch separately from a common source. */
UCLASS()
class SMSYSTEMEDITOR_API USMGraphNode_StateNodeBase : public USMGraphNode_Base, public ISMEditorGraphNode_StateBaseInterface
{
	GENERATED_UCLASS_BODY()

	/**
	 * Add tags to this state that Any State nodes will recognize.
	 * This can allow control over which specific Any State nodes should impact this state.
	 *
	 * On the Any State you can define an AnyStateTagQuery to control which tags an Any State should recognize.
	 * Only valid in the editor.
	 */
	UPROPERTY(EditAnywhere, Category = "Any State")
	FGameplayTagContainer AnyStateTags;
	
	/**
	 * @deprecated Set on the node template instead.
	 */
	UPROPERTY()
	uint8 bAlwaysUpdate_DEPRECATED: 1;

	/**
	 * @deprecated Set on the node template instead.
	 */
	UPROPERTY()
	uint8 bDisableTickTransitionEvaluation_DEPRECATED: 1;

	/**
	 * @deprecated Set on the node template instead.
	 */
	UPROPERTY()
	uint8 bEvalTransitionsOnStart_DEPRECATED: 1;

	/**
	 * @deprecated Set on the node template instead.
	 */
	UPROPERTY()
	uint8 bExcludeFromAnyState_DEPRECATED: 1;
	
	/**
	 * Set by the editor and read by the schema to allow self transitions.
	 * We don't want to drag / drop self transitions because a single pin click will
	 * trigger them. There doesn't seem to be an ideal way for the schema to detect
	 * mouse movement to prevent single clicks when in CanCreateConnection,
	 * so we're relying on a context menu.
	 */
	uint8 bCanTransitionToSelf: 1;

public:
	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool GetCanRenameNode() const override;
	virtual void OnRenameNode(const FString& NewName) override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual void DestroyNode() override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	// ~UEdGraphNode

	// USMGraphNode_Base
	virtual void ImportDeprecatedProperties() override;
	virtual void OnNodeMoved(const FVector2D& NewPosition) override;
	virtual void OnBoundGraphRenamed(UObject* OldOuter, const FName OldName) override;
	virtual void PreCompile(FSMKismetCompilerContext& CompilerContext) override;
	// ~USMGraphNode_Base

	// ISMEditorGraphNodeInterface
	virtual void ResetNodeName() override;
	// ~ISMEditorGraphNodeInterface
	
	// ISMEditorGraphNode_StateBaseInterface
	virtual FGameplayTagContainer& GetAnyStateTags() override { return AnyStateTags; }
	virtual void SetAnyStateTags(const FGameplayTagContainer& InAnyStateTags) override;
	// ~ISMEditorGraphNode_StateBaseInterface
	
	/** Copy configuration settings to the runtime node. */
	virtual void SetRuntimeDefaults(FSMState_Base& State) const;
	virtual FString GetStateName() const { return GetNodeName(); }

	/**
	 * Checks if there are no outbound transitions.
	 * @param bCheckAnyState Checks if an Any State will prevent this from being an end state.
	 */
	virtual bool IsEndState(bool bCheckAnyState = true) const;

	/** Checks if there are any connections to this node. Does not count self. */
	virtual bool HasInputConnections() const;

	/** Checks if there are any connections from this node. */
	virtual bool HasOutputConnections() const;

	/** If transitions are supposed to run in parallel. */
	bool ShouldDefaultTransitionsToParallel() const;

	/** If this node shouldn't receive transitions from an Any State. */
	bool ShouldExcludeFromAnyState() const;
	
	/** Checks if there is a node connected via outgoing transition. */
	bool HasTransitionToNode(const UEdGraphNode* Node) const;

	/** Checks if there is a node connected via incoming transition. */
	bool HasTransitionFromNode(const UEdGraphNode* Node) const;

	/** Returns the previous node, regardless of if it is a state or transition. */
	USMGraphNode_Base* GetPreviousNode(int32 Index = 0) const;

	/** Returns the next node, regardless of if it is a state or transition. */
	USMGraphNode_Base* GetNextNode(int32 Index = 0) const;
	
	/** Returns the previous node at the given input linked to index. */
	USMGraphNode_StateNodeBase* GetPreviousState(int32 Index = 0, bool bIncludeReroute = false, bool bIncludeEntryState = false) const;

	/** Returns the next node at the given output linked to index. */
	USMGraphNode_StateNodeBase* GetNextState(int32 Index = 0, bool bIncludeReroute = false) const;

	/** Returns a transition going to the input pin. */
	USMGraphNode_TransitionEdge* GetPreviousTransition(int32 Index = 0) const;

	/** Returns a transitions from the output pin. */
	USMGraphNode_TransitionEdge* GetNextTransition(int32 Index = 0) const;

	/** Return all transitions to the input pin. */
	void GetInputTransitions(TArray<USMGraphNode_TransitionEdge*>& OutTransitions) const;

	/** Return all transitions from the output pin. */
	void GetOutputTransitions(TArray<USMGraphNode_TransitionEdge*>& OutTransitions) const;

	/** Return the total number of input connections. Not necessarily transitions such as when coming from an entry state. */
	int32 GetNumInputConnections() const;

	/** Return the total number of output connections. */
	int32 GetNumOutputConnections() const;

	/** Return the entry pin if this states is connected to an entry node, nullptr otherwise. */
	UEdGraphPin* GetConnectedEntryPin() const;
	
	FLinearColor GetBackgroundColorForNodeInstance(const USMNodeInstance* NodeInstance) const;

	/** Return any Link States pointing to this node. */
	const TSet<TObjectPtr<USMGraphNode_LinkStateNode>>& GetLinkStates() const { return LinkStates; }

	/** Checks for any Link States that don't exist and remove them. */
	void RemoveInvalidLinkStates();
	
	/** Notify any transitions they should refresh. */
	void RefreshConnectedTransitions();

protected:
	virtual FLinearColor Internal_GetBackgroundColor() const override;
	
private:
	friend class SGraphNode_StateNode;
	bool bRequestInitialAnimation;

	friend class USMGraphNode_LinkStateNode;

	/** Link States linking to this state. */
	UPROPERTY(DuplicateTransient)
	TSet<TObjectPtr<USMGraphNode_LinkStateNode>> LinkStates;
};
