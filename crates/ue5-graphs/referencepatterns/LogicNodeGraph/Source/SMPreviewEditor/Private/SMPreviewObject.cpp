// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMPreviewObject.h"

#include "Utilities/SMPreviewUtils.h"

#include "Utilities/SMPropertyUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "SMInstance.h"
#include "SMUtils.h"

#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "ScopedTransaction.h"
#include "UnrealEdGlobals.h"

#include "Engine/GameEngine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#define LOCTEXT_NAMESPACE "SMPreviewObject"

#define LD_PREVIEW_OBJ_VERSION 100000

/**
 * Save object properties as strings. Skips component serialization for now. Maybe use FObjectWriter to implement?
 */
struct FObjectComponentAndNameAsStringProxyArchive : public FObjectAndNameAsStringProxyArchive
{
	FObjectComponentAndNameAsStringProxyArchive(FArchive& InInnerArchive, UObject* InOuter, bool bInLoadIfFindFails)
		: FObjectAndNameAsStringProxyArchive(InInnerArchive, bInLoadIfFindFails), Version(0)
	{
		OuterOwner = InOuter;
	}

	virtual FArchive& operator<<(UObject*& Obj) override
	{
		uint8 bIsComponent = 0;
		
		if (IsSaving())
		{
			const UActorComponent* ActorComponent = Cast<UActorComponent>(Obj);
			if (ActorComponent || Obj == nullptr /* No point in serializing null objects; prevent warnings in UE 5.1 */)
			{
				bIsComponent = 1;
			}

			InnerArchive << bIsComponent;

			if (bIsComponent)
			{
				return *this;
			}
		}
		else if (IsLoading())
		{
			InnerArchive << bIsComponent;
			if (bIsComponent)
			{
				return *this;
			}
		}
		
		return FObjectAndNameAsStringProxyArchive::operator<<(Obj);
	}

	/**
	 * Track file version in case we modify it in the future.
	 * May also want to use the built in SetCustomVersion.
	 */
	void SetOurCurrentVersion(int32 InVersion)
	{
		ensure(InVersion == LD_PREVIEW_OBJ_VERSION);
		Version = InVersion;
	}
private:
	UObject* OuterOwner;
	int32 Version;
};


FSMPreviewObjectSpawner::FSMPreviewObjectSpawner(): Location(0.f), Rotation(0.f), Scale(1.f), bIsContext(false),
                                                    SpawnedActor(nullptr), ActorTemplate(nullptr)
{
}

FSMPreviewObjectSpawner::~FSMPreviewObjectSpawner()
{
}

void FSMPreviewObjectSpawner::SaveActorDefaults(UObject* Outer, bool bModify)
{
	check(Outer);
	
	if (SpawnedActor)
	{
		FMemoryWriter Ar(SavedActorProperties, true);
		FObjectComponentAndNameAsStringProxyArchive StringAr(Ar, SpawnedActor, true);

		int32 CurrentVersion = LD_PREVIEW_OBJ_VERSION;
		StringAr << CurrentVersion;

		StringAr.SetOurCurrentVersion(CurrentVersion);
		
		SpawnedActor->Serialize(StringAr);
	
		// Save generic properties for respawning.
		{
			Location = SpawnedActor->GetActorLocation();
			Rotation = SpawnedActor->GetActorRotation();
			Scale = SpawnedActor->GetActorScale();
			ObjectLabel = SpawnedActor->GetActorLabel();
		}

		if (bModify)
		{
			Outer->MarkPackageDirty();
		}
	}
}

void FSMPreviewObjectSpawner::LoadActorDefaults(UObject* Outer)
{
	check(Outer);

	if (SavedActorProperties.Num() > 0 && Class.Get() != nullptr)
	{
		ActorTemplate = NewObject<AActor>(Outer, Class);
		
		FMemoryReader Ar(SavedActorProperties);
		FObjectComponentAndNameAsStringProxyArchive StringAr(Ar, ActorTemplate, true);

		int32 SavedVersion;
		StringAr << SavedVersion;
		StringAr.SetOurCurrentVersion(SavedVersion);
		
		ActorTemplate->Serialize(StringAr);
		check(ActorTemplate->GetOuter() == Outer);
	}
}

