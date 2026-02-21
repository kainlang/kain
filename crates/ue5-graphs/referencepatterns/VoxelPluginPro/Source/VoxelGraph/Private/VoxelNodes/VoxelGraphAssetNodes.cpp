// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelGraphAssetNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "VoxelNodes/VoxelGeneratorSamplerNodes.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelCppUtils.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "CppTranslation/VoxelVariables.h"
#include "VoxelGenerators/VoxelGeneratorInit.h"
#include "VoxelGenerators/VoxelGeneratorInstance.h"
#include "NodeFunctions/VoxelNodeFunctions.h"
#include "VoxelTools/VoxelHardnessHandler.h"
#include "VoxelGraphGenerator.h"

int32 UVoxelGraphAssetNode::GetMaxInputPins() const
{
	return GetMinInputPins();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelEditBaseComputeNode : public FVoxelDataComputeNode
{
public:
	FVoxelEditBaseComputeNode(const UVoxelGraphAssetNode& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelDataComputeNode(Node, CompilationNode)
		, DefaultGenerator(
			Node.DefaultGenerator.IsValid()
			? Node.DefaultGenerator.GetInstance(true) 
			: TVoxelSharedPtr<FVoxelGeneratorInstance>())
	{
	}

	virtual void Init(FVoxelGraphSeed Inputs[], const FVoxelGeneratorInit& InitStruct) override
	{
		if (DefaultGenerator.IsValid())
		{
			DefaultGenerator->Init(InitStruct);
		}
	}

protected:
	TVoxelSharedPtr<FVoxelGeneratorInstance> DefaultGenerator;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_EditGetValue::UVoxelNode_EditGetValue()
{
	SetInputs(
		{ "X", EC::Float, "X in global space. Use Global X" },
		{ "Y", EC::Float, "Y in global space. Use Global Y" },
		{ "Z", EC::Float, "Z in global space. Use Global Z" }
	);
	SetOutputs(
		EC::Float
	);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_EditGetValue::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelEditBaseComputeNode
	{
	public:
		using FVoxelEditBaseComputeNode::FVoxelEditBaseComputeNode;

		GENERATED_DATA_COMPUTE_NODE_BODY();

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetPreviousGeneratorValue(
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Context,
				DefaultGenerator.Get());
		}
		void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetPreviousGeneratorValue(
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Context,
				DefaultGenerator.Get());
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(
				TEXT("%s = FVoxelNodeFunctions::GetPreviousGeneratorValue(%s, %s, %s, %s, nullptr);"),
				*Outputs[0],
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context);
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(
				TEXT("%s = FVoxelNodeFunctions::GetPreviousGeneratorValue(%s, %s, %s, %s, nullptr);"),
				*Outputs[0],
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context);
		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_EditGetMaterial::UVoxelNode_EditGetMaterial()
{
	SetInputs(
		{ "X", EC::Float, "X in global space. Use Global X" },
		{ "Y", EC::Float, "Y in global space. Use Global Y" },
		{ "Z", EC::Float, "Z in global space. Use Global Z" }
	);
	SetOutputs(
		EC::Material
	);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_EditGetMaterial::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelEditBaseComputeNode
	{
	public:
		using FVoxelEditBaseComputeNode::FVoxelEditBaseComputeNode;

		GENERATED_DATA_COMPUTE_NODE_BODY();

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<FVoxelMaterial>() = FVoxelNodeFunctions::GetPreviousGeneratorMaterial(
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Context,
				DefaultGenerator.Get());
		}
		void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(
				TEXT("%s = FVoxelNodeFunctions::GetPreviousGeneratorMaterial(%s, %s, %s, %s, nullptr);"),
				*Outputs[0],
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context);
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_EditGetCustomOutput::UVoxelNode_EditGetCustomOutput()
{
	SetInputs(
		{ "X", EC::Float, "X in global space. Use Global X" },
		{ "Y", EC::Float, "Y in global space. Use Global Y" },
		{ "Z", EC::Float, "Z in global space. Use Global Z" }
	);
	SetOutputs(
		EC::Float
	);
}

FText UVoxelNode_EditGetCustomOutput::GetTitle() const
{
	return FText::FromString("Get Previous Generator Custom Output: " + OutputName.ToString());
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_EditGetCustomOutput::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelEditBaseComputeNode
	{
	public:
		FLocalVoxelComputeNode(const UVoxelNode_EditGetCustomOutput& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelEditBaseComputeNode(Node, CompilationNode)
			, OutputName(Node.OutputName)
		{
		}

		GENERATED_DATA_COMPUTE_NODE_BODY();

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetPreviousGeneratorCustomOutput(
				OutputName,
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Context,
				DefaultGenerator.Get());
		}
		void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetPreviousGeneratorCustomOutput(
				OutputName,
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Context,
				DefaultGenerator.Get());
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(
				TEXT("%s = FVoxelNodeFunctions::GetPreviousGeneratorCustomOutput(STATIC_FNAME(\"%s\"), %s, %s, %s, %s, nullptr);"),
				*Outputs[0],
				*OutputName.ToString(),
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context);
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(
				TEXT("%s = FVoxelNodeFunctions::GetPreviousGeneratorCustomOutput(STATIC_FNAME(\"%s\"), %s, %s, %s, %s, nullptr);"),
				*Outputs[0],
				*OutputName.ToString(),
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context);
		}

	private:
		const FName OutputName;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_EditGetHardness::UVoxelNode_EditGetHardness()
{
	SetInputs(
		EC::Material
	);
	SetOutputs(
		EC::Float
	);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_EditGetHardness::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_EditGetHardness& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, HardnessHandlerVariable("TUniquePtr<FVoxelHardnessHandler>", UniqueName.ToString() + "_HardnessHandler")
		{
		}

		void Init(FVoxelGraphSeed Inputs[], const FVoxelGeneratorInit& InitStruct) override
		{
			if (InitStruct.World)
			{
				HardnessHandler = MakeUnique<FVoxelHardnessHandler>(*InitStruct.World);
			}
		}
		void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine("if (" + FVoxelCppIds::InitStruct + ".World)");
			Constructor.StartBlock();
			Constructor.AddLine(
				HardnessHandlerVariable.CppName +
				" = MakeUnique<FVoxelHardnessHandler>(*" +
				FVoxelCppIds::InitStruct +
				".World);");
			Constructor.EndBlock();
		}

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() =
				HardnessHandler.IsValid()
				? HardnessHandler->GetHardness(Inputs[0].Get<FVoxelMaterial>())
				: 1.f;
		}
		void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() =
				HardnessHandler.IsValid()
				? TVoxelRange<v_flt>(0.f, 1000000.f)
				: TVoxelRange<v_flt>(1.f, 1.f);
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(
				TEXT("%s = %s.IsValid() ? %s->GetHardness(%s) : 1.f;"),
				*Outputs[0],
				*HardnessHandlerVariable.CppName,
				*Inputs[0]);
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(
				TEXT("%s = %s.IsValid() ? TVoxelRange<v_flt>(0.f, 1000000.f) : TVoxelRange<v_flt>(1.f, 1.f);"),
				*Outputs[0],
				*HardnessHandlerVariable.CppName);
		}

	private:
		TUniquePtr<FVoxelHardnessHandler> HardnessHandler;
		const FVoxelVariable HardnessHandlerVariable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}
