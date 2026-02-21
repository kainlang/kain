// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMGraphNode_StateNodeBase.h"
#include "NodeStack/NodeStackContainer.h"

#include "SMStateInstance.h"

#include "SMGraphNode_StateNode.generated.h"

class USMGraphNode_TransitionEdge;
class USMGraphNode_LinkStateNode;
struct FSMState_Base;

/**
 * Regular state nodes which have K2 graphs.
 */
UCLASS()
class SMSYSTEMEDITOR_API USMGraphNode_StateNode : public USMGraphNode_StateNodeBase
{
public:
	GENERATED_UCLASS_BODY()

	/** Select a custom node class to use for this node. This can be a blueprint or C++ class. */
	UPROPERTY(EditAnywhere, NoClear, Category = "State", meta = (BlueprintBaseOnly))
	TSubclassOf<USMStateInstance> StateClass;

	/** Augment the state by adding additional state classes to perform logic processing. */
	UPROPERTY(EditAnywhere, Category = "State")
	TArray<FStateStackContainer> StateStack;

	// UEdGraphNode
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual void DestroyNode() override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	//~ UEdGraphNode
	
	// USMGraphNode_Base
	virtual void PlaceDefaultInstanceNodes() override;
	virtual FName GetNodeClassPropertyName() const override { return GET_MEMBER_NAME_CHECKED(USMGraphNode_StateNode, StateClass); }
	virtual FName GetNodeStackPropertyName() override { return GET_MEMBER_NAME_CHECKED(USMGraphNode_StateNode, StateStack); }
	virtual FName GetNodeStackElementClassPropertyName() const override { return GET_MEMBER_NAME_CHECKED(FStateStackContainer, StateStackClass); }
	virtual UClass* GetNodeClass() const override { return StateClass; }
	virtual void SetNodeClass(UClass* Class) override;
	virtual bool SupportsPropertyGraphs() const override { return true; }
	virtual FName GetFriendlyNodeName() const override { return "State"; }
	virtual const FSlateBrush* GetNodeIcon() const override;
	virtual void InitTemplate() override;
	virtual bool AreTemplatesFullyLoaded() const override;
	virtual bool DoesNodePossiblyHaveConstructionScripts() const override;
	virtual void RunAllConstructionScripts_Internal() override;
	virtual void RestoreArchetypeValuesPriorToConstruction() override;
	virtual void PreCompileNodeInstanceValidation(FCompilerResultsLog& CompilerContext, USMCompilerLog* CompilerLog, USMGraphNode_Base* OwningNode) override;
	virtual void OnCompile(FSMKismetCompilerContext& CompilerContext) override;
	/**
	 * Retrieve the array index from the template guid.
	 *
	 * @return the array index or INDEX_NONE if not found.
	 */
	virtual int32 GetIndexOfTemplate(const FGuid& TemplateGuid) const override;

	virtual void GetAllNodeTemplates(TArray<USMNodeInstance*>& OutNodeInstances) const override;
	// ~USMGraphNode_Base

	// ISMEditorGraphNodeInterface
	virtual USMNodeInstance* AddStackNode(TSubclassOf<USMNodeInstance> NodeClass, int32 StackIndex) override;
	virtual void RemoveStackNode(int32 StackIndex) override;
	virtual void ClearStackNodes() override;
	// ~ISMEditorGraphNodeInterface

	/** Return all state stack templates. */
	const TArray<FStateStackContainer>& GetAllNodeStackTemplates() const;

	/**
	 * Retrieve the template instance from an index.
	 *
	 * @return the NodeInstance template.
	 */
	virtual USMNodeInstance* GetTemplateFromIndex(int32 Index) const override;
	
	void InitStateStack();
	void DestroyStateStack();
};
