// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelVoronoiNoiseNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelVariables.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "VoxelGraphGenerator.h"
#include "FastNoise/VoxelFastNoise.inl"

int32 UVoxelNode_VoronoiNoiseBase::GetMinInputPins() const
{
	return GetPins().InputPins.Num();
}

int32 UVoxelNode_VoronoiNoiseBase::GetMaxInputPins() const
{
	return GetMinInputPins();
}

int32 UVoxelNode_VoronoiNoiseBase::GetOutputPinsCount() const
{
	return GetPins().OutputPins.Num();
}

EVoxelPinCategory UVoxelNode_VoronoiNoiseBase::GetInputPinCategory(int32 PinIndex) const
{
	return GetPins().GetInputPin(PinIndex).Category;
}

EVoxelPinCategory UVoxelNode_VoronoiNoiseBase::GetOutputPinCategory(int32 PinIndex) const
{
	return GetPins().GetOutputPin(PinIndex).Category;
}

FName UVoxelNode_VoronoiNoiseBase::GetInputPinName(int32 PinIndex) const
{
	return GetPins().GetInputPin(PinIndex).Name;
}

FName UVoxelNode_VoronoiNoiseBase::GetOutputPinName(int32 PinIndex) const
{
	return GetPins().GetOutputPin(PinIndex).Name;
}

FString UVoxelNode_VoronoiNoiseBase::GetInputPinToolTip(int32 PinIndex) const
{
	return GetPins().GetInputPin(PinIndex).ToolTip;
}

FString UVoxelNode_VoronoiNoiseBase::GetOutputPinToolTip(int32 PinIndex) const
{
	return GetPins().GetOutputPin(PinIndex).ToolTip;
}

FVoxelPinDefaultValueBounds UVoxelNode_VoronoiNoiseBase::GetInputPinDefaultValueBounds(int32 PinIndex) const
{
	return GetPins().GetInputPin(PinIndex).DefaultValueBounds;
}

FString UVoxelNode_VoronoiNoiseBase::GetInputPinDefaultValue(int32 PinIndex) const
{
	return GetPins().GetInputPin(PinIndex).DefaultValue;
}

const FVoxelPinsHelper& UVoxelNode_VoronoiNoiseBase::GetPins() const
{
	static const TArray<FVoxelHelperPin> InputPinsDim2 =
	{
		{"X", EC::Float, "X"},
		{"Y", EC::Float, "Y"},
		{"Jitter", EC::Float, "Jitter of the noise. Increase this to make the noise less blocky", "0.45"},
		{"Seed", EC::Seed, "Seed"}
	};
	static const TArray<FVoxelHelperPin> InputPinsDim3 =
	{
		{"X", EC::Float, "X"},
		{"Y", EC::Float, "Y"},
		{"Z", EC::Float, "Z"},
		{"Jitter", EC::Float, "Jitter of the noise. Increase this to make the noise less blocky", "0.45"},
		{"Seed", EC::Seed, "Seed"}
	};
	static const TArray<FVoxelHelperPin> OutputPinsDim2 =
	{
		{"X", EC::Float, "X"},
		{"Y", EC::Float, "Y"}
	};
	static const TArray<FVoxelHelperPin> OutputPinsDim3 =
	{
		{"X", EC::Float, "X"},
		{"Y", EC::Float, "Y"},
		{"Z", EC::Float, "Z"}
	};
	static const TArray<FVoxelHelperPin> OutputPinsDim2Neighbors =
	{
		{"X 0", EC::Float, "X coordinate of the closest cell"},
		{"Y 0", EC::Float, "Y coordinate of the closest cell"},
		{"X 1", EC::Float, "X coordinate of the second closest cell"},
		{"Y 1", EC::Float, "Y coordinate of the second closest cell"},
		{"Distance 1", EC::Float, "Distance to the border of second closest cell"},
		{"X 2", EC::Float, "X coordinate of the third closest cell"},
		{"Y 2", EC::Float, "Y coordinate of the third closest cell"},
		{"Distance 2", EC::Float, "Distance to the border of the third closest cell"},
		{"X 3", EC::Float, "X coordinate of the fourth closest cell"},
		{"Y 3", EC::Float, "Y coordinate of the fourth closest cell"},
		{"Distance 3", EC::Float, "Distance to the border of the fourth closest cell"}
	};
	static const TArray<FVoxelHelperPin> OutputPinsDim3Neighbors =
	{
	};

	static const FVoxelPinsHelper PinsDim2 = { InputPinsDim2, OutputPinsDim2 };
	static const FVoxelPinsHelper PinsDim3 = { InputPinsDim3, OutputPinsDim3 };
	static const FVoxelPinsHelper PinsDim2Neighbors = { InputPinsDim2, OutputPinsDim2Neighbors };
	static const FVoxelPinsHelper PinsDim3Neighbors = { InputPinsDim3, OutputPinsDim3Neighbors };

	if (bComputeNeighbors)
	{
		return Dimension == 2 ? PinsDim2Neighbors : PinsDim3Neighbors;
	}
	else
	{
		return Dimension == 2 ? PinsDim2 : PinsDim3;
	}
}

template<uint32 Dimension>
class TVoxelVoronoiNoiseComputeNode : public FVoxelDataComputeNode
{
public:
	using FLocalVoxelComputeNode = TVoxelVoronoiNoiseComputeNode<Dimension>;
	GENERATED_DATA_COMPUTE_NODE_BODY();

