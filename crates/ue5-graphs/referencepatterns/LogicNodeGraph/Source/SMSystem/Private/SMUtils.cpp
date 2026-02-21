// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMUtils.h"

#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "SMCachedPropertyData.h"
#include "SMLogging.h"
#include "SMTransition.h"

#include "Engine/GameEngine.h"
#include "Engine/InputDelegateBinding.h"
#include "Engine/World.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"

USMInstance* USMBlueprintUtils::CreateStateMachineInstance(TSubclassOf<USMInstance> StateMachineClass, UObject* Context, bool bInitializeNow)
{
	return CreateStateMachineInstanceInternal(StateMachineClass, Context, nullptr, bInitializeNow);
}

USMInstance* USMBlueprintUtils::CreateStateMachineInstanceAsync(TSubclassOf<USMInstance> StateMachineClass, UObject* Context, const FOnStateMachineInstanceInitializedAsync& OnCompletedDelegate)
{
	if (USMInstance* Instance = CreateStateMachineInstanceInternal(StateMachineClass, Context, nullptr, false))
	{
		Instance->InitializeAsync(Context, OnCompletedDelegate);
		return Instance;
	}

	return nullptr;
}

USMInstance* USMBlueprintUtils::K2_CreateStateMachineInstance(TSubclassOf<USMInstance> StateMachineClass, UObject* Context, bool bInitializeNow)
{
	return CreateStateMachineInstanceInternal(StateMachineClass, Context, nullptr, bInitializeNow);
}

USMInstance* USMBlueprintUtils::K2_CreateStateMachineInstanceAsync(TSubclassOf<USMInstance> StateMachineClass, UObject* Context, FLatentActionInfo LatentInfo)
{
	if (USMInstance* Instance = CreateStateMachineInstanceInternal(StateMachineClass, Context, nullptr, false))
	{
		Instance->K2_InitializeAsync(Context, LatentInfo);
		return Instance;
	}

	return nullptr;
}

USMInstance* USMBlueprintUtils::K2_CreateStateMachineInstancePure(TSubclassOf<USMInstance> StateMachineClass, UObject* Context, bool bInitializeNow)
{
	return CreateStateMachineInstanceInternal(StateMachineClass, Context, nullptr, bInitializeNow);
}

USMInstance* USMBlueprintUtils::CreateStateMachineInstanceFromTemplate(TSubclassOf<USMInstance> StateMachineClass,
                                                                       UObject* Context, USMInstance* Template, bool bInitializeNow)
{
	return CreateStateMachineInstanceInternal(StateMachineClass, Context, Template, bInitializeNow);
}

USMInstance* USMBlueprintUtils::CreateStateMachineInstanceInternal(TSubclassOf<USMInstance> StateMachineClass,
                                                                   UObject* Context, USMInstance* Template, bool bInitializeNow)
{
	if (StateMachineClass.Get() == nullptr)
	{
		LD_LOG_ERROR(TEXT("No state machine class provided to CreateStateMachineInstance for context: %s"), Context ? *Context->GetName() : TEXT("No Context"));
		return nullptr;
	}

	if (Context == nullptr)
	{
		LD_LOG_ERROR(TEXT("No context provided to CreateStateMachineInstance."));
		return nullptr;
	}

	if (Template && Template->GetClass() != StateMachineClass)
	{
		LD_LOG_ERROR(TEXT("Attempted to instantiate state machine with template of class %s but was expecting: %s. Try restarting the play session."), *Template->GetClass()->GetName(), *StateMachineClass->GetName());
		return nullptr;
	}

	USMInstance* Instance = NewObject<USMInstance>(Context, StateMachineClass, NAME_None, RF_NoFlags, Template);

	if (bInitializeNow)
	{
		Instance->Initialize(Context);
	}

	return Instance;
}

