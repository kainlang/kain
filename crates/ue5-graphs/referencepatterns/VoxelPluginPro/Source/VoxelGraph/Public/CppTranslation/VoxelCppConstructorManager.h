// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UVoxelGraphGenerator;
struct FVoxelCompiledGraphs;
class FVoxelGraphErrorReporter;

class FVoxelCppConstructorManager
{
public:
	FVoxelCppConstructorManager(const FString& ClassName, UVoxelGraphGenerator* Graph);
	~FVoxelCppConstructorManager();

	bool Compile(FString& OutHeader, FString& OutCpp);

private:
	FString const ClassName;
	UVoxelGraphGenerator* const VoxelGraphGenerator;
	TUniquePtr<FVoxelCompiledGraphs> const Graphs;
	TUniquePtr<FVoxelGraphErrorReporter> const ErrorReporter;

	bool CompileInternal(FString& OutHeader, FString& OutCpp);
};

