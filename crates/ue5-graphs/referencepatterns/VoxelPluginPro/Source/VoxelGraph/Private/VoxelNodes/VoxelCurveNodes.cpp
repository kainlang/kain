// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelCurveNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "VoxelNodes/VoxelNodeVariables.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphErrorReporter.h"
#include "NodeFunctions/VoxelNodeFunctions.h"

#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"

UVoxelNode_Curve::UVoxelNode_Curve()
{
	SetInputs(EC::Float);
	SetOutputs(EC::Float);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_Curve::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_Curve& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Curve(Node.Curve)
			, Variable(MakeShared<FVoxelCurveVariable>(Node, Node.Curve))
		{
		}

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve, Inputs[0].Get<v_flt>());
		}
		void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve, Inputs[0].Get<v_flt>());
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = FVoxelNodeFunctions::GetCurveValue(" + Variable->CppName + ", " + Inputs[0] + ");");
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
		const FVoxelRichCurve Curve;
		const TSharedRef<FVoxelCurveVariable> Variable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

FText UVoxelNode_Curve::GetTitle() const
{
	return FText::Format(VOXEL_LOCTEXT("Float Curve: {0}"), Super::GetTitle());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_CurveColor::UVoxelNode_CurveColor()
{
	SetInputs(EC::Float);
	SetOutputs(
		{ "R", EC::Float, "Red between 0 and 1" },
		{ "G", EC::Float, "Green between 0 and 1" },
		{ "B", EC::Float, "Blue between 0 and 1" },
		{ "A", EC::Float, "Alpha between 0 and 1" });
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_CurveColor::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_CurveColor& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Curve(Node.Curve)
			, Variable(MakeShared<FVoxelColorCurveVariable>(Node, Node.Curve))
		{
		}

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[0], Inputs[0].Get<v_flt>());
			Outputs[1].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[1], Inputs[0].Get<v_flt>());
			Outputs[2].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[2], Inputs[0].Get<v_flt>());
			Outputs[3].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[3], Inputs[0].Get<v_flt>());
		}
		void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[0], Inputs[0].Get<v_flt>());
			Outputs[1].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[1], Inputs[0].Get<v_flt>());
			Outputs[2].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[2], Inputs[0].Get<v_flt>());
			Outputs[3].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[3], Inputs[0].Get<v_flt>());
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = FVoxelNodeFunctions::GetCurveValue(" + Variable->CppName + ".Curves[0], " + Inputs[0] + ");");
			Constructor.AddLine(Outputs[1] + " = FVoxelNodeFunctions::GetCurveValue(" + Variable->CppName + ".Curves[1], " + Inputs[0] + ");");
			Constructor.AddLine(Outputs[2] + " = FVoxelNodeFunctions::GetCurveValue(" + Variable->CppName + ".Curves[2], " + Inputs[0] + ");");
			Constructor.AddLine(Outputs[3] + " = FVoxelNodeFunctions::GetCurveValue(" + Variable->CppName + ".Curves[3], " + Inputs[0] + ");");
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
		const FVoxelColorRichCurve Curve;
		const TSharedRef<FVoxelColorCurveVariable> Variable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

FText UVoxelNode_CurveColor::GetTitle() const
{
	return FText::Format(VOXEL_LOCTEXT("Color Curve: {0}"), Super::GetTitle());
}