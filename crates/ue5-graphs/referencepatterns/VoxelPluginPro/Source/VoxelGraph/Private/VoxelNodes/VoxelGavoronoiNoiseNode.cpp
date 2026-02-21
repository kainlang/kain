// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelGavoronoiNoiseNode.h"
#include "VoxelNodes/VoxelNoiseNodeHelper.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelCppConstructor.h"

inline void AddGavoronoiPins(FVoxelPinsHelper& Pins)
{
	Pins.InputPins.Add(FVoxelHelperPin("Direction X", EVoxelPinCategory::Float, "Direction of the noise", "0.5"));
	Pins.InputPins.Add(FVoxelHelperPin("Direction Y", EVoxelPinCategory::Float, "Direction of the noise", "0.5"));
	Pins.InputPins.Add(FVoxelHelperPin("Direction Variation", EVoxelPinCategory::Float, "Strength of the noise added to the direction, between 0 and 1", "0.4"));
}

UVoxelNode_2DGavoronoiNoise::UVoxelNode_2DGavoronoiNoise()
{
	AddGavoronoiPins(CustomNoisePins);
}

UVoxelNode_2DGavoronoiNoiseFractal::UVoxelNode_2DGavoronoiNoiseFractal()
{
	AddGavoronoiPins(CustomNoisePins);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename TParent>
class TVoxelGavoronoiNoiseComputeNode : public TParent
{
public:
	template<typename TNode>
	TVoxelGavoronoiNoiseComputeNode(const TNode& Node, const FVoxelCompilationNode& CompilationNode)
		: TParent(Node, CompilationNode)
		, Jitter(Node.Jitter)
	{
	}

	virtual void InitNoise(const IVoxelNoiseNodeHelper& Noise) const override
	{
		FVoxelNoiseComputeNode::InitNoise(Noise);
		Noise.SetCellularJitter(Jitter);
	}

protected:
	const float Jitter;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int32 UVoxelNode_2DGavoronoiNoise::SetupInputsForComputeOutputRanges(TVoxelStaticArray<FVoxelNodeType, 256>& Inputs) const
{
	Super::SetupInputsForComputeOutputRanges(Inputs);

	// Direction X
	Inputs[GetBaseInputPinsCount() + 0].Get<v_flt>() = 0.5f;
	// Direction Y
	Inputs[GetBaseInputPinsCount() + 1].Get<v_flt>() = 0.5f;
	// Direction Variation
	Inputs[GetBaseInputPinsCount() + 2].Get<v_flt>() = 0.5f;

	return GetBaseInputPinsCount() + 3;
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_2DGavoronoiNoise::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public TVoxelGavoronoiNoiseComputeNode<FVoxelNoiseComputeNode>
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

		using TVoxelGavoronoiNoiseComputeNode<FVoxelNoiseComputeNode>::TVoxelGavoronoiNoiseComputeNode;

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = GetNoise().GetGavoronoi_2D(
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Inputs[4].Get<v_flt>(),
				Inputs[5].Get<v_flt>(),
				Inputs[6].Get<v_flt>());

			Clamp(Outputs, 0);
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s.GetGavoronoi_2D(%s, %s, %s, %s, %s, %s);"),
				*Outputs[0],
				*GetNoiseName(),
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*Inputs[4],
				*Inputs[5],
				*Inputs[6]);

			Clamp(Constructor, Outputs, 0);
		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int32 UVoxelNode_2DGavoronoiNoiseFractal::SetupInputsForComputeOutputRanges(TVoxelStaticArray<FVoxelNodeType, 256>& Inputs) const
{
	Super::SetupInputsForComputeOutputRanges(Inputs);

	// Direction X
	Inputs[GetBaseInputPinsCount() + 0].Get<v_flt>() = 0.5f;
	// Direction Y
	Inputs[GetBaseInputPinsCount() + 1].Get<v_flt>() = 0.5f;
	// Direction Variation
	Inputs[GetBaseInputPinsCount() + 2].Get<v_flt>() = 0.5f;

	return GetBaseInputPinsCount() + 3;
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_2DGavoronoiNoiseFractal::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public TVoxelGavoronoiNoiseComputeNode<FVoxelNoiseFractalComputeNode>
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

		using TVoxelGavoronoiNoiseComputeNode<FVoxelNoiseFractalComputeNode>::TVoxelGavoronoiNoiseComputeNode;

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = GetNoise().GetGavoronoiFractal_2D(
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				GET_OCTAVES,
				Inputs[4].Get<v_flt>(),
				Inputs[5].Get<v_flt>(),
				Inputs[6].Get<v_flt>());

			Clamp(Outputs, 0);
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s.GetGavoronoiFractal_2D(%s, %s, %s, %s, %s, %s, %s);"),
				*Outputs[0],
				*GetNoiseName(),
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*GET_OCTAVES_CPP,
				*Inputs[4],
				*Inputs[5],
				*Inputs[6]);

			Clamp(Constructor, Outputs, 0);
		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_2DErosion::UVoxelNode_2DErosion()
{
	bComputeDerivative = true;

	const FString ToolTip = "The derivative of the noise to erode. You can get them by ticking Compute Derivatives in the node details";
	
	CustomNoisePins.InputPins.Add(FVoxelHelperPin("Noise DX", EVoxelPinCategory::Float, ToolTip));
	CustomNoisePins.InputPins.Add(FVoxelHelperPin("Noise DY", EVoxelPinCategory::Float, ToolTip));
}

void UVoxelNode_2DErosion::ComputeOutputRanges()
{
	// Since the values depend on the input, we cannot sample
	
	OutputRanges.Reset();
	OutputRanges.Add({ -1.f, 1.f });
	OutputRanges.Add({ -1.f, 1.f });
	OutputRanges.Add({ -1.f, 1.f });
	FVoxelNoiseComputeNode::ExpandRanges(OutputRanges, Tolerance);
}

#if WITH_EDITOR
bool UVoxelNode_2DErosion::CanEditChange(const FProperty* InProperty) const
{
	return
		Super::CanEditChange(InProperty) &&
		InProperty->GetFName() != GET_MEMBER_NAME_STATIC(UVoxelNode_2DErosion, bComputeDerivative) &&
		InProperty->GetFName() != GET_MEMBER_NAME_STATIC(UVoxelNode_2DErosion, FractalType);
}
#endif

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_2DErosion::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelNoiseFractalComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

		FLocalVoxelComputeNode(const UVoxelNode_2DErosion& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelNoiseFractalComputeNode(Node, CompilationNode)
			, Jitter(Node.Jitter)
		{
		}

		virtual void InitNoise(const IVoxelNoiseNodeHelper& Noise) const override
		{
			FVoxelNoiseFractalComputeNode::InitNoise(Noise);
			Noise.SetCellularJitter(Jitter);
		}

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = GetNoise().GetErosion_2D(
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				GET_OCTAVES,
				Inputs[4].Get<v_flt>(),
				Inputs[5].Get<v_flt>(),
				Outputs[1].Get<v_flt>(),
				Outputs[2].Get<v_flt>());
			
			Clamp(Outputs, 0);
			Clamp(Outputs, 1);
			Clamp(Outputs, 2);
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s.GetErosion_2D(%s, %s, %s, %s, %s, %s, %s, %s);"),
				*Outputs[0],
				*GetNoiseName(),
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*GET_OCTAVES_CPP,
				*Inputs[4],
				*Inputs[5],
				*Outputs[1],
				*Outputs[2]);

			Clamp(Constructor, Outputs, 0);
			Clamp(Constructor, Outputs, 1);
			Clamp(Constructor, Outputs, 2);
		}

	private:
		const float Jitter;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}
