// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "CppTranslation/VoxelCppIds.h"

const FString FVoxelCppIds::InitStruct                 = "InitStruct";
const FString FVoxelCppIds::Context                    = "Context";
											           
const FString FVoxelCppIds::GraphOutputs               = "Outputs";
const FString FVoxelCppIds::GraphOutputsType           = "FOutputs";

const FString FVoxelCppIds::ExposedVariablesStruct     = "Params";
const FString FVoxelCppIds::ExposedVariablesStructType = "FParams";

FString FVoxelCppIds::GetCacheName(EVoxelAxisDependencies Dependencies)
{
	switch (Dependencies)
	{
	case EVoxelAxisDependencies::Constant:
		return "BufferConstant";
	case EVoxelAxisDependencies::X:
		return "BufferX";
	case EVoxelAxisDependencies::XY:
		return "BufferXY";
	case EVoxelAxisDependencies::XYZ:
	default:
		check(false);
		return "BufferXYZ";
	}
}

FString FVoxelCppIds::GetCacheType(EVoxelAxisDependencies Dependencies)
{
	switch (Dependencies)
	{
	case EVoxelAxisDependencies::Constant:
		return "FBufferConstant";
	case EVoxelAxisDependencies::X:
		return "FBufferX";
	case EVoxelAxisDependencies::XY:
		return "FBufferXY";
	case EVoxelAxisDependencies::XYZ:
	default:
		check(false);
		return "FBufferXYZ";
	}
}