bool USMUtils::GenerateStateMachine(UObject* Instance, FSMStateMachine& StateMachineOut,
	const TSet<FStructProperty*>& RunTimeProperties, bool bForCompile)
{
	GeneratingStateMachines Generation;
	return GenerateStateMachine_Internal(Instance, StateMachineOut, RunTimeProperties, bForCompile, Generation);
}

bool USMUtils::GenerateStateMachine_Internal(UObject* Instance, FSMStateMachine& StateMachineOut, const TSet<FStructProperty*>& RunTimeProperties, bool bForCompile, GeneratingStateMachines& CurrentGeneration)
{
	const bool bIsTopLevel = 0 == CurrentGeneration.CallCount++;
	// If the state machine is a reference instantiate its blueprint and pass our context in.
	if (TSubclassOf<USMInstance> StateMachineClassReference = StateMachineOut.GetClassReference())
	{
		if (USMInstance* SMInstance = Cast<USMInstance>(Instance))
		{
			USMInstance* TemplateInstance = nullptr;

			// Check if we are using a template.
			const FName& TemplateName = StateMachineOut.GetReferencedTemplateName();
			if (TemplateName != NAME_None)
			{
				TemplateInstance = Cast<USMInstance>(FindTemplateFromInstance(SMInstance, TemplateName));
				if (TemplateInstance == nullptr)
				{
					LD_LOG_ERROR(TEXT("Could not find reference template %s for use within state machine %s from package %s. Loading defaults."), *TemplateName.ToString(), *StateMachineOut.GetNodeName(), *Instance->GetName());
				}
				else if (TemplateInstance->GetClass() != StateMachineClassReference)
				{
					/*
					 * This error can occur when setting an sm actor comp state machine class, then switching it to another that uses a reference with a template.
					 * The ReferencedStateMachineClass in the FSM struct will be set to the value of the class that was just placed in the actor component,
					 * but nothing else appears to be out of place. This problem occurs until the sm with the reference is recompiled or the editor restarted.
					 *
					 * Fix for now: The template instance appears to be correct, so use that and log a warning.
					 *
					 * It is unknown what causes it specifically since this happens in the runtime module when setting the actor component class. Somehow
					 * this effects the ReferencedStateMachineClass in the struct owning the sm reference template. The most likely cause would be in the component under
					 * InitInstanceTemplate when CopyPropertiesForUnrelatedObjects is called. But setting BlueprintCompiledGeneratedDefaults on ReferencedStateMachineClass
					 * had no effect.
					 */

					// TODO: This may need to be an info log when a guid cache occurs during compile, and a warning otherwise.

					LD_LOG_INFO(TEXT("State machine node %s in package %s uses a reference template %s with class %s, but was expecting class %s. Automatically switching to the correct class. The package may just need to be recompiled."),
						*StateMachineOut.GetNodeName(), *Instance->GetName(), *TemplateName.ToString(), *TemplateInstance->GetClass()->GetName(), *StateMachineClassReference->GetName());

					StateMachineClassReference = TemplateInstance->GetClass();
				}
			}
			// Check for circular referencing
			{
				// Prevent infinite loop if this machine references itself.
				if (const int32* CurrentInstancesOfClass = CurrentGeneration.InstancesGenerating.Find(StateMachineClassReference))
				{
					// This should never be greater than 1 otherwise it means this state machine class has a reference to itself.
					// If we don't stop here we will be in an infinite loop.
					if (*CurrentInstancesOfClass > 1)
					{
						LD_LOG_ERROR(TEXT("Attempted to generate state machine with circular referencing. This behavior is no longer allowed. Offending state machine: %s"), *SMInstance->GetName())
						FinishStateMachineGeneration(CurrentGeneration, bIsTopLevel);
						return false;
					}
				}
			}

			int32& CurrentInstances = CurrentGeneration.InstancesGenerating.FindOrAdd(StateMachineClassReference);
			CurrentInstances++;

			UClass* ClassToUse = StateMachineClassReference;

			const FName& DynamicVariableName = StateMachineOut.GetDynamicReferenceVariableName();
			if (!DynamicVariableName.IsNone() && !bForCompile)
			{
				FProperty* Property = SMInstance->GetClass()->FindPropertyByName(DynamicVariableName);
				if (Property == nullptr)
				{
					LD_LOG_ERROR(TEXT("Dynamic state machine reference creation failed. Could not find the property %s within state machine %s from package %s."),
						*Property->GetName(), *StateMachineOut.GetNodeName(), *Instance->GetName());
					return false;
				}

				FClassProperty* ClassProperty = CastField<FClassProperty>(Property);
				if (ClassProperty == nullptr)
				{
					LD_LOG_ERROR(TEXT("Dynamic state machine reference creation failed. Property %s is not a class variable! Property in state machine %s from package %s."),
						*Property->GetName(), *StateMachineOut.GetNodeName(), *Instance->GetName());
					return false;
				}

				UClass* PropertyValue = static_cast<UClass*>(ClassProperty->GetPropertyValue_InContainer(SMInstance));
				if (PropertyValue == nullptr)
				{
					LD_LOG_WARNING(TEXT("Dynamic state machine reference creation failed. Property %s value is null. Using default reference. Property in state machine %s from package %s."),
						*Property->GetName(), *StateMachineOut.GetNodeName(), *Instance->GetName());
				}
				else
				{
					ClassToUse = PropertyValue;
				}

				if (ClassToUse == SMInstance->GetClass())
				{
					LD_LOG_ERROR(TEXT("Dynamic state machine reference creation failed. The class %s is the same as the owner class, you can't recursively reference a state machine. Property %s in state machine %s from package %s."),
						*ClassToUse->GetName(), *Property->GetName(), *StateMachineOut.GetNodeName(), *Instance->GetName());
					return false;
				}

				if (TemplateInstance && TemplateInstance->GetClass() != ClassToUse)
				{
					// Template is of the wrong type, which can be expected with dynamic references, but they should preferably be sub classes of the template.
					if (!ClassToUse->IsChildOf(TemplateInstance->GetClass()))
					{
						LD_LOG_INFO(TEXT("Dynamic state machine reference class is not a subclass of the template provided. The template may have missing data. Actual class is %s and expected a subclass of %s. State machine %s from package %s."),
							*ClassToUse->GetName(), *TemplateInstance->GetClass()->GetName(), *StateMachineOut.GetNodeName(), *Instance->GetName());
					}

					USMInstance* NewTemplate = NewObject<USMInstance>(GetTransientPackage(), ClassToUse, NAME_None);
					UEngine::CopyPropertiesForUnrelatedObjects(TemplateInstance, NewTemplate);
					TemplateInstance = NewTemplate;
				}
			}

			// Instantiate reference, or in the case of the client look for a replicated reference.
			const FGuid PathGuid = StateMachineOut.CalculatePathGuidConst();
			check(PathGuid.IsValid());

			USMInstance* ReplicatedReference = SMInstance->FindReplicatedReference(PathGuid);

			USMInstance* Reference = ReplicatedReference ? ReplicatedReference :
			NewObject<USMInstance>(SMInstance, ClassToUse, NAME_None, RF_NoFlags, TemplateInstance);

			if (Reference == nullptr)
			{
				LD_LOG_ERROR(TEXT("Could not create reference %s for use within state machine %s from package %s."), *ClassToUse->GetName(), *StateMachineOut.GetNodeName(), *Instance->GetName());
				return false;
			}

			Reference->SetReferenceOwner(SMInstance);
			if (ReplicatedReference == nullptr && Reference->CanReplicateAsReference())
			{
				SMInstance->AddReplicatedReference(PathGuid, Reference);
			}

			if (!bForCompile)
			{
				Reference->Initialize(SMInstance->GetContext());
			}
#if WITH_EDITORONLY_DATA
			else
			{
				UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(Reference->GetClass());
				check(Blueprint && Blueprint->HasAnyFlags(RF_LoadCompleted));

				if (!Blueprint->bBeingCompiled)
				{
					TSet<FStructProperty*> Properties;
					if (TryGetStateMachinePropertiesForClass(Reference->GetClass(), Properties, Reference->RootStateMachineGuid))
					{
						Reference->GetRootStateMachine().SetNodeGuid(Reference->RootStateMachineGuid);
						GenerateStateMachine(Reference, Reference->GetRootStateMachine(), Properties, bForCompile);
					}
				}
				else
				{
					// This could happen on a conversion from old state machine versions to new versions on load.
					LD_LOG_INFO(TEXT("Reference %s is compiling; skipping guid cache."), *Blueprint->GetName());
				}
			}
#endif
			// Make sure the container node is aware of the state machine node class to use. This is embedded in the reference.
			StateMachineOut.SetNodeInstanceClass(Reference->GetRootStateMachineNodeClass());

			int32* CurrentInstancesAfter = CurrentGeneration.InstancesGenerating.Find(StateMachineClassReference);
			if (ensureAlwaysMsgf(CurrentInstancesAfter, TEXT("The reference class instance %s should be found."), *StateMachineClassReference->GetName()))
			{
				// Should go back to zero but could be more in the event of an attempted self reference.
				(*CurrentInstancesAfter)--;
			}

			// Notify the state machine of the correct instance.
			StateMachineOut.SetInstanceReference(Reference);
			
			FinishStateMachineGeneration(CurrentGeneration, bIsTopLevel);
			return true;
		}
	}

	// Only match properties belonging to this state machine.
	const FGuid& StateMachineNodeGuid = StateMachineOut.GetNodeGuid();

	// Used for quick lookup when linking to states.
	TMap<FGuid, FSMState_Base*> MappedStates;
	TMap<FGuid, FSMTransition*> MappedTransitions;
	// Retrieve pointers to the runtime states and store in state machine for quick access.
	for (auto& Property : RunTimeProperties)
	{	
		if (Property->Struct->IsChildOf(FSMState_Base::StaticStruct()))
		{
			FSMState_Base* State = Property->ContainerPtrToValuePtr<FSMState_Base>(Instance);

			if (State->GetOwnerNodeGuid() != StateMachineNodeGuid)
			{
				continue;
			}

			StateMachineOut.AddState(State);

			/*
			 * Unique GUID check 1:
			 * The NodeGuid at this stage should always be unique and the ensure should never be tripped.
			 * Multiple inheritance parent calls is the only scenario where NodeGuid duplicates could exist but the sm compiler will adjust them.
			 *
			 * If this is triggered please check to make sure the state machine blueprint in question doesn't do anything abnormal such as use circular referencing.
			 */
			ensureMsgf(!MappedStates.Contains(State->GetNodeGuid()), TEXT("State machine generation error for state machine %s: found node %s but its guid %s has already been added."),
				*Instance->GetName(), *State->GetNodeName(), *State->GetNodeGuid().ToString());
			
			MappedStates.Add(State->GetNodeGuid(), State);

			if (Property->Struct->IsChildOf(FSMStateMachine::StaticStruct()))
			{
				FSMStateMachine& NestedStateMachine = *(FSMStateMachine*)State;
				if (!GenerateStateMachine_Internal(Instance, NestedStateMachine, RunTimeProperties, bForCompile, CurrentGeneration))
				{
					FinishStateMachineGeneration(CurrentGeneration, bIsTopLevel);
					return false;
				}
			}

			if (State->IsRootNode())
			{
				StateMachineOut.AddInitialState(State);
			}
		}
	}

	// Second pass build transitions.
	for (auto& Property : RunTimeProperties)
	{
		if (Property->Struct->IsChildOf(FSMTransition::StaticStruct()))
		{
			FSMTransition* Transition = Property->ContainerPtrToValuePtr<FSMTransition>(Instance);

			if (Transition->GetOwnerNodeGuid() != StateMachineNodeGuid)
			{
				continue;
			}

			// Convert linked guids to the actual states.
			FSMState_Base* FromState = MappedStates.FindRef(Transition->FromGuid);
			if (!FromState)
			{
				LD_LOG_ERROR(TEXT("Critical error creating state machine %s for package %s. The transition %s could not locate the FromState using Guid %s."), *StateMachineOut.GetNodeName(), *Instance->GetName(),
					*Transition->GetNodeName(), *Transition->FromGuid.ToString());
				return false;
			}
			FSMState_Base* ToState = MappedStates.FindRef(Transition->ToGuid);
			if (!ToState)
			{
				LD_LOG_ERROR(TEXT("Critical error creating state machine %s for package %s. The transition %s could not locate the ToState using Guid %s."), *StateMachineOut.GetNodeName(), *Instance->GetName(),
					*Transition->GetNodeName(), *Transition->ToGuid.ToString());
				return false;
			}

			// The transition will handle updating the state.
			Transition->SetFromState(FromState);
			Transition->SetToState(ToState);

			StateMachineOut.AddTransition(Transition);
			
			/*
			 * Unique GUID check 1:
			 * The NodeGuid at this stage should always be unique and the ensure should never be tripped.
			 * Multiple inheritance parent calls is the only scenario where NodeGuid duplicates could exist but the sm compiler will adjust them.
			 *
			 * If this is triggered please check to make sure the state machine blueprint in question doesn't do anything abnormal such as use circular referencing.
			 */
			ensureMsgf(!MappedStates.Contains(Transition->GetNodeGuid()), TEXT("State machine generation error for state machine %s: found node %s but its guid %s has already been added."),
				*Instance->GetName(), *Transition->GetNodeName(), *Transition->GetNodeGuid().ToString());
			
			MappedTransitions.Add(Transition->GetNodeGuid(), Transition);
		}
	}

	FinishStateMachineGeneration(CurrentGeneration, bIsTopLevel);
	return true;
}

