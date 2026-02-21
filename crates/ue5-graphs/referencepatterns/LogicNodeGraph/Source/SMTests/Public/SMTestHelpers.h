// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node_CallFunction.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Factories/Factory.h"

#include "Blueprints/SMBlueprint.h"

#include "Utilities/SMBlueprintEditorUtils.h"
#include "SMTestContext.h"
#include "Graph/SMTransitionGraph.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionResultNode.h"
#include "Graph/Schema/SMGraphSchema.h"


#if WITH_DEV_AUTOMATION_TESTS

class UK2Node_DynamicCast;

// Manage a physical asset. Based on UnrealEd\ObjectTools
struct FAssetHandler
{
	FAssetHandler() :
	Name(""),
	GamePath(""),
	Class(nullptr),
	Factory(nullptr),
	Package(nullptr),
	Object(nullptr)
	{}

	FAssetHandler(const FString& ObjectName, UClass* ObjectClass, UFactory* ObjectFactory, FString* ObjectPath = nullptr):
	Name(ObjectName),
	GamePath(ObjectPath ? *ObjectPath : *DefaultGamePath()),
	Class(ObjectClass),
	Factory(ObjectFactory),
	Package(nullptr),
	Object(nullptr)
	{}
	
	FString Name;
	FString GamePath;
	UClass* Class;
	UFactory* Factory;
	UPackage* Package;
	UObject* Object;

	bool CreateAsset();
	bool SaveAsset();
	bool LoadAsset();
	bool DeleteAsset();
	bool UnloadAsset();
	bool ReloadAsset();

	bool CreateAsset(FAutomationTestBase* Test);
	bool SaveAsset(FAutomationTestBase* Test);
	bool LoadAsset(FAutomationTestBase* Test);
	bool DeleteAsset(FAutomationTestBase* Test);
	bool UnloadAsset(FAutomationTestBase* Test);
	bool ReloadAsset(FAutomationTestBase* Test);
	
	UObject* GetObject() const
	{
		return Object;
	}

	template<typename T>
	T* GetObjectAs() const
	{
		return CastChecked<T>(GetObject());
	}

	static FString DefaultFullPath() { return FPackageName::FilenameToLongPackageName(FPaths::AutomationTransientDir() + TEXT("Automation_SMAssetCreation")); }
	static FString DefaultGamePath() { return "/Temp/Automation/Transient/"; }
	static FString GetFullGamePath() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Temp/Automation/Transient/")); }
};

namespace TestHelpers
{
	/** Instantiate a runtime state machine instance from a blueprint class. */
	USMInstance* CreateNewStateMachineInstanceFromBP(FAutomationTestBase* Test, USMBlueprint* Blueprint, USMTestContext* Context, bool bTestNodeMap = true);

	/** Compile the BP, create a new context, and create and initialize the state machine instance. */
	USMInstance* CompileAndCreateStateMachineInstanceFromBP(USMBlueprint* Blueprint, bool bInitialize = true);

	FAssetHandler ConstructNewStateMachineAsset(TSubclassOf<USMInstance> InParentClass = nullptr);
	FAssetHandler CreateAssetFromBlueprint(UBlueprint* InBlueprint);
	
	bool TryCreateNewStateMachineAsset(FAutomationTestBase* Test, FAssetHandler& NewAsset, bool Save = false, TSubclassOf<USMInstance> ParentClass = nullptr);

	/** Verify graphs are correct. */
	void ValidateNewStateMachineBlueprint(FAutomationTestBase* Test, USMBlueprint* NewBP);

	/** Create USMNodeBlueprint for a given instance class. */
	FAssetHandler ConstructNewNodeAsset(UClass* NodeClass);
	
	/** Creates a node class blueprint and validates proper graphs and k2 nodes exist. */
	bool TryCreateNewNodeAsset(FAutomationTestBase* Test, FAssetHandler& NewAsset, UClass* NodeClass, bool Save = false);
	
#pragma region Node Helpers

	/** Creates a context getter for SMInstance within the given graph. */
	UK2Node_CallFunction* CreateContextGetter(FAutomationTestBase* Test, UEdGraph* Graph, UEdGraphPin** ContextOutPin);

	/** Creates a function call for the given function in the given graph. */
	UK2Node_CallFunction* CreateFunctionCall(UEdGraph* Graph, UFunction* Function);

	/** Setup a new cast Node, add it to the graph, and wire it between pins. */
	UK2Node_DynamicCast* CreateAndLinkPureCastNode(FAutomationTestBase* Test, UEdGraph* Graph, UEdGraphPin* ObjectOutPin, UEdGraphPin* ObjectInPin);

