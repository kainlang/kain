// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/VoxelInstancedMeshManager.h"
#include "VoxelSpawners/VoxelHierarchicalInstancedStaticMeshComponent.h"
#include "VoxelSpawners/VoxelSpawnerActor.h"
#include "VoxelEvents/VoxelEventManager.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelMessages.h"
#include "VoxelWorld.h"

#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"

DECLARE_DWORD_COUNTER_STAT(TEXT("InstancedMeshManager Num Spawned Actors"), STAT_FVoxelInstancedMeshManager_NumSpawnedActors, STATGROUP_VoxelCounters);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num HISM"), STAT_FVoxelInstancedMeshManager_NumHISM, STATGROUP_VoxelCounters);

FVoxelInstancedMeshManagerSettings::FVoxelInstancedMeshManagerSettings(
	const AVoxelWorld* World,
	const TVoxelSharedRef<IVoxelPool>& Pool,
	const TVoxelSharedRef<FVoxelEventManager>& EventManager)
	: ComponentsOwner(const_cast<AVoxelWorld*>(World))
	, WorldOffset(World->GetWorldOffsetPtr())
	, Pool(Pool)
	, EventManager(EventManager)
	, HISMChunkSize(FMath::Max(32, World->HISMChunkSize))
	, CollisionDistanceInChunks(FMath::Max(0, World->SpawnersCollisionDistanceInVoxel / int32(CollisionChunkSize)))
	, VoxelSize(World->VoxelSize)
	, MaxNumberOfInstances(World->MaxNumberOfFoliageInstances)
	, OnMaxInstanceReached(FSimpleDelegate::CreateWeakLambda(const_cast<AVoxelWorld*>(World), [this, World]()
	{
		if (World->OnMaxFoliageInstancesReached.IsBound())
		{
			World->OnMaxFoliageInstancesReached.Broadcast();
		}
		else
		{
			FVoxelMessages::Error(FString::Printf(TEXT("Max number of foliage instances reached: %lli"), MaxNumberOfInstances), World);
		}
	}))
{
}

FVoxelInstancedMeshManager::FVoxelInstancedMeshManager(const FVoxelInstancedMeshManagerSettings& Settings)
	: Settings(Settings)
{
}

TVoxelSharedRef<FVoxelInstancedMeshManager> FVoxelInstancedMeshManager::Create(const FVoxelInstancedMeshManagerSettings& Settings)
{
	return MakeShareable(new FVoxelInstancedMeshManager(Settings));
}

void FVoxelInstancedMeshManager::Destroy()
{
	StopTicking();

	// Needed for RegenerateSpawners
	for (const auto& HISM : HISMs)
	{
		if (HISM.IsValid())
		{
			HISM->DestroyComponent();
		}
	}
	DEC_DWORD_STAT_BY(STAT_FVoxelInstancedMeshManager_NumHISM, HISMs.Num());
	HISMs.Empty();
	
	MeshSettingsToChunks.Empty();
}

FVoxelInstancedMeshManager::~FVoxelInstancedMeshManager()
{
	ensure(IsInGameThread());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelInstancedMeshInstancesRef FVoxelInstancedMeshManager::AddInstances(
	const FVoxelInstancedMeshAndActorSettings& MeshAndActorSettings, 
	const FVoxelSpawnerTransforms& Transforms, 
	const FVoxelIntBox& Bounds)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (!ensure(Transforms.Matrices.Num() > 0)) return {};

	if (NumInstances > Settings.MaxNumberOfInstances)
	{
		if (!bMaxNumInstancesReachedFired)
		{
			bMaxNumInstancesReachedFired = true;
			Settings.OnMaxInstanceReached.ExecuteIfBound();
		}
		return {};
	}
	
	NumInstances += Transforms.Matrices.Num();

	const FIntVector Position = ComputeTransformsOffset(Bounds);
	ensure(Transforms.TransformsOffset == Position);
	
	FHISMChunks& Chunks = MeshSettingsToChunks.FindOrAdd(MeshAndActorSettings);
	const FIntVector ChunkKey = FVoxelUtilities::DivideFloor(Position, Settings.HISMChunkSize);

	FHISMChunk& Chunk = Chunks.Chunks.FindOrAdd(ChunkKey);
	Chunk.Bounds += Bounds;
	
	auto& HISM = Chunk.HISM;
	if (!HISM.IsValid())
	{
		HISM = CreateHISM(MeshAndActorSettings, Position);
		HISMs.Add(HISM);
		INC_DWORD_STAT(STAT_FVoxelInstancedMeshManager_NumHISM);
	}

	if (!ensure(HISM.IsValid()))
	{
		// Happens sometimes
		return {};
	}

	FVoxelInstancedMeshInstancesRef Ref;
	Ref.HISM = HISM;
	Ref.Section = HISM->Voxel_AppendTransforms(Transforms.Matrices, Bounds);
	if (ensure(Ref.IsValid()))
	{
		Ref.NumInstances = Transforms.Matrices.Num();
	}
	
	return Ref;
}

