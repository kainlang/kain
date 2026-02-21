// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMGraphProperty_Base.h"

#include "SMNode_Base.generated.h"

class USMInstance;
class USMNodeInstance;

/**
 * Base struct for all state machine nodes. The Guid MUST be manually initialized right after construction.
 */
USTRUCT(BlueprintInternalUseOnly)
struct SMSYSTEM_API FSMNode_Base
{
	GENERATED_USTRUCT_BODY()

	friend class FSMEditorConstructionManager;

#if WITH_EDITORONLY_DATA
	/** Check whether compiled guid cache matches run-time guid calculation. */
	static bool bValidateGuids;
#endif

public:
	const FSMNode_FunctionHandlers* GetFunctionHandlers() const { return FunctionHandlers; }

protected:
	/**
	 * Contains all function handler pointers. Every child node should implement their own type and instantiate it under InitializeFunctionHandlers.
	 * This is used to lower struct memory offset costs during GC.
	 */
	FSMNode_FunctionHandlers* FunctionHandlers;

public:
	/** The current time spent in the state. */
	UPROPERTY(BlueprintReadWrite, Category = "State Machines")
	float TimeInState;

	/**
	 * State Machine is in end state or the state is an end state.
	 * 
	 * The main case for this is when a local transition graph uses "Is State Machine in End State" (USMGraphK2Node_StateMachineReadNode_InEndState)
	 * @todo This should be moved to transitions only, but the now deprecated USMNodeInstance::IsInEndState() uses this.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "State Machines")
	bool bIsInEndState;

	/** State has updated at least once. */
	UPROPERTY(BlueprintReadWrite, Category = "State Machines")
	bool bHasUpdated;

	/** Special indicator in case this node is a duplicate within the same blueprint. If this isn't 0 then the NodeGuid will have been adjusted. */
	UPROPERTY()
	int32 DuplicateId;

	/** The node position in the graph. Set automatically. */
	UPROPERTY()
	FVector2D NodePosition;

	/** This node has at least one input event present. */
	UPROPERTY()
	uint8 bHasInputEvents: 1;

public:
	virtual void UpdateReadStates() {}

public:
	FSMNode_Base();
	virtual ~FSMNode_Base() = default;
	FSMNode_Base(const FSMNode_Base& Node) = default;

	/** Initialize specific properties and node instances. */
	virtual void Initialize(UObject* Instance);

protected:
	/** Map the FunctionHandler pointer. Must be implemented per child struct! */
	virtual void InitializeFunctionHandlers();

public:
	/** Initialize all graph evaluator functions. Must be called from GameThread! */
	virtual void InitializeGraphFunctions();

	/** Resets persistent data. */
	virtual void Reset();
	
	/** Called when the blueprint owning this node is started. */
	virtual void OnStartedByInstance(USMInstance* Instance);

	/** Called when the blueprint owning this node has stopped. */
	virtual void OnStoppedByInstance(USMInstance* Instance);
	
	/** If all graph function initialization has taken place once. */
	bool HaveGraphFunctionsInitialized() const { return bHaveGraphFunctionsInitialized; }

	/** If the node is currently initialized for this run. */
	bool IsInitializedForRun() const { return bIsInitializedForRun; }

	/** Unique identifier used in constructing nodes from a graph. May not be unique if this is from a parent graph or a reference. */
	const FGuid& GetNodeGuid() const;
	void GenerateNewNodeGuid();
	
	/** Unique identifier taking into account qualified path. Unique across blueprints if called after Instance initialization. */
	const FGuid& GetGuid() const;
	/** Calculate the value returned from GetGuid(). Gets all owner nodes and builds a path to this node. Hashes the path and sets PathGuid. */
	virtual void CalculatePathGuid(TMap<FString, int32>& InOutMappedPaths, bool bUseGuidCache = false);
	/** Unhashed string format of the guid path. MappedPaths are used to adjust for collisions. */
	FString GetGuidPath(TMap<FString, int32>& InOutMappedPaths) const;
	/** Calculate the path guid but do not set the guid. */
	FGuid CalculatePathGuidConst() const;
	
	/** Only generate a new guid if the current guid is invalid. This needs to be called
	 * on new nodes. */
	void GenerateNewNodeGuidIfNotSet();
	void SetNodeGuid(const FGuid& NewGuid);

	/** The state machine's NodeGuid owning this node. */
	void SetOwnerNodeGuid(const FGuid& NewGuid);
	/** Unique identifier to help determine which state machine this node belongs to. */
	const FGuid& GetOwnerNodeGuid() const { return OwnerGuid; }

