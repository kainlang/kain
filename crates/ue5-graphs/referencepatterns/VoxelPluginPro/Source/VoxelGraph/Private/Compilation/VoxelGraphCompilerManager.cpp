// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Compilation/VoxelGraphCompilerManager.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "Compilation/VoxelCompilationNodeTree.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"

#include "Passes/VoxelCompactPassthroughsPass.h"
#include "Passes/VoxelOptimizationsPass.h"
#include "Passes/VoxelReplaceLocalVariablesPass.h"
#include "Passes/VoxelReplaceSmartMinMaxPass.h"
#include "Passes/VoxelGetRangeAnalysisPass.h"
#include "Passes/VoxelFunctionsPass.h"
#include "Passes/VoxelSetIdsPass.h"
#include "Passes/VoxelDependenciesPass.h"
#include "Passes/VoxelMacrosPass.h"
#include "Passes/VoxelFlowMergePass.h"
#include "Passes/ReplaceBiomeMergePass.h"
#include "Passes/RangeAnalysisPass.h"

#include "Runtime/VoxelDefaultComputeNodes.h"
#include "Runtime/VoxelGraphFunction.h"
#include "Runtime/VoxelGraph.h"
#include "Runtime/VoxelComputeNodeTree.h"
#include "Runtime/VoxelGraphChecker.h"
#include "Runtime/VoxelCompiledGraphs.h"

#include "VoxelGraphPreviewSettings.h"
#include "VoxelGraphErrorReporter.h"
#include "IVoxelGraphEditor.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphConstants.h"
#include "VoxelNode.h"
#include "VoxelMinimal.h"

#include "VoxelNodes/VoxelExecNodes.h"

#include "Misc/MessageDialog.h"
#include "EdGraph/EdGraphPin.h"

FVoxelGraphCompilerManager::FVoxelGraphCompilerManager(
	UVoxelGraphGenerator* Graph,
	bool bEnableOptimizations,
	bool bPreview,
	const UVoxelGraphPreviewSettings* PreviewSettings,
	bool bAutomaticPreview,
	bool bOnlyShowAxisDependencies)
	: Graph(Graph)
	, bEnableOptimizations(bEnableOptimizations)
	, bPreview(bPreview)
	, PreviewSettings(PreviewSettings)
	, bAutomaticPreview(bAutomaticPreview)
	, bOnlyShowAxisDependencies(bOnlyShowAxisDependencies)
{
	check(Graph);
	check(!bPreview || PreviewSettings);
	FVoxelGraphCompiler::IncreaseCompilationId();
}

FVoxelGraphCompilerManager::~FVoxelGraphCompilerManager()
{
	// UniquePtr forward decl
}

bool FVoxelGraphCompilerManager::Compile(FVoxelCompiledGraphs& OutGraphs)
{
	VOXEL_FUNCTION_COUNTER();
	
	FVoxelGraphCompiler Compiler{ Graph };

	if (!bOnlyShowAxisDependencies)
	{
		FVoxelGraphErrorReporter::ClearCompilationMessages(Graph);
	}

#if VOXEL_ENGINE_VERSION >= 504
	TArray<TObjectPtr<UVoxelNode>>& Nodes = Graph->AllNodes;
#else
	TArray<UVoxelNode*>& Nodes = Graph->AllNodes;
#endif

	Nodes.RemoveAll([](UVoxelNode* Node) { return !IsValid(Node); });
	UVoxelNode* FirstNode = Graph->FirstNode;
	const int32 FirstNodePinIndex = FirstNode ? FirstNode->GetInputPinIndex(Graph->FirstNodePinId) : 0;

	NodesMap = Compiler.InitFromNodes(Nodes, FirstNode, FirstNodePinIndex);

	CompileInternal(Compiler, OutGraphs);

	if (Compiler.ErrorReporter.HasError() && bOnlyShowAxisDependencies)
	{
		// Early exit so that errors are only showed once (and never showed when bOnlyShowAxisDependencies is true)
		return false;
	}
	
	Compiler.ErrorReporter.Apply(!bAutomaticPreview);

	if (Compiler.ErrorReporter.HasError())
	{
		OutGraphs = {};
		return false;
	}
	else
	{
		OutGraphs.Compact();
		return true;
	}
}

#define CHECK_NO_ERRORS() if (Compiler.ErrorReporter.HasError()) return false;