bool USMUtils::TryGetStateMachinePropertiesForClass(const UClass* Class, TSet<FStructProperty*>& PropertiesOut, FGuid& RootGuid, EFieldIteratorFlags::SuperClassFlags SuperFlags)
{
	// Look for properties in this class.
	for (TFieldIterator<FStructProperty> It(Class, SuperFlags); It; ++It)
	{
		if (It->Struct->IsChildOf(FSMNode_Base::StaticStruct()))
		{
			PropertiesOut.Add(*It);
		}
	}

	// Check parent classes.
	if (PropertiesOut.Num() == 0)
	{
		// Blueprint parent.
		if (const USMBlueprintGeneratedClass* NextClass = Cast<USMBlueprintGeneratedClass>(Class->GetSuperClass()))
		{
			// Need to set the guid -- The child class instance won't know this.
			RootGuid = NextClass->GetRootGuid();
			return TryGetStateMachinePropertiesForClass(NextClass, PropertiesOut, RootGuid, SuperFlags);
		}
	}

	return PropertiesOut.Num() > 0;
}

bool USMUtils::TryGetGraphPropertiesForClass(const UClass* Class, TSet<FProperty*>& PropertiesOut,
	const TSharedPtr<FSMCachedPropertyData, ESPMode::ThreadSafe>& CachedPropertyData)
{
	auto IsVisible = [](const FProperty* Property) -> bool
	{
		// The BP compiler will only add valid graph properties, but it's possible the FSMGraphProperty_Base_Runtime
		// struct could have been manually added to a class and isn't intended to be exposed, such as
		// ExposedPropertyOverrides.

		// This doesn't support the case where a graph property was added directly and is intended to be included,
		// but not processed, at run-time.

#if WITH_EDITORONLY_DATA
		return !Property->HasMetaData(TEXT("HideOnNode"));
#else
		return true;
#endif
	};

	if (const TSet<FProperty*>* ExistingCachedData = CachedPropertyData->FindCachedProperties(Class))
	{
		PropertiesOut = *ExistingCachedData;
	}
	else
	{
		for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			if (FStructProperty* StructProperty = CastField<FStructProperty>(*It))
			{
				if (StructProperty->Struct->IsChildOf(FSMGraphProperty_Base_Runtime::StaticStruct()))
				{
					if (IsVisible(StructProperty))
					{
						PropertiesOut.Add(StructProperty);
					}
				}
			}
			else if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(*It))
			{
				if (FStructProperty* InnerProp = CastField<FStructProperty>(ArrayProp->Inner))
				{
					if (InnerProp->Struct->IsChildOf(FSMGraphProperty_Base_Runtime::StaticStruct()))
					{
						if (IsVisible(ArrayProp))
						{
							PropertiesOut.Add(InnerProp);
						}
					}
				}
			}
		}

		CachedPropertyData->AddCachedProperties(Class, PropertiesOut);
	}

	return PropertiesOut.Num() > 0;
}

