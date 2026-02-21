// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMSystemEditorLog.h"

#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"

// Restrict all INVALID_OBJECTNAME_CHARACTERS except for space.
#define LD_INVALID_STATENAME_CHARACTERS	TEXT("\"',/.:|&!~\n\r\t@#(){}[]=;^%$`")

class FSMBlueprintEditor;
class USMBlueprint;
class USMBlueprintGeneratedClass;
class USMNodeBlueprint;
class USMGraph;
class USMGraphK2;
class USMGraphNode_StateNode;
class USMGraphNode_StateMachineStateNode;
class USMGraphNode_TransitionEdge;
class UK2Node_CallParentFunction;
class USMEditorSettings;
class USMGraphNode_Base;
class USMGraphNode_StateNodeBase;
class USMGraphK2Node_StateMachineNode;
class USMGraphNode_AnyStateNode;
class USMGraphK2Node_RuntimeNode_Base;
class USMGraphK2Node_RuntimeNodeContainer;
class USMNodeInstance;
class USMInstance;
class USMProjectEditorSettings;
struct FSMNode_Base;
struct FSMStateMachineNodePlacementValidator;

// Helpers for managing blueprints, editors, and graphs.
class SMSYSTEMEDITOR_API FSMBlueprintEditorUtils : public FBlueprintEditorUtils
{
public:
	/** Locate the state machine editor for blueprints, graphs, or nodes. */
	static FSMBlueprintEditor* GetStateMachineEditor(UObject const* Object);

	/** Lookup the outer chain for a blueprint type. */
	static USMBlueprint* FindBlueprintFromObject(UObject* Object);
	
	/** Return module editor settings. */
	static const USMEditorSettings* GetEditorSettings();

	/** Return module editor settings available to edit. */
	static USMEditorSettings* GetMutableEditorSettings();

	/** Return module editor settings for the project. */
	static const USMProjectEditorSettings* GetProjectEditorSettings();

	/** Return module editor settings for the project available to edit. */
	static USMProjectEditorSettings* GetMutableProjectEditorSettings();

