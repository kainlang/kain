// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UVoxelExposedNode;

class VOXELGRAPH_API FVoxelVariable
{
public:
	static FString SanitizeName(const FString& Name);

public:
	const FString Type;
	const FString ExposedName;
	// ExposedName or eg Params.ExposedName if exposed variable
	// Should be used to access the variable in C++
	const FString CppName;

	FVoxelVariable(const FString& Type, const FString& Name);
	
	inline FString GetDeclaration() const { return Type + TEXT(" ") + ExposedName; }
	inline FString GetRefDeclaration() const { return Type + TEXT("& ") + ExposedName; }
	inline FString GetConstDeclaration() const { return "const " + GetDeclaration(); }
	inline FString GetConstRefDeclaration() const { return "const " + GetRefDeclaration(); }

protected:
	FVoxelVariable(const FString& Type, const FString& Name, const FString& CppPrefix);
};


class VOXELGRAPH_API FVoxelExposedVariable : public FVoxelVariable
{
public:
	const UVoxelExposedNode* const Node;
	const FString DefaultValue;
	const FString ExposedType;
	const FString Category;
	const FString Tooltip;
	const int32 Priority;
	const TMap<FName, FString> CustomMetaData;

	/**
	 * Exposed variable
	 * @param	Type			The type of the variable
	 * @param	Node			The node exposing this variable
	 * @param	DefaultValue	The default value of the variable, can be empty
	 * @param	ExposedType		If the exposed type isn't the same as the instance type. 
								You'll be able to do custom stuff through GetLocalVariableFromExposedOne
	 */
	FVoxelExposedVariable(
		const UVoxelExposedNode& Node,
		const FString& Type,
		const FString& ExposedType,
		const FString& DefaultValue);

	virtual ~FVoxelExposedVariable() = default;

	// For example: Name.GetGenerator()
	virtual FString GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const
	{
		return ExposedNameAccessor;
	}
	// Some exposed variables always have some metadata
	virtual TMap<FName, FString> GetExposedVariableDefaultMetadata() const
	{
		return {};
	}

	bool IsSameAs(const FVoxelExposedVariable& Other, bool bCheckNode) const;
	FString GetMetadataString() const;
};