void USMUtils::TryGetAllOwners(const FSMNode_Base* Node, TArray<const FSMNode_Base*>& OwnersOrdered, const USMInstance* LimitToInstance)
{
	for (const FSMNode_Base* CurrentNode = Node; CurrentNode; CurrentNode = CurrentNode->GetOwnerNode())
	{
		const USMInstance* Instance = CurrentNode->GetOwningInstance();
		if (LimitToInstance && Instance != LimitToInstance)
		{
			continue;
		}
		
		OwnersOrdered.Add(CurrentNode);
	}

	Algo::Reverse(OwnersOrdered);
}

FString USMUtils::BuildGuidPathFromNodes(const TArray<const FSMNode_Base*>& Nodes, TMap<FString, int32>* MappedPaths)
{
	FString Path;
	for (int32 Idx = 0; Idx < Nodes.Num() - 1; ++Idx)
	{
		const FSMNode_Base* Node = Nodes[Idx];
		Path += FString::Printf(TEXT("%s/"), *Node->GetNodeGuid().ToString());
	}

	if (Nodes.Num())
	{
		Path += Nodes[Nodes.Num() - 1]->GetNodeGuid().ToString();
	}

	// Check for duplicates and adjust.
	if (MappedPaths)
	{
		int32& ExistingPaths = MappedPaths->FindOrAdd(Path);
		if (++ExistingPaths > 1)
		{
			Path += FString::Printf(TEXT("_%s"), *FString::FromInt(ExistingPaths - 1));
		}
	}
	
	return Path;
}

