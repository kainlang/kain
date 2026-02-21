// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelNodeHelpers.h"
#include "FastNoise/VoxelFastNoise.inl"


template<typename TParent, typename TImpl, int32 Dimension, bool bIsFractal>
class TVoxelNoiseComputeNode : public TParent
{
public:
	using TParent::TParent;
	GENERATED_DATA_COMPUTE_NODE_BODY_IMPL(TVoxelNoiseComputeNode)

	FORCEINLINE static int32 GetOctaves(const FVoxelNoiseComputeNode& Node, int32 LOD)
	{
		ensure(false);
		return 0;
	}
	FORCEINLINE static int32 GetOctaves(const FVoxelNoiseFractalComputeNode& Node, int32 LOD)
	{
		return Node.LODToOctaves[FMath::Clamp(LOD, 0, 31)];
	}
	
	virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override final
	{
		ensureVoxelSlow(Dimension == this->Dimension);
		ensureVoxelSlow(bIsFractal == this->bIsFractal);
		
		const FVoxelFastNoise& Noise = this->GetNoise();
		if (!bIsFractal)
		{
			if (Dimension == 2)
			{
				if (this->bIsDerivative)
				{
					Outputs[0].Get<v_flt>() = TImpl::Call_Deriv(
						Noise,
						Inputs[0].Get<v_flt>(),
						Inputs[1].Get<v_flt>(),
						Inputs[2].Get<v_flt>(),
						Outputs[1].Get<v_flt>(),
						Outputs[2].Get<v_flt>());

					this->Clamp(Outputs, 0);
					this->Clamp(Outputs, 1);
					this->Clamp(Outputs, 2);
				}
				else
				{
					Outputs[0].Get<v_flt>() = TImpl::Call(
						Noise,
						Inputs[0].Get<v_flt>(),
						Inputs[1].Get<v_flt>(),
						Inputs[2].Get<v_flt>());

					this->Clamp(Outputs, 0);
				}
			}
			else
			{
				if (this->bIsDerivative)
				{
					Outputs[0].Get<v_flt>() = TImpl::Call_Deriv(
						Noise,
						Inputs[0].Get<v_flt>(),
						Inputs[1].Get<v_flt>(),
						Inputs[2].Get<v_flt>(),
						Inputs[3].Get<v_flt>(),
						Outputs[1].Get<v_flt>(),
						Outputs[2].Get<v_flt>(),
						Outputs[3].Get<v_flt>());

					this->Clamp(Outputs, 0);
					this->Clamp(Outputs, 1);
					this->Clamp(Outputs, 2);
					this->Clamp(Outputs, 3);
				}
				else
				{
					Outputs[0].Get<v_flt>() = TImpl::Call(
						Noise,
						Inputs[0].Get<v_flt>(), 
						Inputs[1].Get<v_flt>(), 
						Inputs[2].Get<v_flt>(), 
						Inputs[3].Get<v_flt>());

					this->Clamp(Outputs, 0);
				}
			}
		}
		else
		{
			const int32 Octaves = GetOctaves(*this, Context.LOD);
			if (Dimension == 2)
			{
				if (this->bIsDerivative)
				{
					Outputs[0].Get<v_flt>() = TImpl::Call_Deriv(
						Noise,
						Inputs[0].Get<v_flt>(),
						Inputs[1].Get<v_flt>(),
						Inputs[2].Get<v_flt>(),
						Octaves,
						Outputs[1].Get<v_flt>(),
						Outputs[2].Get<v_flt>());

					this->Clamp(Outputs, 0);
					this->Clamp(Outputs, 1);
					this->Clamp(Outputs, 2);
				}
				else
				{
					Outputs[0].Get<v_flt>() = TImpl::Call(
						Noise,
						Inputs[0].Get<v_flt>(),
						Inputs[1].Get<v_flt>(),
						Inputs[2].Get<v_flt>(),
						Octaves);

					this->Clamp(Outputs, 0);
				}
			}
			else
			{
				if (this->bIsDerivative)
				{
					Outputs[0].Get<v_flt>() = TImpl::Call_Deriv(
						Noise,
						Inputs[0].Get<v_flt>(),
						Inputs[1].Get<v_flt>(),
						Inputs[2].Get<v_flt>(),
						Inputs[3].Get<v_flt>(),
						Octaves,
						Outputs[1].Get<v_flt>(),
						Outputs[2].Get<v_flt>(),
						Outputs[3].Get<v_flt>());

					this->Clamp(Outputs, 0);
					this->Clamp(Outputs, 1);
					this->Clamp(Outputs, 2);
					this->Clamp(Outputs, 3);
				}
				else
				{
					Outputs[0].Get<v_flt>() = TImpl::Call(
						Noise,
						Inputs[0].Get<v_flt>(), 
						Inputs[1].Get<v_flt>(), 
						Inputs[2].Get<v_flt>(), 
						Inputs[3].Get<v_flt>(),
						Octaves);

					this->Clamp(Outputs, 0);
				}
			}
		}
	}
};

