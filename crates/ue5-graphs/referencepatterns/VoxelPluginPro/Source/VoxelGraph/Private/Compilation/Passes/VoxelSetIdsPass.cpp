// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Compilation/Passes/VoxelSetIdsPass.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Compilation/VoxelCompilationNode.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"

void FVoxelSetPinsIdsPass::Apply(FVoxelGraphCompiler& Compiler, int32& Id)
{
	// First set outputs ids
	for (auto* Node : Compiler.GetAllNodes())
	{
		int32 OutputIndex = 0;
		for (auto& OutputPin : Node->IteratePins<EVoxelPinIter::Output>())
		{
			if (OutputPin.PinCategory != EVoxelPinCategory::Exec)
			{
				Node->SetOutputId(OutputIndex++, OutputPin.NumLinkedTo() == 0 ? -1 : Id++); // -1 if not used
			}
		}
	}

	// Then set inputs ids
	for (auto* Node : Compiler.GetAllNodes())
	{
		int32 InputIndex = 0;
		for (auto& InputPin : Node->IteratePins<EVoxelPinIter::Input>())
		{
			if (InputPin.PinCategory != EVoxelPinCategory::Exec)
			{
				int32 InputId;
				if (InputPin.NumLinkedTo() == 0)
				{
					InputId = -1;
				}
				else
				{
					check(InputPin.NumLinkedTo() == 1);
					auto& OtherOutputPin = InputPin.GetLinkedTo(0);
					auto& OtherNode = OtherOutputPin.Node;
					int32 RealOutputIndex = 0;
					// Skip exec pins
					for (int32 OutputIndex = 0; OutputIndex < OtherOutputPin.Index; OutputIndex++)
					{
						if (OtherNode.GetOutputPin(OutputIndex).PinCategory != EVoxelPinCategory::Exec)
						{
							RealOutputIndex++;
						}
					}
					InputId = OtherNode.GetOutputId(RealOutputIndex);
				}
				Node->SetInputId(InputIndex++, InputId);
			}
		}
	}
}

void FVoxelSetFunctionsIdsPass::Apply(FVoxelGraphCompiler& Compiler)
{
	CastCheckedVoxel<FVoxelFunctionSeparatorCompilationNode>(Compiler.FirstNode).FunctionId = 0;
	int32 Id = 1; // 0 is for the function corresponding to the first node
	for (auto* Node : Compiler.GetAllNodes())
	{
		auto* Function = CastVoxel<FVoxelFunctionSeparatorCompilationNode>(Node);
		if (Function && Function != Compiler.FirstNode)
		{
			Function->FunctionId = Id++;
		}
	}
}
