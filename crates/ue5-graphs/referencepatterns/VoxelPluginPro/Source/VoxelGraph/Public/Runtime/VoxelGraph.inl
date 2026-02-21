// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "Runtime/VoxelGraph.h"
#include "Runtime/VoxelGraphFunction.inl"

template<typename T>
void FVoxelGraph::Compute(const FVoxelContext& Context, FVoxelGraphVMComputeBuffers& Buffers, EVoxelFunctionAxisDependencies Dependencies, T& Recorder) const
{
	FirstFunctions.Get(Dependencies).Compute(Context, Buffers, Recorder);
}

template<typename T>
void FVoxelGraph::ComputeRange(const FVoxelContextRange& Context, FVoxelGraphVMComputeRangeBuffers& Buffers, T& Recorder) const
{
	ensure(!FVoxelRangeFailStatus::Get().HasFailed());
	FirstFunctions.Get(EVoxelFunctionAxisDependencies::XYZWithoutCache).ComputeRange(Context, Buffers, Recorder);
}