void FVoxelInstancedMeshManager::RemoveInstances(FVoxelInstancedMeshInstancesRef Ref)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	NumInstances -= Ref.NumInstances;
	ensure(NumInstances >= 0);

	if (!ensure(Ref.IsValid())) return;
	if (!/*ensure*/(Ref.HISM.IsValid())) return; // Can be raised due to DeleteTickable being delayed

	Ref.HISM->Voxel_RemoveSection(Ref.Section);

	if (Ref.HISM->Voxel_IsEmpty())
	{
		ensure(HISMs.Remove(Ref.HISM) == 1);
		DEC_DWORD_STAT(STAT_FVoxelInstancedMeshManager_NumHISM);
		
		LOG_VOXEL(Verbose, TEXT("Instanced Mesh Manager: Removing HISM for mesh %s"), Ref.HISM->GetStaticMesh() ? *Ref.HISM->GetStaticMesh()->GetPathName() : TEXT("NULL"));
		
		// TODO pooling?
		Ref.HISM->DestroyComponent();
	}
}

const TSet<int32>& FVoxelInstancedMeshManager::GetRemovedIndices(FVoxelInstancedMeshInstancesRef Ref)
{
	const auto Pinned = Ref.Section.Pin();
	if (ensure(Pinned.IsValid()))
	{
		// Fine to return a ref, as Section is only used on the game thread so it won't be deleted
		return Pinned->RemovedIndices;
	}
	else
	{
		static const TSet<int32> EmptyArray;
		return EmptyArray;
	}
}

AVoxelSpawnerActor* FVoxelInstancedMeshManager::SpawnActor(
	const FVoxelInstancedMeshAndActorSettings& MeshAndActorSettings,
	const FVoxelSpawnerTransform& Transform) const
{
	VOXEL_FUNCTION_COUNTER();
	INC_DWORD_STAT(STAT_FVoxelInstancedMeshManager_NumSpawnedActors);
	
	if (!MeshAndActorSettings.ActorSettings.ActorClass) return nullptr;

	auto* ComponentsOwner = Settings.ComponentsOwner.Get();
	if (!ComponentsOwner) return nullptr;

	auto* World = ComponentsOwner->GetWorld();
	if (!World) return nullptr;

	if (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview) return nullptr;

	const FTransform LocalTransform(Transform.Matrix.GetCleanMatrix().ConcatTranslation(FVector(Transform.TransformOffset + *Settings.WorldOffset) * Settings.VoxelSize));
	const FTransform GlobalTransform = LocalTransform * ComponentsOwner->GetTransform();
	
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.bDeferConstruction = true;

	auto* Actor = World->SpawnActor<AVoxelSpawnerActor>(MeshAndActorSettings.ActorSettings.ActorClass, SpawnParameters);
	if (!Actor) return nullptr;
	
	Actor->InitialLifeSpan = MeshAndActorSettings.ActorSettings.Lifespan;
	Actor->FinishSpawning(GlobalTransform);

	Actor->SetStaticMesh(MeshAndActorSettings.Mesh.Get(), MeshAndActorSettings.GetSectionsMaterials(), MeshAndActorSettings.ActorSettings.BodyInstance);
	Actor->SetInstanceRandom(Transform.Matrix.GetRandomInstanceId());

	return Actor;
}

void FVoxelInstancedMeshManager::SpawnActors(
	const FVoxelInstancedMeshAndActorSettings& MeshAndActorSettings,
	const FVoxelSpawnerTransforms& Transforms,
	TArray<AVoxelSpawnerActor*>& OutActors) const
{
	VOXEL_FUNCTION_COUNTER();
	
	for (auto& Matrix : Transforms.Matrices)
	{
		auto* Actor = SpawnActor(MeshAndActorSettings, { Transforms.TransformsOffset, Matrix });
		if (!Actor) return;
		OutActors.Emplace(Actor);
	}
}

void FVoxelInstancedMeshManager::SpawnActorsInArea(
	const FVoxelIntBox& Bounds,
	const FVoxelData& Data,
	EVoxelSpawnerActorSpawnType SpawnType,
	TArray<AVoxelSpawnerActor*>& OutActors) const
{
	VOXEL_FUNCTION_COUNTER();

	const auto TransformsMap = RemoveInstancesInArea(Bounds, Data, SpawnType);

	for (auto& It : TransformsMap)
	{
		for (const auto& Transforms : It.Value)
		{
			SpawnActors(It.Key, Transforms, OutActors);
		}
	}
}