	/** Search all blueprint graphs constructing a full list of all nodes matching the type. */
	template<typename T>
	static void GetAllNodesOfClassNested(const UBlueprint* Blueprint, TArray<T*>& Nodes)
	{
		check(Blueprint);
		
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			Graph->GetNodesOfClass<T>(Nodes);
		}
	}
	
	/** Recursively search all children graphs constructing a full list of all nodes matching the type. */
	template<typename T>
	static void GetAllNodesOfClassNested(const UEdGraph* Graph, TArray<T*>& Nodes)
	{
		check(Graph);

		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FSMBlueprintEditorUtils::GetAllNodesOfClassNested"), STAT_GetAllNodesOfClassNested, STATGROUP_LogicDriverEditor);
		
		Graph->GetNodesOfClass<T>(Nodes);

		TArray<UEdGraph*> ChildrenGraphs;
		Graph->GetAllChildrenGraphs(ChildrenGraphs);

		for (UEdGraph* NextGraph : ChildrenGraphs)
		{
			NextGraph->GetNodesOfClass<T>(Nodes);
		}
	}

	/** Recursively search all graphs until the first node of type T is found. */
	template<typename T>
	static T* GetFirstNodeOfClassNested(const UEdGraph* Graph)
	{
		check(Graph);

		for (const TObjectPtr<UEdGraphNode>& Node : Graph->Nodes)
		{
			if (T* CastedNode = Cast<T>(Node))
			{
				return CastedNode;
			}
		}

		TArray<UEdGraph*> ChildrenGraphs;
		Graph->GetAllChildrenGraphs(ChildrenGraphs);

		for (const UEdGraph* NextGraph : ChildrenGraphs)
		{
			if (T* FoundNode = GetFirstNodeOfClassNested<T>(NextGraph))
			{
				return FoundNode;
			}
		}

		return nullptr;
	}

	/**
	 * Checks graphs recursively for any node that is a child of a member of Classes.
	 * @param InGraph The toplevel graph to check.
	 * @param InClasses All possible UEdGraphNode types that should be considered. Children are considered automatically.
	 * @return True when the first match is found.
	 */
	static bool ContainsNodeOfAnyClassNested(const UEdGraph* InGraph, const TArray<UClass*>& InClasses);

	/** Recursively search all children graphs constructing a full list of all graphs matching the type. */
	template <typename T>
	static void GetAllGraphsOfClassNested(const UEdGraph* GraphIn, TSet<T*>& GraphsOut)
	{
		check(GraphIn);

		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FSMBlueprintEditorUtils::GetAllGraphsOfClassNested"), STAT_GetAllGraphsOfClassNested, STATGROUP_LogicDriverEditor);
		
		if (const T* CastedGraph = Cast<T>(GraphIn))
		{
			GraphsOut.Add(const_cast<T*>(CastedGraph));
		}

		TArray<UEdGraph*> ChildGraphs;
		GraphIn->GetAllChildrenGraphs(ChildGraphs);

		for (UEdGraph* Child : ChildGraphs)
		{
			if (const T* CastedGraph = Cast<T>(Child))
			{
				GraphsOut.Add(const_cast<T*>(CastedGraph));
			}
		}
	}

	/** Return array tuple of <ParentGraph, T* FoundGraph>. Elements only filled when the correct child type is found. */
	template <typename T>
	static void GetAllGraphsOfClassNestedWithParents(UEdGraph* GraphIn, TArray<TTuple<UEdGraph*, T*>>& GraphsOut)
	{
		check(GraphIn);
		
		if (T* CastedGraph = Cast<T>(GraphIn))
		{
			GraphsOut.Add(MakeTuple(nullptr, CastedGraph));
		}

		for (const TObjectPtr<UEdGraph>& Graph : GraphIn->SubGraphs)
		{
			if (T* CastedGraph = Cast<T>(Graph))
			{
				GraphsOut.Add(MakeTuple(GraphIn, CastedGraph));
			}
			GetAllGraphsOfClassNestedWithParents(Graph, GraphsOut);
		}
	}

	/** Retrieve all nodes with IsConsideredForEntryConnection(). */
	static void GetAllRuntimeEntryNodes(const UEdGraph* InGraph, TArray<USMGraphK2Node_RuntimeNode_Base*>& OutEntryNodes);
	
	/** Queues the blueprint with construction script manager to compile next frame. Only compiles if not already compiling. */
	static void ConditionallyCompileBlueprint(UBlueprint* Blueprint, bool bUpdateDependencies = true, bool bRecreateGraphProperties = false);
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnBlueprintConditionallyCompiled, UBlueprint* /*Blueprint*/, bool /*bUpdateDependencies*/, bool /*bRecreateGraphProperties*/)
	static FOnBlueprintConditionallyCompiled OnBlueprintPreConditionallyCompiledEvent;
	static FOnBlueprintConditionallyCompiled OnBlueprintPostConditionallyCompiledEvent;

	/** GetDerivedClasses but filters out REINST and abstract. */
	static void GetValidDerivedClasses(const UClass* TargetClass, TArray<UClass*>& OutClasses);

	static UClass* GetMostUpToDateClass(UClass* Class);

	/** Performs a blueprint lookup and returns the generated class. If no blueprint exists the passed in class is returned instead. Accepts null. */
	static UClass* TryGetFullyGeneratedClass(UClass* Class);

	static void GetAllNodeInstancesWithPropertyGraphs(UBlueprint* Blueprint, TSet<TSubclassOf<USMNodeInstance>>& NodeInstances);
	
	static void HandleRefreshAllNodes(UBlueprint* InBlueprint);

	static void HandleRenameVariableEvent(UBlueprint* InBlueprint, UClass* InVariableClass, const FName& InOldVarName, const FName& InNewVarName);

	static void GetAllConnectedNodes(UEdGraphNode* StartNode, EEdGraphPinDirection Direction, TSet<UEdGraphNode*>& FoundNodes);
	
	/** Remove all nodes from a graph. If no blueprint is provided it will be looked up. Modify specifies if the blueprint should be structurally modified. */
	static void RemoveAllNodesFromGraph(UEdGraph* GraphIn, UBlueprint* BlueprintIn = nullptr, bool bModify = true,
	                                    bool bSkipEntryNodes = true, bool bSilently = false);

	static void RemoveNodeSilently(UBlueprint* Blueprint, UEdGraphNode* Node);

	/** Checks if a node is selected in the blueprint editor. */
	static bool IsNodeSelected(UEdGraphNode* Node);
	
	/** Checks the graph and all nested graphs. Call before placing the node. */
	template<typename T>
	static bool IsNodeAlreadyPlaced(const UEdGraph* Graph)
	{
		if (!Graph)
		{
			return false;
		}

		TArray<T*> Nodes;
		GetAllNodesOfClassNested<T>(Graph, Nodes);
		return Nodes.Num() > 0;
	}

	/** Place a node if it is not already set. Returns true on success. The OutNode will either be new or the existing node. */
	template<typename T>
	static bool PlaceNodeIfNotSet(UEdGraph* Graph, UEdGraphNode* NodeToWireFrom = nullptr, T** OutNode = nullptr, EEdGraphPinDirection FromPinDirection = EGPD_Output, int32 DistanceFromNode = 550)
	{
		if (T* ExistingNode = GetFirstNodeOfClassNested<T>(Graph))
		{
			if (OutNode)
			{
				*OutNode = ExistingNode;
			}
			return false;
		}

		check(Graph);

		FGraphNodeCreator<T> NodeCreator(*Graph);
		T* NewNode = NodeCreator.CreateNode();
		NodeCreator.Finalize();

		UEdGraphNode* NewGraphNode = CastChecked<UEdGraphNode>(NewNode);
		if (OutNode)
		{
			*OutNode = Cast<T>(NewGraphNode);
		}

		if (NodeToWireFrom)
		{
			PlaceNodeIfNotSet_WireNode_Internal(NodeToWireFrom, NewNode, FromPinDirection, DistanceFromNode);
		}

		return true;
	}