FGuid USMUtils::PathToGuid(const FString& UnhashedPath, FGuid* OutGuid)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("USMUtils::PathToGuid"), STAT_SMUtils_PathToGuid, STATGROUP_LogicDriver);
	FGuid Guid;
	if (!OutGuid)
	{
		OutGuid = &Guid;
	}

	const FString HashedPath = FMD5::HashAnsiString(*UnhashedPath);
	FGuid::Parse(HashedPath, *OutGuid);

	return *OutGuid;
}

UObject* USMUtils::FindTemplateFromInstance(const USMInstance* Instance, const FName& TemplateName)
{
	check(Instance);
	
	UClass* CurrentClass = Instance->GetClass();
	while (CurrentClass)
	{
		if (UObject* FoundTemplate = Cast<UObject>(CurrentClass->GetDefaultSubobjectByName(TemplateName)))
		{
			return FoundTemplate;
		}
		CurrentClass = CurrentClass->GetSuperClass();
	}

	return nullptr;
}

bool USMUtils::TryGetAllReferenceTemplatesFromInstance(const USMInstance* Instance, TSet<USMInstance*>& TemplatesOut, bool bIncludeNested)
{
	for (const TObjectPtr<UObject>& Template : Instance->ReferenceTemplates)
	{
		USMInstance* ReferenceTemplate = Cast<USMInstance>(Template);
		
		if (!ReferenceTemplate)
		{
			continue;
		}
		
		TemplatesOut.Add(ReferenceTemplate);

		if (bIncludeNested)
		{
			TryGetAllReferenceTemplatesFromInstance(ReferenceTemplate, TemplatesOut, bIncludeNested);
		}
	}

	return TemplatesOut.Num() > 0;
}

