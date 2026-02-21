// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/VoxelSpawner.h"
#include "VoxelSpawners/VoxelSpawnerManager.h"
#include "VoxelSpawners/VoxelEmptySpawner.h"
#include "VoxelSpawners/VoxelMeshSpawner.h"
#include "VoxelSpawners/VoxelAssetSpawner.h"
#include "VoxelSpawners/VoxelSpawnerGroup.h"

DEFINE_VOXEL_MEMORY_STAT(STAT_VoxelSpawnerResults);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Voxel Spawners Num Results"), STAT_VoxelSpawners_NumResults, STATGROUP_VoxelCounters);

FVoxelSpawnerProxyResult::FVoxelSpawnerProxyResult(EVoxelSpawnerProxyType Type, const FVoxelSpawnerProxy& Proxy)
	: Type(Type)
	, Proxy(Proxy)
{
	ensure(Type == Proxy.Type || Type == EVoxelSpawnerProxyType::EmptySpawner);
	INC_DWORD_STAT(STAT_VoxelSpawners_NumResults);
}

FVoxelSpawnerProxyResult::~FVoxelSpawnerProxyResult()
{
	DEC_DWORD_STAT(STAT_VoxelSpawners_NumResults);
	DEC_VOXEL_MEMORY_STAT_BY(STAT_VoxelSpawnerResults, AllocatedSize);
}

void FVoxelSpawnerProxyResult::Create()
{
	ensure(!bCreated);
	bCreated = true;
	CreateImpl();
	UpdateStats();
}

void FVoxelSpawnerProxyResult::Destroy()
{
	ensure(bCreated);
	bCreated = false;
	DestroyImpl();
	UpdateStats();
}

void FVoxelSpawnerProxyResult::SerializeProxy(FArchive& Ar, FVoxelSpawnersSaveVersion::Type Version)
{
	SerializeImpl(Ar, Version);
	UpdateStats();
}

TVoxelSharedRef<FVoxelSpawnerProxyResult> FVoxelSpawnerProxyResult::CreateFromType(EVoxelSpawnerProxyType Type, FVoxelSpawnerProxy& Proxy)
{
	check(Type == Proxy.Type || Type == EVoxelSpawnerProxyType::EmptySpawner);
	
	switch (Type)
	{
	case EVoxelSpawnerProxyType::EmptySpawner:
		return MakeVoxelShared<FVoxelEmptySpawnerProxyResult>(Proxy);
	case EVoxelSpawnerProxyType::AssetSpawner:
		return MakeVoxelShared<FVoxelAssetSpawnerProxyResult>(static_cast<FVoxelAssetSpawnerProxy&>(Proxy));
	case EVoxelSpawnerProxyType::MeshSpawner:
		return MakeVoxelShared<FVoxelMeshSpawnerProxyResult>(static_cast<FVoxelMeshSpawnerProxy&>(Proxy));
	case EVoxelSpawnerProxyType::SpawnerGroup:
		return MakeVoxelShared<FVoxelSpawnerGroupProxyResult>(static_cast<FVoxelMeshSpawnerGroupProxy&>(Proxy));
	case EVoxelSpawnerProxyType::Invalid:
	default:
		check(false);
		return TVoxelSharedPtr<FVoxelSpawnerProxyResult>().ToSharedRef();
	}
}

void FVoxelSpawnerProxyResult::UpdateStats()
{
	DEC_VOXEL_MEMORY_STAT_BY(STAT_VoxelSpawnerResults, AllocatedSize);
	AllocatedSize = GetAllocatedSize();
	INC_VOXEL_MEMORY_STAT_BY(STAT_VoxelSpawnerResults, AllocatedSize);
}

///////////////////////////////////////////////////////////////////////////////

FVoxelSpawnerProxy::FVoxelSpawnerProxy(UVoxelSpawner* Spawner, FVoxelSpawnerManager& Manager, EVoxelSpawnerProxyType Type, uint32 Seed)
	: Manager(Manager)
	, Type(Type)
	, SpawnerSeed(Spawner->SeedOverride == 0 ? FVoxelUtilities::MurmurHash32(FCrc::StrCrc32(*Spawner->GetPathName())) + Seed : Spawner->SeedOverride)
{

}

TVoxelSharedRef<FVoxelSpawnerProxy> UVoxelSpawner::GetSpawnerProxy(FVoxelSpawnerManager& Manager)
{
	unimplemented();
	return TVoxelSharedPtr<FVoxelSpawnerProxy>().ToSharedRef();
}

bool UVoxelSpawner::GetSpawners(TSet<UVoxelSpawner*>& OutSpawners)
{
	check(this);
	OutSpawners.Add(this);
	return true;
}

bool FVoxelSpawnersSaveImpl::Serialize(FArchive& Ar)
{
	if ((Ar.IsLoading() || Ar.IsSaving()) && !Ar.IsTransacting())
	{
		if (Ar.IsSaving())
		{
			Version = FVoxelSpawnersSaveVersion::LatestVersion;
		}

		Ar << Version;
		Ar << Guid;
		Ar << CompressedData;
	}

	return true;
}