	/** Property name of the NodeGuid. */
	static FName GetNodeGuidPropertyName() { return GET_MEMBER_NAME_CHECKED(FSMNode_Base, Guid); }
	
	/** The node directly owning this node. Should be a StateMachine. */
	void SetOwnerNode(FSMNode_Base* Owner);
	/** The node directly owning this node. Should be a StateMachine. */
	virtual FSMNode_Base* GetOwnerNode() const { return OwnerNode; }
	
	/** The state machine instance owning this node. */
	USMInstance* GetOwningInstance() const { return OwningInstance; }

	/** Create the node instance if a node instance class is set. */
	void CreateNodeInstance();
	void CreateStackInstances();
	virtual void RunConstructionScripts();
	
	/** Calls CheckNodeInstanceCompatible. */
	void SetNodeInstanceClass(UClass* NewNodeInstanceClass);
	
	/** Derived nodes should overload and check for the correct type. */
	virtual bool IsNodeInstanceClassCompatible(UClass* NewNodeInstanceClass) const;
	
	/** Return the current node instance. Only valid after initialization and may be nullptr. */
	virtual USMNodeInstance* GetNodeInstance() const { return NodeInstance; }

	/** Create a node instance on demand if needed. Only required for default node classes. Initialization should be completed before calling. */
	virtual USMNodeInstance* GetOrCreateNodeInstance();

	/** If the node can at some point create a node instance. */
	virtual bool CanEverCreateNodeInstance() const { return true; }
	
	/** Returns the current stack instances. */
	const TArray<TObjectPtr<USMNodeInstance>>& GetStackInstancesConst() const { return StackNodeInstances; }
	TArray<TObjectPtr<USMNodeInstance>>& GetStackInstances() { return StackNodeInstances; }

	/** Returns a specific state from the stack. */
	USMNodeInstance* GetNodeInStack(int32 Index) const;
	
	/** The default node instance class. Each derived node class needs to implement. */
	virtual UClass* GetDefaultNodeInstanceClass() const { return nullptr; }

	/** The current in use node class. */
	UClass* GetNodeInstanceClass() const { return NodeInstanceClass; }

	/** Is the default node class assigned. */
	bool IsUsingDefaultNodeClass() const { return GetDefaultNodeInstanceClass() == GetNodeInstanceClass(); }
	
	void AddVariableGraphProperty(const FSMGraphProperty_Base_Runtime& GraphProperty, const FGuid& OwningTemplateGuid);

	void SetNodeName(const FString& Name);
	const FString& GetNodeName() const { return NodeName; }
	
	void SetTemplateName(const FName& Name);
	
	/**
	 * Return the template name used to find a node instance archetype.
	 *
	 * @param InNodeInstance The current node instance we want the template name for. Can be either the main node instance
	 * or a stack instance. When nullptr the main node instance template name is used.
	 * @return The node template name used to discover node instance archetypes or NAME_None if not found.
	 */
	FName GetTemplateName(USMNodeInstance* InNodeInstance = nullptr) const;
	
	void AddStackTemplateName(const FName& Name, UClass* TemplateClass);
	
	/** If this node is active. */
	virtual bool IsActive() const { return bIsActive; }

	virtual void ExecuteInitializeNodes();
	virtual void ExecuteShutdownNodes();

	/** Set the time in state as recorded from the server. */
	virtual void SetServerTimeInState(float InTime);

	/** The time in state as recorded by the server. Kept in the base node as transitions can utilize it. */
	float GetServerTimeInState() const { return ServerTimeInState; }
	
	/**
	* Checks if the instance is allowed to execute properties automatically.
	* @param OnEvent GRAPH_PROPERTY_EVAL_[TYPE]
	* @param ForTemplate The specific template to check against.
	*/
	virtual bool CanExecuteGraphProperties(uint32 OnEvent, const class USMStateInstance_Base* ForTemplate) const { return false; }
	
	/** Execute desired graph properties for the given event. */
	virtual bool TryExecuteGraphProperties(uint32 OnEvent);
	
	/**
	 * Evaluates graph properties.
	 * @param ForNodeInstance The node instance we are evaluating.
	 * @param ForTemplateGuid If specified only graph properties for this template will be executed. If null all properties will be executed.
	 */
	void ExecuteGraphProperties(USMNodeInstance* ForNodeInstance, const FGuid* ForTemplateGuid);

	/** Retrieve the embedded graph properties. */
	const TArray<FSMGraphProperty_Base_Runtime*>& GetGraphProperties() const { return GraphProperties; }

	/** Retrieve the template variable graph properties. */
	const TMap<FGuid, FSMGraphPropertyTemplateOwner>& GetTemplateGraphProperties() const { return TemplateVariableGraphProperties; }

