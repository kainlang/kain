// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelOptimizationNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "Runtime/VoxelComputeNode.h"
#include "Runtime/VoxelComputeNodeTree.h"
#include "Runtime/VoxelGraphVMUtils.h"
#include "Runtime/Recorders/VoxelGraphEmptyRecorder.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelContext.h"
#include "VoxelMessages.h"
#include "VoxelGraphConstants.h"
#include "VoxelGraphGenerator.h"
#include "NodeFunctions/VoxelNodeFunctions.h"

#include "Async/Async.h"

UVoxelNode_StaticClampFloat::UVoxelNode_StaticClampFloat()
{
	SetInputs(EC::Float);
	SetOutputs(EC::Float);
}

FText UVoxelNode_StaticClampFloat::GetTitle() const
{
	return FText::FromString("Static Clamp: " + FString::SanitizeFloat(Min) + " <= X <= " + FString::SanitizeFloat(Max));
}

TSharedPtr<FVoxelCompilationNode> UVoxelNode_StaticClampFloat::GetCompilationNode() const
{
	return MakeShared<FVoxelRangeAnalysisConstantCompilationNode>(*this);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_StaticClampFloat::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

		FLocalVoxelComputeNode(const UVoxelNode_StaticClampFloat& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Min(Node.Min)
			, Max(Node.Max)
		{
		}

		virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = FMath::Clamp<v_flt>(Inputs[0].Get<v_flt>(), Min, Max);
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = FMath::Clamp<v_flt>(%s, %f, %f);"), *Outputs[0], *Inputs[0], Min, Max);
		}
		virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = { Min, Max };
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = { %ff, %ff };"), *Outputs[0], Min, Max);
		}

	private:
		const float Min;
		const float Max;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_RangeAnalysisDebuggerFloat::UVoxelNode_RangeAnalysisDebuggerFloat()
{
	SetInputs(EC::Float);
	SetOutputs(EC::Float);
}

void UVoxelNode_RangeAnalysisDebuggerFloat::UpdateFromBin()
{
	if (Bins.IsValid())
	{
		Min = Bins->bMinMaxInit ? Bins->MinValue : 0;
		Max = Bins->bMinMaxInit ? Bins->MaxValue : 0;
	}
}

void UVoxelNode_RangeAnalysisDebuggerFloat::UpdateGraph()
{
	Curve.GetRichCurve()->Reset();
	Bins->AddToCurve(*Curve.GetRichCurve());
}

void UVoxelNode_RangeAnalysisDebuggerFloat::Reset()
{
	Bins = MakeUnique<FVoxelBins>(GraphMin, GraphMax, GraphStep);
	UpdateFromBin();
}

#if WITH_EDITOR
void UVoxelNode_RangeAnalysisDebuggerFloat::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		Reset();
		UpdateFromBin();
	}
}

void UVoxelNode_RangeAnalysisDebuggerFloat::PostLoad()
{
	Super::PostLoad();
	Reset();
	UpdateFromBin();
}
#endif

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_RangeAnalysisDebuggerFloat::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

		FLocalVoxelComputeNode(const UVoxelNode_RangeAnalysisDebuggerFloat& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Bins(Node.GraphMin, Node.GraphMax, Node.GraphStep)
			, WeakNode(const_cast<UVoxelNode_RangeAnalysisDebuggerFloat*>(&Node))
		{
		}
		~FLocalVoxelComputeNode()
		{
			AsyncTask(ENamedThreads::GameThread, [WeakNode = WeakNode, Bins = Bins]()
			{
				if (WeakNode.IsValid())
				{
					WeakNode->Bins->AddOtherBins(Bins);
					WeakNode->UpdateFromBin();
					WeakNode->UpdateGraph();
				}
				else
				{
					FVoxelMessages::Error("Range Analysis Debugger node was deleted after the graph, values won't be reported");
				}
			});
		}

		virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = Inputs[0].Get<v_flt>();
			
			Bins.AddStat(Inputs[0].Get<v_flt>());
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s;"), *Outputs[0], *Inputs[0]);
		}
		virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = Inputs[0].Get<v_flt>();
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s;"), *Outputs[0], *Inputs[0]);
		}

	private:
		mutable FVoxelBins Bins;
		TWeakObjectPtr<UVoxelNode_RangeAnalysisDebuggerFloat> WeakNode;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_Sleep::UVoxelNode_Sleep()
{
	SetInputs(EC::Float);
	SetOutputs(EC::Float);
}

