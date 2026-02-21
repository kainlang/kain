// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/VoxelAssetSpawner.h"
#include "VoxelSpawners/VoxelSpawnerManager.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelRender/IVoxelLODManager.h"
#include "VoxelGenerators/VoxelEmptyGenerator.h"
#include "VoxelMessages.h"

#include "Async/Async.h"

inline FVoxelIntBox GetGeneratorsBounds(const FVoxelAssetSpawnerProxy& Proxy, const TArray<FMatrix>& Matrices)
{
	if (Matrices.Num() > 0)
	{
		FVoxelIntBoxWithValidity BoundsWithValidity;
		for (const auto& Matrix : Matrices)
		{
			BoundsWithValidity += Proxy.GeneratorLocalBounds.ApplyTransform(FTransform(Matrix));
		}
		return BoundsWithValidity.GetBox();
	}
	else
	{
		return FVoxelIntBox();
	}
}

FVoxelAssetSpawnerProxyResult::FVoxelAssetSpawnerProxyResult(const FVoxelAssetSpawnerProxy& Proxy)
	: FVoxelSpawnerProxyResult(EVoxelSpawnerProxyType::AssetSpawner, Proxy)
{
	check(Matrices.Num() == GeneratorsIndices.Num());
}

void FVoxelAssetSpawnerProxyResult::Init(TArray<FMatrix>&& InMatrices, TArray<int32>&& InGeneratorsIndices)
{
	check(InMatrices.Num() == InGeneratorsIndices.Num());
	
	Bounds = GetGeneratorsBounds(static_cast<const FVoxelAssetSpawnerProxy&>(Proxy), InMatrices);
	Matrices = MoveTemp(InMatrices);
	GeneratorsIndices = MoveTemp(InGeneratorsIndices);
}

void FVoxelAssetSpawnerProxyResult::CreateImpl()
{
	VOXEL_FUNCTION_COUNTER();
	
	if (Matrices.Num() == 0) return;

	const auto& AssetProxy = static_cast<const FVoxelAssetSpawnerProxy&>(Proxy);
	auto& Data = *AssetProxy.Manager.Settings.Data;

	{
		FVoxelWriteScopeLock Lock(Data, Bounds, FUNCTION_FNAME);
		Items.Reserve(Matrices.Num());
		for (int32 Index = 0; Index < Matrices.Num(); Index++)
		{
			const auto Generator = AssetProxy.Generators[GeneratorsIndices[Index]].ToSharedRef();
			const FTransform Transform(Matrices[Index]);
			
			Items.Emplace(Data.AddItem<FVoxelAssetItem>(
				Generator,
				AssetProxy.GeneratorLocalBounds.ApplyTransform(Transform),
				Transform,
				AssetProxy.Priority));
		}
	}

	Proxy.Manager.Settings.LODManager->UpdateBounds(Bounds);
}

void FVoxelAssetSpawnerProxyResult::DestroyImpl()
{
	VOXEL_FUNCTION_COUNTER();
	
	if (Matrices.Num() == 0) return;
	
	auto& Data = *Proxy.Manager.Settings.Data;
	{
		FVoxelWriteScopeLock Lock(Data, Bounds, FUNCTION_FNAME);
		for (auto& Item : Items)
		{
			FString Error;
			Data.RemoveItem(Item, Error);
		}
	}

	Items.Empty();
}

void FVoxelAssetSpawnerProxyResult::SerializeImpl(FArchive& Ar, FVoxelSpawnersSaveVersion::Type Version)
{
	Ar << Bounds;
	Ar << Matrices;
	Ar << GeneratorsIndices;
}

