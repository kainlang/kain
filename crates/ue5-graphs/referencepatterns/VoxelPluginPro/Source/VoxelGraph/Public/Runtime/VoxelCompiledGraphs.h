// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"
#include "VoxelGraphOutputs.h"

class FVoxelGraph;

struct VOXELGRAPH_API FVoxelCompiledGraphs
{
	inline const TVoxelSharedPtr<FVoxelGraph>& GetFast(uint32 Hash) const
	{
		return FastAccess[Hash];
	}
	inline const TVoxelSharedPtr<FVoxelGraph>& Get(FVoxelGraphPermutationArray Array) const
	{
		Array.Sort();
		return Graphs[Array];
	}
	inline TVoxelSharedPtr<FVoxelGraph>& Add(FVoxelGraphPermutationArray Array)
	{
		Array.Sort();
		return Graphs.FindOrAdd(Array);
	}
	inline const auto& GetGraphsMap() const
	{
		return Graphs;
	}

	void Compact();

private:
	TMap<FVoxelGraphPermutationArray, TVoxelSharedPtr<FVoxelGraph>> Graphs;
	TMap<uint32, TVoxelSharedPtr<FVoxelGraph>> FastAccess;
};
