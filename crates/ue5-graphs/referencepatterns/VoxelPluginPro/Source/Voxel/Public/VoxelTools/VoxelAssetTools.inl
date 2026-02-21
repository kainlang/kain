// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelAssetTools.h"
#include "VoxelTools/VoxelToolHelpers.h"

template<typename TData, typename T1, typename T2>
void UVoxelAssetTools::ImportDataAssetImpl(
	TData& Data,
	const FVoxelVector& Position,
	const FVoxelDataAssetData& AssetData,
	bool bSubtractive,
	T1 GetValue,
	bool bSetMaterials,
	T2 GetMaterial)
{
	const FVoxelIntBox Bounds = FVoxelIntBox(Position, Position + AssetData.GetSize());
	VOXEL_TOOL_FUNCTION_COUNTER(Bounds.Count());

	const FVoxelValue DefaultValue = bSubtractive ? FVoxelValue::Full() : FVoxelValue::Empty();
	const auto GetInstanceValue = [&](int32 X, int32 Y, int32 Z)
	{
		return AssetData.GetInterpolatedValue(
			X - Position.X,
			Y - Position.Y,
			Z - Position.Z,
			DefaultValue);
	};
	const auto GetInstanceMaterial = [&](int32 X, int32 Y, int32 Z)
	{
		return AssetData.HasMaterials() ? AssetData.GetInterpolatedMaterial(
			X - Position.X,
			Y - Position.Y,
			Z - Position.Z) : FVoxelMaterial::Default();
	};

	if (bSetMaterials)
	{
		Data.template Set<FVoxelValue, FVoxelMaterial>(Bounds, [&](int32 X, int32 Y, int32 Z, FVoxelValue& Value, FVoxelMaterial& Material)
		{
			const float InstanceValue = GetInstanceValue(X, Y, Z);
			const FVoxelMaterial InstanceMaterial = GetInstanceMaterial(X, Y, Z);
			
			const float OldValue = Value.ToFloat();
			const float NewValue = GetValue(OldValue, InstanceValue);
			
			Value = FVoxelValue(NewValue);
			Material = GetMaterial(OldValue, NewValue, Material, InstanceValue, InstanceMaterial);
		});
	}
	else
	{
		Data.template Set<FVoxelValue>(Bounds, [&](int32 X, int32 Y, int32 Z, FVoxelValue& Value)
		{
			const float InstanceValue = GetInstanceValue(X, Y, Z);
			
			const float OldValue = Value.ToFloat();
			const float NewValue = GetValue(OldValue, InstanceValue);
			
			Value = FVoxelValue(NewValue);
		});
	}
}