bool FVoxelGraphCompilerManager::CompileInternal(FVoxelGraphCompiler& Compiler, FVoxelCompiledGraphs& OutGraphs)
{
	VOXEL_FUNCTION_COUNTER();
	
	CHECK_NO_ERRORS();

	// Replace portal in current graph only. To have valid refs, each macro will do it for their own graphs
	Compiler.ApplyPass<FVoxelReplaceLocalVariablesPass>();

	if (Graph->DebugLevel == EVoxelGraphGeneratorDebugLevel::BeforeMacroInlining)
	{
		DebugCompiler(Compiler);
	}

	Compiler.ApplyPass<FVoxelGraphInlineMacrosPass>(/*bClearMessages*/ !bOnlyShowAxisDependencies);
	Compiler.ApplyPass<FVoxelGraphReplaceMacroInputOutputsPass>();

	if (Graph->DebugLevel == EVoxelGraphGeneratorDebugLevel::AfterMacroInlining)
	{
		DebugCompiler(Compiler);
	}

	Compiler.ApplyPass<FVoxelReplaceBiomeMergePass>();

	if (Graph->DebugLevel == EVoxelGraphGeneratorDebugLevel::AfterBiomeMergeReplace)
	{
		DebugCompiler(Compiler);
	}

	CHECK_NO_ERRORS();

	Compiler.ApplyPass<FVoxelReplaceSmartMinMaxPass>();

	if (Graph->DebugLevel == EVoxelGraphGeneratorDebugLevel::AfterSmartMinMaxReplace)
	{
		DebugCompiler(Compiler);
	}

	CHECK_NO_ERRORS();

	if (bPreview)
	{
		SetupPreview(Compiler);
	}
	if (!Compiler.FirstNode)
	{
		// Create a dummy set value node to avoid spamming "First node not connected"
		auto* SetValueNode = Compiler.AddNode(GetDefault<UVoxelNode_SetValueNode>()->GetCompilationNode());
		SetValueNode->GetInputPin(1).SetDefaultValue("1");
		Compiler.FirstNode = SetValueNode;
	}

	CHECK_NO_ERRORS();
	
	check(Compiler.FirstNode);

	Compiler.ApplyPass<FVoxelAddFirstFunctionPass>();

	const auto Outputs = Graph->GetOutputs();
	const auto Permutations = Graph->GetPermutations();

	for (auto& Permutation : Permutations)
	{
		const FString OutputName = FVoxelGraphOutputsUtils::GetPermutationName(Permutation, Outputs);
		TSharedRef<FVoxelGraphCompiler> TargetCompiler = Compiler.Clone("target " + OutputName);
		if (!CompileOutput(Permutation, OutputName, *TargetCompiler, OutGraphs.Add(Permutation)))
		{
			return false;
		}
	}

	return true;
}

struct FVoxelCompilationFunction
{
	FVoxelGraphCompilerManager& Manager;
	const int32 FunctionId;
	const TSharedRef<FVoxelGraphCompiler> Compiler;
	FVoxelGraphFunctions CompiledFunctions;

	FVoxelCompilationFunction(
		FVoxelGraphCompilerManager& Manager,
		int32 FunctionId,
		const TSharedRef<FVoxelGraphCompiler>& Compiler)
		: Manager(Manager)
		, FunctionId(FunctionId)
		, Compiler(Compiler)
	{
	}

	inline bool HasErrors() const
	{
		return Compiler->ErrorReporter.HasError();
	}

	void PreCompile()
	{
		Compiler->ApplyPass<FVoxelReplaceFunctionSeparatorsPass>();
		Compiler->ApplyPass<FVoxelReplaceFlowMergePass>();

		Manager.Optimize(*Compiler);

		// Must be done after optimizing
		Compiler->ApplyPass<FVoxelFixMultipleOutputsExecPass>();

		// Make sure to do it before FVoxelMarkDependenciesPass!
		Compiler->ApplyPass<FVoxelGetRangeAnalysisPass>();

		Compiler->ApplyPass<FVoxelMarkDependenciesPass>();
	}

	void AssignPinsIds(int32& Id)
	{
		Compiler->ApplyPass<FVoxelSetPinsIdsPass>(Id);
	}

	void FindConstants(
		FVoxelCreatedComputeNodes& CreatedNodes,
		TArray<TVoxelSharedRef<FVoxelDataComputeNode>>& OutConstantNodes,
		TArray<TVoxelSharedRef<FVoxelSeedComputeNode>>& OutSeedNodes)
	{
		// Must be done after MarkDependenciesPass & SetPinsIdsPass
		Compiler->ApplyPass<FVoxelGetSortedConstantsAndRemoveConstantsPass>(CreatedNodes, OutConstantNodes, OutSeedNodes);
	}

