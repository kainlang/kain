// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/VoxelMeshSpawner.h"
#include "VoxelSpawners/VoxelSpawnerManager.h"
#include "VoxelSpawners/VoxelInstancedMeshManager.h"
#include "VoxelSpawners/VoxelSpawnerGroup.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelVector.h"
#include "VoxelMessages.h"
#include "VoxelWorldInterface.h"
#include "VoxelUtilities/VoxelGeneratorUtilities.h"

#include "Async/Async.h"
#include "TimerManager.h"
#include "Engine/StaticMesh.h"

#if WITH_EDITOR
bool UVoxelMeshSpawner::NeedsToRebuild(UObject* Object, const FPropertyChangedEvent& PropertyChangedEvent)
{
	return Object == Mesh;
}

bool UVoxelMeshSpawnerGroup::NeedsToRebuild(UObject* Object, const FPropertyChangedEvent& PropertyChangedEvent)
{
	return Meshes.Contains(Object);
}
#endif

FVoxelMeshSpawnerProxyResult::FVoxelMeshSpawnerProxyResult(const FVoxelMeshSpawnerProxy& Proxy)
	: FVoxelSpawnerProxyResult(EVoxelSpawnerProxyType::MeshSpawner, Proxy)
{
}

FVoxelMeshSpawnerProxyResult::~FVoxelMeshSpawnerProxyResult()
{
}

void FVoxelMeshSpawnerProxyResult::Init(const FVoxelIntBox& InBounds, FVoxelSpawnerTransforms&& InTransforms)
{
	Bounds = InBounds;
	Transforms = MoveTemp(InTransforms);
}

void FVoxelMeshSpawnerProxyResult::CreateImpl()
{
	check(IsInGameThread());

	const auto& MeshProxy = static_cast<const FVoxelMeshSpawnerProxy&>(Proxy);

	auto& MeshManager = *MeshProxy.Manager.Settings.MeshManager;
	if (MeshProxy.bAlwaysSpawnActor)
	{
		// Only spawn actors the first time
		if (!IsDirty())
		{
			TArray<AVoxelSpawnerActor*> Actors;
			MeshManager.SpawnActors(MeshProxy.InstanceSettings, Transforms, Actors);
			MarkDirty();
		}
	}
	else
	{
		if (Transforms.Matrices.Num() > 0)
		{
			// Matrices can be empty if all instances were removed
			ensure(!InstancesRef.IsValid());
			const auto Ref = MeshManager.AddInstances(MeshProxy.InstanceSettings, Transforms, Bounds);
			if (Ref.IsValid())
			{
				// Ref might be invalid if we reached instances limit
				InstancesRef = MakeUnique<FVoxelInstancedMeshInstancesRef>(Ref);
			}
		}
	}
}

void FVoxelMeshSpawnerProxyResult::DestroyImpl()
{
	check(IsInGameThread());
	ApplyRemovedIndices();
		
	const auto& MeshProxy = static_cast<const FVoxelMeshSpawnerProxy&>(Proxy);
	if (MeshProxy.bAlwaysSpawnActor)
	{
		// Do nothing
		ensure(IsDirty());
	}
	else
	{
		auto& MeshManager = *MeshProxy.Manager.Settings.MeshManager;
		if (InstancesRef.IsValid())
		{
			MeshManager.RemoveInstances(*InstancesRef);
			InstancesRef.Reset();
		}
	}
}

void FVoxelMeshSpawnerProxyResult::SerializeImpl(FArchive& Ar, FVoxelSpawnersSaveVersion::Type Version)
{
	check(IsInGameThread());
	ApplyRemovedIndices();
	
	Ar << Bounds;

	if (Version < FVoxelSpawnersSaveVersion::StoreSpawnerMatricesRelativeToComponent)
	{
		Transforms.TransformsOffset = FIntVector::ZeroValue;
		Ar << Transforms.Matrices;
	}
	else
	{
		Ar << Transforms;
	}
}

uint32 FVoxelMeshSpawnerProxyResult::GetAllocatedSize()
{
	return sizeof(*this) + Transforms.Matrices.GetAllocatedSize();
}

