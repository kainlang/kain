// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMInstance.h"

#include "GameFramework/Pawn.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "SMUtils.generated.h"

/**
 * General Blueprint helpers for creating state machines.
 */
UCLASS()
class SMSYSTEM_API USMBlueprintUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Create a new state machine instance initialized with the given context.
	 * The state machine is instantiated with the Context as the outer object.
	 * 
	 * @param StateMachineClass The class of the state machine to be instantiated.
	 * @param Context The context object this state machine will run for. Often an actor.
	 * @param bInitializeNow If the Initialize method should be called. Initialize must be called before the state machine can be started.
	 *
	 * @return The state machine instance created.
	 */
	static USMInstance* CreateStateMachineInstance(TSubclassOf<USMInstance> StateMachineClass, UObject* Context, bool bInitializeNow = true);

	/**
	 * Create and initialize a new state machine instance with the given context.
	 * The state machine is instantiated with the Context as the outer object.
	 * Initialize is automatically run on a separate thread.
	 * 
	 * @param StateMachineClass The class of the state machine to be instantiated.
	 * @param Context The context object this state machine will run for. Often an actor.
	 * @param OnCompletedDelegate Delegate to assign which is called when the process completes.
	 *
	 * @return The state machine instance created. It will not be initialized at the time of function return.
	 */
	static USMInstance* CreateStateMachineInstanceAsync(TSubclassOf<USMInstance> StateMachineClass, UObject* Context, const FOnStateMachineInstanceInitializedAsync& OnCompletedDelegate);
	
	/**
	 * Create a new state machine instance initialized with the given context.
	 * The state machine is instantiated with the Context as the outer object.
	 * 
	 * @param StateMachineClass The class of the state machine to be instantiated.
	 * @param Context The context object this state machine will run for. Often an actor.
	 * @param bInitializeNow If the Initialize method should be called. Initialize must be called before the state machine can be started.
	 *
	 * @return The state machine instance created.
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Create State Machine Instance"), Category = "Logic Driver|State Machine Utilities")
	static USMInstance* K2_CreateStateMachineInstance(TSubclassOf<USMInstance> StateMachineClass, UObject* Context, bool bInitializeNow = true);

	/**
	 * Create and initialize a new state machine instance with the given context.
	 * The state machine is instantiated with the Context as the outer object.
	 * Initialize is automatically run on a separate thread.
	 * 
	 * @param StateMachineClass The class of the state machine to be instantiated.
	 * @param Context The context object this state machine will run for. Often an actor.
	 *
	 * @return The state machine instance created.
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Create State Machine Instance Async", Latent, LatentInfo="LatentInfo"), Category = "Logic Driver|State Machine Utilities")
	static USMInstance* K2_CreateStateMachineInstanceAsync(TSubclassOf<USMInstance> StateMachineClass, UObject* Context, FLatentActionInfo LatentInfo);

	/**
	 * Create a new state machine instance initialized with the given context.
	 * The state machine is instantiated with the Context as the outer object.
	 *
	 * This is a legacy pure node and the execution version `Create State Machine Instance` should be used instead.
	 * @warning: Every pin you split from here will create and initialize a new instance!
	 * 
	 * @param StateMachineClass The class of the state machine to be instantiated.
	 * @param Context The context object this state machine will run for. Often an actor.
	 * @param bInitializeNow If the Initialize method should be called. Initialize must be called before the state machine can be started.
	 *
	 * @return The state machine instance created.
	 */
	UE_DEPRECATED(5.1, "This function is deprecated. Use the execution version of `CreateStateMachineInstance` instead.")
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DisplayName = "Create State Machine Instance Pure"), Category = "Logic Driver|State Machine Utilities|Deprecated", meta = (DeprecatedFunction, DeprecationMessage = "Use the execution version of `Create State Machine Instance` instead."))
	static USMInstance* K2_CreateStateMachineInstancePure(TSubclassOf<USMInstance> StateMachineClass, UObject* Context, bool bInitializeNow = true);

	/**
	 * Create a new state machine instance from a template initialized with the given context.
	 * The state machine is instantiated with the Context as the outer object.
	 *
	 * @warning: Every pin you split from here will create and initialize a new instance!
	 * @todo: This function is intended to be deprecated and instead rely on the engine's NewObject<> for templates, but there isn't a good replacement for existing BP usage. 
	 *
	 * @param StateMachineClass The class of the state machine to be instantiated.
	 * @param Context The context object this state machine will run for. Often an actor.
	 * @param Template A template archetype for setting initial property values of the state machine instance.
	 * @param bInitializeNow If the Initialize method should be called. Initialize must be called before the state machine can be started.
	 *
	 * @return The state machine instance created.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Logic Driver|State Machine Utilities|Deprecated")
	static USMInstance* CreateStateMachineInstanceFromTemplate(TSubclassOf<USMInstance> StateMachineClass, UObject* Context, USMInstance* Template, bool bInitializeNow = true);
	
private:
	static USMInstance* CreateStateMachineInstanceInternal(TSubclassOf<USMInstance> StateMachineClass, UObject* Context, USMInstance* Template, bool bInitializeNow);
};

/**
 * Internal helpers around utilizing state machine instances.
 */