void USMUtils::EnableInputForObject(APlayerController* InPlayerController, UObject* InObject,
                                    TObjectPtr<UInputComponent>& InOutComponent, int32 InputPriority, bool bBlockInput, bool bPushPopInput)
{
	check(InPlayerController);
	check(InObject);
	
	if (!InOutComponent)
	{
		InOutComponent = NewObject<UInputComponent>(InObject, UInputSettings::GetDefaultInputComponentClass());
		InOutComponent->RegisterComponent();
		InOutComponent->bBlockInput = bBlockInput;
		InOutComponent->Priority = InputPriority;

		UInputDelegateBinding::BindInputDelegates(InObject->GetClass(), InOutComponent, InObject);
	}
	else if (bPushPopInput)
	{
		InPlayerController->PopInputComponent(InOutComponent);
	}

	if (bPushPopInput)
	{
		InPlayerController->PushInputComponent(InOutComponent);
	}
}

void USMUtils::DisableInput(UWorld* InWorld, TObjectPtr<UInputComponent>& InOutComponent)
{
	if (InWorld && InOutComponent)
	{
		for (FConstPlayerControllerIterator PCIt = InWorld->GetPlayerControllerIterator(); PCIt; ++PCIt)
		{
			if (APlayerController* PC = PCIt->Get())
			{
				PC->PopInputComponent(InOutComponent);
			}
		}
	}

	if (InOutComponent)
	{
		InOutComponent->DestroyComponent();
		InOutComponent = nullptr;
	}
}

