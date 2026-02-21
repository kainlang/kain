// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/VoxelSpawnerRandomGenerator.h"
#include "VoxelUtilities/VoxelBaseUtilities.h"
#include "Math/Sobol.h"

FVoxelSpawnerSobolRandomGenerator::FVoxelSpawnerSobolRandomGenerator(int32 CellBits)
	: CellBits(CellBits)
{
}

void FVoxelSpawnerSobolRandomGenerator::Init(int32 SeedX, int32 SeedY)
{
	Value = FSobol::Evaluate(0, CellBits, FIntPoint::ZeroValue, FIntPoint(SeedX, SeedY));
}

void FVoxelSpawnerSobolRandomGenerator::Next() const
{
	Value = FSobol::Next(Index++, CellBits, Value);
}

void FVoxelSpawnerHaltonRandomGenerator::Init(int32 SeedX, int32 SeedY)
{
	// No symmetry!
	Index = FVoxelUtilities::MurmurHash32(SeedX) + FVoxelUtilities::MurmurHash32(SeedY * 23);
	Next();
}

void FVoxelSpawnerHaltonRandomGenerator::Next() const
{
	Value.X = FVoxelUtilities::Halton<2>(Index);
	Value.Y = FVoxelUtilities::Halton<3>(Index);
	Index++;
}