	template<typename T>
	T* CreateNewNode(FAutomationTestBase* Test, UEdGraph* GraphOwner, UEdGraphPin* FromPin, bool bExpectInputWired = true)
	{
		FSMGraphSchemaAction_NewNode AddNodeAction;
		AddNodeAction.GraphNodeTemplate = NewObject<T>();
		T* Result = Cast<T>(AddNodeAction.PerformAction(GraphOwner, FromPin, FVector2D(ForceInitToZero), false));
		Test->TestNotNull("Node should have added", Result);

		// Test connection.
		if (bExpectInputWired)
		{
			Test->TestTrue("Node should be auto-wired", Result->GetInputPin()->LinkedTo.Num() > 0);
		}

		return Result;
	}

	void TestNodeHasGuid(FAutomationTestBase* Test, FSMNode_Base* RuntimeNode);

	/** Checks if a transition is allowed to exist. */
	bool CanTransitionExist(const UEdGraphNode* InGraphNode);
	
#pragma endregion

#pragma region Builder Helpers

	/** Build a single linear state machine. */
	void BuildLinearStateMachine(FAutomationTestBase* Test, USMGraph* StateMachineGraph, int32 NumStates, UEdGraphPin** FromPinInOut, UClass* StateClass = nullptr, UClass* TransitionClass = nullptr, bool bForceTransitionsToTrue = true);

	/** Build a state machine where each row branches from the previous state. */
	void BuildBranchingStateMachine(FAutomationTestBase* Test, USMGraph* StateMachineGraph, int32 Rows, int32 Branches, bool bRunParallel, TArray<UEdGraphPin*>* FromPinsInOut = nullptr, bool bLeaveActive = false,
		bool bReEnterStates = false, bool bEvalIfNextStateActive = true, UClass* StateClass = nullptr, UClass* TransitionClass = nullptr);
	
	/** Build a state machine and assign it to a state machine state node. If FromPinInOut is null the entry state is connected instead. */
	USMGraphNode_StateMachineStateNode* BuildNestedStateMachine(FAutomationTestBase* Test, USMGraph* StateMachineGraph, int32 NumStates, UEdGraphPin** FromPinInOut, UEdGraphPin** NestedPinOut);

	/**
	 * Build a state machine with normal states, references, and more normal states.
	 * @return Total states added.
	 */
	int32 BuildStateMachineWithReferences(FAutomationTestBase* Test, USMGraph* StateMachineGraph, int32 NumStatesBeforeReferences, int32 NumStatesAfterReferences, int32 NumReferences, int32 NumNestedStates,
		TArray<FAssetHandler>& OutCreatedReferenceAssets, TArray<USMGraphNode_StateMachineStateNode*>& OutNestedStateMachineNodes);
	
	/** Thoroughly test a single state machine. Does not include nested tests. */
	USMInstance* TestLinearStateMachine(FAutomationTestBase* Test, USMBlueprint* Blueprint, int32 NumStates, bool bShutdownStateMachine = true);

	/** Run a state machine until it is in an end state. Works with nested state machines. */
	USMInstance* RunStateMachineToCompletion(FAutomationTestBase* Test, USMBlueprint* Blueprint,
		int32& LogicEntryValueOut, int32& LogicUpdateValueOut, int32& LogicEndValueOut, int32 MaxIterations = 1000,
		bool bShutdownStateMachine = true, bool bTestCompletion = true, bool bCompile = true, int32* IterationsRan = nullptr, USMInstance* UseInstance = nullptr);

	/** Recursively run state machines until the end state is reached of each one. Tests retrieving nested active state and retrieving node information.
	 * If the state machine isn't started it will start it. Bind events verifies events are fired but not an accurate count. */
	int32 RunAllStateMachinesToCompletion(FAutomationTestBase* Test, USMInstance* Instance, FSMStateMachine* StateMachine = nullptr, int32 AbortAfterStatesHit=-1, int32 CheckStatesHit = 0, bool bBindEvents = true);

#pragma endregion

#pragma region Logic Helpers

	/** Adds a function on the test context to an execution entry node. */
	UK2Node_CallFunction* AddGenericContextLogicToExecutionEntry(FAutomationTestBase* Test, USMGraphK2Node_RuntimeNode_Base* ExecutionEntry, const FName& ContextFunctionName);
	
	/** Increment an entry int from the context. */
	void AddStateEntryLogic(FAutomationTestBase* Test, USMGraphNode_StateNode* StateNode);

	/** Increment an update int from the context. */
	void AddStateUpdateLogic(FAutomationTestBase* Test, USMGraphNode_StateNode* StateNode);

	/** Increment an end int from the context. */
	void AddStateEndLogic(FAutomationTestBase* Test, USMGraphNode_StateNode* StateNode);

