// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UVoxelNode;
class FVoxelGraphErrorReporter;
class FVoxelExposedVariable;

struct FVoxelCppInclude
{
	const FString Name;
	const bool bSTDInclude;

	FVoxelCppInclude(const FString& Name, bool bSTDInclude)
		: Name(Name)
		, bSTDInclude(bSTDInclude)
	{
	}
	FVoxelCppInclude(const FString& Name)
		: FVoxelCppInclude(Name, false)
	{
	}
	template<typename T>
	FVoxelCppInclude(const T* Name)
		: FVoxelCppInclude(FString(Name), false)
	{
	}

	inline bool operator==(const FVoxelCppInclude& Other) const
	{
		return bSTDInclude == Other.bSTDInclude && Name == Other.Name;
	}

	inline FString ToString() const
	{
		if (bSTDInclude)
		{
			return "#include <" + Name + ">";
		}
		else
		{
			return "#include \"" + Name + "\"";
		}
	}
};

class VOXELGRAPH_API FVoxelCppConfig
{
public:
	FVoxelGraphErrorReporter& ErrorReporter;

	explicit FVoxelCppConfig(FVoxelGraphErrorReporter& ErrorReporter);

	// Add an exposed variable to the generated class
	void AddExposedVariable(const TSharedRef<FVoxelExposedVariable>& Variable);
	// Add an include to the generated file. Example: AddInclude("CoreMinimal.h")
	void AddInclude(const FVoxelCppInclude& Include);

	void BuildExposedVariablesArray();

	const TArray<TSharedRef<FVoxelExposedVariable>>& GetExposedVariables() const { return ExposedVariablesArray; }
	const TArray<FVoxelCppInclude>& GetIncludes() const { return Includes; }

private:
	TMap<FName, TSharedRef<FVoxelExposedVariable>> ExposedVariables;
	TArray<TSharedRef<FVoxelExposedVariable>> ExposedVariablesArray;
	TArray<FVoxelCppInclude> Includes;
};