void FVoxelMeshSpawnerProxyResult::ApplyRemovedIndices()
{
	const auto& MeshProxy = static_cast<const FVoxelMeshSpawnerProxy&>(Proxy);
	auto& MeshManager = *MeshProxy.Manager.Settings.MeshManager;

	if (MeshProxy.bAlwaysSpawnActor)
	{
		// Do nothing
	}
	else
	{
		if (InstancesRef.IsValid())
		{
			// Convert to array, to sort in decreasing order
			TArray<int32> RemovedIndices = MeshManager.GetRemovedIndices(*InstancesRef).Array();
			RemovedIndices.Sort([](int32 A, int32 B) { return A > B; }); // Need to sort in decreasing order for the RemoveAtSwap
			for (int32 RemovedIndex : RemovedIndices)
			{
				Transforms.Matrices.RemoveAtSwap(RemovedIndex, 1, UE_505_SWITCH(false, EAllowShrinking::No));
			}
			Transforms.Matrices.Shrink();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelMeshSpawnerProxy::FVoxelMeshSpawnerProxy(
		UVoxelMeshSpawnerBase* Spawner, 
		TWeakObjectPtr<UStaticMesh> Mesh, 
		const TMap<int32, UMaterialInterface*>& SectionsMaterials, 
		FVoxelSpawnerManager& Manager, 
		uint32 Seed)
	: FVoxelBasicSpawnerProxy(Spawner, Manager, EVoxelSpawnerProxyType::MeshSpawner, Seed)
	, InstanceSettings(Mesh, SectionsMaterials, Spawner->InstancedMeshSettings, Spawner->ActorSettings)
	, InstanceRandom(Spawner->InstanceRandom)
	, ColorOutputName(Spawner->ColorOutputName)
	, bAlwaysSpawnActor(Spawner->bAlwaysSpawnActor)
	, FloatingDetectionOffset(Spawner->FloatingDetectionOffset)
{
	auto& Generator = *Manager.Settings.Data->Generator;

	if (InstanceRandom == EVoxelMeshSpawnerInstanceRandom::ColorOutput &&
		!Generator.GetOutputsPtrMap<FColor>().Contains(ColorOutputName))
	{
		FVoxelMessages::Error(
			"Mesh Spawner: Color output for instance random not found: " +
			FVoxelUtilities::GetMissingGeneratorOutputErrorString<FColor>(ColorOutputName, Generator),
			Spawner);
	}
}

TUniquePtr<FVoxelSpawnerProxyResult> FVoxelMeshSpawnerProxy::ProcessHits(
	const FVoxelIntBox& Bounds, 
	const TArray<FVoxelSpawnerHit>& Hits, 
	const FVoxelConstDataAccelerator& Accelerator) const
{
	check(Hits.Num() > 0);
	
	const uint32 Seed = Bounds.GetMurmurHash() ^ SpawnerSeed;
	auto& Generator = *Manager.Settings.Data->Generator;
	const float VoxelSize = Manager.Settings.VoxelSize;

	const FRandomStream Stream(Seed);

	FVoxelSpawnerTransforms Transforms;
	Transforms.TransformsOffset = Manager.Settings.MeshManager->ComputeTransformsOffset(Bounds);
	Transforms.Matrices.Reserve(Hits.Num());

	const auto ColorOutputPtr = Generator.GetOutputsPtrMap<FColor>().FindRef(ColorOutputName);
	
	for (auto& Hit : Hits)
	{
		const FVector& LocalPosition = Hit.LocalPosition;
		const FVector& Normal = Hit.Normal;
		const FVoxelVector VoxelPosition = FVoxelVector(Bounds.Min) + FVoxelVector(LocalPosition);
		const FVector WorldUp = Generator.GetUpVector(VoxelPosition);

		if (!CanSpawn(Stream, VoxelPosition, Normal, WorldUp))
		{
			continue;
		}

		const FVector RelativeGlobalPosition = VoxelSize * (LocalPosition + FVector(Bounds.Min - Transforms.TransformsOffset));
		const FMatrix Transform = GetTransform(Stream, Normal, WorldUp, RelativeGlobalPosition);

		FVoxelSpawnerMatrix SpawnerMatrix(Transform);
		
		if (InstanceRandom == EVoxelMeshSpawnerInstanceRandom::Random)
		{
			SpawnerMatrix.SetRandomInstanceId(Stream.GetFraction());
		}
		else if (InstanceRandom == EVoxelMeshSpawnerInstanceRandom::VoxelMaterial)
		{
			// Note: instead of RoundToInt, should maybe use the nearest voxel that's not empty?
			const auto Material = Accelerator.GetMaterial(Bounds.Min + FVoxelUtilities::RoundToInt(LocalPosition), 0);
			SpawnerMatrix.SetRandomInstanceId(Material.GetPackedColor());
		}
		else
		{
			check(InstanceRandom == EVoxelMeshSpawnerInstanceRandom::ColorOutput);
			const auto Color =
				ColorOutputPtr
				? (Generator.*ColorOutputPtr)(VoxelPosition.X, VoxelPosition.Y, VoxelPosition.Z, 0, FVoxelItemStack::Empty)
				: FColor::Black;
			SpawnerMatrix.SetRandomInstanceId(FVoxelMaterial::CreateFromColor(Color).GetPackedColor());
		}

		SpawnerMatrix.SetPositionOffset(RelativeGlobalPosition - Transform.GetOrigin() + Transform.TransformVector(FloatingDetectionOffset));
		
		Transforms.Matrices.Add(SpawnerMatrix);
	}

	if (Transforms.Matrices.Num() == 0) 
	{
		return nullptr;
	}
	else
	{
		Transforms.Matrices.Shrink();
		
		auto Result = MakeUnique<FVoxelMeshSpawnerProxyResult>(*this);
		Result->Init(Bounds, MoveTemp(Transforms));
		return Result;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

inline TArray<TVoxelSharedPtr<FVoxelMeshSpawnerProxy>> CreateMeshProxies(UVoxelMeshSpawnerGroup* Spawner, FVoxelSpawnerManager& Manager)
{
	TArray<TVoxelSharedPtr<FVoxelMeshSpawnerProxy>> Result;
	uint32 Seed = 1;
	for (UStaticMesh* Mesh : Spawner->Meshes)
	{
		const TMap<int32, UMaterialInterface*> SectionsMaterials; // TODO
		Result.Add(MakeVoxelShared<FVoxelMeshSpawnerProxy>(Spawner, Mesh, SectionsMaterials, Manager, Seed++));
	}
	return Result;
}

FVoxelMeshSpawnerGroupProxy::FVoxelMeshSpawnerGroupProxy(UVoxelMeshSpawnerGroup* Spawner, FVoxelSpawnerManager& Manager)
	: FVoxelSpawnerProxy(Spawner, Manager, EVoxelSpawnerProxyType::SpawnerGroup, 0)
	, Proxies(CreateMeshProxies(Spawner, Manager))
{
}

TUniquePtr<FVoxelSpawnerProxyResult> FVoxelMeshSpawnerGroupProxy::ProcessHits(
	const FVoxelIntBox& Bounds,
	const TArray<FVoxelSpawnerHit>& Hits,
	const FVoxelConstDataAccelerator& Accelerator) const
{
	TArray<TUniquePtr<FVoxelSpawnerProxyResult>> Results;
	
	const int32 NumHitsPerProxy = FVoxelUtilities::DivideCeil(Hits.Num(), Proxies.Num());
	int32 HitsStartIndex = 0;
	for (auto& Proxy : Proxies)
	{
		const int32 HitsEndIndex = FMath::Min(HitsStartIndex + NumHitsPerProxy, Hits.Num());
		if (HitsStartIndex < HitsEndIndex) 
		{
			TArray<FVoxelSpawnerHit> ProxyHits(Hits.GetData() + HitsStartIndex, HitsEndIndex - HitsStartIndex);
			auto Result = Proxy->ProcessHits(Bounds, ProxyHits, Accelerator);
			if (Result.IsValid())
			{
				Results.Emplace(MoveTemp(Result));
			}
		}
		HitsStartIndex = HitsEndIndex;
	}

	if (Results.Num() == 0)
	{
		return nullptr;
	}
	else
	{
		auto Result = MakeUnique<FVoxelSpawnerGroupProxyResult>(*this);
		Result->Init(MoveTemp(Results));
		return Result;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelSharedRef<FVoxelSpawnerProxy> UVoxelMeshSpawner::GetSpawnerProxy(FVoxelSpawnerManager& Manager)
{
	if (!Mesh && !bAlwaysSpawnActor)
	{
		FVoxelMessages::Error("Invalid mesh!", this);
	}
	TMap<int32, UMaterialInterface*> SectionsMaterials;
	if (Mesh)
	{
		const int32 NumSections = Mesh->GetStaticMaterials().Num();
		for (auto& It : MaterialsOverrides)
		{
			const int32 Index = It.Key;
			if (Index < 0 || Index >= NumSections)
			{
				FVoxelMessages::Error(FString::Printf(TEXT("Invalid Material Override section index: %d"), Index), this);
				continue;
			}
			SectionsMaterials.Add(Index, It.Value);
		}
	}
	return MakeVoxelShared<FVoxelMeshSpawnerProxy>(this, Mesh, SectionsMaterials, Manager, 0);
}

FString UVoxelMeshSpawner::GetDebugInfo() const
{
	return Mesh ? Mesh->GetName() : "NULL";
}

///////////////////////////////////////////////////////////////////////////////

TVoxelSharedRef<FVoxelSpawnerProxy> UVoxelMeshSpawnerGroup::GetSpawnerProxy(FVoxelSpawnerManager& Manager)
{
	if (!bAlwaysSpawnActor)
	{
		for (UStaticMesh* Mesh : Meshes)
		{
			if (!Mesh)
			{
				FVoxelMessages::Error("Invalid mesh!", this);
			}
		}
	}
	return MakeVoxelShared<FVoxelMeshSpawnerGroupProxy>(this, Manager);
}

float UVoxelMeshSpawnerGroup::GetDistanceBetweenInstancesInVoxel() const
{
	// Scale it to account for instances split between meshes
	return DistanceBetweenInstancesInVoxel / FMath::Max(1, Meshes.Num());
}

FString UVoxelMeshSpawnerGroup::GetDebugInfo() const
{
	FString Result;
	for (auto& Mesh : Meshes)
	{
		if (!Result.IsEmpty())
		{
			Result += ", ";
		}
		Result += Mesh ? Mesh->GetName() : "NULL";
	}
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelMeshSpawnerBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	
	if (!IsTemplate())
	{
		ActorSettings.BodyInstance.FixupData(this);
		InstancedMeshSettings.BodyInstance.FixupData(this);
	}
}