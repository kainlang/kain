// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"

#include "SMStateMachineInstance.h"

#include "SMGraphNode_StateMachineStateNode.generated.h"

class USMBlueprint;
class USMInstance;
class USMGraph;

UCLASS()
class SMSYSTEMEDITOR_API USMGraphNode_StateMachineStateNode : public USMGraphNode_StateNodeBase
{
	GENERATED_UCLASS_BODY()

	/**
	 * Dynamically choose the state machine class for the reference at run-time.
	 * 
	 * Select a variable from this state machine of type TSubclassOf<USMInstance>
	 * (State Machine Instance -> Class Reference)
	 * 
	 * This variable will be checked during initialization time and the reference will be created
	 * based on the class the variable is set to.
	 *
	 * The class should be a subclass of the default reference provided.
	 */
	UPROPERTY(EditAnywhere, Category = "State Machine Reference")
	FName DynamicClassVariable;
	
	/**
	 * @deprecated Set on the node template instead.
	 */
	UPROPERTY()
	uint8 bReuseCurrentState_DEPRECATED: 1;

	/**
	 * @deprecated Set on the node template instead.
	 */
	UPROPERTY()
	uint8 bReuseIfNotEndState_DEPRECATED: 1;

	/**
	 * Allows the state machine reference to tick on its own.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "State Machine Reference")
	uint8 bAllowIndependentTick: 1;

	/**
	 * The Update method will call Tick only if Update was not called by native Tick.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "State Machine Reference")
	uint8 bCallTickOnManualUpdate: 1;

	/**
	 * Reuse one instance of this class multiple times in the same blueprint.
	 * Only works with other references of the exact same class that have this flag checked.
	 * Will not work with templating.
	 * Will not work properly with looking up nodes by Guid or for serializing state information.
	 * Do NOT use if the state machine needs to be saved to disk and reloaded during run-time.

	 * This is to maintain legacy behavior and not encouraged for use.
	 *
	 * @deprecated Reusing references is no longer supported.
	 */
	UPROPERTY()
	uint8 bReuseReference_DEPRECATED: 1;
	
	/**
	 * Enable the use of an archetype to allow default values to be set.
	 */
	UPROPERTY(EditAnywhere, Category = "State Machine Reference")
	uint8 bUseTemplate: 1;

	UPROPERTY(VisibleDefaultsOnly, Export, Category = "State Machine Reference", meta = (DisplayName=Template, DisplayThumbnail=false, ShowInnerProperties, LogicDriverExport))
	TObjectPtr<USMInstance> ReferencedInstanceTemplate;

	/** Select a custom node class to use for this node. This can be a blueprint or C++ class. */
	UPROPERTY(EditAnywhere, NoClear, Category = "State Machine", meta = (BlueprintBaseOnly))
	TSubclassOf<USMStateMachineInstance> StateMachineClass;

public:
	// UObject
	virtual void PostLoad() override;
	// ~UObject

	// UEdGraphNode
	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void DestroyNode() override;
	virtual void ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual void JumpToDefinition() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	// ~UEdGraphNode

	// USMGraphNode_Base
	virtual void PreCompile(FSMKismetCompilerContext& CompilerContext) override;
protected:
	virtual void OnConvertToCurrentVersion(bool bOnlyOnLoad) override;
public:
	virtual void ImportDeprecatedProperties() override;
	virtual void CheckSetErrorMessages() override;
	virtual FName GetNodeClassPropertyName() const override { return GET_MEMBER_NAME_CHECKED(USMGraphNode_StateMachineStateNode, StateMachineClass); }
	virtual UClass* GetNodeClass() const override { return StateMachineClass; }
	virtual void SetNodeClass(UClass* Class) override;
	virtual bool SupportsPropertyGraphs() const override { return true; }
	virtual FName GetFriendlyNodeName() const override { return "State Machine"; }
	virtual const FSlateBrush* GetNodeIcon() const override;
	virtual void GoToLocalGraph() const override;
	virtual bool CanGoToLocalGraph() const override;
	virtual bool IsNodeFastPathEnabled() const override;
	// ~USMGraphNode_Base

