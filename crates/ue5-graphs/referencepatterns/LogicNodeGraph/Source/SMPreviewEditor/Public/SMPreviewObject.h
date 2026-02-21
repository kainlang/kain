// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"

#include "SMPreviewObject.generated.h"

class USMInstance;

/**
 * Contains spawn data and an exported template.
 */
USTRUCT()
struct FSMPreviewObjectSpawner
{
	GENERATED_BODY()

public:
	FSMPreviewObjectSpawner();
	~FSMPreviewObjectSpawner();
	
	UPROPERTY(EditAnywhere, Category = "Preview")
	TSubclassOf<UObject> Class;

	UPROPERTY(VisibleAnywhere, Category = "Preview | Transform")
	FVector Location;

	UPROPERTY(VisibleAnywhere, Category = "Preview | Transform")
	FRotator Rotation;

	UPROPERTY(VisibleAnywhere, Category = "Preview | Transform")
	FVector Scale;

	UPROPERTY()
	bool bIsContext;
	
	/** Set by world outliner. */
	UPROPERTY(VisibleAnywhere, Category = "Preview")
	FString ObjectLabel;
	
	/** A reference to an actor spawned from the ActorTemplate. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> SpawnedActor;

	/** Loaded from serialized actor properties, used for instantiating the SpawnedActor. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> ActorTemplate;

	/** Serialize the SpawnedActor's properties. */
	void SaveActorDefaults(UObject* Outer, bool bModify = true);

	/** Deserialize properties to the ActorTemplate. */
	void LoadActorDefaults(UObject* Outer);
	
	bool operator==(const FSMPreviewObjectSpawner& OtherSpawner) const
	{
		return SpawnedActor == OtherSpawner.SpawnedActor;
	}

private:
	/** Properties for the actor template, serialized separately to prevent circular dependency load issues. */
	UPROPERTY()
	TArray<uint8> SavedActorProperties;
};

/**
 * Logic Driver custom game instance to use when running a preview simulation.
 */
UCLASS(MinimalAPI, Transient)
class USMPreviewGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	void SetWorldContext(FWorldContext* InContext);
};

/**
 * Hosts the State Machine during a preview so the state machine will be copied over properly
 * in the simulated world and any actor references updated.
 */
UCLASS(MinimalAPI)
class ASMPreviewStateMachineActor : public AActor
{
	GENERATED_BODY()

public:

	UPROPERTY()
	TObjectPtr<USMInstance> StateMachineInstance;
};

/**
 * Single object per blueprint to manage simulation data.
 */
UCLASS()
class SMPREVIEWEDITOR_API USMPreviewObject : public UObject
{
	GENERATED_BODY()

public:
	USMPreviewObject();
	virtual ~USMPreviewObject() override;
	
	// UObject
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual bool IsEditorOnly() const override { return true; }
	// ~UObject

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreviewObjectChanged, USMPreviewObject* /*Object*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreviewWorldChanged, UWorld* /*Actor*/);
	
	/** When a property of the preview object has changed. */
	FOnPreviewObjectChanged OnPreviewObjectChangedEvent;

	/** When the preview object needs a new world. */
	FOnPreviewObjectChanged OnWorldRefreshRequiredEvent;

	/** When the simulation first starts. */
	FOnPreviewObjectChanged OnSimulationStartedEvent;

	/** When the simulation ends. */
	FOnPreviewObjectChanged OnSimulationEndedEvent;

	/** When a new world has been set such as from preview to simulation or back. */
	FOnPreviewWorldChanged OnCurrentWorldChangedEvent;
	
protected:
	void OnWorldDestroyed(UWorld* World);
	
private:
	FDelegateHandle OnWorldDestroyedHandle;
	
public:
	/** Initialize the live state machine instance. */
	USMInstance* InitializeStateMachine(UObject* InContext);

	/** Gracefully shutdown the state machine. */
	void ShutdownStateMachine();

	/** Sets state machine properties from a blueprint. */
	void SetFromBlueprint(UBlueprint* Blueprint);

	/** The actor that was spawned in a preview world if any. */
	ASMPreviewStateMachineActor* GetPreviewStateMachineActor() const { return PreviewStateMachineActor; }
	
	/** Call before use so the preview object knows what world to spawn and destroy actors. */
	void SetPreviewWorld(UWorld* InWorld, const bool bModify = false);

	/** The current world: either simulation or preview. */
	void SetCurrentWorld(UWorld* InWorld);

	/** Signal that the game mode has updated. */
	void UpdateGameMode();

	/** Spawns context and all preview actors. */
	void SpawnAllActors();

	/** Destroys context and all preview actors. */
	void DestroyAllActors();

	/** Signals to refresh actors, such as after a package has saved. */
	void RefreshPreviewWorldActors();

	/** Checks if the actor is contained in the spawned actors. */
	bool ContainsActor(AActor* CompareActor) const;

	/** Save actor reference paths so they can be restored after an editor reset. */
	void SaveAllActorReferences();

	/** Use saved actor paths to find the real actor references in the world. */
	void RestoreAllActorReferences();

	/** Checks if a state machine is currently running for simulation. */
	bool IsSimulationRunning() const;

	/** The current preview world if one exists. */
	UWorld* GetPreviewWorld() const { return PreviewWorld; }

	/** The preview or simulation world. */
	UWorld* GetCurrentWorld() const { return CurrentWorld; }

	USMInstance* GetStateMachineTemplate() const { return StateMachineTemplate; }

	void SetSimulatedStateMachineInstance(USMInstance* InInstance);
	USMInstance* GetSimulatedStateMachineInstance() const {return SimulatedStateMachineInstance; }

	TSubclassOf<AGameModeBase> GetGameMode() const { return GameMode; }

	bool ShouldPossessPawnContext() const { return bPossessPawnContext; }
	
	void SetContextActor(AActor* InActor);
	AActor* GetContextActor() const;

	/** Create an initial template and spawn the actor. */
	void AddPreviewActor(FSMPreviewObjectSpawner& NewPreviewObject);
	
	/** Searches for the preview spawner associated with this actor and removes it and despawns it. */
	void RemovePreviewActor(AActor* ActorToRemove);

	/** Inform the preview object simulation has started. */
	void NotifySimulationStarted();

	/** Inform the preview object simulation has ended. */
	void NotifySimulationEnded();
	
