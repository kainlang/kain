// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FastNoise/VoxelFastNoise.h"
#include "CppTranslation/VoxelCppUtils.h"
#include "CppTranslation/VoxelCppConstructor.h"

class IVoxelNoiseNodeHelper
{
public:
	virtual ~IVoxelNoiseNodeHelper() = default;
	
#define FUNCTION(Name, ArgType) virtual void Name(ArgType Value) const = 0;
	FUNCTION(SetInterpolation, EVoxelNoiseInterpolation);
	FUNCTION(SetFractalLacunarity, v_flt);
	FUNCTION(SetFractalType, EVoxelNoiseFractalType);
	FUNCTION(SetMatrixFromRotation_2D, float);
	FUNCTION(SetMatrixFromRotation_3D, const FRotator&);
	FUNCTION(SetCellularDistanceFunction, EVoxelCellularDistanceFunction);
	FUNCTION(SetCellularReturnType, EVoxelCellularReturnType);
	FUNCTION(SetCellularJitter, v_flt);
	FUNCTION(SetCraterFalloffExponent, v_flt);
#undef FUNCTION
	
	virtual void SetFractalOctavesAndGain(int32 Octaves, v_flt NewGain) const = 0;
};

class FVoxelNoiseNodeHelper_Runtime : public IVoxelNoiseNodeHelper
{
public:
	FVoxelFastNoise& Noise;
	
	explicit FVoxelNoiseNodeHelper_Runtime(FVoxelFastNoise& Noise)
		: Noise(Noise)
	{
	}

#define FUNCTION(Name, ArgType) virtual void Name(ArgType Value) const override { Noise.Name(Value); }
	FUNCTION(SetInterpolation, EVoxelNoiseInterpolation);
	FUNCTION(SetFractalLacunarity, v_flt);
	FUNCTION(SetFractalType, EVoxelNoiseFractalType);
	FUNCTION(SetMatrixFromRotation_2D, float);
	FUNCTION(SetMatrixFromRotation_3D, const FRotator&);
	FUNCTION(SetCellularDistanceFunction, EVoxelCellularDistanceFunction);
	FUNCTION(SetCellularReturnType, EVoxelCellularReturnType);
	FUNCTION(SetCellularJitter, v_flt);
	FUNCTION(SetCraterFalloffExponent, v_flt);
#undef FUNCTION

	virtual void SetFractalOctavesAndGain(int32 Octaves, v_flt NewGain) const override
	{
		Noise.SetFractalOctavesAndGain(Octaves, NewGain);
	}
};

class FVoxelNoiseNodeHelper_Cpp : public IVoxelNoiseNodeHelper
{
public:
	FString NoiseName;
	FVoxelCppConstructor& Constructor;

	FVoxelNoiseNodeHelper_Cpp(const FString& NoiseName, FVoxelCppConstructor& Constructor)
		: NoiseName(NoiseName)
		, Constructor(Constructor)
	{
	}
	
#define FUNCTION(Name, ArgType) virtual void Name(ArgType Value) const override { Constructor.AddLinef(TEXT("%s.%s(%s);"), *NoiseName, *FString(#Name), *FVoxelCppUtils::LexToCpp(Value)); }
	FUNCTION(SetInterpolation, EVoxelNoiseInterpolation);
	FUNCTION(SetFractalLacunarity, v_flt);
	FUNCTION(SetFractalType, EVoxelNoiseFractalType);
	FUNCTION(SetMatrixFromRotation_2D, float);
	FUNCTION(SetMatrixFromRotation_3D, const FRotator&);
	FUNCTION(SetCellularDistanceFunction, EVoxelCellularDistanceFunction);
	FUNCTION(SetCellularReturnType, EVoxelCellularReturnType);
	FUNCTION(SetCellularJitter, v_flt);
	FUNCTION(SetCraterFalloffExponent, v_flt);
#undef FUNCTION

	virtual void SetFractalOctavesAndGain(int32 Octaves, v_flt NewGain) const override
	{
		Constructor.AddLinef(TEXT("%s.SetFractalOctavesAndGain(%s, %s);"), *NoiseName, *FVoxelCppUtils::LexToCpp(Octaves), *FVoxelCppUtils::LexToCpp(NewGain));
	}
};

