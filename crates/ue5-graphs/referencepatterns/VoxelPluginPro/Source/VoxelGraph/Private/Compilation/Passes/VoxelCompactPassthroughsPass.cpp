// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Compilation/Passes/VoxelCompactPassthroughsPass.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "Compilation/VoxelCompilationNode.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelGraphErrorReporter.h"

inline bool RemovePassthroughIfPossible(FVoxelGraphCompiler& Compiler, FVoxelCompilationNode* PassthroughNode)
{
	if (PassthroughNode == Compiler.FirstNode)
	{
		return false;
	}

	auto& InputPin = PassthroughNode->GetInputPin(0);
	auto& OutputPin = PassthroughNode->GetOutputPin(0);
	const auto Category = InputPin.PinCategory;
	check(Category == OutputPin.PinCategory);

	if (InputPin.NumLinkedTo() == 0)
	{
		if (Category != EVoxelPinCategory::Exec)
		{
			for (auto& LinkedToPin : OutputPin.IterateLinkedTo())
			{
				LinkedToPin.SetDefaultValue(InputPin.GetDefaultValue());
			}
		}

		PassthroughNode->BreakAllLinks();
		Compiler.RemoveNode(PassthroughNode);
		return true;
	}
	else if (OutputPin.NumLinkedTo() > 0)
	{
		if (InputPin.PinCategory == EVoxelPinCategory::Exec)
		{
			check(OutputPin.NumLinkedTo() == 1);
			auto& PinLinkedToOutputPin = OutputPin.GetLinkedTo(0);
			FVoxelGraphCompilerHelpers::MovePin(InputPin, PinLinkedToOutputPin);
		}
		else
		{
			check(InputPin.NumLinkedTo() == 1);
			auto& PinLinkedToInputPin = InputPin.GetLinkedTo(0);
			FVoxelGraphCompilerHelpers::MovePin(OutputPin, PinLinkedToInputPin);
		}

		PassthroughNode->BreakAllLinks();
		Compiler.RemoveNode(PassthroughNode);
		return true;
	}
	else
	{
		return false;
	}
}

void FVoxelCompactPassthroughsPass::Apply(FVoxelGraphCompiler& Compiler)
{
	bool bContinue = true;
	while (bContinue && !Compiler.ErrorReporter.HasError())
	{
		bContinue = false;
		for (auto& Node : Compiler.GetAllNodes())
		{
			if (Node->IsA<FVoxelPassthroughCompilationNode>() && RemovePassthroughIfPossible(Compiler, Node))
			{
				bContinue = true;
				break;
			}
			if (Compiler.ErrorReporter.HasError())
			{
				break;
			}
		}
	}
}
