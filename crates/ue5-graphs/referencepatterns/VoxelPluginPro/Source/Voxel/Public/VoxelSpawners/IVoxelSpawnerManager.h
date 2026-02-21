// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FVoxelIntBox;
struct FVoxelSpawnersSaveImpl;
struct FVoxelSpawnerTransforms;

class IVoxelSpawnerManager
{
public:
	virtual ~IVoxelSpawnerManager() = default;

	virtual void Destroy() = 0;

	virtual void Regenerate(const FVoxelIntBox& Bounds) = 0;
	virtual void MarkDirty(const FVoxelIntBox& Bounds) = 0;
	
	virtual void SaveTo(FVoxelSpawnersSaveImpl& Save) = 0;
	virtual void LoadFrom(const FVoxelSpawnersSaveImpl& Save) = 0;

	virtual bool GetMeshSpawnerTransforms(const FGuid& SpawnerGuid, TArray<FVoxelSpawnerTransforms>& OutTransforms) const = 0;

	virtual int32 GetTaskCount() const = 0;
};
