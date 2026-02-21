// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTools/VoxelPhysicsPartSpawner.h"
#include "VoxelData/VoxelDataUtilities.h"
#include "VoxelWorld.h"
#include "VoxelWorldRootComponent.h"
#include "VoxelUtilities/VoxelExampleUtilities.h"

#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TScriptInterface<IVoxelPhysicsPartSpawnerResult> UVoxelPhysicsPartSpawner_VoxelWorlds::SpawnPart(
	TVoxelSharedPtr<FSimpleDelegate>& OutOnWorldUpdateDone,
	AVoxelWorld* World,
	TVoxelSharedPtr<FVoxelData>&& Data,
	TArray<FVoxelPositionValueMaterial>&& Voxels,
	const FIntVector& PartPosition)
{
	check(Data.IsValid());
	
	UWorld* GameWorld = World->GetWorld();

	FActorSpawnParameters ActorSpawnParameters;
	ActorSpawnParameters.Owner = World;
	ActorSpawnParameters.bDeferConstruction = true;

	UClass* Class = VoxelWorldClass == nullptr ? AVoxelWorld::StaticClass() : VoxelWorldClass.Get();
	FTransform Transform = World->GetTransform();
	Transform.SetLocation(World->LocalToGlobal(PartPosition));
	AVoxelWorld* NewWorld = GameWorld->SpawnActor<AVoxelWorld>(
		Class,
		Transform,
		ActorSpawnParameters);
	if (!ensure(NewWorld)) return nullptr;

	// Can't use the world as a template: this would require the new world to have the same class, which we really don't want if the world is eg a BP
	for (TFieldIterator<FProperty> It(AVoxelWorld::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		auto* Property = *It;
		const FName Name = Property->GetFName();
		if (!Property->HasAnyPropertyFlags(EPropertyFlags::CPF_Transient) &&
			Name != STATIC_FNAME("WorldRoot") &&
			Name != STATIC_FNAME("OnWorldLoaded") &&
			Name != STATIC_FNAME("OnWorldDestroyed"))
		{
			Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(NewWorld), Property->ContainerPtrToValuePtr<void>(World));
		}
	}
	NewWorld->bCreateWorldAutomatically = false;
	NewWorld->FinishSpawning(Transform);

	NewWorld->GetWorldRoot().BodyInstance.bSimulatePhysics = true;
	NewWorld->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseSimpleAndComplex;
	
	ConfigureVoxelWorld.ExecuteIfBound(NewWorld);

	if (!ensure(!NewWorld->IsCreated())) return nullptr;

	NewWorld->bCreateWorldAutomatically = false;
	NewWorld->SetRenderOctreeDepth(FVoxelUtilities::ConvertDepth<DATA_CHUNK_SIZE, RENDER_CHUNK_SIZE>(Data->Depth));
	NewWorld->bEnableUndoRedo = Data->bEnableUndoRedo;
	NewWorld->bEnableMultiplayer = Data->bEnableMultiplayer;
	NewWorld->bCreateGlobalPool = false;

	OutOnWorldUpdateDone = MakeVoxelShared<FSimpleDelegate>();
	OutOnWorldUpdateDone->BindLambda([VoxelWorld = TWeakObjectPtr<AVoxelWorld>(NewWorld), Data]
		{
			if (VoxelWorld.IsValid() && ensure(!VoxelWorld->IsCreated()))
			{
				FVoxelWorldCreateInfo Info;
				Info.bOverrideData = true;
				Info.DataOverride_Raw = Data;
				VoxelWorld->CreateWorld(Info);
			}
		});

	auto* Result = NewObject<UVoxelPhysicsPartSpawnerResult_VoxelWorlds>();
	Result->VoxelWorld = NewWorld;
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelPhysicsPartSpawner_Cubes::UVoxelPhysicsPartSpawner_Cubes()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(TEXT("/Engine/BasicShapes/Cube"));
	CubeMesh = MeshFinder.Object;

	Material = FVoxelExampleUtilities::LoadExampleObject<UMaterialInterface>(TEXT("Material'/Voxel/Examples/Materials/RGB/M_VoxelMaterial_Colors_Parameter.M_VoxelMaterial_Colors_Parameter'"));
}

TScriptInterface<IVoxelPhysicsPartSpawnerResult> UVoxelPhysicsPartSpawner_Cubes::SpawnPart(
	TVoxelSharedPtr<FSimpleDelegate>& OutOnWorldUpdateDone,
	AVoxelWorld* World,
	TVoxelSharedPtr<FVoxelData>&& Data,
	TArray<FVoxelPositionValueMaterial>&& Voxels,
	const FIntVector& PartPosition)
{
	auto* Result = NewObject<UVoxelPhysicsPartSpawnerResult_Cubes>();
	
	const FRotator Rotation = FRotator(World->GetTransform().GetRotation());
	for (const auto& Voxel : Voxels)
	{
		if (SpawnProbability < 1.f && FMath::FRand() > SpawnProbability)
		{
			continue;
		}
		
		auto* StaticMeshActor = GetWorld()->SpawnActor<AStaticMeshActor>(
			World->LocalToGlobal(Voxel.Position + PartPosition),
			Rotation);
		if (!ensure(StaticMeshActor)) continue;

		StaticMeshActor->SetActorScale3D(FVector(World->VoxelSize / 100));
		StaticMeshActor->SetMobility(EComponentMobility::Movable);
		
		UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();
		if (!ensure(StaticMeshComponent)) continue;
		
		StaticMeshComponent->SetStaticMesh(CubeMesh);
		StaticMeshComponent->SetSimulatePhysics(false); // False until world is updated
		{
			auto* Instance = UMaterialInstanceDynamic::Create(Material, StaticMeshActor);
			Instance->SetVectorParameterValue(STATIC_FNAME("VertexColor"), Voxel.Material.GetLinearColor());
			StaticMeshComponent->SetMaterial(0, Instance);
		}
		Result->Cubes.Add(StaticMeshActor);
	}

	OutOnWorldUpdateDone = MakeVoxelShared<FSimpleDelegate>();
	OutOnWorldUpdateDone->BindLambda([Cubes = TArray<TWeakObjectPtr<AStaticMeshActor>>(Result->Cubes)]
		{
			for (auto& Cube : Cubes)
			{
				if (Cube.IsValid())
				{
					UStaticMeshComponent* StaticMeshComponent = Cube->GetStaticMeshComponent();
					if (!ensure(StaticMeshComponent)) continue;
					StaticMeshComponent->SetSimulatePhysics(true);
				}
			}
		});

	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TScriptInterface<IVoxelPhysicsPartSpawnerResult> UVoxelPhysicsPartSpawner_GetVoxels::SpawnPart(
	TVoxelSharedPtr<FSimpleDelegate>& OutOnWorldUpdateDone,
	AVoxelWorld* World,
	TVoxelSharedPtr<FVoxelData>&& Data,
	TArray<FVoxelPositionValueMaterial>&& InVoxels,
	const FIntVector& PartPosition)
{
	auto* Result = NewObject<UVoxelPhysicsPartSpawnerResult_GetVoxels>();
	Result->Voxels = MoveTemp(InVoxels);
	return Result;
}
