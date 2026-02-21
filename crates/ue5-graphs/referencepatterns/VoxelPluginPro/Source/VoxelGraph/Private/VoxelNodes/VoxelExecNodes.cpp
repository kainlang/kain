// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelExecNodes.h"

#include "VoxelNodes/VoxelNodeColors.h"
#include "Runtime/VoxelComputeNode.h"
#include "Runtime/VoxelGraphVMUtils.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppIds.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelGraphConstants.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphErrorReporter.h"

int32 UVoxelNode_MaterialSetter::GetOutputIndex() const
{
	return FVoxelGraphOutputsIndices::MaterialIndex;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_SetColor::UVoxelNode_SetColor()
{
	SetInputs(
		EC::Exec,
		{ "Color", EC::Color, "Color" });
	SetOutputs(EC::Exec);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_SetColor::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelSetterComputeNode
	{
	public:
		using FVoxelSetterComputeNode::FVoxelSetterComputeNode;

		void ComputeSetterNode(FVoxelNodeInputBuffer Inputs, FVoxelGraphVMOutputBuffers& GraphOutputs) const override
		{
			GraphOutputs.MaterialBuilder.SetColor(Inputs[0].Get<FColor>());
		}
		void ComputeRangeSetterNode(FVoxelNodeInputRangeBuffer Inputs, FVoxelGraphVMRangeOutputBuffers& GraphOutputs) const override
		{

		}
		void ComputeSetterNodeCpp(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, const TArray<FString>& GraphOutputs) const override
		{
			Constructor.AddLine(FVoxelCppIds::GraphOutputs + ".MaterialBuilder.SetColor(" + Inputs[0] + ");");
		}
		void ComputeRangeSetterNodeCpp(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, const TArray<FString>& GraphOutputs) const override
		{

		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_SetSingleIndex::UVoxelNode_SetSingleIndex()
{
	SetInputs(
		EC::Exec,
		{ "Index", EC::Int, "Index between 0 and 255", "", {0, 255} });
	SetOutputs(EC::Exec);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_SetSingleIndex::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelSetterComputeNode
	{
	public:
		using FVoxelSetterComputeNode::FVoxelSetterComputeNode;

		void ComputeSetterNode(FVoxelNodeInputBuffer Inputs, FVoxelGraphVMOutputBuffers& GraphOutputs) const override
		{
			GraphOutputs.MaterialBuilder.SetSingleIndex(Inputs[0].Get<int32>());
		}
		void ComputeRangeSetterNode(FVoxelNodeInputRangeBuffer Inputs, FVoxelGraphVMRangeOutputBuffers& GraphOutputs) const override
		{

		}
		void ComputeSetterNodeCpp(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, const TArray<FString>& GraphOutputs) const override
		{
			Constructor.AddLine(FVoxelCppIds::GraphOutputs + ".MaterialBuilder.SetSingleIndex(" + Inputs[0] + ");");
		}
		void ComputeRangeSetterNodeCpp(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, const TArray<FString>& GraphOutputs) const override
		{

		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_SetMultiIndexWetness::UVoxelNode_SetMultiIndexWetness()
{
	SetInputs(
		EC::Exec,
		{ "Wetness", EC::Float, "Wetness between 0 and 1", "", {0, 1 } });
	SetOutputs(EC::Exec);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_SetMultiIndexWetness::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelSetterComputeNode
	{
	public:
		using FVoxelSetterComputeNode::FVoxelSetterComputeNode;

		void ComputeSetterNode(FVoxelNodeInputBuffer Inputs, FVoxelGraphVMOutputBuffers& GraphOutputs) const override
		{
			GraphOutputs.MaterialBuilder.SetWetness(Inputs[0].Get<v_flt>());
		}
		void ComputeRangeSetterNode(FVoxelNodeInputRangeBuffer Inputs, FVoxelGraphVMRangeOutputBuffers& GraphOutputs) const override
		{

		}
		void ComputeSetterNodeCpp(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, const TArray<FString>& GraphOutputs) const override
		{
			Constructor.AddLine(FVoxelCppIds::GraphOutputs + ".MaterialBuilder.SetWetness(" + Inputs[0] + ");");
		}
		void ComputeRangeSetterNodeCpp(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, const TArray<FString>& GraphOutputs) const override
		{

		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_AddMultiIndex::UVoxelNode_AddMultiIndex()
{
	SetInputs(
		EC::Exec,
		{ "Index", EC::Int, "Material index between 0 and 255", "", {0, 255 } },
		{ "Strength", EC::Float, "Strength, usually between 0 and 1", "1" },
		{ "Lock Strength", EC::Boolean, "If true, the strength won't be normalized. For example, if you want small rocks with the same density everywhere." });
	SetOutputs(EC::Exec);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_AddMultiIndex::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelSetterComputeNode
	{
	public:
		using FVoxelSetterComputeNode::FVoxelSetterComputeNode;

		void ComputeSetterNode(FVoxelNodeInputBuffer Inputs, FVoxelGraphVMOutputBuffers& GraphOutputs) const override
		{
			GraphOutputs.MaterialBuilder.AddMultiIndex(Inputs[0].Get<int32>(), Inputs[1].Get<v_flt>(), Inputs[2].Get<bool>());
		}
		void ComputeRangeSetterNode(FVoxelNodeInputRangeBuffer Inputs, FVoxelGraphVMRangeOutputBuffers& GraphOutputs) const override
		{

		}
		void ComputeSetterNodeCpp(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, const TArray<FString>& GraphOutputs) const override
		{
			Constructor.AddLine(FVoxelCppIds::GraphOutputs + ".MaterialBuilder.AddMultiIndex(" + Inputs[0] + ", " + Inputs[1] + ", " + Inputs[2] + ");");
		}
		void ComputeRangeSetterNodeCpp(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, const TArray<FString>& GraphOutputs) const override
		{

		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_SetUVs::UVoxelNode_SetUVs()
{
	SetInputs(
		EC::Exec,
		{ "Channel", EC::Int, "Channel, should be 0 or 1", "", {0, 255 } },
		{ "U", EC::Float, "U coordinate between 0 and 1", "", {0, 1} },
		{ "V", EC::Float, "V coordinate between 0 and 1", "", {0, 1} });
	SetOutputs(EC::Exec);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_SetUVs::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelSetterComputeNode
	{
	public:
		const bool bSetU;
		const bool bSetV;
		
		FLocalVoxelComputeNode(const UVoxelNode_SetUVs& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelSetterComputeNode(Node, CompilationNode)
			, bSetU(Node.bSetU)
			, bSetV(Node.bSetV)
		{
		}

		void ComputeSetterNode(FVoxelNodeInputBuffer Inputs, FVoxelGraphVMOutputBuffers& GraphOutputs) const override
		{
			if (bSetU) GraphOutputs.MaterialBuilder.SetU(Inputs[0].Get<int32>(), Inputs[1].Get<v_flt>());
			if (bSetV) GraphOutputs.MaterialBuilder.SetV(Inputs[0].Get<int32>(), Inputs[2].Get<v_flt>());
		}
		void ComputeRangeSetterNode(FVoxelNodeInputRangeBuffer Inputs, FVoxelGraphVMRangeOutputBuffers& GraphOutputs) const override
		{

		}
		void ComputeSetterNodeCpp(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, const TArray<FString>& GraphOutputs) const override
		{
			if (bSetU) Constructor.AddLine(FVoxelCppIds::GraphOutputs + ".MaterialBuilder.SetU(" + Inputs[0] + ", " + Inputs[1] + ");");
			if (bSetV) Constructor.AddLine(FVoxelCppIds::GraphOutputs + ".MaterialBuilder.SetV(" + Inputs[0] + ", " + Inputs[2] + ");");
		}
		void ComputeRangeSetterNodeCpp(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, const TArray<FString>& GraphOutputs) const override
		{

		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_SetNode::UVoxelNode_SetNode()
{
	SetInputs(EC::Exec, EC::Exec);
	SetOutputs(EC::Exec);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_SetNode::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelSetterComputeNode
	{
	public:
		const uint32 Index;

		FLocalVoxelComputeNode(const UVoxelNode& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelSetterComputeNode(Node, CompilationNode)
			, Index(CastCheckedVoxel<const FVoxelSetterCompilationNode>(CompilationNode).OutputIndex)
		{
		}

		void ComputeSetterNode(FVoxelNodeInputBuffer Inputs, FVoxelGraphVMOutputBuffers& GraphOutputs) const override
		{
			if (Index == FVoxelGraphOutputsIndices::MaterialIndex)
			{
				GraphOutputs.MaterialBuilder = Inputs[0].template Get<FVoxelMaterial>();
			}
			else
			{
				GraphOutputs.Buffer[Index] = Inputs[0];
			}
		}
		void ComputeRangeSetterNode(FVoxelNodeInputRangeBuffer Inputs, FVoxelGraphVMRangeOutputBuffers& GraphOutputs) const override
		{
			// No material in range
			GraphOutputs.Buffer[Index] = Inputs[0];
		}
		void ComputeSetterNodeCpp(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, const TArray<FString>& GraphOutputs) const override
		{
			Constructor.AddLine(GraphOutputs[Index] + " = " + Inputs[0] + ";");
		}
		void ComputeRangeSetterNodeCpp(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, const TArray<FString>& GraphOutputs) const override
		{
			Constructor.AddLine(GraphOutputs[Index] + " = " + Inputs[0] + ";");
		}
	};
	return MakeVoxelShared<FLocalVoxelComputeNode>(*this, InCompilationNode);
}

FText UVoxelNode_SetNode::GetTitle() const
{
	return FText::FromString("Set " + CachedOutput.Name.ToString());
}

EVoxelPinCategory UVoxelNode_SetNode::GetInputPinCategory(int32 PinIndex) const
{
	return PinIndex == 0
		       ? EVoxelPinCategory::Exec
		       : FVoxelPinCategory::DataPinToPin(CachedOutput.Category);
}

FName UVoxelNode_SetNode::GetInputPinName(int32 PinIndex) const
{
	return PinIndex == 0 ? FName() : CachedOutput.Name;
}

void UVoxelNode_SetNode::LogErrors(FVoxelGraphErrorReporter& ErrorReporter)
{
	Super::LogErrors(ErrorReporter);
	
#if WITH_EDITOR
	if (!UpdateSetterNode())
	{
		ErrorReporter.AddMessageToNode(this, "invalid output", EVoxelGraphNodeMessageType::Error);
	}
#endif
}

int32 UVoxelNode_SetNode::GetOutputIndex() const
{
	return Index;
}

#if WITH_EDITOR
bool UVoxelNode_SetNode::UpdateSetterNode()
{
	if (Graph)
	{
		auto Outputs = Graph->GetOutputs();
		FVoxelGraphOutput NewOutput;
		if (Outputs.Contains(Index) && !FVoxelGraphOutputsUtils::IsVoxelGraphOutputHidden(Index))
		{
			NewOutput = Outputs[Index];
		}
		if (CachedOutput.GUID.IsValid() && NewOutput.GUID != CachedOutput.GUID)
		{
			// Try to find it by GUID and name
			TArray<FVoxelGraphOutput> OutputsArray;
			Outputs.GenerateValueArray(OutputsArray);
			auto* NewOutputPtr = OutputsArray.FindByPredicate([&](auto& Output) {return Output.GUID == CachedOutput.GUID; });
			if (!NewOutputPtr)
			{
				NewOutputPtr = OutputsArray.FindByPredicate([&](auto& Output) {return Output.Name == CachedOutput.Name; });
			}
			if (NewOutputPtr)
			{
				NewOutput = *NewOutputPtr;
				Index = NewOutputPtr->Index;
			}
			else
			{
				return false;
			}
		}
		const bool bDiffCategory = CachedOutput.Category != NewOutput.Category;
		const bool bDiffName = CachedOutput.Name != NewOutput.Name;
		if (GraphNode && (bDiffCategory || bDiffName))
		{
			CachedOutput = NewOutput;
			GraphNode->ReconstructNode();
			if (bDiffCategory)
			{
				Graph->CompileVoxelNodesFromGraphNodes();
			}
		}
	}
	return CachedOutput.GUID.IsValid();
}

void UVoxelNode_SetNode::SetIndex(uint32 NewIndex)
{
	Index = NewIndex;
	UpdateSetterNode();
}

void UVoxelNode_SetNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		UpdateSetterNode();
	}
}

void UVoxelNode_SetNode::PostLoad()
{
	Super::PostLoad();
	UpdateSetterNode();
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_FunctionSeparator::UVoxelNode_FunctionSeparator()
{
	SetColor(FVoxelNodeColors::ExecNode);
	AddInput("", "", EC::Exec);
	AddOutput("", "", EC::Exec);
}

TSharedPtr<FVoxelCompilationNode> UVoxelNode_FunctionSeparator::GetCompilationNode() const
{
	return MakeShared<FVoxelFunctionSeparatorCompilationNode>(*this);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FLinearColor UVoxelNode_FlowMerge::GetColor() const
{
	return FColor::White;
}

EVoxelPinCategory UVoxelNode_FlowMerge::GetInputPinCategory(int32 PinIndex) const
{
	PinIndex = PinIndex % (Types.Num() + 1);
	return PinIndex == 0 ? EVoxelPinCategory::Exec : FVoxelPinCategory::DataPinToPin(Types[PinIndex - 1].Type);
}

EVoxelPinCategory UVoxelNode_FlowMerge::GetOutputPinCategory(int32 PinIndex) const
{
	return PinIndex == 0 ? EVoxelPinCategory::Exec : FVoxelPinCategory::DataPinToPin(Types[PinIndex - 1].Type);
}

FName UVoxelNode_FlowMerge::GetInputPinName(int32 PinIndex) const
{
	const bool bIsA = PinIndex <= Types.Num();
	PinIndex = PinIndex % (Types.Num() + 1);
	if (PinIndex == 0)
	{
		return bIsA ? FName("Exec A") : FName("Exec B");
	}
	else
	{
		return FName(*(Types[PinIndex - 1].Name + (bIsA ? " A" : " B")));
	}
}

FName UVoxelNode_FlowMerge::GetOutputPinName(int32 PinIndex) const
{
	return PinIndex == 0 ? FName("Exec") : FName(*Types[PinIndex - 1].Name);
}

int32 UVoxelNode_FlowMerge::GetMinInputPins() const
{
	return 2 + 2 * Types.Num();
}

int32 UVoxelNode_FlowMerge::GetMaxInputPins() const
{
	return GetMinInputPins();
}

int32 UVoxelNode_FlowMerge::GetOutputPinsCount() const
{
	return 1 + Types.Num();
}

TSharedPtr<FVoxelCompilationNode> UVoxelNode_FlowMerge::GetCompilationNode() const
{
	return MakeShared<FVoxelFlowMergeCompilationNode>(*this);
}