	// USMGraphNode_StateNodeBase
	virtual void SetRuntimeDefaults(FSMState_Base& State) const override;
	// ~USMGraphNode_StateNodeBase

	/** Return the contained state machine graph. */
	USMGraph* GetBoundStateMachineGraph() const;

	/** Returns the best graph of the reference to jump to. */
	UObject* GetReferenceToJumpTo() const;

	/** Jumps to the reference regardless of intermediate graph. */
	void JumpToReference() const;

	/** Tells the hyperlink target to use the current debug object. */
	void SetDebugObjectForReference() const;
	
	/** Signals that this state machine is actually a reference to another blueprint. */
	virtual bool ReferenceStateMachine(USMBlueprint* OtherStateMachine);

	/** Instantiate a template for use as an archetype. */
	virtual void InitStateMachineReferenceTemplate(bool bInitialLoad = false);

	/** Transfer the template to the transient package. */
	void DestroyReferenceTemplate();

	/** Return the protected ReferencedStateMachine property name. */
	static FName GetStateMachineReferencePropertyName() { return GET_MEMBER_NAME_CHECKED(USMGraphNode_StateMachineStateNode, ReferencedStateMachine); }
	
	/** The blueprint state machine this node references. */
	USMBlueprint* GetStateMachineReference() const { return ReferencedStateMachine; }

	/** Return the pointer to the reference template. */
	USMInstance* GetStateMachineReferenceTemplateDirect() const { return ShouldUseTemplate() ? ReferencedInstanceTemplate : nullptr; }
	
	/** Signal if a reference graph should be used. Will create one if necessary. */
	void SetUseIntermediateGraph(bool bValue);

	/** Creates the appropriate bound graph depending on settings. */
	virtual void CreateBoundGraph();
	
	/** Set the read only state of the graph if this is a reference. */
	virtual void UpdateEditState();

	/** If this node references a state machine blueprint. */
	bool IsStateMachineReference() const { return bNeedsNewReference || ReferencedStateMachine != nullptr; }

	bool IsBoundGraphInvalid() const;
	
	/** Doesn't have intermediate graph but requires it. */
	bool NeedsIntermediateGraph() const;

	/** Set but may not be in use. */
	bool HasIntermediateGraph() const;

	/** Enabled and in use. */
	bool IsUsingIntermediateGraph() const;

	/** User has indicated to use intermediate graph. */
	bool ShouldUseIntermediateGraph() const;

	/** If true a template will be generated. */
	bool ShouldUseTemplate() const;

	/** If the current state is reused on end/start */
	bool ShouldReuseCurrentState() const;

	/**  Do not reuse if in an end state */
	bool ShouldReuseIfNotEndState() const;

	/** If the FSM is configured to wait for an end state. */
	bool ShouldWaitForEndState() const;
	
	bool IsSwitchingGraphTypes() const;

protected:
	// USMGraphNode_StateNodeBase
	virtual FLinearColor Internal_GetBackgroundColor() const override;
	// ~USMGraphNode_StateNodeBase

	/** If this state machine contains any actual states. */
	virtual bool HasLogicStates() const;

	/** First time setup when enabling or disabling templates. */
	void ConfigureInitialReferenceTemplate();
	
	/** Checks the reference template for a node class assigned and sets it to this node if it is different. */
	void SetNodeClassFromReferenceTemplate();
	
protected:
	UPROPERTY(meta = (LogicDriverExport))
	TObjectPtr<USMBlueprint> ReferencedStateMachine;

	UPROPERTY(meta = (LogicDriverExport))
	FString DesiredNodeName;

	UPROPERTY(meta = (LogicDriverExport))
	uint8 bShouldUseIntermediateGraph: 1;

	UPROPERTY()
	uint8 bNeedsNewReference: 1;

	uint8 bSwitchingGraphTypes: 1;
};