#define VOXEL_NOISE_ARG_TYPES_2D               v_flt x, v_flt y,          v_flt frequency               
#define VOXEL_NOISE_ARG_NAMES_2D                     x,       y,                frequency               
#define VOXEL_NOISE_ARG_TYPES_2D_FRACTAL       v_flt x, v_flt y,          v_flt frequency, int32 octaves
#define VOXEL_NOISE_ARG_NAMES_2D_FRACTAL             x,       y,                frequency,       octaves
#define VOXEL_NOISE_ARG_TYPES_2D_DERIV         v_flt x, v_flt y,          v_flt frequency               , v_flt& outDx, v_flt& outDy
#define VOXEL_NOISE_ARG_NAMES_2D_DERIV               x,       y,                frequency               ,        outDx,        outDy
#define VOXEL_NOISE_ARG_TYPES_2D_DERIV_FRACTAL v_flt x, v_flt y,          v_flt frequency, int32 octaves, v_flt& outDx, v_flt& outDy
#define VOXEL_NOISE_ARG_NAMES_2D_DERIV_FRACTAL       x,       y,                frequency,       octaves,        outDx,        outDy

#define VOXEL_NOISE_ARG_TYPES_3D               v_flt x, v_flt y, v_flt z, v_flt frequency               
#define VOXEL_NOISE_ARG_NAMES_3D                     x,       y,       z,       frequency               
#define VOXEL_NOISE_ARG_TYPES_3D_FRACTAL       v_flt x, v_flt y, v_flt z, v_flt frequency, int32 octaves
#define VOXEL_NOISE_ARG_NAMES_3D_FRACTAL             x,       y,       z,       frequency,       octaves
#define VOXEL_NOISE_ARG_TYPES_3D_DERIV         v_flt x, v_flt y, v_flt z, v_flt frequency               , v_flt& outDx, v_flt& outDy, v_flt& outDz
#define VOXEL_NOISE_ARG_NAMES_3D_DERIV               x,       y,       z,       frequency               ,        outDx,        outDy,        outDz
#define VOXEL_NOISE_ARG_TYPES_3D_DERIV_FRACTAL v_flt x, v_flt y, v_flt z, v_flt frequency, int32 octaves, v_flt& outDx, v_flt& outDy, v_flt& outDz
#define VOXEL_NOISE_ARG_NAMES_3D_DERIV_FRACTAL       x,       y,       z,       frequency,       octaves,        outDx,        outDy,        outDz

#define VOXEL_NOISE_IS_FRACTAL         false
#define VOXEL_NOISE_IS_FRACTAL_FRACTAL true

#define DEFINE_VOXEL_NOISE_IMPL(Name, ArgTypes, ArgNames, ArgTypesDeriv, ArgNamesDeriv) \
	struct FVoxelNoiseComputeNodeImpl \
	{ \
		FORCEINLINE static v_flt Call(const FVoxelFastNoise& Noise, ArgTypes) { return Noise.Name(ArgNames); } \
		template<typename... TArgTypes> \
		FORCEINLINE static v_flt Call(TArgTypes...) { ensure(false); return 0.f; } \
		template<typename... TArgTypes> \
		FORCEINLINE static v_flt Call_Deriv(TArgTypes...) { ensure(false); return 0.f; } \
	};

