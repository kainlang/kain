// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FVoxelSpawnerRandomGenerator
{
public:
	virtual ~FVoxelSpawnerRandomGenerator() = default;

	inline FVector2D GetValue() const
	{
		return Value;
	}

	virtual void Init(int32 SeedX, int32 SeedY) = 0;
	virtual void Next() const = 0;

protected:
	mutable FVector2D Value;
};

class FVoxelSpawnerSobolRandomGenerator : public FVoxelSpawnerRandomGenerator
{
public:
	FVoxelSpawnerSobolRandomGenerator(int32 CellBits = 1);

	virtual void Init(int32 SeedX, int32 SeedY) override;
	virtual void Next() const override;

private:
	const int32 CellBits;
	mutable int32 Index = 0;
};

class FVoxelSpawnerHaltonRandomGenerator : public FVoxelSpawnerRandomGenerator
{
public:
	FVoxelSpawnerHaltonRandomGenerator() = default;

	virtual void Init(int32 SeedX, int32 SeedY) override;
	virtual void Next() const override;

private:
	mutable uint32 Index = 0;
};

