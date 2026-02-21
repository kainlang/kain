// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CppTranslation/VoxelVariables.h"

class UVoxelDataAsset;
class UTexture2D;
class UCurveFloat;
class UCurveLinearColor;
class UMaterialInterface;
class UVoxelHeightmapAssetFloat;
class UVoxelHeightmapAssetUINT16;
class UVoxelNode_GeneratorMerge;
struct FVoxelGeneratorPicker;

class VOXELGRAPH_API FVoxelColorTextureVariable : public FVoxelExposedVariable
{
public:
	FVoxelColorTextureVariable(const UVoxelExposedNode& Node, UTexture2D* Texture);

	virtual FString GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const override;
};

class VOXELGRAPH_API FVoxelFloatTextureVariable : public FVoxelExposedVariable
{
public:
	FVoxelFloatTextureVariable(const UVoxelExposedNode& Node);

	virtual FString GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const override;
};

class VOXELGRAPH_API FVoxelCurveVariable : public FVoxelExposedVariable
{
public:
	FVoxelCurveVariable(const UVoxelExposedNode& Node, UCurveFloat* Curve);

	virtual FString GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const override;
};

class VOXELGRAPH_API FVoxelColorCurveVariable : public FVoxelExposedVariable
{
public:
	FVoxelColorCurveVariable(const UVoxelExposedNode& Node, UCurveLinearColor* Curve);

	virtual FString GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const override;
};

class VOXELGRAPH_API FVoxelHeightmapVariable : public FVoxelExposedVariable
{
public:
	FVoxelHeightmapVariable(const UVoxelExposedNode& Node, UVoxelHeightmapAssetFloat* Heightmap);
	FVoxelHeightmapVariable(const UVoxelExposedNode& Node, UVoxelHeightmapAssetUINT16* Heightmap);

	virtual FString GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const override;
};

class VOXELGRAPH_API FVoxelDataAssetVariable : public FVoxelExposedVariable
{
public:
	FVoxelDataAssetVariable(const UVoxelExposedNode& Node, UVoxelDataAsset* Asset);

	virtual FString GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const override;
};

class VOXELGRAPH_API FVoxelGeneratorVariable : public FVoxelExposedVariable
{
public:
	FVoxelGeneratorVariable(const UVoxelExposedNode& Node, const FVoxelGeneratorPicker& Generator);

	virtual FString GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const override;
};

class VOXELGRAPH_API FVoxelGeneratorArrayVariable : public FVoxelExposedVariable
{
public:
	FVoxelGeneratorArrayVariable(const UVoxelExposedNode& Node, const TArray<FVoxelGeneratorPicker>& Generators);

	virtual FString GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const override;
};

class VOXELGRAPH_API FVoxelMaterialInterfaceVariable : public FVoxelExposedVariable
{
public:
	FVoxelMaterialInterfaceVariable(const UVoxelExposedNode& Node, UMaterialInterface* Material);

	virtual FString GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const override;
};

class VOXELGRAPH_API FVoxelColorVariable : public FVoxelExposedVariable
{
public:
	FVoxelColorVariable(const UVoxelExposedNode& Node, FLinearColor Color);

	virtual FString GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const override;
};
