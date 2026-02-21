// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelHeightSplitterNode.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "Runtime/VoxelNodeType.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "NodeFunctions/VoxelMathNodeFunctions.h"

UVoxelNode_HeightSplitter::UVoxelNode_HeightSplitter()
{
	SetInputs(EC::Float);
	SetOutputs(EC::Float);
}

int32 UVoxelNode_HeightSplitter::GetMinInputPins() const
{
	return 1 + NumSplits * 2;
}

int32 UVoxelNode_HeightSplitter::GetMaxInputPins() const
{
	return GetMinInputPins();
}

int32 UVoxelNode_HeightSplitter::GetOutputPinsCount() const
{
	return NumSplits + 1;
}

FName UVoxelNode_HeightSplitter::GetInputPinName(int32 PinIndex) const
{
	if (PinIndex == 0)
	{
		return "Height";
	}
	PinIndex--;
	if (PinIndex < 2 * NumSplits)
	{
		const int32 SplitIndex = PinIndex / 2;
		if (PinIndex % 2 == 0)
		{
			return *FString::Printf(TEXT("Height %d"), SplitIndex);
		}
		else
		{
			return *FString::Printf(TEXT("Falloff %d"), SplitIndex);
		}
	}
	return "Error";
}

FName UVoxelNode_HeightSplitter::GetOutputPinName(int32 PinIndex) const
{
	return *FString::Printf(TEXT("Layer %d"), PinIndex);
}

FString UVoxelNode_HeightSplitter::GetInputPinDefaultValue(int32 PinIndex) const
{
	if (PinIndex == 0)
	{
		return "0";
	}
	PinIndex--;

	if (PinIndex % 2 == 1)
	{
		return "5";
	}

	PinIndex /= 2;

	const int32 PreviousPinIndex = 2 * (PinIndex - 1) + 1;
	if (InputPins.IsValidIndex(PreviousPinIndex))
	{
		return FString::SanitizeFloat(FCString::Atof(*InputPins[PreviousPinIndex].DefaultValue) + 10.f);
	}

	return FString::SanitizeFloat(PinIndex * 10);
}

class FVoxelNode_HeightSplitter_LocalVoxelComputeNode : public FVoxelDataComputeNode
{
public:
	GENERATED_DATA_COMPUTE_NODE_BODY_IMPL(FVoxelNode_HeightSplitter_LocalVoxelComputeNode)

	FVoxelNode_HeightSplitter_LocalVoxelComputeNode(const UVoxelNode_HeightSplitter& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelDataComputeNode(Node, CompilationNode)
		, NumSplits(Node.NumSplits)
	{
	}

	virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
	{
		ComputeImpl(Inputs, Outputs);
	}
	virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
	{
		ComputeImpl(Inputs, Outputs);
	}
	virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
	{
		ComputeCppImpl(Inputs, Outputs, Constructor);
	}
	virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
	{
		ComputeCppImpl(Inputs, Outputs, Constructor);
	}

private:
	const int32 NumSplits;

	template<typename TInputs, typename TOutputs>
	void ComputeImpl(TInputs Inputs, TOutputs Outputs) const
	{
		using T = typename TDecay<decltype(Inputs[0].template Get<v_flt>())>::Type;

		TArray<T, TFixedAllocator<MAX_VOXELNODE_PINS>> InputsArray;
		TArray<T, TFixedAllocator<MAX_VOXELNODE_PINS>> OutputsArray;

		for (int32 Split = 0; Split < NumSplits; Split++)
		{
			InputsArray.Add(Inputs[1 + 2 * Split + 0].template Get<v_flt>());
			InputsArray.Add(Inputs[1 + 2 * Split + 1].template Get<v_flt>());
		}
		OutputsArray.SetNumUninitialized(NumSplits + 1);
		
		FVoxelMathNodeFunctions::HeightSplit(Inputs[0].template Get<v_flt>(), InputsArray, OutputsArray);
		
		for (int32 Layer = 0; Layer < NumSplits + 1; Layer++)
		{
			Outputs[Layer].template Get<v_flt>() = OutputsArray[Layer];
		}
	}
	void ComputeCppImpl(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const
	{
		Constructor.StartBlock();
		
		Constructor.AddLinef(TEXT("TVoxelStaticArray<%s, %d> InputsArray;"), *Constructor.GetTypeString(EVoxelPinCategory::Float), 2 * NumSplits);
		Constructor.AddLinef(TEXT("TVoxelStaticArray<%s, %d> OutputsArray;"), *Constructor.GetTypeString(EVoxelPinCategory::Float), NumSplits + 1);

		for (int32 Split = 0; Split < NumSplits; Split++)
		{
			Constructor.AddLinef(TEXT("InputsArray[%d] = %s;"), 2 * Split + 0, *Inputs[1 + 2 * Split + 0]);
			Constructor.AddLinef(TEXT("InputsArray[%d] = %s;"), 2 * Split + 1, *Inputs[1 + 2 * Split + 1]);
		}

		Constructor.AddLinef(TEXT("FVoxelMathNodeFunctions::HeightSplit(%s, InputsArray, OutputsArray);"), *Inputs[0]);
		
		for (int32 Layer = 0; Layer < NumSplits + 1; Layer++)
		{
			Constructor.AddLinef(TEXT("%s = OutputsArray[%d];"), *Outputs[Layer], Layer);
		}
		
		Constructor.EndBlock();
	}
};

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_HeightSplitter::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	return MakeShareable(new FVoxelNode_HeightSplitter_LocalVoxelComputeNode(*this, InCompilationNode));
}
