// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Matrix with special meaning for the last column
// Matrix[0][3] : random instance id, // Matrix[1 2 3][3]: position offset (used for voxel lookup)
struct FVoxelSpawnerMatrix
{
	FVoxelSpawnerMatrix() = default;
	explicit FVoxelSpawnerMatrix(const FMatrix & Matrix)
		: Matrix(Matrix)
	{
	}

	FORCEINLINE float GetRandomInstanceId() const
	{
		return Matrix.M[0][3];
	}
	FORCEINLINE void SetRandomInstanceId(float RandomInstanceId)
	{
		Matrix.M[0][3] = RandomInstanceId;
	}
	FORCEINLINE void SetRandomInstanceId(uint32 PackedInt)
	{
		SetRandomInstanceId(*reinterpret_cast<const float*>(&PackedInt));
	}

	// Used for floating detection: the voxel position is GetMatrixTranslation() + GetPositionOffset()
	FORCEINLINE FVector GetPositionOffset() const
	{
		return FVector(Matrix.M[1][3], Matrix.M[2][3], Matrix.M[3][3]);
	}
	FORCEINLINE void SetPositionOffset(const FVector& PositionOffset)
	{
		Matrix.M[1][3] = PositionOffset.X;
		Matrix.M[2][3] = PositionOffset.Y;
		Matrix.M[3][3] = PositionOffset.Z;
	}

	FORCEINLINE FMatrix GetCleanMatrix() const
	{
		auto Copy = Matrix;
		Copy.M[0][3] = 0;
		Copy.M[1][3] = 0;
		Copy.M[2][3] = 0;
		Copy.M[3][3] = 1;
		return Copy;
	}

	FORCEINLINE bool operator==(const FVoxelSpawnerMatrix& Other) const
	{
		return Matrix == Other.Matrix;
	}
	FORCEINLINE bool operator!=(const FVoxelSpawnerMatrix& Other) const
	{
		return Matrix != Other.Matrix;
	}

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FVoxelSpawnerMatrix& SpawnerMatrix)
	{
		Ar << SpawnerMatrix.Matrix;
		return Ar;
	}
	
private:
	FMatrix Matrix;
};

struct FVoxelSpawnerTransform
{
	// Used to reduce precision errors
	FIntVector TransformOffset;
	// Relative to TransformOffset
	FVoxelSpawnerMatrix Matrix;
	
	FORCEINLINE bool operator==(const FVoxelSpawnerTransform& Other) const
	{
		return TransformOffset == Other.TransformOffset && Matrix == Other.Matrix;
	}
	FORCEINLINE bool operator!=(const FVoxelSpawnerTransform& Other) const
	{
		return TransformOffset != Other.TransformOffset || Matrix != Other.Matrix;
	}
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FVoxelSpawnerTransform& Transform)
	{
		Ar << Transform.TransformOffset;
		Ar << Transform.Matrix;
		return Ar;
	}
};
struct FVoxelSpawnerTransforms
{
	// Used to reduce precision errors. In voxels
	FIntVector TransformsOffset;
	// Relative to TransformsOffset. Not in voxels, but multiplied by Voxel Size!
	TArray<FVoxelSpawnerMatrix> Matrices;

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FVoxelSpawnerTransforms& Transforms)
	{
		Ar << Transforms.TransformsOffset;
		Ar << Transforms.Matrices;
		return Ar;
	}
};