UE_DISABLE_OPTIMIZATION
TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_Sleep::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

		FLocalVoxelComputeNode(const UVoxelNode_Sleep& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, NumberOfLoops(Node.NumberOfLoops)
		{
		}
		
		virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			int32 K = 0;
			for (int32 Index = 0; Index < NumberOfLoops; Index++)
			{
				K++;
			}
			check(K == NumberOfLoops);
			Outputs[0] = Inputs[0];
		}

		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s;"), *Outputs[0], *Inputs[0]);
		}
		virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0] = Inputs[0];
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s;"), *Outputs[0], *Inputs[0]);
		}

	private:
		const int32 NumberOfLoops;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}
UE_ENABLE_OPTIMIZATION

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_RangeUnion::UVoxelNode_RangeUnion()
{
	SetInputs(EC::Float);
	SetOutputs(EC::Float);
	SetInputsCount(2, MAX_VOXELNODE_PINS);
}

GENERATED_VOXELNODE_IMPL_PREFIXOPLOOP(UVoxelNode_RangeUnion, FVoxelNodeFunctions::Union, v_flt)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_IsSingleBool::UVoxelNode_IsSingleBool()
{
	SetInputs(EC::Boolean);
	SetOutputs(EC::Boolean);
}

GENERATED_VOXELNODE_IMPL
(
	UVoxelNode_IsSingleBool,
	DEFINE_INPUTS(bool),
	DEFINE_OUTPUTS(bool),
	_O0 = FVoxelNodeFunctions::IsSingleBool(_I0);
)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_GetRangeAnalysis::UVoxelNode_GetRangeAnalysis()
{
	SetInputs(EC::Float);
	AddOutput("Min", "The min value of the input value for the current voxel");
	AddOutput("Max", "The max value of the input value for the current voxel");
}