	void Compile(
		FVoxelCreatedComputeNodes& CreatedNodes,
		TArray<FVoxelFunctionCallComputeNode*>& OutFunctionCallsToLink,
		const bool bDebug)
	{
		VOXEL_FUNCTION_COUNTER();
		
		const auto FunctionInit = CastCheckedVoxel<FVoxelFunctionInitCompilationNode>(Compiler->FirstNode).GetComputeNode();
		const auto CompilationTree = FVoxelCompilationNodeTree::Create(Compiler->FirstNode);
		
		TMap<EVoxelFunctionAxisDependencies, TVoxelSharedRef<FVoxelGraphFunction>> Functions;
		for (EVoxelFunctionAxisDependencies Dependencies : FVoxelAxisDependencies::GetAllFunctionDependencies())
		{
			TSet<FVoxelCompilationNode*> UsedNodes;
			auto ComputeTree = MakeVoxelShared<FVoxelComputeNodeTree>();
			CompilationTree->ConvertToComputeNodeTree(Dependencies, *ComputeTree, OutFunctionCallsToLink, UsedNodes, CreatedNodes);
			Functions.Add(Dependencies, MakeShareable(new FVoxelGraphFunction(ComputeTree, FunctionInit.ToSharedRef(), FunctionId, Dependencies)));

			// Debug
			if (Manager.Graph->DebugLevel == EVoxelGraphGeneratorDebugLevel::Axis &&
				Manager.Graph->FunctionToDebug == FunctionId &&
				Manager.Graph->AxisDependenciesToDebug == Dependencies &&

				bDebug)
			{
				Manager.DebugNodes(UsedNodes);
			}
		}

		CompiledFunctions = FVoxelGraphFunctions(
			FunctionId,
			Functions[EVoxelFunctionAxisDependencies::X],
			Functions[EVoxelFunctionAxisDependencies::XYWithCache],
			Functions[EVoxelFunctionAxisDependencies::XYWithoutCache],
			Functions[EVoxelFunctionAxisDependencies::XYZWithCache],
			Functions[EVoxelFunctionAxisDependencies::XYZWithoutCache]
		);
	}
};

