// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Compilation/Passes/VoxelReplaceSmartMinMaxPass.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "VoxelNodes/VoxelOptimizationNodes.h"
#include "VoxelNodes/VoxelMathNodes.h"
#include "VoxelNodes/VoxelBinaryNodes.h"
#include "VoxelNodes/VoxelExecNodes.h"
#include "VoxelNodes/VoxelIfNode.h"

void FVoxelReplaceSmartMinMaxPass::Apply(FVoxelGraphCompiler& Compiler)
{
	for (auto* NodeIt : Compiler.GetAllNodesCopy())
	{
		auto* Node = CastVoxel<FVoxelSmartMinMaxCompilationNode>(NodeIt);
		if (!Node)
		{
			continue;
		}

		const int32 NumInputs = Node->GetInputCountWithoutExecs();

		FVoxelCompilationNode* InputExecPassthrough;
		TArray<FVoxelCompilationNode*> InputPassthroughs;
		FVoxelCompilationNode* OutputExecPassthrough;
		FVoxelCompilationNode* OutputPassthrough;
		
		// Create passthroughs
		{
			InputExecPassthrough = FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, Node->GetInputPin(0));

			for (auto& Pin : Node->IteratePins<EVoxelPinIter::Input>())
			{
				if (Pin.PinCategory == EVoxelPinCategory::Float)
				{
					InputPassthroughs.Add(FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, Pin));
				}
			}

			OutputExecPassthrough = FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, Node->GetOutputPin(0));
			OutputPassthrough = FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, Node->GetOutputPin(1));
		}

		// Add GetRange to each pin
		TArray<FVoxelCompilationNode*> GetRangeNodes;
		for (int32 InputIndex = 0; InputIndex < NumInputs; InputIndex++)
		{
			auto* GetRange = Compiler.AddNode(GetDefault<UVoxelNode_GetRangeAnalysis>()->GetCompilationNode(), Node);
			GetRange->GetInputPin(0).LinkTo(InputPassthroughs[InputIndex]->GetOutputPin(0));
			GetRangeNodes.Add(GetRange);
		}

		// Add a Max of all the GetRange.Min
		FVoxelCompilationNode* MaxOfRangeMin;
		{
			UVoxelNode* MaxVoxelNode;
			int32 GetRangePinIndex;
			if (Node->bIsMin)
			{
				// We take the min of the max
				MaxVoxelNode = NewObject<UVoxelNode_FMin>();
				GetRangePinIndex = 1;
			}
			else
			{
				// We take the max of the min
				MaxVoxelNode = NewObject<UVoxelNode_FMax>();
				GetRangePinIndex = 0;
			}
			MaxVoxelNode->InputPinCount = NumInputs;

			MaxOfRangeMin = Compiler.AddNode(MaxVoxelNode->GetCompilationNode(), Node);

			// Link all the GetRange pins to it
			for (int32 InputIndex = 0; InputIndex < NumInputs; InputIndex++)
			{
				MaxOfRangeMin->GetInputPin(InputIndex).LinkTo(GetRangeNodes[InputIndex]->GetOutputPin(GetRangePinIndex));
			}
		}

		// Then for each of the pin, add a if (Range.Max < MaxOfRangeMin) FlowMerge MaxOfRangeMin else FlowMerge Value
		auto* LastExecPin = &InputExecPassthrough->GetOutputPin(0);
		TArray<FVoxelCompilationPin*> FlowMergeValueOutputPins;
		for (int32 InputIndex = 0; InputIndex < NumInputs; InputIndex++)
		{
			UVoxelNode* LessThanVoxelNode;
			int32 GetRangePinIndex;
			if (Node->bIsMin)
			{
				// We take the min of the max
				LessThanVoxelNode = NewObject<UVoxelNode_FGreater>();
				GetRangePinIndex = 0;
			}
			else
			{
				// We take the max of the min
				LessThanVoxelNode = NewObject<UVoxelNode_FLess>();
				GetRangePinIndex = 1;
			}

			auto* LessThan = Compiler.AddNode(LessThanVoxelNode->GetCompilationNode(), Node);
			LessThan->GetInputPin(0).LinkTo(GetRangeNodes[InputIndex]->GetOutputPin(GetRangePinIndex));
			LessThan->GetInputPin(1).LinkTo(MaxOfRangeMin->GetOutputPin(0));

			auto* If = Compiler.AddNode(GetDefault<UVoxelNode_IfWithDefaultToTrue>()->GetCompilationNode(), Node);
			If->GetInputPin(0).LinkTo(*LastExecPin);
			If->GetInputPin(1).LinkTo(LessThan->GetOutputPin(0));

			auto* FlowMerge = Compiler.AddNode(GetDefault<UVoxelNode_FlowMerge>()->GetCompilationNode(), Node);
			// if (Range.Max < MaxOfRangeMin) FlowMerge MaxOfRangeMin
			FlowMerge->GetInputPin(0).LinkTo(If->GetOutputPin(0));
			FlowMerge->GetInputPin(1).LinkTo(MaxOfRangeMin->GetOutputPin(0));
			// else FlowMerge Value
			FlowMerge->GetInputPin(2).LinkTo(If->GetOutputPin(1));
			FlowMerge->GetInputPin(3).LinkTo(InputPassthroughs[InputIndex]->GetOutputPin(0));
			
			LastExecPin = &FlowMerge->GetOutputPin(0);
			FlowMergeValueOutputPins.Add(&FlowMerge->GetOutputPin(1));
		}

		// Add a function separator (only one at the end, else flow merge are useless)
		FVoxelCompilationNode* FunctionSeparator = Compiler.AddNode(GetDefault<UVoxelNode_FunctionSeparator>()->GetCompilationNode(), Node);
		LastExecPin->LinkTo(FunctionSeparator->GetInputPin(0));
		LastExecPin = &FunctionSeparator->GetOutputPin(0);

		// Link the last exec pin to the actual exec output
		LastExecPin->LinkTo(OutputExecPassthrough->GetInputPin(0));

		// Do the max of the flow merge values, and link it to the function separator input
		{
			UVoxelNode* MaxVoxelNode;
			if (Node->bIsMin)
			{
				MaxVoxelNode = NewObject<UVoxelNode_FMin>();
			}
			else
			{
				MaxVoxelNode = NewObject<UVoxelNode_FMax>();
			}
			MaxVoxelNode->InputPinCount = NumInputs;

			auto* Max = Compiler.AddNode(MaxVoxelNode->GetCompilationNode(), Node);
			for (int32 InputIndex = 0; InputIndex < NumInputs; InputIndex++)
			{
				Max->GetInputPin(InputIndex).LinkTo(*FlowMergeValueOutputPins[InputIndex]);
			}
			Max->GetOutputPin(0).LinkTo(OutputPassthrough->GetInputPin(0));
		}

		// For preview
		Node->OutputPassthrough = OutputPassthrough;
		
		if (Compiler.FirstNode == Node)
		{
			Compiler.FirstNode = InputExecPassthrough;
			ensure(Compiler.FirstNodePinIndex == 0);
		}
		Node->BreakAllLinks();
		Compiler.RemoveNode(Node);
	}
}
