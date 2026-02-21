// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SMExposedFunctions.generated.h"

UENUM()
enum class ESMExposedFunctionExecutionType : uint8
{
	SM_Graph,           // BP Graph eval required
	SM_NodeInstance,    // Node instance only
	SM_None             // No execution
};

UENUM()
enum class ESMConditionalEvaluationType : uint8
{
	SM_Graph,           // BP Graph eval required
	SM_NodeInstance,    // Node instance only 
	SM_AlwaysFalse,     // Never eval graph and never take conditionally
	SM_AlwaysTrue       // Never eval graph and always take conditionally
};

/**
 * Handles execution of functions exposed in blueprint graphs. This is meant to be defined once per class function
 * and then executed for a given object context.
 */
USTRUCT(BlueprintInternalUseOnly)
struct SMSYSTEM_API FSMExposedFunctionHandler
{
	GENERATED_BODY()

	FSMExposedFunctionHandler()
		: BoundFunction(NAME_None)
		  , ExecutionType(), Function(nullptr)
	{
	}

	/** Name of the graph function we will be evaluating. */
	UPROPERTY()
	FName BoundFunction;

	/** The type of execution for this function. */
	UPROPERTY()
	ESMExposedFunctionExecutionType ExecutionType;

	/**
	 * Lookup the UFunction by the BoundFunction name.
	 *
	 * @param InClass The class owning the function.
	 */
	void Initialize(const UClass* InClass);

	/**
	 * Execute the function for a given object.
	 *
	 * @param InObject The object instance to execute the function on.
	 * @param InParams Optional params to provide to the function.
	 */
	void Execute(UObject* InObject, void* InParams = nullptr) const;

#if WITH_EDITOR
	UFunction* GetFunction() const { return Function; }
#endif

private:
	UPROPERTY()
	TObjectPtr<UFunction> Function;
};

/**
 * Contains an array of function handlers. This struct exists so a container can be the value of a map UProperty.
 */
USTRUCT()
struct FSMExposedFunctionContainer
{
	GENERATED_BODY()

	FSMExposedFunctionContainer() = default;
	FSMExposedFunctionContainer(const FSMExposedFunctionHandler& Handler)
	{
		ExposedFunctionHandlers.Add(Handler);
	}

	UPROPERTY()
	TArray<FSMExposedFunctionHandler> ExposedFunctionHandlers;
};

USTRUCT()
struct FSMNode_FunctionHandlers
{
	GENERATED_BODY()

	/** Entry point to when a node is first initialized. */
	UPROPERTY()
	TArray<FSMExposedFunctionHandler> NodeInitializedGraphEvaluators;

	/** Entry point to when a node is shutdown. */
	UPROPERTY()
	TArray<FSMExposedFunctionHandler> NodeShutdownGraphEvaluators;

	/** When the owning blueprint's root state machine starts. */
	UPROPERTY()
	TArray<FSMExposedFunctionHandler> OnRootStateMachineStartedGraphEvaluator;

	/** When the owning blueprint's root state machine stops. */
	UPROPERTY()
	TArray<FSMExposedFunctionHandler> OnRootStateMachineStoppedGraphEvaluator;

	/** A pointer back to the owning exposed node functions. */
	struct FSMExposedNodeFunctions* ExposedFunctionsOwner = nullptr;
};

USTRUCT()
struct FSMState_FunctionHandlers : public FSMNode_FunctionHandlers
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FSMExposedFunctionHandler> BeginStateGraphEvaluator;

	UPROPERTY()
	TArray<FSMExposedFunctionHandler> UpdateStateGraphEvaluator;

	UPROPERTY()
	TArray<FSMExposedFunctionHandler> EndStateGraphEvaluator;
};

USTRUCT()
struct FSMConduit_FunctionHandlers : public FSMNode_FunctionHandlers
{
	GENERATED_BODY()

	/** Primary conduit evaluation. */
	UPROPERTY()
	TArray<FSMExposedFunctionHandler> CanEnterConduitGraphEvaluator;

	/** Entry point when the conduit is entered. */
	UPROPERTY()
	TArray<FSMExposedFunctionHandler> ConduitEnteredGraphEvaluator;
};

USTRUCT()
struct FSMTransition_FunctionHandlers : public FSMNode_FunctionHandlers
{
	GENERATED_BODY()

	/** Primary transition evaluation. */
	UPROPERTY()
	TArray<FSMExposedFunctionHandler> CanEnterTransitionGraphEvaluator;

	/** Entry point to when a transition is taken. */
	UPROPERTY()
	TArray<FSMExposedFunctionHandler> TransitionEnteredGraphEvaluator;

	/** Entry point to before a transition evaluates. */
	UPROPERTY()
	TArray<FSMExposedFunctionHandler> TransitionPreEvaluateGraphEvaluator;

	/** Entry point to after a transition evaluates. */
	UPROPERTY()
	TArray<FSMExposedFunctionHandler> TransitionPostEvaluateGraphEvaluator;
};

/**
 * Contains defined node function handlers and graph property function handlers.
 */
USTRUCT()
struct FSMExposedNodeFunctions
{
	GENERATED_BODY()

	/** State and State Machine function handlers. */
	UPROPERTY()
	TArray<FSMState_FunctionHandlers> FSMState_FunctionHandlers;

	UPROPERTY()
	TArray<FSMConduit_FunctionHandlers> FSMConduit_FunctionHandlers;

	UPROPERTY()
	TArray<FSMTransition_FunctionHandlers> FSMTransition_FunctionHandlers;

public:
	/** A property guid mapped to the exposed function container. */
	UPROPERTY()
	TMap<FGuid, FSMExposedFunctionContainer> GraphPropertyFunctionHandlers;

	/** Return a flattened array of all Node Function handlers. This can be slow. */
	SMSYSTEM_API TArray<FSMExposedFunctionHandler*> GetFlattedArrayOfAllNodeFunctionHandlers();

	/** Get or retrieve the first element of the node function based on the struct type. */
	SMSYSTEM_API FSMNode_FunctionHandlers* GetOrAddInitialElement(const UScriptStruct* InStructType);

	/** Locate graph property exposed function from GraphPropertyFunctionHandlers. */
	FORCEINLINE TArray<FSMExposedFunctionHandler>* FindExposedGraphPropertyFunctionHandler(const FGuid& InGraphPropertyGuid)
	{
		if (ensure(InGraphPropertyGuid.IsValid()))
		{
			if (FSMExposedFunctionContainer* Handler = GraphPropertyFunctionHandlers.Find(
				InGraphPropertyGuid))
			{
				return &Handler->ExposedFunctionHandlers;
			}
		}

		return nullptr;
	}
};