void USMUtils::HandlePawnControllerChange(APawn* InPawn, AController* InController, UObject* InObject, TObjectPtr<UInputComponent>& InOutComponent, int32 InputPriority, bool bBlockInput)
{
	check(InObject);
	
	if (!InPawn || InPawn != InObject->GetTypedOuter<APawn>())
	{
		return;
	}
	
	DisableInput(InObject->GetWorld(), InOutComponent);
	if (InController)
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(InController))
		{
			EnableInputForObject(PlayerController, InObject, InOutComponent, InputPriority, bBlockInput, false);
		}
	}
}

void USMUtils::ActivateStateNetOrLocal(FSMState_Base* InState, bool bValue, bool bSetAllParents, bool bActivateNow)
{
	if (USMInstance* Instance = InState ? InState->GetOwningInstance() : nullptr)
	{
		// Network.
		if (ISMStateMachineNetworkedInterface* Network = Instance->TryGetNetworkInterface())
		{
			Network->ServerActivateState(InState->GetGuid(), bValue, bSetAllParents, bActivateNow);
			return;
		}

		// Local.
		if (InState)
		{
			Instance->ActivateStateLocally(InState->GetGuid(), bValue, bSetAllParents, bActivateNow);
		}
	}
}

void USMUtils::FinishStateMachineGeneration(GeneratingStateMachines& Generation, bool bTopLevel)
{
	if (bTopLevel)
	{
#if !UE_BUILD_SHIPPING
		for (const auto& InstanceCount : Generation.InstancesGenerating)
		{
			ensureAlwaysMsgf(InstanceCount.Value == 0, TEXT("Ref count is %s when it should be 0. Offending class instance %s."), *FString::FromInt(InstanceCount.Value), *InstanceCount.Key->GetName());
		}
#endif		
		Generation.InstancesGenerating.Empty();
	}
}