class SMSYSTEM_API USMUtils
{
private:
	struct GeneratingStateMachines
	{
		TMap<const TSubclassOf<USMInstance>, int32> InstancesGenerating;
		uint32 CallCount = 0;
	};

	/** Returns true if the state machine has completely finished generation. Can only be true when called from the top of the stack. */
	static void FinishStateMachineGeneration(GeneratingStateMachines& Generation, bool bTopLevel);
	
public:
	/**
	 * Compiles a state machine from an object. (Should be SMInstance). Guid needs to be set correctly prior to calling.
	 *
	 * @param Instance The object containing instance data. Should be an USMInstance.
	 * @param StateMachineOut The state machine struct which will be assembled.
	 * @param RunTimeProperties Class properties which will be used to create the state machine.
	 * @param bForCompile Prevents the full initialize sequence from running on references and used to gather information for compile.
	 */
	static bool GenerateStateMachine(UObject* Instance, FSMStateMachine& StateMachineOut, const TSet<FStructProperty*>& RunTimeProperties, bool bForCompile = false);

private:
	static bool GenerateStateMachine_Internal(UObject* Instance, FSMStateMachine& StateMachineOut, const TSet<FStructProperty*>& RunTimeProperties, bool bForCompile, GeneratingStateMachines& CurrentGeneration);
	
public:
	/** Locate the properties required for a state machine looking backwards up the parent classes. */
	static bool TryGetStateMachinePropertiesForClass(const UClass* Class, TSet<FStructProperty*>& PropertiesOut, FGuid& RootGuid, EFieldIteratorFlags::SuperClassFlags SuperFlags = EFieldIteratorFlags::ExcludeSuper);

	/** Locate any Graph Properties for a given class. */
	static bool TryGetGraphPropertiesForClass(const UClass* Class, TSet<FProperty*>& PropertiesOut, const TSharedPtr<FSMCachedPropertyData, ESPMode::ThreadSafe>& CachedPropertyData);

	/** Look up all node owners. Results will be ordered oldest to newest with the given Node as the last entry. */
	static void TryGetAllOwners(const FSMNode_Base* Node, TArray<const FSMNode_Base*>& OwnersOrdered, const USMInstance* LimitToInstance = nullptr);

	/** Construct a path of guids from the nodes. */
	static FString BuildGuidPathFromNodes(const TArray<const FSMNode_Base*>& Nodes, TMap<FString, int32>* MappedPaths = nullptr);
	
	/** Convert an unhashed path to a hashed guid path. */
	static FGuid PathToGuid(const FString& UnhashedPath, FGuid* OutGuid = nullptr);
	
	/** Search up parents for a default sub objects for a template. */
	static UObject* FindTemplateFromInstance(const USMInstance* Instance, const FName& TemplateName);

	/** Find all reference templates from an instance. Nested children shouldn't be found after a compile or during run-time! */
	static bool TryGetAllReferenceTemplatesFromInstance(const USMInstance* Instance, TSet<USMInstance*>& TemplatesOut, bool bIncludeNested = false);

	/** Attempt to find a controller of type T from the context object. */
	template<typename T>
	static T* FindControllerFromContext(UObject* InContextObject)
	{
		if (InContextObject == nullptr)
		{
			return nullptr;
		}
	
		if (T* PlayerController = Cast<T>(InContextObject))
		{
			return PlayerController;
		}
	
		if (const APawn* Pawn = Cast<APawn>(InContextObject))
		{
			return Cast<T>(Pawn->GetController());
		}

		if (const APawn* Pawn = InContextObject->GetTypedOuter<APawn>())
		{
			return Cast<T>(Pawn->GetController());
		}

		return InContextObject->GetTypedOuter<T>();
	}

	/** Create an input component for an object if necessary and register with a player controller. */
	static void EnableInputForObject(APlayerController* InPlayerController, UObject* InObject, TObjectPtr<UInputComponent>& InOutComponent, int32 InputPriority, bool bBlockInput, bool bPushPopInput);