bool FVoxelGraphCompilerManager::CompileOutput(const FVoxelGraphPermutationArray& Permutation, const FString& OutputName, FVoxelGraphCompiler& Compiler, TVoxelSharedPtr<FVoxelGraph>& OutGraph)
{
	VOXEL_FUNCTION_COUNTER();
	
	Compiler.ApplyPass<FVoxelOptimizeForPermutationPass>(Permutation);

	TMap<FName, FString> Constants;
	Constants.Add("IsRangeAnalysis", LexToString(Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex)));
	Constants.Add("IsPreview", LexToString(bPreview));
	Constants.Add(*("Target=" + OutputName), LexToString(true));
	if(bPreview)
	{
		Constants.Add("Preview.LeftToRight", LexToString(int32(PreviewSettings->LeftToRight)));
		Constants.Add("Preview.BottomToTop", LexToString(int32(PreviewSettings->BottomToTop)));
		Constants.Add("Preview.Center.X", LexToString(PreviewSettings->Center.X));
		Constants.Add("Preview.Center.Y", LexToString(PreviewSettings->Center.Y));
		Constants.Add("Preview.Center.Z", LexToString(PreviewSettings->Center.Z));
		Constants.Add("Preview.ResolutionScale", LexToString(PreviewSettings->ResolutionMultiplierLog));
		Constants.Add("Preview.PreviewType2D", LexToString(int32(PreviewSettings->PreviewType2D)));
		Constants.Add("Preview.MaterialConfig", LexToString(int32(PreviewSettings->MaterialConfig)));
		Constants.Add("Preview.HeightBasedColor", LexToString(PreviewSettings->bHeightBasedColor));
		Constants.Add("Preview.EnableWater", LexToString(PreviewSettings->bEnableWater));
		Constants.Add("Preview.MinValue", LexToString(PreviewSettings->NormalizeMinValue));
		Constants.Add("Preview.MaxValue", LexToString(PreviewSettings->NormalizeMaxValue));
		Constants.Add("Preview.Resolution", LexToString(PreviewSettings->Resolution));
		Constants.Add("Preview.LODToPreview", LexToString(PreviewSettings->LODToPreview));
		Constants.Add("Preview.Height", LexToString(PreviewSettings->Height));
	}
	Compiler.ApplyPass<FVoxelReplaceCompileTimeConstantsPass>(Constants);
	
	CHECK_NO_ERRORS();

	if (Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex))
	{
		Compiler.ApplyPass<FVoxelDisconnectRangeAnalysisConstantsPass>();
		Compiler.ApplyPass<FVoxelRemoveAllSeedNodesPass>();
	}

	CHECK_NO_ERRORS();

	Optimize(Compiler);

	CHECK_NO_ERRORS();
	
	const bool bDebug = Graph->TargetToDebug.ToLower() == OutputName.ToLower();
	if (Graph->DebugLevel == EVoxelGraphGeneratorDebugLevel::BeforeFillFunctionSeparators && bDebug)
	{
		DebugCompiler(Compiler);
	}
	
	Compiler.ApplyPass<FVoxelFillFunctionSeparatorsPass>();

	CHECK_NO_ERRORS();

	if (Graph->DebugLevel == EVoxelGraphGeneratorDebugLevel::Output && bDebug)
	{
		DebugCompiler(Compiler);
	}

	CHECK_NO_ERRORS();

	Compiler.ApplyPass<FVoxelSetFunctionsIdsPass>();

	TArray<FVoxelCompilationFunctionDescriptor> FunctionsDescriptors;
	Compiler.ApplyPass<FVoxelFindFunctionsPass>(FunctionsDescriptors);

	CHECK_NO_ERRORS();

	if (Graph->bShowFunctions && (bDebug || Graph->bDetailedErrors))
	{
		for (auto& FunctionDescriptor : FunctionsDescriptors)
		{
			check(FunctionDescriptor.FirstNode);
			for (auto& Node : FunctionDescriptor.Nodes)
			{
				Compiler.ErrorReporter.AddMessageToNode(Node, FString::Printf(TEXT("function %d"), FunctionDescriptor.FunctionId), EVoxelGraphNodeMessageType::Info, false);
			}
		}
	}

	TArray<FVoxelCompilationFunction> Functions;
	Functions.Reserve(FunctionsDescriptors.Num()); // Else the array will realloc and the ptrs be invalid
	TMap<int32, FVoxelCompilationFunction*> FunctionIdsToFunctions;
	for (auto& FunctionDescriptor : FunctionsDescriptors)
	{
		TMap<FVoxelCompilationNode*, FVoxelCompilationNode*> OldNodesToNewNodes;
		auto FunctionCompiler = Compiler.Clone(FString::Printf(TEXT("function %d"), FunctionDescriptor.FunctionId), OldNodesToNewNodes);

		FunctionCompiler->FirstNode = OldNodesToNewNodes[FunctionDescriptor.FirstNode];

		TSet<FVoxelCompilationNode*> NodesToKeep;
		for (auto* Node : FunctionDescriptor.Nodes)
		{
			NodesToKeep.Add(OldNodesToNewNodes[Node]);
		}
		FunctionCompiler->ApplyPass<FVoxelRemoveNodesOutsideFunction>(NodesToKeep);

		if (FunctionCompiler->ErrorReporter.HasError())
		{
			return false;
		}

		int32 Index = Functions.Emplace(*this, FunctionDescriptor.FunctionId, FunctionCompiler);
		check(!FunctionIdsToFunctions.Contains(FunctionDescriptor.FunctionId));
		FunctionIdsToFunctions.Add(FunctionDescriptor.FunctionId, &Functions[Index]);
	}
	for (auto& Function : Functions)
	{
		Function.PreCompile();
		if (Function.HasErrors())
		{
			return false;
		}
	}

	int32 Id = 0;
	for (auto& Function : Functions)
	{
		Function.AssignPinsIds(Id);
	}

	// Debug here as FindConstants will remove some nodes
	if (Graph->DebugLevel == EVoxelGraphGeneratorDebugLevel::Function && bDebug)
	{
		auto* Function = FunctionIdsToFunctions.Find(Graph->FunctionToDebug);
		if (Function)
		{
			DebugCompiler(*(*Function)->Compiler);
		}
	}
	if (bOnlyShowAxisDependencies && (bDebug || Graph->bDetailedErrors))
	{
		for (auto& Function : Functions)
		{
			Function.Compiler->ApplyPass<FVoxelDebugDependenciesPass>();
		}
	}

	FVoxelCreatedComputeNodes CreatedNodes;
	TArray<TVoxelSharedRef<FVoxelDataComputeNode>> ConstantNodes;
	TArray<TVoxelSharedRef<FVoxelSeedComputeNode>> SeedNodes;
	for (auto& Function : Functions)
	{
		Function.FindConstants(CreatedNodes, ConstantNodes, SeedNodes);
	}

	TArray<FVoxelFunctionCallComputeNode*> FunctionCallsToLink;
	for (auto& Function : Functions)
	{
		Function.Compile(CreatedNodes, FunctionCallsToLink, bDebug);
	}

	for (auto* FunctionCall : FunctionCallsToLink)
	{
		FVoxelCompilationFunction* Function = FunctionIdsToFunctions.FindChecked(FunctionCall->FunctionId);
		FunctionCall->SetFunctions(Function->CompiledFunctions);
	}

	TArray<FVoxelGraphFunctions> AllCompiledFunctions;
	for (auto& Function : Functions)
	{
		AllCompiledFunctions.Add(Function.CompiledFunctions);
	}
	AllCompiledFunctions.Sort([](auto& A, auto& B) { return A.FunctionId < B.FunctionId; });
	check(AllCompiledFunctions[0].FunctionId == 0);

	OutGraph = MakeShareable(new FVoxelGraph(
		OutputName,
		AllCompiledFunctions,
		FunctionIdsToFunctions.FindChecked(0)->CompiledFunctions,
		ConstantNodes,
		SeedNodes,
		Id));

	FVoxelGraphChecker::CheckGraph(Compiler.ErrorReporter, *OutGraph);

	return true;
}