protected: 
	void GetAllActorReferences(UObject* InObject, TMap<FName, FName>& PropertyNameValue) const;

	void RestoreActorReferences(UObject* InObject, ULevel* InLevel, const TMap<FName, FName>& PropertyNameValue);

	void SpawnActorForWorld(FSMPreviewObjectSpawner& InOutSpawner);
	AActor* SpawnActorForWorld(UWorld* InWorld, UClass* ActorClass, AActor* ActorTemplate, const FTransform& Transform);
	/** Destroy an actor but will not null out actor from the object spawner. */
	void DestroyActor(AActor* Actor);

	/** Find the preview reference from an actor. */
	FSMPreviewObjectSpawner* GetPreviewSpawnerFromActor(AActor* Actor);
	/** Quick access to finding a preview spawner given an actor. Should be rebuilt whenever PreviewObjects is modified. */
	void BuildActorMap();
	
private:
	void OnPieStarted(bool bIsSimulating);
	FDelegateHandle PieStartedHandle;

public:
	/** Bind to engine actor delegates. */
	void BindActorDelegates();

	/** Safely release and reset all delegate handles. */
	void ReleaseActorHandles();
private:
	void OnActorMoved(AActor* Actor);
	void OnActorPostEditChangeProperty(UObject* InObject, struct FPropertyChangedEvent& PropertyChangedEvent);

	FDelegateHandle ActorMovingHandle;
	FDelegateHandle ActorMovedHandle;
	FDelegateHandle ActorPropertyChangeHandle;
	
private:
	/** All objects to spawn into the preview world. */
	UPROPERTY()
	TArray<FSMPreviewObjectSpawner> PreviewObjects;

	/** Actor name to the index of the PreviewObjects array. */
	UPROPERTY(Transient)
	TMap<FName, int32> ActorNameToPreviewIndex;

	/** Actor name to use as the context. */
	UPROPERTY()
	FName ContextName;

	/** Current context actor for this session. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> CachedContextActor;

	/** The game mode to use when simulating. */
	UPROPERTY(EditAnywhere, Category = "Simulation")
	TSubclassOf<AGameModeBase> GameMode;

	/** Possess a pawn context with the default player controller when simulating. */
	UPROPERTY(EditAnywhere, Category = "Simulation")
	bool bPossessPawnContext;
	
	/** The state machine to spawn into the simulation world. */
	UPROPERTY(VisibleAnywhere, Export, Category = "Simulation", meta = (DisplayName = "State Machine", DisplayThumbnail = false, ShowInnerProperties))
	USMInstance* StateMachineTemplate;
	
	/**
	 * The SIMULATED state machine that is running. Hosted under a PreviewStateMachineActor if there is a valid world.
	 * This is set externally when a user starts simulation.
	 */
	UPROPERTY(Transient)
	TObjectPtr<USMInstance> SimulatedStateMachineInstance;

	/** Full property names mapped to actor names. */
	UPROPERTY()
	TMap<FName, FName> ActorPropertyToActorName;
	
	/** The state machine instance in the PREVIEW world. */
	UPROPERTY(Transient)
	TObjectPtr<USMInstance> PreviewStateMachineInstance;

	/** An actor to host a PREVIEW state machine. */
	UPROPERTY(Transient, NonTransactional)
	TObjectPtr<ASMPreviewStateMachineActor> PreviewStateMachineActor;

	/** The PREVIEW world if one exists. */
	UPROPERTY(Transient)
	TObjectPtr<UWorld> PreviewWorld;

	/** Either preview or simulation. */
	UPROPERTY(Transient)
	TObjectPtr<UWorld> CurrentWorld;

	/** Actors currently spawned in the world. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedActors;

	/** True only during a spawn. */
	bool bSpawningActor;

	/** Prevents Modify() from being called. */
	bool bDontModify;

	/** True during serialize writing. */
	bool bIsSaving;
};