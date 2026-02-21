// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelParameterNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "VoxelNodes/VoxelNodeVariables.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppUtils.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "CppTranslation/VoxelVariables.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"

template<typename T>
class TVoxelParameterComputeNode : public FVoxelDataComputeNode
{
public:
	GENERATED_DATA_COMPUTE_NODE_BODY_IMPL(TVoxelParameterComputeNode<T>);

	template<typename TNode>
	TVoxelParameterComputeNode(const TNode& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelDataComputeNode(Node, CompilationNode)
		, Value(Node.template GetParameter<T>())
		, Variable(MakeShared<FVoxelExposedVariable>(Node, FVoxelCppUtils::TypeToString<T>(), FVoxelCppUtils::TypeToString<T>(), FVoxelCppUtils::LexToCpp(Node.GetValue())))
	{
	}

#if VOXEL_ENGINE_VERSION >= 504
	using Type = typename std::conditional_t<std::is_same_v<T, float>, v_flt, T>;
#else
	using Type = typename TChooseClass<std::is_same_v<T, float>, v_flt, T>::Result;
#endif

	void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
	{
		Outputs[0].Get<Type>() = Value;
	}
	void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
	{
		Outputs[0].Get<Type>() = Value;
	}
	void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
	{
		Constructor.AddLine(Outputs[0] + " = " + Variable->CppName + ";");
	}
	void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
	{
		ComputeCpp(Inputs, Outputs, Constructor);
	}
	void SetupCpp(FVoxelCppConfig& Config) const override
	{
		Config.AddExposedVariable(Variable);
	}

private:
	const T Value;
	const TSharedRef<FVoxelExposedVariable> Variable;
};

UVoxelNode_FloatParameter::UVoxelNode_FloatParameter()
{
	SetOutputs(EC::Float);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_FloatParameter::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	return MakeVoxelShared<TVoxelParameterComputeNode<float>>(*this, InCompilationNode);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_IntParameter::UVoxelNode_IntParameter()
{
	SetOutputs(EC::Int);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_IntParameter::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	return MakeVoxelShared<TVoxelParameterComputeNode<int32>>(*this, InCompilationNode);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_ColorParameter::UVoxelNode_ColorParameter()
{
	SetOutputs(EC::Color);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_ColorParameter::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_ColorParameter& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Value(Node.GetParameter<FLinearColor>().ToFColor(false))
			, Variable(MakeShared<FVoxelColorVariable>(Node, Node.Color))
		{
		}

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<FColor>() = Value;
		}
		void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<FColor>() = Value;
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = " + Variable->CppName + ";");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}
		void SetupCpp(FVoxelCppConfig& Config) const override
		{
			Config.AddExposedVariable(Variable);
		}

	private:
		const FColor Value;
		const TSharedRef<FVoxelExposedVariable> Variable;
	};
	return MakeVoxelShared<FLocalVoxelComputeNode>(*this, InCompilationNode);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_BoolParameter::UVoxelNode_BoolParameter()
{
	SetOutputs(EC::Boolean);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_BoolParameter::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	return MakeVoxelShared<TVoxelParameterComputeNode<bool>>(*this, InCompilationNode);
}