#undef CHECK_NO_ERRORS

void FVoxelGraphCompilerManager::SetupPreview(FVoxelGraphCompiler& Compiler)
{
#if WITH_EDITORONLY_DATA
	auto* PreviewedGraphPin = Graph->PreviewedPin.Get();
	if (!PreviewedGraphPin)
	{
		return;
	}

	auto* PreviewedGraphNode = Cast<UVoxelGraphNodeInterface>(PreviewedGraphPin->GetOwningNode());
	if (!ensure(PreviewedGraphNode))
	{
		return;
	}
	auto* PreviewedVoxelNode = PreviewedGraphNode->GetVoxelNode();
	if (!ensure(PreviewedVoxelNode))
	{
		return;
	}
	const int32 PreviewedPinIndex = PreviewedVoxelNode->GetOutputPinIndex(PreviewedGraphPin->PinId);
	if (!ensure(PreviewedPinIndex != -1))
	{
		return;
	}
	if(!ensure(NodesMap.Contains(PreviewedVoxelNode)))
	{
		return;
	}
	
	FVoxelCompilationNode* PreviewedCompilationNode = NodesMap.FindChecked(PreviewedVoxelNode);
	int32 PinIndex = PreviewedPinIndex;
	
	if (auto* MacroNode = CastVoxel<FVoxelMacroCompilationNode>(PreviewedCompilationNode))
	{
		auto* OutputNode = MacroNode->OutputNode;
		check(OutputNode->IsA<FVoxelMacroInputOuputCompilationNode>());
		PreviewedCompilationNode = OutputNode;
	}

	if (auto* MacroOutputNode = CastVoxel<FVoxelMacroInputOuputCompilationNode>(PreviewedCompilationNode))
	{
		auto* Passthrough = MacroOutputNode->Passthroughs[PinIndex];
		check(Passthrough);

		PinIndex = 0;
		PreviewedCompilationNode = Passthrough;
	}

	if (auto* PortalNode = CastVoxel<FVoxelLocalVariableUsageCompilationNode>(PreviewedCompilationNode))
	{
		check(PinIndex == 0);
		PreviewedCompilationNode = PortalNode->Passthrough;
		if (!PreviewedCompilationNode)
		{
			// Should never happen
			Compiler.ErrorReporter.AddError("Invalid local variable!");
			Compiler.ErrorReporter.AddNodeToSelect(PortalNode);
			return;
		}
	}

	if (auto* BiomeNode = CastVoxel<FVoxelBiomeMergeCompilationNode>(PreviewedCompilationNode))
	{
		if(!BiomeNode->OutputPassthroughs.IsValidIndex(PinIndex - 1))
		{
			// Should never happen
			Compiler.ErrorReporter.AddError("Invalid biome merge node!");
			Compiler.ErrorReporter.AddNodeToSelect(BiomeNode);
			return;
		}
		PreviewedCompilationNode = BiomeNode->OutputPassthroughs[PinIndex - 1];
		check(PreviewedCompilationNode);
		PinIndex = 0;
	}

	if (auto* SmartNode = CastVoxel<FVoxelSmartMinMaxCompilationNode>(PreviewedCompilationNode))
	{
		ensure(PinIndex == 1);
		PreviewedCompilationNode = SmartNode->OutputPassthrough;
		check(PreviewedCompilationNode);
		PinIndex = 0;
	}

	auto SetValueNode = Compiler.AddNode(GetDefault<UVoxelNode_SetValueNode>()->GetCompilationNode());

	auto& Pin = PreviewedCompilationNode->GetOutputPin(PinIndex);
	check(Pin.PinCategory == EVoxelPinCategory::Float);
	SetValueNode->GetInputPin(1).LinkTo(Pin);

	auto* FlowMergeOrFunction = FVoxelGraphCompilerHelpers::GetPreviousFlowMergeOrFunctionSeparatorNode(PreviewedCompilationNode);
	if (FlowMergeOrFunction)
	{
		if (Graph->bShowFlowMergeAndFunctionsWarnings)
		{
			const auto Result = FMessageDialog::Open(EAppMsgType::YesNo,
				VOXEL_LOCTEXT(
					"Warning: Flow merge & function node preview might have unexpected results if it's not always executed.\n"
					"Hide this warning?"));
			if (Result == EAppReturnType::Yes)
			{
				Graph->bShowFlowMergeAndFunctionsWarnings = false;
			}
		}

		if (!ensure(Compiler.FirstNode))
		{
			Compiler.ErrorReporter.AddError("Start node not connected");
			return;
		}

		auto& OutputExecPin = FlowMergeOrFunction->GetOutputPin(0);
		check(OutputExecPin.PinCategory == EVoxelPinCategory::Exec);
		OutputExecPin.BreakAllLinks(); // Remove existing linked nodes

		auto& SetValueInputExecPin = SetValueNode->GetInputPin(0);
		OutputExecPin.LinkTo(SetValueInputExecPin);
	}
	else
	{
		Compiler.FirstNode = SetValueNode;
		Compiler.FirstNodePinIndex = 0;
	}

	Compiler.Check();
#endif
}

