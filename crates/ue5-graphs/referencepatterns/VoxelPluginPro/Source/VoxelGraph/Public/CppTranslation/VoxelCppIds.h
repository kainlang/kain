// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelAxisDependencies.h"

struct VOXELGRAPH_API FVoxelCppIds
{
	const static FString InitStruct;
	const static FString Context;
	const static FString GraphOutputs;
	const static FString GraphOutputsType;
	const static FString ExposedVariablesStruct;
	const static FString ExposedVariablesStructType;

	static inline FString GetVariableName(int32 Id) { return "Variable_" + FString::FromInt(Id); }
	static FString GetCacheName(EVoxelAxisDependencies Dependencies);
	static FString GetCacheType(EVoxelAxisDependencies Dependencies);
};