void USMPreviewGameInstance::SetWorldContext(FWorldContext* InContext)
{
	WorldContext = InContext;
}

USMPreviewObject::USMPreviewObject() : CachedContextActor(nullptr), bPossessPawnContext(false), PreviewWorld(nullptr), bSpawningActor(false), bDontModify(false), bIsSaving(false)
{
	SetFlags(RF_Transactional);
}

USMPreviewObject::~USMPreviewObject()
{
	if (GEngine && OnWorldDestroyedHandle.IsValid())
	{
		GEngine->OnWorldDestroyed().Remove(OnWorldDestroyedHandle);
	}

	if (PieStartedHandle.IsValid())
	{
		FEditorDelegates::PreBeginPIE.Remove(PieStartedHandle);
	}

	ReleaseActorHandles();
}

void USMPreviewObject::Serialize(FArchive& Ar)
{
	bIsSaving = Ar.IsSaving();

	if (bIsSaving)
	{
		bDontModify = true;
		SaveAllActorReferences();
		bDontModify = false;
	}
	
	Super::Serialize(Ar);

	bIsSaving = false;
}

void USMPreviewObject::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent); // Calls PostEditChangeProperty which broadcasts our change.
	
	if (FProperty* HeadProperty = PropertyChangedEvent.PropertyChain.GetHead()->GetValue())
	{
		// The direct property on this preview object that changed.
		const FName DirectPropertyName = HeadProperty->GetFName();

		if (DirectPropertyName == GET_MEMBER_NAME_CHECKED(USMPreviewObject, GameMode))
		{
			UpdateGameMode();
		}
	}

	SaveAllActorReferences();
	RestoreAllActorReferences();

	OnPreviewObjectChangedEvent.Broadcast(this);
}

void USMPreviewObject::PostEditUndo()
{
	Super::PostEditUndo();

	if (PreviewWorld && PreviewWorld->PersistentLevel)
	{
		/*
		 * Fix actors that should be deleted coming back on an undo when the undo was initiated
		 * during the simulation. PreviewStateMachineActor is the culprit, but force clearing
		 * transactional flags doesn't solve the problem and causes another crash when retrieving
		 * the cloned actor under start simulation after an undo.
		 */

		auto IsActorDeleted = [](AActor* Actor)
		{
			return Actor && Actor->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed);
		};
		
		TSet<AActor*> ActorsToRemove;
		for (AActor* Actor : PreviewWorld->PersistentLevel->Actors)
		{
			if (IsActorDeleted(Actor))
			{
				ActorsToRemove.Add(Actor);
			}
		}

		for (AActor* Actor : PreviewWorld->PersistentLevel->ActorsForGC)
		{
			if (IsActorDeleted(Actor))
			{
				ActorsToRemove.Add(Actor);
			}
		}

		for (AActor* ActorToDelete : ActorsToRemove)
		{
			PreviewWorld->PersistentLevel->Actors.Remove(ActorToDelete);
			PreviewWorld->PersistentLevel->ActorsForGC.Remove(ActorToDelete);
		}
	}
	
	OnPreviewObjectChangedEvent.Broadcast(this);
}

void USMPreviewObject::OnWorldDestroyed(UWorld* World)
{
	if (World == PreviewWorld)
	{
		if (UPackage* Package = GetPackage())
		{
			if (Package->IsDirty())
			{
				SaveAllActorReferences();
			}
		}
		DestroyAllActors();
	}
	
	if (PreviewStateMachineActor && PreviewStateMachineActor->GetWorld() == World)
	{
		// Null out the actor indicating that it should be respawned.
		PreviewStateMachineActor = nullptr;
	}
}