void FVoxelGraphCompilerManager::DebugCompiler(const FVoxelGraphCompiler& Compiler) const
{
#if WITH_EDITOR
	if (Compiler.FirstNode)	Compiler.FirstNode->DebugMessages.Add("First Node");
	DebugNodes(Compiler.GetAllNodes());
	if (Compiler.FirstNode)	Compiler.FirstNode->DebugMessages.Pop();
#endif
}

void FVoxelGraphCompilerManager::DebugNodes(const TSet<FVoxelCompilationNode*>& Nodes) const
{
#if WITH_EDITOR
	auto* VoxelGraphEditor = IVoxelGraphEditor::GetVoxelGraphEditor();
	if (Graph->bEnableDebugGraph && VoxelGraphEditor)
	{
		VoxelGraphEditor->DebugNodes(Graph->VoxelDebugGraph, Nodes);
	}
#endif
}

void FVoxelGraphCompilerManager::Optimize(FVoxelGraphCompiler& Compiler) const
{
	VOXEL_FUNCTION_COUNTER();
	
	Compiler.ApplyPass<FVoxelCompactPassthroughsPass>();
	bool bContinue = true;
	while (bContinue)
	{
		bContinue = false;
		Compiler.ApplyPass<FVoxelRemoveUnusedExecsPass>(bContinue);
		Compiler.ApplyPass<FVoxelRemoveUnusedNodesPass>(bContinue);

		if (bEnableOptimizations)
		{
			Compiler.ApplyPass<FVoxelDisconnectUnusedFlowMergePinsPass>(bContinue);
			Compiler.ApplyPass<FVoxelRemoveFlowMergeWithNoPinsPass>(bContinue);
			Compiler.ApplyPass<FVoxelRemoveFlowMergeWithSingleExecPinLinkedPass>(bContinue);
			Compiler.ApplyPass<FVoxelRemoveConstantIfsPass>(bContinue);
			Compiler.ApplyPass<FVoxelRemoveConstantSwitchesPass>(bContinue);
			Compiler.ApplyPass<FVoxelRemoveIfsWithSameTargetPass>(bContinue);
			Compiler.ApplyPass< FVoxelReplaceConstantPureNodesPass>(bContinue);
		}

		if (Compiler.ErrorReporter.HasError())
		{
			return;
		}
	}
}
