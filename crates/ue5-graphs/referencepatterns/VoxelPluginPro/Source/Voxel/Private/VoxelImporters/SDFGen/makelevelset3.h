// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelTexture.h"
#include "VoxelContainers/VoxelArray3.h"
#include "VoxelContainers/VoxelArrayView.h"

struct FVoxelMeshImporterSettingsBase;

struct FMakeLevelSet3InData
{
	TVoxelArrayView<const FVector> Vertices;
	TVoxelArrayView<const FIntVector> Triangles;
	
	TVoxelArrayView<const FVector2D> UVs;
	TArray<TVoxelTexture<FColor>> ColorTextures;
	
	FVector Origin { ForceInit};
	FIntVector Size { ForceInit };

	bool bExportPositions = false;
	bool bExportUVs = false;
};

struct FMakeLevelSet3OutData
{
	TVoxelArray3<float> Phi;
	// In voxel space
	TVoxelArray3<FVector3f> Positions;
	TVoxelArray3<FVector2D> UVs;
	TArray<TVoxelArray3<FColor>> Colors;

	int32 NumLeaks = 0;
};

VOXEL_API void MakeLevelSet3(
	const FVoxelMeshImporterSettingsBase& Settings,
	const FMakeLevelSet3InData& InData,
	FMakeLevelSet3OutData& OutData);