USMInstance* USMPreviewObject::InitializeStateMachine(UObject* InContext)
{
	UWorld* World = InContext->GetWorld();
	
	PreviewStateMachineInstance = USMBlueprintUtils::CreateStateMachineInstanceFromTemplate(StateMachineTemplate->GetClass(), InContext, StateMachineTemplate);
	PreviewStateMachineInstance->SetTickBeforeBeginPlay(true);

	if (World && !PreviewStateMachineActor)
	{
		PreviewStateMachineActor = CastChecked<ASMPreviewStateMachineActor>(World->SpawnActor(ASMPreviewStateMachineActor::StaticClass()));
		PreviewStateMachineActor->ClearFlags(RF_Public);
	}

	if (World && PreviewStateMachineActor)
	{
		PreviewStateMachineActor->StateMachineInstance = PreviewStateMachineInstance;
	}
	
	return PreviewStateMachineInstance;
}

void USMPreviewObject::ShutdownStateMachine()
{
	if (PreviewStateMachineActor)
	{
		PreviewStateMachineActor->ConditionalBeginDestroy();
		PreviewStateMachineActor = nullptr;
	}
	
	if (PreviewStateMachineInstance && PreviewStateMachineInstance->IsInitialized())
	{
		PreviewStateMachineInstance->Shutdown();
	}

	if (SimulatedStateMachineInstance && SimulatedStateMachineInstance->IsInitialized())
	{
		SimulatedStateMachineInstance->Shutdown();
	}
	
	PreviewStateMachineInstance = nullptr;
	SimulatedStateMachineInstance = nullptr;
}

void USMPreviewObject::SetFromBlueprint(UBlueprint* Blueprint)
{
	ShutdownStateMachine();
	PreviewStateMachineActor = nullptr;
	
	if (Blueprint && Blueprint->GeneratedClass && !Blueprint->GeneratedClass->HasAnyClassFlags(CLASS_Abstract))
	{
		USMInstance* DefaultObject = CastChecked<USMInstance>(Blueprint->GeneratedClass->GetDefaultObject());
		const bool bHasClassChanged = !StateMachineTemplate || DefaultObject->GetClass() != StateMachineTemplate->GetClass();
		
		if (bHasClassChanged)
		{
			USMInstance* OldTemplate = StateMachineTemplate;
			StateMachineTemplate = NewObject<USMInstance>(this, DefaultObject->GetClass());

			if (OldTemplate)
			{
				UEngine::CopyPropertiesForUnrelatedObjects(OldTemplate, StateMachineTemplate);
			}
		}
	}
}

void USMPreviewObject::SetPreviewWorld(UWorld* InWorld, const bool bModify)
{
	PreviewWorld = InWorld;

	if (GEngine && !OnWorldDestroyedHandle.IsValid())
	{
		OnWorldDestroyedHandle = GEngine->OnWorldDestroyed().AddUObject(this, &USMPreviewObject::OnWorldDestroyed);
	}

	bDontModify = !bModify;
	
	DestroyAllActors();
	SpawnAllActors();
	RestoreAllActorReferences();
	UpdateGameMode();

	SetCurrentWorld(InWorld);

	bDontModify = false;
}

void USMPreviewObject::SetCurrentWorld(UWorld* InWorld)
{
	if (CurrentWorld != InWorld)
	{
		CurrentWorld = InWorld;
		OnCurrentWorldChangedEvent.Broadcast(InWorld);
	}
}

void USMPreviewObject::UpdateGameMode()
{
	OnWorldRefreshRequiredEvent.Broadcast(this);
}

void USMPreviewObject::SpawnAllActors()
{
	if (!PreviewWorld)
	{
		return;
	}
	
	for (FSMPreviewObjectSpawner& PreviewSpawner : PreviewObjects)
	{
		SpawnActorForWorld(PreviewSpawner);
	}

	BuildActorMap();
}

void USMPreviewObject::DestroyAllActors()
{
	// Iterate our spawned actors and not the preview spawners.
	// Preview spawners actor reference may have been nulled out by a reset to default.
	TArray<AActor*> AllActors = SpawnedActors;
	for (AActor* Actor : AllActors)
	{
		DestroyActor(Actor);
	}

	for (FSMPreviewObjectSpawner& PreviewSpawner : PreviewObjects)
	{
		PreviewSpawner.SpawnedActor = nullptr;
	}
}

