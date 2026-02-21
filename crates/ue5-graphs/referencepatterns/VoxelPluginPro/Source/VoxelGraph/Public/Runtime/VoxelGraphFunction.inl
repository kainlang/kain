// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "Runtime/VoxelGraphFunction.h"
#include "Runtime/VoxelComputeNodeTree.inl"

template<typename T>
void FVoxelGraphFunction::Compute(const FVoxelContext& Context, FVoxelGraphVMComputeBuffers& Buffers, T& Recorder) const
{
	FunctionInit->CopyOutputsToVariables(Buffers.FunctionInputsOutputs, Buffers.Variables);
	const FVoxelGraphFunction* NextFunction = Tree->Compute(Context, Buffers, Recorder);
	if (NextFunction)
	{
		NextFunction->Compute(Context, Buffers, Recorder);
	}
}

template<typename T>
void FVoxelGraphFunction::ComputeRange(const FVoxelContextRange& Context, FVoxelGraphVMComputeRangeBuffers& Buffers, T& Recorder) const
{
	FunctionInit->CopyOutputsToVariables(Buffers.FunctionInputsOutputs, Buffers.Variables);
	const FVoxelGraphFunction* NextFunction = Tree->ComputeRange(Context, Buffers, Recorder);
	if (NextFunction && !FVoxelRangeFailStatus::Get().HasFailed())
	{
		NextFunction->ComputeRange(Context, Buffers, Recorder);
	}
}