private:
	static void PlaceNodeIfNotSet_WireNode_Internal(UEdGraphNode* NodeToWireFrom, UEdGraphNode* ToNode, EEdGraphPinDirection FromPinDirection, int32 DistanceFromNode);
	
public:
	/** Place an existing variable onto a graph and wire it to the destination pin if provided. Returns true if the node was placed and wired properly. */
	static bool PlacePropertyOnGraph(UEdGraph* Graph, FProperty* Property, UEdGraphPin* DestinationPin, UK2Node_VariableGet** VariableNodeOut, float WidthOffset = -100.f, bool bAutoWireObjects = true);

	/** Place a function call onto a graph and wire it to the destination pin if provided. Returns true if the node was placed and wired properly. */
	static bool PlaceFunctionOnGraph(UEdGraph* Graph, UFunction* Function, UEdGraphPin* DestinationPin, UEdGraphNode** FunctionNodeOut = nullptr,
		UEdGraphPin** FunctionArgumentPinOut = nullptr, float WidthOffset = -100.f, float VerticalOffset = 100.f, bool bAutoWireObjects = true);

	static bool GetOutputProperties(UFunction* Function, TArray<FProperty*>& Outputs);
	
	/** Create and wire two nodes. If T1 already exists it will use that node. If both already exist no changes will be made. */
	template<typename T1, typename T2>
	static void SetupDefaultPassthroughNodes(UEdGraph* Graph)
	{
		T1* Entered;
		if (FSMBlueprintEditorUtils::PlaceNodeIfNotSet<T1>(Graph, nullptr, &Entered))
		{
			const FVector2D NewPosition = Graph->GetGoodPlaceForNewNode();
			Entered->NodePosX = NewPosition.X;
			Entered->NodePosY = NewPosition.Y;
		}

		FSMBlueprintEditorUtils::PlaceNodeIfNotSet<T2>(Graph, Entered);
	}

	/** Splits a category string into separate categories, such as Category|NestedCategory. */
	static void SplitCategories(const FString& InCategoryString, TArray<FString>& OutCategories);
	
	/** Checks if this is a default graph node. This isn't very useful since if a graph was duplicated it won't have copied the meta data over. */
	static bool IsNodeGraphDefault(const UEdGraphNode* Node);
	
	/** K2 Graphs have different base classes then UEdGraph. This will return the correct runtime node if one exists. */
	static FSMNode_Base* GetRuntimeNodeFromGraph(const UEdGraph* Graph);

	/** Retrieve the runtime node only if this node contains one. Container nodes have different handling from state machine entry nodes. */
	static FSMNode_Base* GetRuntimeNodeFromExactNode(UEdGraphNode* Node);

	/** Retrieve the runtime node only if this node contains one. Container nodes have different handling from state machine entry nodes. */
	static FSMNode_Base* GetRuntimeNodeFromExactNodeChecked(UEdGraphNode* Node);

	/** Search a graph and its children for the graph which represents a runtime node guid. Optionally limit to a graph name in case more than one graph represents the same node. */
	static UEdGraph* FindGraphFromRuntimeNodeGuid(UEdGraph* InGraph, const FGuid& InNodeGuid, const FString& InGraphName = FString());
	
	static USMNodeInstance* GetNodeTemplate(const UEdGraph* ForGraph);
	static TSubclassOf<UObject> GetNodeTemplateClass(const UEdGraph* ForGraph, bool bReturnDefaultIfNone = false, const FGuid& TemplateGuid = FGuid());

	static UClass* GetNodeClassFromPin(const UEdGraphPin* Pin);
	static UClass* GetStateMachineClassFromGraph(const UEdGraph* Graph);

	/** Jump to the node blueprint. */
	static void GoToNodeBlueprint(const USMGraphNode_Base* InGraphNode);

	/** Find the USMNodeBlueprint from a node class and set the debug target if applicable. */
	static USMNodeBlueprint* GetNodeBlueprintFromClassAndSetDebugObject(const UClass* InClass, const USMGraphNode_Base* InGraphNode, const FGuid* InTemplateGuid = nullptr);

	/** Return the run-time debug node for a graph node, if one exists. */
	static const FSMNode_Base* GetDebugNode(const USMGraphNode_Base* Node);
	
	/** Search graphs to return a chain of runtime nodes ordered oldest to newest. Mimics runtime behavior of TryGetAllOwners. */
	static void FindRuntimeNodeWithOwners(const UEdGraph* Graph, TArray<const FSMNode_Base*>& RuntimeNodesOrdered, TSet<const UObject*>* StopOnOuters = nullptr);

	/** Attempts to build out a qualified path GUID resembling what the run time instances use. Won't work with references or children. */
	static FGuid TryCreatePathGuid(const UEdGraph* Graph);

	/** Sanitize a name. */
	static FString GetSafeName(const FString& InName);

	/** Sanitize a name for a state. */
	static FString GetSafeStateName(const FString& InName);
	
	/** Find a unique name by incrementing a counter. Attempts to utilize an existing index. */
	static FString FindUniqueName(const FString& InName, UEdGraph* Graph);
	
	/** Finds the runtime node associated with a graph and updates it. Looks for any containers references and updates their guid references. */
	static void UpdateRuntimeNodeForGraph(FSMNode_Base* Node, const UEdGraph* Graph);

	/** Update the runtime node for the graph and any contained graphs. Checks to make sure graph to update contains right runtime node. */
	static void UpdateRuntimeNodeForNestedGraphs(const FGuid& CurrentGuid, FSMNode_Base* Node, const UEdGraph* Graph);
	
	static void UpdateRuntimeNodeForBlueprint(const FGuid& CurrentGuid, FSMNode_Base* Node,
	                                                             UBlueprint* Blueprint);

	static FMulticastDelegateProperty* GetDelegateProperty(const FName& DelegatePropertyName, UClass* DelegateOwnerClass, const UFunction* SignatureFunction = nullptr);

	static FMulticastDelegateProperty* FindDelegatePropertyByFunction(const UFunction* SignatureFunction);

	/** Checks for any supported input events in the graph. */
	static bool DoesGraphHaveInputEvents(const UEdGraph* InGraph);
	
	/** K2 Graphs have different base classes then UEdGraph. This will check if the graph has logic connections from any entry points. */
	static bool GraphHasAnyLogicConnections(const UEdGraph* Graph);

	/** Looks for top level node of any node... StateNode would return StateMachineNode... a K2 node could return a StateNode or TransitionNode...
	 * Useful for finding way out of nested graphs. */
	static USMGraphNode_Base* FindTopLevelOwningNode(const UEdGraph* InGraph);

	/** Returns the top most graph below an owning node which could be this graph. */
	static const UEdGraph* FindTopLevelOwningGraph(const UEdGraph* InGraph);
	
	// Locates the correct state machine entry graph for a blueprint.
	static USMGraphK2* GetTopLevelStateMachineGraph(UBlueprint* Blueprint);

	/** Locate the top level graph and the root state machine selected. Use parent will look up the chain if a root state machine doesn't exist. */
	static USMGraphK2Node_StateMachineNode* GetRootStateMachineNode(UBlueprint* Blueprint, bool bUseParent = false);

	/** Return the actual root state machine graph. This contains the entry point leading to the first state which will be executed. */
	static USMGraph* GetRootStateMachineGraph(UBlueprint* Blueprint, bool bUseParent = false);

	/** Find the runtime container from a graph. */
	static USMGraphK2Node_RuntimeNodeContainer* GetRuntimeContainerFromGraph(const UEdGraph* Graph);
	
	/**
	 * Look for Any State nodes and determine if they impact the given node.
	 * @param StateNode The normal state base node to check against Any States.
	 * @param OutAllAnyStates If provided all Any States impacting the state node will be returned.
	 */
	static bool IsNodeImpactedFromAnyStateNode(const USMGraphNode_StateNodeBase* StateNode, TArray<USMGraphNode_AnyStateNode*>* OutAllAnyStates = nullptr);

	/** Retrieve all Any State nodes for the given graph only. */
	static bool TryGetAnyStateNodesForGraph(USMGraph* Graph, TArray<USMGraphNode_AnyStateNode*>& OutNodes);

	/** Checks if a specific Any State node impacts a specific state node. */
	static bool DoesAnyStateImpactOtherNode(const USMGraphNode_AnyStateNode* AnyStateNode, const USMGraphNode_StateNodeBase* OtherNode);
	
	/** Retrieve all generated class parents of a blueprint from newest to oldest. */
	static bool TryGetParentClasses(UBlueprint* Blueprint, TArray<USMBlueprintGeneratedClass*>& OutClassesOrdered);
	
	static bool IsStateMachineInstanceGraph(UEdGraph* GraphIn);

	static bool IsGraphConfiguredForTransitionEvents(const UEdGraph* Graph);
	
	static bool TryGetVariableByName(UBlueprint* Blueprint, const FName& Name, FBPVariableDescription& VariableOut);

	static bool TryGetVariableByGuid(UBlueprint* Blueprint, const FGuid& Guid, FBPVariableDescription& VariableOut);

	/** Attempt to find the most valid FProperty for a variable name. */
	static FProperty* GetPropertyByName(const UBlueprint* InBlueprint, const FName& InName);

	/** Purge and trash all nested reference templates within this template. */
	static void CleanReferenceTemplates(USMInstance* Template);

	/** Renames the object to the transient package and invalidates exports. */
	static void TrashObject(UObject* Object);

	struct FCacheInvalidationArgs
	{
		bool bAllowDuringCompile;
		bool bAllowIfTransacting;

		FCacheInvalidationArgs() :
		bAllowDuringCompile(false),
		bAllowIfTransacting(false)
		{
		}
	};

	/** Fires OnCacheClearedEvent which graphs and nodes should listen for and handle cache invalidation. */
	static void InvalidateCaches(const UBlueprint* InBlueprint, const FCacheInvalidationArgs& InInvalidationArgs = FCacheInvalidationArgs());
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCacheCleared, const USMBlueprint*);

	/** An event that fires when UI related caches should be cleared. */
	static FOnCacheCleared OnCacheClearedEvent;

	struct FBulkCacheInvalidation
	{
		TWeakObjectPtr<USMBlueprint> Blueprint;
		FCacheInvalidationArgs InvalidationArgs;

		FBulkCacheInvalidation(USMBlueprint* InBlueprint, const FCacheInvalidationArgs& InInvalidationArgs = FCacheInvalidationArgs());

		~FBulkCacheInvalidation();
	};

	/** Looks for Composite nodes that have no bound graph then attempt to find that graph and link it to the node.*/
	static void FixUpCollapsedGraphs(UEdGraph* TopLevelGraph);

	/** Looks for graph nodes that contain duplicate ids and change. */
	static void FixUpDuplicateGraphNodeGuids(UBlueprint* Blueprint);

	/** Looks for graph nodes that contain duplicate runtime ids and change. */
	static int32 FixUpDuplicateRuntimeGuids(UBlueprint* Blueprint, FCompilerResultsLog* MessageLog = nullptr);

	/** Looks for reference nodes which don't match their container owner and changes them to match. */
	static int32 FixUpMismatchedRuntimeGuids(UBlueprint* Blueprint, FCompilerResultsLog* MessageLog = nullptr);

	/** Find function graphs that were autogenerated by the engine but incorrectly set as SMK2 graphs. */
	static void FixUpAutoGeneratedFunctions(UBlueprint* Blueprint, bool bFocusTab = false, FCompilerResultsLog* MessageLog = nullptr);
	
	/** Searches for runtime graph nodes with duplicate guids in a blueprint and its parent classes. */
	static bool FindNodesWithDuplicateRuntimeGuids(UBlueprint* Blueprint, TMap<FGuid, TArray<UEdGraphNode*>>& RuntimeNodes);

	static bool FindNodesWithDuplicateRuntimeGuids(UEdGraph* Graph, TMap<FGuid, TArray<UEdGraphNode*>>& RuntimeNodes);

	static void CleanUpIsolatedTransitions(UEdGraph* Graph);

	/** Check for and remove property graphs which don't have a blueprint associated. This can be required after
	 * a property graph deletion & undo. */
	static void CleanupInvalidPropertyGraphs(UBlueprint* InBlueprint, FCompilerResultsLog* MessageLog = nullptr);

	/** Check if any children make a call to the parent and clean up stale functions. */
	static void CleanupInvalidParentFunctionsInChildren(const UBlueprint* InBlueprint);
	
	/** Creates a function call for the given function in the given graph. */
	static UK2Node_CallFunction* CreateFunctionCall(UEdGraph* Graph, UFunction* Function);

	/** Creates a parent function call and wires it to the child. */
	static UK2Node_CallParentFunction* CreateParentFunctionCall(UEdGraph* Graph, UFunction* ParentFunction, UEdGraphNode* ChildNode, int32 XPostion = 200, TOptional<int32> YPosition = TOptional<int32>(), bool bIsDefault = true);

	/** Checks if a generic state machine can be placed in a graph based on rule behavior. If there are matching rules they are output. */
	static bool CanStateMachineBePlacedInGraph(const USMGraph* Graph, FSMStateMachineNodePlacementValidator& OutRules);

	/** Checks if a state machine can be converted to a reference based on rule behavior. */
	static bool CanStateMachineBeConvertedToReference(USMGraph* Graph);
	
	/** Collapse the given nodes into their own sub state machine. */
	static USMGraphNode_StateMachineStateNode* CollapseNodesAndCreateStateMachine(const TSet<UObject*>& InNodes);

	/** Helper utility to combine multiple selected states */
	static void CombineStates(UEdGraphNode* DestinationNode, const TSet<UObject*>& NodesToMerge, bool bDestroyStates);

	/** Copy property graphs and default values from a state node to a destination state node. The original template guid needs to be provided to the original property graph can be found. */
	static bool DuplicateStackTemplatePropertyGraphs(USMGraphNode_StateNode* FromStateNode, USMGraphNode_StateNode* DestinationStateNode, struct FStateStackContainer& NewStackContainer, const FGuid& OriginalTemplateGuid);
	
	/** Move a transition to different states. */
	static void MoveTransition(USMGraphNode_TransitionEdge* Transition, USMGraphNode_StateNodeBase* FromState, USMGraphNode_StateNodeBase* ToState);

	/** Convert USMGraphNode to another USMGraphNode, wire up the connections, and delete the old node. */
	template<typename T>
	static T* ConvertNodeTo(USMGraphNode_Base* OriginalNode, bool bDontOverrideDefaultClass = false, bool bClearEditorSelection = true)
	{
		return Cast<T>(ConvertNodeTo_Internal(T::StaticClass(), OriginalNode, bDontOverrideDefaultClass, bClearEditorSelection));
	}

private:
	static UEdGraphNode* ConvertNodeTo_Internal(const UClass* NewClass, USMGraphNode_Base* OriginalNode, bool bDontOverrideDefaultClass, bool bClearEditorSelection);

public:
	/** Convert a state machine in-place to a referenced state machine. If Asset Name and Path are null they will be calculated. */
	static USMBlueprint* ConvertStateMachineToReference(USMGraphNode_StateMachineStateNode* StateMachineNode,
		bool bUserPrompt = true, const FString* AssetName = nullptr, const FString* AssetPath = nullptr, TSubclassOf<USMInstance> ParentClass = nullptr);

	/**
	 * Clear the current editor selection.
	 * @param EditorContextObject The object to find the blueprint editor for.
	 * @param bNextTick Whether the selection should be cleared on the next tick instead.
	 */
	static void ClearEditorSelection(UObject* EditorContextObject, bool bNextTick = false);

	/** Disable tooltips for 2 frames. */
	static void DisableToolTipsTemporarily();
};
