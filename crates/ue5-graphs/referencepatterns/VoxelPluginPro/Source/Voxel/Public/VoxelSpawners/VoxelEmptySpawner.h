// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelSpawner.h"

class FVoxelEmptySpawnerProxyResult : public FVoxelSpawnerProxyResult
{
public:
	explicit FVoxelEmptySpawnerProxyResult(const FVoxelSpawnerProxy& Proxy)
		: FVoxelSpawnerProxyResult(EVoxelSpawnerProxyType::EmptySpawner, Proxy)
	{
	}

	//~ Begin FVoxelSpawnerProxyResult Interface
	virtual void CreateImpl() override {}
	virtual void DestroyImpl() override {}

	virtual void SerializeImpl(FArchive& Ar, FVoxelSpawnersSaveVersion::Type Version) override {}
	
	virtual uint32 GetAllocatedSize() override { return sizeof(*this); }
	//~ End FVoxelSpawnerProxyResult Interface
};