TSharedPtr<FVoxelCompilationNode> UVoxelNode_GetRangeAnalysis::GetCompilationNode() const
{
	return MakeShared<FVoxelGetRangeAnalysisCompilationNode>(*this);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_GetRangeAnalysis::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

		const int32 VariablesBufferSize;
		const TVoxelSharedRef<FVoxelComputeNodeTree> Tree;

		const TVoxelSharedRef<int32> BufferId = MakeVoxelShared<int32>();

		struct FThreadRangeVariables : TThreadSingleton<FThreadRangeVariables>
		{
			TMap<TVoxelWeakPtr<int32>, TArray<FVoxelNodeRangeType>> Map;
		};

		FLocalVoxelComputeNode(const UVoxelNode& Node, const FVoxelGetRangeAnalysisCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, VariablesBufferSize(CompilationNode.VariablesBufferSize)
			, Tree(CompilationNode.Tree.ToSharedRef())
		{
			check(VariablesBufferSize >= 0);
		}

		virtual void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const override
		{
			TSet<FVoxelComputeNode*> Nodes;
			Tree->GetNodes(Nodes);

			for (auto& Node : Nodes)
			{
				Node->GetPrivateVariables(PrivateVariables);
			}
		}
		virtual void SetupCpp(FVoxelCppConfig& Config) const override
		{
			TSet<FVoxelComputeNode*> Nodes;
			Tree->GetNodes(Nodes);

			for (auto& Node : Nodes)
			{
				Node->CallSetupCpp(Config);
			}
		}

		virtual void Init(FVoxelGraphSeed Inputs[], const FVoxelGeneratorInit& InitStruct) override
		{
			TArray<FVoxelGraphSeed> SeedVariables;
			SeedVariables.SetNumUninitialized(VariablesBufferSize, UE_505_SWITCH(false, EAllowShrinking::No));

			FVoxelGraphVMInitBuffers Buffers(SeedVariables.GetData());
			Tree->Init(InitStruct, Buffers);
		}
		virtual void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override
		{
			Tree->InitCpp(Constructor);
		}

		virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			// Note: GetWorldZ etc might not be set if the node isn't depending on them
			// However, that should have no impact because then the tree isn't using them
			const FVoxelContextRange ContextRange(
				Context.LOD,
				Context.Items,
				Context.LocalToWorld,
				Context.bHasCustomTransform,
				FVoxelIntBox(Context.GetWorldX(), Context.GetWorldY(), Context.GetWorldZ()));

			FVoxelGraphVMComputeRangeBuffers Buffers(GetBuffer());
			FVoxelGraphEmptyRangeRecorder EmptyRecorder;
			Tree->ComputeRange(ContextRange, Buffers, EmptyRecorder);

			const auto ValueRange = Buffers.GraphOutputs.Buffer[FVoxelGraphOutputsIndices::ValueIndex].Get<v_flt>();

			Outputs[0].Get<v_flt>() = ValueRange.Min;
			Outputs[1].Get<v_flt>() = ValueRange.Max;
		}
		virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			FVoxelGraphVMComputeRangeBuffers Buffers(GetBuffer());
			FVoxelGraphEmptyRangeRecorder EmptyRecorder;
			Tree->ComputeRange(Context, Buffers, EmptyRecorder);

			const auto ValueRange = Buffers.GraphOutputs.Buffer[FVoxelGraphOutputsIndices::ValueIndex].Get<v_flt>();

			Outputs[0].Get<v_flt>() = ValueRange;
			Outputs[1].Get<v_flt>() = ValueRange;
		}

		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCppImpl(Outputs, Constructor, false);
		}
		virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCppImpl(Outputs, Constructor, true);
		}

	private:
		FVoxelNodeRangeType* GetBuffer() const
		{
			auto& BuffersMap = FThreadRangeVariables::Get().Map;

			auto* BufferPtr = BuffersMap.Find(BufferId);
			if (BufferPtr)
			{
				// Fast path
				return BufferPtr->GetData();
			}

			// Cleanup
			BuffersMap.Remove(nullptr);
			// Add new one
			auto& Buffer = BuffersMap.Add(BufferId);
			// Resize accordingly
			Buffer.Empty(VariablesBufferSize);
			Buffer.SetNumUninitialized(VariablesBufferSize);
			return Buffer.GetData();
		}
		void ComputeCppImpl(const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor, bool bIsRangeAnalysis) const
		{			
			Constructor.StartBlock();
			Constructor.AddLine("const auto GetRangeAnalysisValue = [this](const FVoxelContextRange& Context)");
			Constructor.StartBlock();
			{
				Constructor.AddLine("TVoxelRange<v_flt> RangeAnalysisValue;");
				Constructor.NewLine();

				auto Permutation = Constructor.Permutation;
				Permutation.AddUnique(FVoxelGraphOutputsIndices::RangeAnalysisIndex);
				FVoxelCppConstructor LocalConstructor(Permutation, Constructor.ErrorReporter);
				
				TArray<FString> GraphOutputs;
				GraphOutputs.SetNum(FVoxelGraphOutputsIndices::ValueIndex + 1);
				GraphOutputs[FVoxelGraphOutputsIndices::ValueIndex] = "RangeAnalysisValue";

				Tree->ComputeRangeCpp(
					LocalConstructor,
					FVoxelVariableAccessInfo::Compute(EVoxelFunctionAxisDependencies::XYZWithoutCache),
					GraphOutputs);

				Constructor.AddOtherConstructor(LocalConstructor);

				Constructor.NewLine();
				Constructor.AddLine("return RangeAnalysisValue;");
			}
			Constructor.EndBlock(true);

			if (bIsRangeAnalysis)
			{
				Constructor.AddLine("const TVoxelRange<v_flt> RangeAnalysisValue = GetRangeAnalysisValue(Context);");
				Constructor.AddLinef(TEXT("%s = RangeAnalysisValue;"), *Outputs[0]);
				Constructor.AddLinef(TEXT("%s = RangeAnalysisValue;"), *Outputs[1]);
			}
			else
			{
				Constructor.AddLine("const TVoxelRange<v_flt> RangeAnalysisValue = GetRangeAnalysisValue(FVoxelContextRange(");
				Constructor.Indent();
				Constructor.AddLine("Context.LOD,");
				Constructor.AddLine("Context.Items,");
				Constructor.AddLine("Context.LocalToWorld,");
				Constructor.AddLine("Context.bHasCustomTransform,");
				Constructor.AddLine("FVoxelIntBox(Context.GetWorldX(), Context.GetWorldY(), Context.GetWorldZ())));");
				Constructor.Unindent();
				Constructor.NewLine();
				Constructor.AddLinef(TEXT("%s = RangeAnalysisValue.Min;"), *Outputs[0]);
				Constructor.AddLinef(TEXT("%s = RangeAnalysisValue.Max;"), *Outputs[1]);
			}
			
			Constructor.EndBlock();
		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, CastCheckedVoxel<const FVoxelGetRangeAnalysisCompilationNode>(InCompilationNode)));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_SmartMin::UVoxelNode_SmartMin()
{
	SetInputs(EC::Float);
	SetOutputs(EC::Float);
	SetInputsCount(3, MAX_VOXELNODE_PINS);
}

TSharedPtr<FVoxelCompilationNode> UVoxelNode_SmartMin::GetCompilationNode() const
{
	const auto Node = MakeShared<FVoxelSmartMinMaxCompilationNode>(*this);
	Node->bIsMin = true;
	return Node;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_SmartMax::UVoxelNode_SmartMax()
{
	SetInputs(EC::Exec, EC::Float);
	SetOutputs(EC::Exec, EC::Float);
	SetInputsCount(3, MAX_VOXELNODE_PINS);
}

TSharedPtr<FVoxelCompilationNode> UVoxelNode_SmartMax::GetCompilationNode() const
{
	const auto Node = MakeShared<FVoxelSmartMinMaxCompilationNode>(*this);
	Node->bIsMin = false;
	return Node;
}