	/** See if the user wants variables reset. */
	virtual void TryResetVariables();
	
#if WITH_EDITORONLY_DATA
	virtual bool IsDebugActive() const { return bIsActive; }
	virtual bool WasDebugActive() const { return bWasActive; }
	
	/** Debug helper in case a state switches to inactive in one frame. */
	mutable bool bWasActive = false;
#endif

#if WITH_EDITOR
	/** Performs a safe reset. It's possible referenced structs have changed in the BP and may not be valid. */
	virtual void EditorShutdown();

	/** Reset any values set from state machine generation. */
	virtual void ResetGeneratedValues();
#endif
	
protected:
	/** Execute the graph. */
	virtual void PrepareGraphExecution();
	virtual void SetActive(bool bValue);

	void CreateGraphProperties();
	void CreateGraphPropertiesForTemplate(USMNodeInstance* Template, const TMap<FGuid, FSMGraphProperty_Base_Runtime*>& MappedGraphPropertyInstances);
protected:
	/**
	 * NodeGuid must always be unique. Do not duplicate the guid in any other node in any blueprint.
	 *
	 * This is not the same guid that is used at run-time. At run-time all NodeGuids in a path to a node
	 * are hashed to form the PathGuid. This is done to account for multiple references and parent graph calls.
	 *
	 * If you need to change the path of a node (such as collapse it to a nested state machine) and you need to maintain
	 * the old guid for run-time saves to work, you should use the GuidRedirectMap on the primary state machine instance
	 * which accepts PathGuids.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Graph Node")
	FGuid Guid;

	/** The state machine's NodeGuid owning this node. */
	UPROPERTY()
	FGuid OwnerGuid;

	/**
	 * Unique identifier calculated from this node's place in an instance.
	 * Calculated by taking the MD5 hash of the full path of all owner NodeGuids and
	 * this NodeGuid.
	 * This is what is returned from GetGuid().
	 * 
	 * ReadWrite so it can be easily read from custom graph nodes.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Graph Node")
	FGuid PathGuid;
	
	/** The node directly owning this node. Should be a StateMachine. */
	FSMNode_Base* OwnerNode;

	UPROPERTY()
	FString NodeName;

	/** The name of a template archetype to use when constructing an instance. This allows default values be passed into the instance. */
	UPROPERTY()
	FName TemplateName;

	/** The names of stack template archetypes used when constructing the stack. */
	UPROPERTY()
	TArray<FName> StackTemplateNames;

	/** The node instances for this stack. */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "Node Class")
	TArray<TObjectPtr<USMNodeInstance>> StackNodeInstances;

	/**
	 * All classes used in the node stack. The classes are stored here only to help with dependency loading by the engine,
	 * specifically with BP nativization. This isn't very useful otherwise as the archetypes (dynamically added
	 * default sub-objects) contain instance information which the class won't have.
	 *
	 * Without this there may be runtime errors when trying to access internal resources.
	 * An example is setting a custom node icon for a stack node with the primary node not loading a custom icon.
	 *
	 * The nativization code generated that fails is:
	 * "NodeIcon = CastChecked<UTexture2D>(CastChecked<UDynamicClass>(UTransitionStackA_C__pf1010915279::StaticClass())->UsedAssets[0], ECastCheckedType::NullAllowed);"
	 *
	 * TODO: Consider removing with UE 5.0 as nativization is deprecated.
	 */
	UPROPERTY()
	TArray<TObjectPtr<UClass>> NodeStackClasses;
	
	/** The state machine instance owning this node. */
	UPROPERTY()
	TObjectPtr<USMInstance> OwningInstance;

	/** The node instance for this node if it exists. */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "Node Class")
	TObjectPtr<USMNodeInstance> NodeInstance;

	/** Custom graph structs with special handling. Dynamically loaded on initialization from embedded structs. */
	TArray<FSMGraphProperty_Base_Runtime*> GraphProperties;

	/** Set by the BP compiler. Template Guid -> GraphProperties. Contains data necessary to evaluate variables which have instanced BP graphs. */
	UPROPERTY()
	TMap<FGuid, FSMGraphPropertyTemplateOwner> TemplateVariableGraphProperties;
	
	/** The class to use to construct the node instance if one exists. */
	UPROPERTY(BlueprintReadWrite, Category = "Node Class")
	TObjectPtr<UClass> NodeInstanceClass;

private:
	/** Last recorded active time in state from the server. */
	float ServerTimeInState;
	
	uint8 bHaveGraphFunctionsInitialized: 1;
	uint8 bIsInitializedForRun: 1;
	uint8 bIsActive: 1;
};