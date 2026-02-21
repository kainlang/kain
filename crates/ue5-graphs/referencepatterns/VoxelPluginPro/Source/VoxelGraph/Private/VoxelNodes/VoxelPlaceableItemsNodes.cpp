// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelPlaceableItemsNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "Runtime/VoxelNodeType.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "VoxelContext.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelGraphDataItemConfig.h"
#include "NodeFunctions/VoxelPlaceableItemsNodeFunctions.h"

UVoxelNode_DataItemSample::UVoxelNode_DataItemSample()
{
	SetInputs(
		{ "X", EC::Float, "X" },
		{ "Y", EC::Float, "Y" },
		{ "Z", EC::Float, "Z" },
		{ "Smoothness", EC::Float, "The smoothness of the union. The value is a distance: it should be in voxels. If <= 0 Min will be used instead (faster). See SmoothUnion for more info" },
		{ "Default", EC::Float, "The value returned when there are no data item nearby", "10" });
	SetOutputs(EC::Float);
}

class FVoxelNode_DataItemSample_LocalVoxelComputeNode : public FVoxelDataComputeNode
{
public:
	GENERATED_DATA_COMPUTE_NODE_BODY_IMPL(FVoxelNode_DataItemSample_LocalVoxelComputeNode);

	FVoxelNode_DataItemSample_LocalVoxelComputeNode(const UVoxelNode_DataItemSample& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelDataComputeNode(Node, CompilationNode)
		, Mask(Node.Mask)
		, CombineMode(Node.CombineMode)
	{
	}

	virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
	{
		ComputeImpl(Inputs, Outputs, Context);
	}
	virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
	{
		ComputeImpl(Inputs, Outputs, Context);
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
	const uint32 Mask;
	const EVoxelDataItemCombineMode CombineMode;

	template<typename TInputs, typename TOutputs, typename TContext>
	void ComputeImpl(TInputs Inputs, TOutputs Outputs, const TContext& Context) const
	{
		Outputs[0].template Get<v_flt>() = FVoxelNodeFunctions::GetDataItemDistance(
			Context.Items.ItemHolder, 
			Inputs[0].template Get<v_flt>(),
			Inputs[1].template Get<v_flt>(),
			Inputs[2].template Get<v_flt>(), 
			Inputs[3].template Get<v_flt>(),
			Inputs[4].template Get<v_flt>(),
			Mask,
			CombineMode);
		
	}
	void ComputeCppImpl(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const
	{
		Constructor.AddLinef(TEXT("%s = FVoxelNodeFunctions::GetDataItemDistance(%s.Items.ItemHolder, %s, %s, %s, %s, %s, %uu, %s);"), 
			*Outputs[0],
			*FVoxelCppIds::Context,
			*Inputs[0],
			*Inputs[1],
			*Inputs[2],
			*Inputs[3],
			*Inputs[4],
			Mask,
			*UEnum::GetValueAsString(CombineMode));
	}
};

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_DataItemSample::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	return MakeShareable(new FVoxelNode_DataItemSample_LocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int32 UVoxelNode_DataItemParameters::GetOutputPinsCount() const
{
	return Config ? Config->Parameters.Num() : 0;
}

FName UVoxelNode_DataItemParameters::GetOutputPinName(int32 PinIndex) const
{
	if (!Config || !Config->Parameters.IsValidIndex(PinIndex))
	{
		return STATIC_FNAME("Invalid");
	}
	else
	{
		return Config->Parameters[PinIndex];
	}
}

EVoxelPinCategory UVoxelNode_DataItemParameters::GetOutputPinCategory(int32 PinIndex) const
{
	return EVoxelPinCategory::Float;
}

#if WITH_EDITOR
void UVoxelNode_DataItemParameters::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive && Config)
	{
		const auto OldPreviewValues = MoveTemp(PreviewValues);
		if (Config)
		{
			for (auto& Parameter : Config->Parameters)
			{
				PreviewValues.Add(Parameter, OldPreviewValues.FindRef(Parameter));
			}
		}
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

TArray<v_flt> UVoxelNode_DataItemParameters::GetPreviewValues() const
{
	TArray<v_flt> Result;
	if (Config)
	{
		for (auto& Parameter : Config->Parameters)
		{
			Result.Add(PreviewValues.FindRef(Parameter));
		}
	}
	return Result;
}

class FVoxelNode_DataItemParameters_LocalVoxelComputeNode : public FVoxelDataComputeNode
{
public:
	GENERATED_DATA_COMPUTE_NODE_BODY_IMPL(FVoxelNode_DataItemParameters_LocalVoxelComputeNode);

	FVoxelNode_DataItemParameters_LocalVoxelComputeNode (const UVoxelNode_DataItemParameters& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelDataComputeNode(Node, CompilationNode)
		, PreviewValues(Node.GetPreviewValues())
	{
		check(OutputCount == PreviewValues.Num());
	}

	virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
	{
		ComputeImpl(Outputs, Context);
	}
	virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
	{
		ComputeImpl(Outputs, Context);
	}
	virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
	{
		ComputeCppImpl(Outputs, Constructor);
	}
	virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
	{
		ComputeCppImpl(Outputs, Constructor);
	}

private:
	const TArray<v_flt> PreviewValues;

	template<typename TOutputs, typename TContext>
	void ComputeImpl(TOutputs Outputs, const TContext& Context) const
	{
		if (Context.Items.CustomData && Context.Items.CustomData->Num() == PreviewValues.Num())
		{
			for (int32 Index = 0; Index < OutputCount; Index++)
			{
				Outputs[Index].template Get<v_flt>() = (*Context.Items.CustomData).GetData()[Index];
			}
		}
		else
		{
			for (int32 Index = 0; Index < OutputCount; Index++)
			{
				Outputs[Index].template Get<v_flt>() = PreviewValues.GetData()[Index];
			}
		}
	}
	void ComputeCppImpl(const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const
	{
		Constructor.AddLinef(TEXT("if (%s.Items.CustomData && %s.Items.CustomData->Num() == %d)"), *FVoxelCppIds::Context, *FVoxelCppIds::Context, PreviewValues.Num());
		Constructor.StartBlock();
		for (int32 Index = 0; Index < OutputCount; Index++)
		{
			Constructor.AddLinef(TEXT("%s = (*%s.Items.CustomData).GetData()[%d];"), *Outputs[Index], *FVoxelCppIds::Context, Index);
		}
		Constructor.EndBlock();
		Constructor.AddLine("else");
		Constructor.StartBlock();
		for (int32 Index = 0; Index < OutputCount; Index++)
		{
			Constructor.AddLinef(TEXT("%s = %f;"), *Outputs[Index], PreviewValues[Index]);
		}
		Constructor.EndBlock();
	}
};

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_DataItemParameters::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	return MakeShareable(new FVoxelNode_DataItemParameters_LocalVoxelComputeNode (*this, InCompilationNode));
}