	/** Check if the context allows a transition change. */
	void AddTransitionResultLogic(FAutomationTestBase* Test, USMGraphNode_TransitionEdge* TransitionEdge);
	
	/** Create an event node of type T and wire the execution pin to a new context function call of ContextTestFunction. */
	template<typename T>
	UK2Node_CallFunction* AddEventWithLogic(FAutomationTestBase* Test, USMGraphNode_Base* Node, UFunction* ContextTestFunction)
	{
		UEdGraph* Graph = Node->GetBoundGraph();
		const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());

		T* NewNode = CreateNewNode<T>(Test, Graph, nullptr, false);
		Test->TestNotNull("Expected helper node to be created", NewNode);

		// Add a get context node.
		UEdGraphPin* ContextOutPin = nullptr;
		UK2Node_CallFunction* GetContextNode = CreateContextGetter(Test, Graph, &ContextOutPin);

		// Add a call to execute logic on the context.
		UK2Node_CallFunction* ExecuteNode = CreateFunctionCall(Graph, ContextTestFunction);

		// The logic self pin (make this function a method).
		UEdGraphPin* LogicSelfPin = ExecuteNode->FindPin(TEXT("self"), EGPD_Input);
		Test->TestNotNull("Expected to find ExecuteTargetPin", LogicSelfPin);

		// Convert the context 'object' type out to our context type.
		UK2Node_DynamicCast* CastNode = CreateAndLinkPureCastNode(Test, Graph, ContextOutPin, LogicSelfPin);

		// Now connect end exec out pin to the logic exec in pin.
		Test->TestTrue("Tried to make connection from event node to logic execute node", GraphSchema->TryCreateConnection(NewNode->GetOutputPin(), ExecuteNode->GetExecPin()));

		return ExecuteNode;
	}
	
	/** Adds a helper read node specific to state machines. */
	template<typename T>
	void AddSpecialBooleanTransitionLogic(FAutomationTestBase* Test, USMGraphNode_TransitionEdge* TransitionEdge)
	{
		UEdGraph* Graph = TransitionEdge->GetBoundGraph();
		USMGraphK2Node_TransitionResultNode* Result = CastChecked<USMTransitionGraph>(Graph)->ResultNode;

		T* NewNode = CreateNewNode<T>(Test, Graph, Result->GetInputPin(), false);
		Test->TestNotNull("Expected helper node to be created", NewNode);
	}

	/** Replace the transition graph with the specified logic. */
	template<typename T>
	void OverrideTransitionResultLogic(FAutomationTestBase* Test, USMGraphNode_TransitionEdge* TransitionEdge)
	{
		UEdGraph* TransitionGraph = TransitionEdge->GetBoundGraph();
		TransitionGraph->Nodes.Empty();
		TransitionGraph->GetSchema()->CreateDefaultNodesForGraph(*TransitionGraph);

		TestHelpers::AddSpecialBooleanTransitionLogic<T>(Test, TransitionEdge);
	}

	template<typename T>
	void AddSpecialFloatTransitionLogic(FAutomationTestBase* Test, USMGraphNode_TransitionEdge* TransitionEdge)
	{
		UEdGraph* Graph = TransitionEdge->GetBoundGraph();
		const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());
		USMGraphK2Node_TransitionResultNode* Result = CastChecked<USMTransitionGraph>(Graph)->ResultNode;

		T* NewNode = CreateNewNode<T>(Test, Graph, Result->GetInputPin(), false);
		Test->TestNotNull("Expected helper node to be created", NewNode);

		UEdGraphPin** FloatOutPin = NewNode->Pins.FindByPredicate([&](UEdGraphPin* Pin)
		{
			return Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Real;
		});
		Test->TestNotNull("Expected to find FloatOutPin", FloatOutPin);

		// Add a get context node.
		UEdGraphPin* ContextOutPin = nullptr;
		UK2Node_CallFunction* GetContextNode = CreateContextGetter(Test, Graph, &ContextOutPin);

		// Add a call to read from the context.
		UK2Node_CallFunction* CanTransitionGetter = CreateFunctionCall(Graph, USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, FloatGreaterThan)));

		// Find the float in pin.
		UEdGraphPin** FloatInPin = CanTransitionGetter->Pins.FindByPredicate([&](UEdGraphPin* Pin)
		{
			return Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Real;
		});
		Test->TestNotNull("Expected to find FloatInPin", FloatInPin);

		// Now connect the float out pin to the float in pin.
		Test->TestTrue("Tried to make connection from float out pin to float in pin.", GraphSchema->TryCreateConnection(*FloatOutPin, *FloatInPin));

		// The logic self pin (make this function a method).
		UEdGraphPin* GetterPin = CanTransitionGetter->FindPin(TEXT("self"), EGPD_Input);
		Test->TestNotNull("Expected to find GetterPin", GetterPin);

		// Convert the context 'object' type out to our context type and wire it to the getter.
		UK2Node_DynamicCast* CastNode = CreateAndLinkPureCastNode(Test, Graph, ContextOutPin, GetterPin);

		UEdGraphPin** FoundPin = CanTransitionGetter->Pins.FindByPredicate([&](UEdGraphPin* Pin)
		{
			return Pin->Direction == EGPD_Output;
		});
		Test->TestNotNull("Expected to find Getter out pin", FoundPin);

		// Now connect the getter out pin to the result in pin.
		Test->TestTrue("Tried to make connection from getter node to result node", GraphSchema->TryCreateConnection(*FoundPin, Result->GetInputPin()));

		// Now verify it is possible to transition.
		Test->TestTrue("Transition should read as possible to transition", TransitionEdge->PossibleToTransition());
	}

	/** Sets the node instance class to use and tests it was set and proper variables exposed. */
	void SetNodeClass(FAutomationTestBase* Test, USMGraphNode_Base* Node, TSubclassOf<USMNodeInstance> Class);
	