#define DEFINE_VOXEL_NOISE_IMPL_DERIV(Name, ArgTypes, ArgNames, ArgTypesDeriv, ArgNamesDeriv) \
	struct FVoxelNoiseComputeNodeImpl \
	{ \
		FORCEINLINE static v_flt Call(const FVoxelFastNoise& Noise, ArgTypes) { return Noise.Name(ArgNames); } \
		template<typename... TArgTypes> \
		FORCEINLINE static v_flt Call(TArgTypes...) { ensure(false); return 0.f; } \
		FORCEINLINE static v_flt Call_Deriv(const FVoxelFastNoise& Noise, ArgTypesDeriv) { return Noise.Name##_Deriv(ArgNamesDeriv); } \
		template<typename... TArgTypes> \
		FORCEINLINE static v_flt Call_Deriv(TArgTypes...) { ensure(false); return 0.f; } \
	};

#define GENERATED_NOISENODE_BODY_DIM_IMPL(Dimension, Derivative, Fractal, FunctionName, Parent) \
	virtual uint32 GetDimension() const override final { return Dimension; } \
	TVoxelSharedPtr<FVoxelComputeNode> GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const \
	{ \
		return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode)); \
	} \
	DEFINE_VOXEL_NOISE_IMPL##Derivative( \
		FunctionName##_##Dimension##D, \
		VOXEL_NOISE_ARG_TYPES_##Dimension##D##Fractal, \
		VOXEL_NOISE_ARG_NAMES_##Dimension##D##Fractal, \
		VOXEL_NOISE_ARG_TYPES_##Dimension##D##Derivative##Fractal, \
		VOXEL_NOISE_ARG_NAMES_##Dimension##D##Derivative##Fractal) \
	class FLocalVoxelComputeNode : public TVoxelNoiseComputeNode<Parent, FVoxelNoiseComputeNodeImpl, Dimension, VOXEL_NOISE_IS_FRACTAL##Fractal> \
	{ \
	public: \
		using Super = TVoxelNoiseComputeNode<Parent, FVoxelNoiseComputeNodeImpl, Dimension, VOXEL_NOISE_IS_FRACTAL##Fractal>; \
		using Super::Super; \
		virtual FString GetFunctionName() const override { return FString::Printf(TEXT("%s_%dD%s"), TEXT(#FunctionName), Dimension, bIsDerivative ? TEXT("_Deriv") : TEXT("")); } \
	};


#define GENERATED_NOISENODE_BODY_DIM2(FunctionName) \
	GENERATED_NOISENODE_BODY_DIM_IMPL(2,,, FunctionName, FVoxelNoiseComputeNode)
#define GENERATED_NOISENODE_BODY_DIM3(FunctionName) \
	GENERATED_NOISENODE_BODY_DIM_IMPL(3,,, FunctionName, FVoxelNoiseComputeNode)

#define GENERATED_NOISENODE_BODY_FRACTAL_DIM2(FunctionName) \
	GENERATED_NOISENODE_BODY_DIM_IMPL(2,, _FRACTAL, FunctionName, FVoxelNoiseFractalComputeNode)
#define GENERATED_NOISENODE_BODY_FRACTAL_DIM3(FunctionName) \
	GENERATED_NOISENODE_BODY_DIM_IMPL(3,, _FRACTAL, FunctionName, FVoxelNoiseFractalComputeNode)

#define GENERATED_NOISENODE_BODY_DERIVATIVE_DIM2(FunctionName) \
	GENERATED_NOISENODE_BODY_DIM_IMPL(2, _DERIV,, FunctionName, FVoxelNoiseComputeNode)
#define GENERATED_NOISENODE_BODY_DERIVATIVE_DIM3(FunctionName) \
	GENERATED_NOISENODE_BODY_DIM_IMPL(3, _DERIV,, FunctionName, FVoxelNoiseComputeNode)

#define GENERATED_NOISENODE_BODY_FRACTAL_DERIVATIVE_DIM2(FunctionName) \
	GENERATED_NOISENODE_BODY_DIM_IMPL(2, _DERIV, _FRACTAL, FunctionName, FVoxelNoiseFractalComputeNode)
#define GENERATED_NOISENODE_BODY_FRACTAL_DERIVATIVE_DIM3(FunctionName) \
	GENERATED_NOISENODE_BODY_DIM_IMPL(3, _DERIV, _FRACTAL, FunctionName, FVoxelNoiseFractalComputeNode)