	TVoxelVoronoiNoiseComputeNode(const UVoxelNode_VoronoiNoiseBase& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelDataComputeNode(Node, CompilationNode)
		, bComputeNeighbors(Node.bComputeNeighbors)
		, NoiseVariable("FVoxelFastNoise", UniqueName.ToString() + "_Noise")
	{
	}

	virtual void Init(FVoxelGraphSeed Inputs[], const FVoxelGeneratorInit& InitStruct) override
	{
		PrivateNoise.SetSeed(Inputs[Dimension + 1]);
	}
	virtual void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override
	{
		Constructor.AddLine(NoiseVariable.CppName + ".SetSeed(" + Inputs[Dimension + 1] + ");");
	}

	virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
	{
		if (Dimension == 2)
		{
			if (bComputeNeighbors)
			{
				PrivateNoise.GetVoronoiNeighbors_2D(
					Inputs[0].Get<v_flt>(),
					Inputs[1].Get<v_flt>(),
					Inputs[2].Get<v_flt>(),
					Outputs[0].Get<v_flt>(),
					Outputs[1].Get<v_flt>(),
					Outputs[2].Get<v_flt>(),
					Outputs[3].Get<v_flt>(),
					Outputs[4].Get<v_flt>(),
					Outputs[5].Get<v_flt>(),
					Outputs[6].Get<v_flt>(),
					Outputs[7].Get<v_flt>(),
					Outputs[8].Get<v_flt>(),
					Outputs[9].Get<v_flt>(),
					Outputs[10].Get<v_flt>());
			}
			else
			{
				PrivateNoise.GetVoronoi_2D(
					Inputs[0].Get<v_flt>(),
					Inputs[1].Get<v_flt>(),
					Inputs[2].Get<v_flt>(),
					Outputs[0].Get<v_flt>(),
					Outputs[1].Get<v_flt>());
			}
		}
	}
	// TODO distances are positive
	virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
	{
		if (Dimension == 2)
		{
			if (bComputeNeighbors)
			{
				Outputs[0].Get<v_flt>() = TVoxelRange<v_flt>::Infinite();
				Outputs[1].Get<v_flt>() = TVoxelRange<v_flt>::Infinite();
				Outputs[2].Get<v_flt>() = TVoxelRange<v_flt>::Infinite();
				Outputs[3].Get<v_flt>() = TVoxelRange<v_flt>::Infinite();
				
				Outputs[4].Get<v_flt>() = TVoxelRange<v_flt>::PositiveInfinite();

				Outputs[5].Get<v_flt>() = TVoxelRange<v_flt>::Infinite();
				Outputs[6].Get<v_flt>() = TVoxelRange<v_flt>::Infinite();

				Outputs[7].Get<v_flt>() = TVoxelRange<v_flt>::PositiveInfinite();

				Outputs[8].Get<v_flt>() = TVoxelRange<v_flt>::Infinite();
				Outputs[9].Get<v_flt>() = TVoxelRange<v_flt>::Infinite();

				Outputs[10].Get<v_flt>() = TVoxelRange<v_flt>::PositiveInfinite();
			}
			else
			{
				Outputs[0].Get<v_flt>() = TVoxelRange<v_flt>::Infinite();
				Outputs[1].Get<v_flt>() = TVoxelRange<v_flt>::Infinite();
			}
		}
	}

	virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
	{
		if (Dimension == 2)
		{
			if (bComputeNeighbors)
			{
				Constructor.AddLinef(TEXT("%s.GetVoronoiNeighbors_2D(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s);"),
					*NoiseVariable.CppName,
					*Inputs[0],
					*Inputs[1],
					*Inputs[2],
					*Outputs[0],
					*Outputs[1],
					*Outputs[2],
					*Outputs[3],
					*Outputs[4],
					*Outputs[5],
					*Outputs[6],
					*Outputs[7],
					*Outputs[8],
					*Outputs[9],
					*Outputs[10]);
			}
			else
			{
				Constructor.AddLinef(TEXT("%s.GetVoronoi_2D(%s, %s, %s, %s, %s);"),
					*NoiseVariable.CppName,
					*Inputs[0],
					*Inputs[1],
					*Inputs[2],
					*Outputs[0],
					*Outputs[1]);
			}
		}
	}
	virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
	{
		if (Dimension == 2)
		{
			if (bComputeNeighbors)
			{
				Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::Infinite();"), *Outputs[0]);
				Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::Infinite();"), *Outputs[1]);
				Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::Infinite();"), *Outputs[2]);
				Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::Infinite();"), *Outputs[3]);
				
				Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::PositiveInfinite();"), *Outputs[4]);
				
				Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::Infinite();"), *Outputs[5]);
				Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::Infinite();"), *Outputs[6]);
				
				Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::PositiveInfinite();"), *Outputs[7]);
				
				Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::Infinite();"), *Outputs[8]);
				Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::Infinite();"), *Outputs[9]);
				
				Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::PositiveInfinite();"), *Outputs[10]);
			}
			else
			{
				Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::Infinite();"), *Outputs[0]);
				Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::Infinite();"), *Outputs[1]);
			}
		}
	}

	virtual void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const override
	{
		PrivateVariables.Add(NoiseVariable);
	}

private:
	const bool bComputeNeighbors;
	const FVoxelVariable NoiseVariable;
	FVoxelFastNoise PrivateNoise;
};

UVoxelNode_2DVoronoiNoise::UVoxelNode_2DVoronoiNoise()
{
	Dimension = 2;
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_2DVoronoiNoise::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	return MakeShareable(new TVoxelVoronoiNoiseComputeNode<2>(*this, InCompilationNode));
}