#pragma endregion

#pragma region K2Node Helpers
	/** Mostly taken directly from K2Node_DynamicCast.h because none of the methods are exported. */

	/** Get the 'valid cast' exec pin */
	UEdGraphPin* GetValidCastPin(UK2Node_DynamicCast* CastNode);

	/** Get the cast result pin */
	UEdGraphPin* GetCastResultPin(UK2Node_DynamicCast* CastNode);

	/** Get the input object to be casted pin */
	UEdGraphPin* GetCastSourcePin(UK2Node_DynamicCast* CastNode);

	/** Looks up the node type from the graph and validates the pin of PinName is wired from Node. */
	template<typename T>
	void VerifyNodeWiredFromPin(FAutomationTestBase* Test, UEdGraph* Graph, FName PinName, FName* RenameTo = nullptr)
	{
		TArray<T*> Nodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested(Graph, Nodes);

		check(Nodes.Num() == 1)

		UEdGraphPin* Pin = Nodes[0]->FindPinChecked(PinName);
		Test->TestEqual("Node local graph wired to instance node", Pin->LinkedTo.Num(), 1);

		if (RenameTo)
		{
			Pin->PinName = *RenameTo;
		}
	}

	template<typename T>
	void TestNodeNotInGraph(FAutomationTestBase* Test, UEdGraph* Graph)
	{
		TArray<T*> Nodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested(Graph, Nodes);

		Test->TestEqual("Node not present", Nodes.Num(), 0);
	}
	
#pragma endregion

#pragma region Misc Helpers

	/** Given array contents return the number that are in the compare array. */
	template<typename T>
	int32 ArrayContentsInArray(const TArray<T>& Contents, const TArray<T>& CompareArray)
	{
		int32 Match = 0;
		for (const T& Obj : Contents)
		{
			if (CompareArray.Contains(Obj))
			{
				Match++;
			}
		}

		return Match;
	}

	// Are two arrays identical?
	template <typename T>
	bool AreContainersIdentical(const TArray<T>& Array1, const TArray<T>& Array2)
	{
		if(Array1.Num() != Array2.Num())
		{
			return false;
		}

		for (int32 Idx = 0; Idx < Array1.Num(); ++Idx)
		{
			if (Array1[Idx] != Array2[Idx])
			{
				return false;
			}
		}

		return true;
	}
	
	// Are two sets identical?
	template <typename T>
	bool AreContainersIdentical(const TSet<T>& Set1, const TSet<T>& Set2)
	{
		return Set1.Num() == Set2.Num() && Set1.Includes(Set2);
	}
	
	// Are two maps identical?
	template <typename K, typename V>
	bool AreContainersIdentical(const TMap<K, V>& Map1, const TMap<K, V>& Map2)
	{
		if(Map1.Num() != Map2.Num())
		{
			return false;
		}

		for(const auto& Pair : Map1)
		{
			if(!Map2.Contains(Pair.Key) || Map2[Pair.Key] != Pair.Value)
			{
				return false;
			}
		}

		return true;
	}

	/** Set string properties of a template to the given value. */
	void TestSetTemplate(FAutomationTestBase* Test, USMInstance* Template, const FString& DefaultStringValue, const FString& NewStringValue);

	/** Duplicates a given node. */
	TSet<UEdGraphNode*> DuplicateNodes(const TArray<UEdGraphNode*>& InNodes);

	/** Validate a state machine has been converted to a reference. */
	void TestStateMachineConvertedToReference(FAutomationTestBase* Test, const USMGraphNode_StateMachineStateNode* StateMachineStateNode);
#pragma endregion


}

#endif