TMap<FVoxelInstancedMeshAndActorSettings, TArray<FVoxelSpawnerTransforms>> FVoxelInstancedMeshManager::RemoveInstancesInArea(
	const FVoxelIntBox& Bounds, 
	const FVoxelData& Data, 
	EVoxelSpawnerActorSpawnType SpawnType) const
{
	VOXEL_FUNCTION_COUNTER();

	const auto ExtendedBounds = Bounds.Extend(1); // As we are accessing floats, they can be between Max - 1 and Max

	TMap<FVoxelInstancedMeshAndActorSettings, TArray<FVoxelSpawnerTransforms>> TransformsMap;
	
	FVoxelReadScopeLock Lock(Data, ExtendedBounds, "SpawnActorsInArea");
	
	const TUniquePtr<FVoxelConstDataAccelerator> Accelerator =
		SpawnType == EVoxelSpawnerActorSpawnType::All
		? nullptr
		: MakeUnique<FVoxelConstDataAccelerator>(Data, ExtendedBounds);
	
	for (auto& MeshSettingsIt : MeshSettingsToChunks)
	{
		TArray<FVoxelSpawnerTransforms>& TransformsArray = TransformsMap.FindOrAdd(MeshSettingsIt.Key);
		for (auto& ChunkIt : MeshSettingsIt.Value.Chunks)
		{
			const FHISMChunk& Chunk = ChunkIt.Value;
			if (Chunk.HISM.IsValid() && Chunk.Bounds.IsValid() && Chunk.Bounds.GetBox().Intersect(Bounds))
			{
				auto Transforms = Chunk.HISM->Voxel_RemoveInstancesInArea(Bounds, Accelerator.Get(), SpawnType);
				TransformsArray.Emplace(MoveTemp(Transforms));
			}
		}
	}

	return TransformsMap;
}

AVoxelSpawnerActor* FVoxelInstancedMeshManager::SpawnActorByIndex(UVoxelHierarchicalInstancedStaticMeshComponent* Component, int32 InstanceIndex)
{
	VOXEL_FUNCTION_COUNTER();
	
	if (!ensure(Component)) 
	{
		return nullptr;
	}

	// Not one of ours
	if (!ensure(HISMs.Contains(Component)))
	{
		return nullptr;
	}
	
	FVoxelSpawnerTransform Transform;
	if (!Component->Voxel_RemoveInstanceByIndex(InstanceIndex, Transform))
	{
		return nullptr;
	}

	ensure(Component->Voxel_GetMeshAndActorSettings().Mesh == Component->GetStaticMesh());

	return SpawnActor(Component->Voxel_GetMeshAndActorSettings(), Transform);
}

void FVoxelInstancedMeshManager::RecomputeMeshPositions()
{
	VOXEL_FUNCTION_COUNTER();

	for (auto& HISM : HISMs)
	{
		if (ensure(HISM.IsValid()))
		{
			HISM->Voxel_SetRelativeLocation();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelInstancedMeshManager::Tick(float DeltaTime)
{
	VOXEL_FUNCTION_COUNTER();
	
	FQueuedBuildCallback Callback;
	while (HISMBuiltDataQueue.Dequeue(Callback))
	{
		auto* HISM = Callback.Component.Get();
		if (!HISM) continue;

		HISM->Voxel_FinishBuilding(*Callback.Data);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelInstancedMeshManager::HISMBuildTaskCallback(
	TWeakObjectPtr<UVoxelHierarchicalInstancedStaticMeshComponent> Component, 
	const TVoxelSharedRef<FVoxelHISMBuiltData>& BuiltData)
{
	HISMBuiltDataQueue.Enqueue({ Component, BuiltData });
}

UVoxelHierarchicalInstancedStaticMeshComponent* FVoxelInstancedMeshManager::CreateHISM(const FVoxelInstancedMeshAndActorSettings& MeshAndActorSettings, const FIntVector& Position) const
{
	VOXEL_FUNCTION_COUNTER();

	LOG_VOXEL(Verbose, TEXT("Instanced Mesh Manager: Creating a new HISM for mesh %s"), *MeshAndActorSettings.Mesh->GetPathName());

	auto* ComponentsOwner = Settings.ComponentsOwner.Get();
	if (!ComponentsOwner)
	{
		return nullptr;
	}

	auto& MeshSettings = MeshAndActorSettings.MeshSettings;

	UVoxelHierarchicalInstancedStaticMeshComponent* HISM;
	if (MeshSettings.HISMTemplate)
	{
		HISM = NewObject<UVoxelHierarchicalInstancedStaticMeshComponent>(ComponentsOwner, MeshSettings.HISMTemplate.Get(), NAME_None, RF_Transient);
	}
	else
	{
		HISM = NewObject<UVoxelHierarchicalInstancedStaticMeshComponent>(ComponentsOwner, NAME_None, RF_Transient);
	}

	HISM->Voxel_Init(
		Settings.Pool,
		const_cast<FVoxelInstancedMeshManager&>(*this).AsShared(),
		Settings.VoxelSize,
		Position,
		MeshAndActorSettings);	
	HISM->SetupAttachment(ComponentsOwner->GetRootComponent(), NAME_None);
	HISM->RegisterComponent();

	if (MeshSettings.BodyInstance.GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		Settings.EventManager->BindEvent(
			true,
			Settings.CollisionChunkSize,
			Settings.CollisionDistanceInChunks,
			FChunkDelegate::CreateUObject(HISM, &UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_EnablePhysics),
			FChunkDelegate::CreateUObject(HISM, &UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_DisablePhysics));
	}

	return HISM;
}