void USMPreviewObject::RefreshPreviewWorldActors()
{
	for (AActor* Actor : SpawnedActors)
	{
		// Transient can be added after a package is saved, probably because the owning world is in the transient package.
		// This needs to be cleared or the actors won't show up in the world outliner or copied to the simulation world.
		if (Actor)
		{
			Actor->ClearFlags(RF_Transient);
		}
	}
}

bool USMPreviewObject::ContainsActor(AActor* CompareActor) const
{
	if (UWorld* WorldToCheck = GetCurrentWorld())
	{
		if (FSMPreviewUtils::DoesWorldContainActor(WorldToCheck, CompareActor))
		{
			return true;
		}
	}
	
	return SpawnedActors.Contains(CompareActor);
}

void USMPreviewObject::SaveAllActorReferences()
{
	UWorld* CurrentPreviewWorld = GetPreviewWorld();
	if (!IsValid(CurrentPreviewWorld) || CurrentPreviewWorld->IsUnreachable() || CurrentPreviewWorld->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
	{
		// Nothing to save, likely this is saving after the editor has closed.
		// Don't continue to avoid wiping out saved actor property names.
		return;
	}
	
	ActorPropertyToActorName.Empty();

	GetAllActorReferences(StateMachineTemplate, ActorPropertyToActorName);
	for (FSMPreviewObjectSpawner& PreviewSpawner : PreviewObjects)
	{
		PreviewSpawner.SaveActorDefaults(this, !bDontModify);
		GetAllActorReferences(PreviewSpawner.SpawnedActor, ActorPropertyToActorName);
	}
}

void USMPreviewObject::RestoreAllActorReferences()
{
	if (PreviewWorld)
	{
		RestoreActorReferences(StateMachineTemplate, PreviewWorld->PersistentLevel, ActorPropertyToActorName);
		for (FSMPreviewObjectSpawner& PreviewSpawner : PreviewObjects)
		{
			RestoreActorReferences(PreviewSpawner.SpawnedActor, PreviewWorld->PersistentLevel, ActorPropertyToActorName);
		}
	}
}

bool USMPreviewObject::IsSimulationRunning() const
{
	return SimulatedStateMachineInstance != nullptr;
}

void USMPreviewObject::SetSimulatedStateMachineInstance(USMInstance* InInstance)
{
	SimulatedStateMachineInstance = InInstance;
}

void USMPreviewObject::SetContextActor(AActor* InActor)
{
	if (IsSimulationRunning())
	{
		return;
	}
	
	FScopedTransaction Transaction(TEXT(""), NSLOCTEXT("LogicDriverPreview", "SetPreviewContext", "Set Preview Context"), this);
	SetFlags(RF_Transactional);

	if (!bDontModify)
	{
		Modify();
	}
	
	if (InActor)
	{
		ContextName = InActor->GetFName();
	}
	else
	{
		ContextName = NAME_None;
	}

	CachedContextActor = InActor;

	for (FSMPreviewObjectSpawner& PreviewObject : PreviewObjects)
	{
		// Update context status.
		if (InActor && PreviewObject.SpawnedActor == InActor)
		{
			PreviewObject.bIsContext = true;
		}
		else
		{
			PreviewObject.bIsContext = false;
		}
	}
}

AActor* USMPreviewObject::GetContextActor() const
{
	if (ContextName.IsNone() || !GetCurrentWorld() || !GetCurrentWorld()->GetCurrentLevel())
	{
		return nullptr;
	}

	return Cast<AActor>(StaticFindObjectFast(AActor::StaticClass(), GetCurrentWorld()->GetCurrentLevel(), ContextName));
}

void USMPreviewObject::AddPreviewActor(FSMPreviewObjectSpawner& NewPreviewObject)
{
	if (IsSimulationRunning())
	{
		return;
	}
	
	FScopedTransaction Transaction(TEXT(""), NSLOCTEXT("LogicDriverPreview", "AddPreviewActor", "Add a Preview Actor"), this);
	SetFlags(RF_Transactional);
	if (!bDontModify)
	{
		Modify();
	}
	
	SpawnActorForWorld(NewPreviewObject);
	
	PreviewObjects.Add(NewPreviewObject);
	BuildActorMap();
	
	if (PreviewObjects.Num() == 1)
	{
		// Set initial context.
		SetContextActor(NewPreviewObject.SpawnedActor);
	}
}

void USMPreviewObject::RemovePreviewActor(AActor* ActorToRemove)
{
	if (IsSimulationRunning())
	{
		return;
	}
	
	if (ActorToRemove)
	{
		for (int32 PreviewIdx = 0; PreviewIdx < PreviewObjects.Num(); ++PreviewIdx)
		{
			AActor* SpawnedActor = PreviewObjects[PreviewIdx].SpawnedActor;
			if (SpawnedActor == ActorToRemove)
			{
				FScopedTransaction Transaction(TEXT(""), NSLOCTEXT("LogicDriverPreview", "RemovePreviewActor", "Remove a Preview Actor"), this);
				SetFlags(RF_Transactional);
				if (!bDontModify)
				{
					Modify();
				}
				
				const int32 SpawnedActorIdx = SpawnedActors.IndexOfByKey(ActorToRemove);
				const bool bIsContext = GetContextActor() == SpawnedActor;
				
				if (GUnrealEd->edactDeleteSelected(ActorToRemove->GetWorld(), true, true, true))
				{
					bool bUserChoseToDelete = false;
					if (SpawnedActorIdx != INDEX_NONE)
					{
						// The original UPROPERTY will be nullptr if the user chose to delete the actor.
						// It won't be nullptr if the user received a prompt and chose to cancel.
						bUserChoseToDelete = SpawnedActors[SpawnedActorIdx] == nullptr;
					}

					if (bUserChoseToDelete)
					{
						if (SpawnedActor != nullptr)
						{
							const FString TrashName = FString::Printf(TEXT("TRASH_%s_%s"), *SpawnedActor->GetName(), *FGuid::NewGuid().ToString());
							SpawnedActor->Rename(*TrashName, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
						}
						
						// User has chosen to delete the actor.
						PreviewObjects.RemoveAt(PreviewIdx);
						
						if (SpawnedActorIdx != INDEX_NONE)
						{
							// Remove by index because the SpawnedActor will be nulled at this point.
							SpawnedActors.RemoveAt(SpawnedActorIdx);
						}

						BuildActorMap();

						if (bIsContext)
						{
							// User deleted the context actor.
							SetContextActor(nullptr);
						}
					}
				}
				break;
			}
		}
	}
}

void USMPreviewObject::NotifySimulationStarted()
{
	PieStartedHandle = FEditorDelegates::PreBeginPIE.AddUObject(this, &USMPreviewObject::OnPieStarted);
	
	OnSimulationStartedEvent.Broadcast(this);
}

void USMPreviewObject::NotifySimulationEnded()
{
	if (PieStartedHandle.IsValid())
	{
		FEditorDelegates::PreBeginPIE.Remove(PieStartedHandle);
	}
	
	OnSimulationEndedEvent.Broadcast(this);
}

void USMPreviewObject::GetAllActorReferences(UObject* InObject, TMap<FName, FName>& PropertyNameValue) const
{
	if (!InObject)
	{
		return;
	}
	TArray<LD::PropertyUtils::FPropertyRetrieval> OutProperties;
	LD::PropertyUtils::GetAllObjectProperties(InObject, InObject->GetClass(), OutProperties);

	for (const LD::PropertyUtils::FPropertyRetrieval& PropertyRetrieved : OutProperties)
	{
		if (UObject* ObjectValue = PropertyRetrieved.GetObjectValue())
		{
			if (AActor* ActorReference = Cast<AActor>(ObjectValue))
			{
				// Only check properties that could have been edited by the user in this world.
				if (PropertyRetrieved.ObjectProperty->HasAllPropertyFlags(CPF_BlueprintVisible) && !PropertyRetrieved.ObjectProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
				{
					PropertyNameValue.Add(*FSMPreviewUtils::MakeFullObjectPropertyName(InObject, PropertyRetrieved.ObjectProperty), ActorReference->GetFName());
				}
			}
		}
	}
}

void USMPreviewObject::RestoreActorReferences(UObject* InObject, ULevel* InLevel, const TMap<FName, FName>& PropertyNameValue)
{
	if (!InObject)
	{
		return;
	}
	check(InLevel);
	
	TArray<LD::PropertyUtils::FPropertyRetrieval> OutProperties;
	LD::PropertyUtils::GetAllObjectProperties(InObject, InObject->GetClass(), OutProperties);

	for (LD::PropertyUtils::FPropertyRetrieval& PropertyRetrieved : OutProperties)
	{
		FName FullObjectName = *FSMPreviewUtils::MakeFullObjectPropertyName(InObject, PropertyRetrieved.ObjectProperty);
		if (PropertyNameValue.Contains(FullObjectName))
		{
			if (PropertyRetrieved.ObjectProperty->HasAllPropertyFlags(CPF_BlueprintVisible) && !PropertyRetrieved.ObjectProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
			{
				// Only check properties that could have been edited by the user in this world.
				AActor* FoundActor = Cast<AActor>(StaticFindObjectFast(AActor::StaticClass(), InLevel, PropertyNameValue[*FSMPreviewUtils::MakeFullObjectPropertyName(InObject, PropertyRetrieved.ObjectProperty)]));
				if (FoundActor && !InLevel->Actors.Contains(FoundActor))
				{
					// The actor outer could still be valid but the actor was destroyed from the level.
					FoundActor = nullptr;
				}
				
				PropertyRetrieved.SetObjectValue(FoundActor); // Sets to null if actor isn't valid.
			}
		}
	}
}

void USMPreviewObject::SpawnActorForWorld(FSMPreviewObjectSpawner& InOutSpawner)
{
	InOutSpawner.LoadActorDefaults(this);

	if (InOutSpawner.Class.Get() == nullptr)
	{
		// TODO: Error log. Class likely deleted.
		return;
	}
	
	bSpawningActor = true;
	
	InOutSpawner.SpawnedActor = SpawnActorForWorld(
		PreviewWorld,
		InOutSpawner.Class,
		InOutSpawner.ActorTemplate,
		FTransform(
			InOutSpawner.Rotation.Quaternion(),
			InOutSpawner.Location,
			InOutSpawner.Scale));

	if (!ensure(InOutSpawner.SpawnedActor))
	{
		return;
	}
	
	if (InOutSpawner.ObjectLabel.IsEmpty())
	{
		InOutSpawner.ObjectLabel = InOutSpawner.SpawnedActor->GetActorLabel();
	}
	
	InOutSpawner.SpawnedActor->SetActorLabel(InOutSpawner.ObjectLabel, false);

	if (InOutSpawner.bIsContext)
	{
		// Recache the context.
		SetContextActor(InOutSpawner.SpawnedActor);
	}
	
	bSpawningActor = false;
}

AActor* USMPreviewObject::SpawnActorForWorld(UWorld* InWorld, UClass* ActorClass, AActor* ActorTemplate, const FTransform& Transform)
{
	check(ActorClass);
	check(InWorld);

	FActorSpawnParameters Params;
	Params.Template = ActorTemplate;
	Params.ObjectFlags = RF_Public | RF_Transactional;
	Params.Name = *ActorClass->GetFName().GetPlainNameString();
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (AActor* SpawnedActor = InWorld->SpawnActor(ActorClass, &Transform, Params))
	{
		if (Transform.IsValid() && !Transform.Equals(FTransform(), 0.f))
		{
			// If the transform has been previously set always use that transform. When spawning UE4 will adjust based on root component offsets
			// which we don't want for repeated spawns. Only on the initial spawn do we want to adjust for floor collision.
			SpawnedActor->SetActorTransform(Transform);
		}
		SpawnedActors.Add(SpawnedActor);
		return SpawnedActor;
	}

	return nullptr;
}

void USMPreviewObject::DestroyActor(AActor* Actor)
{
	if (!Actor)
	{
		// Can be null in certain situations after undo/redo.
		return;
	}
	
	check(Actor->GetWorld());

	SpawnedActors.Remove(Actor);
	Actor->GetWorld()->DestroyActor(Actor, false, false);
	Actor->SetFlags(RF_Transient);
	Actor->ConditionalBeginDestroy();
}

FSMPreviewObjectSpawner* USMPreviewObject::GetPreviewSpawnerFromActor(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}
	
	if (int32* Index = ActorNameToPreviewIndex.Find(Actor->GetFName()))
	{
		if (*Index >= 0 && *Index < PreviewObjects.Num())
		{
			return &PreviewObjects[*Index];
		}
	}

	return nullptr;
}

void USMPreviewObject::BuildActorMap()
{
	ActorNameToPreviewIndex.Reset();

	for (int32 Idx = 0; Idx < PreviewObjects.Num(); ++Idx)
	{
		const FSMPreviewObjectSpawner& PreviewObject = PreviewObjects[Idx];
		if (AActor* SpawnedActor = PreviewObject.SpawnedActor)
		{
			ActorNameToPreviewIndex.Add(SpawnedActor->GetFName(), Idx);
		}
	}
}

void USMPreviewObject::OnPieStarted(bool bIsSimulating)
{
	if (USMBlueprint* Blueprint = Cast<USMBlueprint>(GetOuter()))
	{
		FSMPreviewUtils::StopSimulation(Blueprint);
	}
}

void USMPreviewObject::BindActorDelegates()
{
	ActorMovingHandle = GEngine->OnActorMoving().AddUObject(this, &USMPreviewObject::OnActorMoved);
	ActorMovedHandle = GEngine->OnActorMoved().AddUObject(this, &USMPreviewObject::OnActorMoved);
	ActorPropertyChangeHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &USMPreviewObject::OnActorPostEditChangeProperty);
}

void USMPreviewObject::ReleaseActorHandles()
{
	if (GEngine)
	{
		if (ActorMovedHandle.IsValid())
		{
			GEngine->OnActorMoved().Remove(ActorMovedHandle);
		}
		if (ActorMovingHandle.IsValid())
		{
			GEngine->OnActorMoving().Remove(ActorMovingHandle);
		}
	}
	if (ActorPropertyChangeHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ActorPropertyChangeHandle);
	}

	ActorMovedHandle.Reset();
	ActorMovingHandle.Reset();
	ActorPropertyChangeHandle.Reset();
}

void USMPreviewObject::OnActorMoved(AActor* Actor)
{
	if (IsSimulationRunning())
	{
		return;
	}
	
	if (FSMPreviewObjectSpawner* PreviewObject = GetPreviewSpawnerFromActor(Actor))
	{
		if (PreviewObject->SpawnedActor)
		{
			// Required for undo to function correctly in some cases.
			PreviewObject->SpawnedActor->Modify();
		}
		MarkPackageDirty();
	}
}

void USMPreviewObject::OnActorPostEditChangeProperty(UObject* InObject, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (bSpawningActor)
	{
		// If a spawn setting modifies a property we don't want to save references until after the spawn is fully finished.
		return;
	}

	if (SpawnedActors.Contains(InObject))
	{
		SaveAllActorReferences();

		if (PropertyChangedEvent.GetPropertyName() == TEXT("ActorLabel"))
		{
			if (InObject == CachedContextActor)
			{
				// Context has been renamed, update the saved name.
				SetContextActor(CachedContextActor);
			}

			// Rebuild names after a rename.
			BuildActorMap();
		}
	}
}

#undef LOCTEXT_NAMESPACE