uint32 FVoxelAssetSpawnerProxyResult::GetAllocatedSize()
{
	return sizeof(*this) + Matrices.GetAllocatedSize() + GeneratorsIndices.GetAllocatedSize() + Items.GetAllocatedSize();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TArray<TVoxelSharedPtr<FVoxelTransformableGeneratorInstance>> CreateGenerators(UVoxelAssetSpawner& Spawner, FVoxelSpawnerManager& Manager)
{
	TArray<TVoxelSharedPtr<FVoxelTransformableGeneratorInstance>> Result;
	if (!Spawner.Generator.IsValid())
	{
		Result.Add(MakeShareable(new FVoxelTransformableEmptyGeneratorInstance()));
		return Result;
	}

	const FRandomStream Stream(FCrc::StrCrc32(*Spawner.GetPathName()));
	for (int32 Index = 0; Index < FMath::Max(1, Spawner.NumberOfDifferentSeedsToUse); Index++)
	{
		auto NewGenerator = Spawner.Generator.GetInstance(false);
		
		FVoxelGeneratorInit Init;
		Init.VoxelSize = Manager.Settings.VoxelSize;

		NewGenerator->Init(Init);

		Result.Add(NewGenerator);
	}

	return Result;
}

FVoxelAssetSpawnerProxy::FVoxelAssetSpawnerProxy(UVoxelAssetSpawner* Spawner, FVoxelSpawnerManager& Manager)
	: FVoxelBasicSpawnerProxy(Spawner, Manager, EVoxelSpawnerProxyType::AssetSpawner, 0)
	, Generators(CreateGenerators(*Spawner, Manager))
	, GeneratorLocalBounds(Spawner->GeneratorLocalBounds)
	, Priority(Spawner->Priority)
	, bRoundAssetPosition(Spawner->bRoundAssetPosition)
{
}

FVoxelAssetSpawnerProxy::~FVoxelAssetSpawnerProxy()
{
}

TUniquePtr<FVoxelSpawnerProxyResult> FVoxelAssetSpawnerProxy::ProcessHits(
	const FVoxelIntBox& Bounds,
	const TArray<FVoxelSpawnerHit>& Hits,
	const FVoxelConstDataAccelerator& Accelerator) const
{
	const uint32 Seed = Bounds.GetMurmurHash() ^ SpawnerSeed;
	const auto& Settings = Manager.Settings;
	auto& Data = *Settings.Data;
	const auto& Generator = *Data.Generator;

	const FRandomStream Stream(Seed);
	
	TArray<FMatrix> Transforms;
	Transforms.Reserve(Hits.Num());
	
	for (auto& Hit : Hits)
	{
		const FVector& LocalPosition = Hit.LocalPosition;
		const FVector& Normal = Hit.Normal;
		// NOTE: will glitch if far from origin
		const FVector Position = LocalPosition + FVector(Bounds.Min);
		const FVector WorldUp = Generator.GetUpVector(Position);

		if (!CanSpawn(Stream, Position, Normal, WorldUp))
		{
			continue;
		}

		const FVector PositionToUse = bRoundAssetPosition ? FVector(FVoxelUtilities::RoundToInt(Position)) : Position;
		const FMatrix Transform = GetTransform(Stream, Normal, WorldUp, PositionToUse);
		Transforms.Emplace(Transform);
	}

	if (Transforms.Num() == 0)
	{
		return nullptr;
	}
	else 
	{
		Transforms.Shrink();

		TArray<int32> GeneratorsIndices;
		GeneratorsIndices.Reserve(Transforms.Num());
		for (auto& Transform : Transforms)
		{
			GeneratorsIndices.Add(Stream.RandHelper(Generators.Num()));
		}

		auto Result = MakeUnique<FVoxelAssetSpawnerProxyResult>(*this);
		Result->Init(MoveTemp(Transforms), MoveTemp(GeneratorsIndices));
		return Result;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelSharedRef<FVoxelSpawnerProxy> UVoxelAssetSpawner::GetSpawnerProxy(FVoxelSpawnerManager& Manager)
{
	if (!Generator.IsValid())
	{
		FVoxelMessages::Error("Invalid generator!", this);
	}
	return MakeVoxelShared<FVoxelAssetSpawnerProxy>(this, Manager);
}

FString UVoxelAssetSpawner::GetDebugInfo() const
{
	return Generator.GetObject() ? Generator.GetObject()->GetName() : "NULL";
}