	/** Disable input for all player controllers using this input component. */
	static void DisableInput(UWorld* InWorld, TObjectPtr<UInputComponent>& InOutComponent);
	
	/** Call when a controller has changed for a tracked pawn. */
	static void HandlePawnControllerChange(APawn* InPawn, AController* InController, UObject* InObject, TObjectPtr<UInputComponent>& InOutComponent, int32 InputPriority, bool bBlockInput);

	/** Change the active state of a state machine instance, handling replication or local. */
	static void ActivateStateNetOrLocal(FSMState_Base* InState, bool bValue, bool bSetAllParents = false, bool bActivateNow = true);

	/** Iterate properties of an instance finding all structs derived from the given type (such as FSMNode_Base). */
	template<typename T>
	static bool TryGetAllRuntimeNodesFromInstance(USMInstance* Instance, TSet<T*>& NodesOut)
	{
		TSet<FStructProperty*> Properties;
		FGuid RootGuid;
		TryGetStateMachinePropertiesForClass(Instance->GetClass(), Properties, RootGuid, EFieldIteratorFlags::IncludeSuper);

		for (FStructProperty* Property : Properties)
		{
			if (Property->Struct->IsChildOf(T::StaticStruct()))
			{
				if (T* StateMachinePtr = Property->ContainerPtrToValuePtr<T>(Instance))
				{
					NodesOut.Add(StateMachinePtr);
				}
			}
		}

		return NodesOut.Num() > 0;
	}

	template<typename T>
	static void BlueprintPropertyToNativeProperty(FProperty* Property, UObject* Scope, TArray<T>& OutNativeProperties)
	{
		TArray<T*> PtrArray;

		BlueprintPropertyToNativeProperty(Property, Scope, PtrArray);
		for (T* Ptr : PtrArray)
		{
			OutNativeProperties.Add(*Ptr);
		}
	}
	
	template<typename T>
	static void BlueprintPropertyToNativeProperty(FProperty* Property, UObject* Scope, TArray<T*>& OutNativeProperties)
	{
		// Check if this is an array or if this is an element in an array.
		const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property);
		if (!ArrayProp)
		{
			if (FField* OwnerField = Property->GetOwnerProperty())
			{
				ArrayProp = CastField<FArrayProperty>(OwnerField);
			}
		}
		
		if (ArrayProp)
		{
			// Blueprint array.
			FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<uint8>(Scope));
			
			const int32 ArrayCount = Helper.Num();
			OutNativeProperties.Reserve(ArrayCount);
			
			for (int32 i = 0; i < ArrayCount; ++i)
			{
				if (uint8** ObjectPtr = (uint8**)Helper.GetRawPtr(i))
				{
					OutNativeProperties.Add((T*)*&ObjectPtr);
				}
			}
		}
		else if (Property->ArrayDim > 1)
		{
			const int32 ArrayCount = Property->ArrayDim;
			OutNativeProperties.Reserve(ArrayCount);
			
			// Native array.
			for (int32 i = 0; i < ArrayCount; ++i)
			{
				if (uint8** ResolvedObject = Property->ContainerPtrToValuePtr<uint8*>(Scope, i))
				{
					OutNativeProperties.Add((T*)*&ResolvedObject);
				}
			}
		}
		else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			// Regular property.
			if (StructProperty->Struct->IsChildOf(T::StaticStruct()))
			{
				if (T* Object = StructProperty->ContainerPtrToValuePtr<T>(Scope))
				{
					OutNativeProperties.Add(Object);
				}
			}
		}
	}

	/**
	 * Insert an element into the array if the index is valid, otherwise add to the end.
	 * @return The index the element was inserted to.
	 */
	template<typename T>
	static int32 InsertOrAddToArray(TArray<T>& InArray, const T& InObject, int32 InIndex)
	{
		const bool bUsingValidIndex = InIndex >= 0 && InIndex < InArray.Num();
		if (bUsingValidIndex)
		{
			InArray.Insert(InObject, InIndex);
			return InIndex;
		}

		InArray.Add(InObject);

		return InArray.Num() - 1;
	}

	/**
	 * Remove an element from the array if the index is valid, otherwise remove from the end.
	 */
	template<typename T>
	static void RemoveAtOrPopFromArray(TArray<T>& InArray, int32 InIndex)
	{
		if (InIndex >= 0 && InIndex < InArray.Num())
		{
			InArray.RemoveAt(InIndex);
		}
		else if (InArray.Num() > 0)
		{
			InArray.Pop();
		}
	}
};
