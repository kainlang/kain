// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Compilation/Passes/VoxelOptimizationsPass.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "Runtime/VoxelNodeType.h"
#include "Runtime/VoxelComputeNode.h"
#include "VoxelNodes/VoxelOptimizationNodes.h"
#include "VoxelNodes/VoxelConstantNodes.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelContext.h"

void FVoxelOptimizeForPermutationPass::Apply(FVoxelGraphCompiler& Compiler, const FVoxelGraphPermutationArray& Permutation)
{
	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		auto* Setter = CastVoxel<FVoxelSetterCompilationNode>(Node);
		if (Setter && !Permutation.Contains(Setter->OutputIndex))
		{
			auto& InputPin = Setter->GetInputPin(0);
			auto& OutputPin = Setter->GetOutputPin(0);
			check(InputPin.PinCategory == EVoxelPinCategory::Exec && OutputPin.PinCategory == EVoxelPinCategory::Exec);

			if (OutputPin.NumLinkedTo() > 0 && InputPin.NumLinkedTo() > 0)
			{
				check(OutputPin.NumLinkedTo() == 1);
				auto& PinLinkedToOutput = OutputPin.GetLinkedTo(0);
				FVoxelGraphCompilerHelpers::MovePin(InputPin, PinLinkedToOutput);
			}

			Setter->BreakAllLinks();
			Compiler.RemoveNode(Setter);
		}
	}
}

void FVoxelReplaceCompileTimeConstantsPass::Apply(FVoxelGraphCompiler& Compiler, const TMap<FName, FString>& Constants)
{
	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		auto* Constant = CastVoxel<FVoxelCompileTimeConstantCompilationNode>(Node);
		if (Constant)
		{
			auto& OutputPin = Constant->GetOutputPin(0);

			const FString Value = Constants.FindRef(Constant->Name);
			
			for (auto& LinkedTo : OutputPin.IterateLinkedTo())
			{
				LinkedTo.SetDefaultValue(Value);
			}

			auto* CompileTimeConstantNode = const_cast<UVoxelNode_CompileTimeConstant*>(Cast<UVoxelNode_CompileTimeConstant>(&Node->Node));
			if (ensure(CompileTimeConstantNode))
			{
				CompileTimeConstantNode->Constants.Append(Constants);
				CompileTimeConstantNode->Constants.KeySort([](auto& A, auto& B) {return A.FastLess(B); });
			}

			Constant->BreakAllLinks();
			Compiler.RemoveNode(Constant);
		}
	}
}

void FVoxelRemoveUnusedExecsPass::Apply(FVoxelGraphCompiler& Compiler, bool& bChanged)
{
	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		if (Node != Compiler.FirstNode && Node->IsExecNode() && !FVoxelGraphCompilerHelpers::HasSetterOrFunctionCallSuccessor(Node))
		{
			Node->BreakAllLinks();
			Compiler.RemoveNode(Node);
			bChanged = true;
		}
	}
}

void FVoxelRemoveUnusedNodesPass::Apply(FVoxelGraphCompiler& Compiler, bool& bChanged)
{
	TSet<FVoxelCompilationNode*> Nodes;
	FVoxelGraphCompilerHelpers::GetAllUsedNodes(Compiler.FirstNode, Nodes);

	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		if (!Nodes.Contains(Node))
		{
			Node->BreakAllLinks();
			Compiler.RemoveNode(Node);
			bChanged = true;
		}
	}
}

void FVoxelDisconnectUnusedFlowMergePinsPass::Apply(FVoxelGraphCompiler& Compiler, bool& bChanged)
{
	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		if (Node->IsA<FVoxelFlowMergeCompilationNode>())
		{
			for (auto& Pin : Node->IteratePins<EVoxelPinIter::Output>())
			{
				if (Pin.PinCategory != EVoxelPinCategory::Exec)
				{
					if (Pin.NumLinkedTo() == 0)
					{
						for (auto* InputPin : { &Node->GetInputPin(Pin.Index), &Node->GetInputPin(Pin.Index + Node->GetOutputCount()) })
						{
							if (InputPin->NumLinkedTo() > 0)
							{
								InputPin->BreakAllLinks();
								bChanged = true;
							}
						}
					}
				}
			}
		}
	}
}

void FVoxelRemoveFlowMergeWithNoPinsPass::Apply(FVoxelGraphCompiler& Compiler, bool& bChanged)
{
	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		if (Node->IsA<FVoxelFlowMergeCompilationNode>())
		{
			bool bRemove = true;
			for (auto& Pin : Node->IteratePins<EVoxelPinIter::Output>())
			{
				if (Pin.PinCategory != EVoxelPinCategory::Exec && Pin.NumLinkedTo() != 0)
				{
					bRemove = false;
					break;
				}
			}

			if (bRemove)
			{
				bChanged = true;

				auto& ExecOutputPin = Node->GetOutputPin(0);
				if (ExecOutputPin.NumLinkedTo() == 0)
				{
					Node->BreakAllLinks();
					Compiler.RemoveNode(Node);
				}
				else
				{
					auto& PinLinkedToExecOutput = ExecOutputPin.GetLinkedTo(0);

					FVoxelGraphCompilerHelpers::MovePin(Node->GetInputPin(0), PinLinkedToExecOutput);
					FVoxelGraphCompilerHelpers::MovePin(Node->GetInputPin(Node->GetOutputCount()), PinLinkedToExecOutput);

					Node->BreakAllLinks();
					Compiler.RemoveNode(Node);
				}
			}
		}
	}
}

void FVoxelRemoveFlowMergeWithSingleExecPinLinkedPass::Apply(FVoxelGraphCompiler& Compiler, bool& bChanged)
{
	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		if (Node->IsA<FVoxelFlowMergeCompilationNode>())
		{
			const int32 NumPins = Node->GetOutputCount();

			auto& ExecInputPinA = Node->GetInputPin(0);
			auto& ExecInputPinB = Node->GetInputPin(NumPins);

			ensureVoxelGraphExitPass(ExecInputPinA.PinCategory == EVoxelPinCategory::Exec, Node);
			ensureVoxelGraphExitPass(ExecInputPinB.PinCategory == EVoxelPinCategory::Exec, Node);

			if (ExecInputPinA.NumLinkedTo() == 0 && ExecInputPinB.NumLinkedTo() == 0)
			{
				Node->BreakAllLinks();
				Compiler.RemoveNode(Node);
				bChanged = true;
			}
			else if (ExecInputPinA.NumLinkedTo() == 0 || ExecInputPinB.NumLinkedTo() == 0)
			{
				const int32 InputOffset = ExecInputPinA.NumLinkedTo() == 0 ? NumPins : 0; // We want to iterate on the valid pins
				for (int32 Index = 0; Index < NumPins; Index++)
				{
					auto& InputPin = Node->GetInputPin(Index + InputOffset);
					auto& OutputPin = Node->GetOutputPin(Index);
					ensureVoxelGraphExitPass(InputPin.PinCategory == OutputPin.PinCategory, Node);
					FVoxelGraphCompilerHelpers::MovePin(InputPin, OutputPin.GetLinkedToArray());
				}
				Node->BreakAllLinks();
				Compiler.RemoveNode(Node);
				bChanged = true;
			}
		}
	}
}

void FVoxelRemoveConstantIfsPass::Apply(FVoxelGraphCompiler& Compiler, bool& bChanged)
{
	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		if (Node->IsA<FVoxelIfCompilationNode>())
		{
			auto& BoolPin = Node->GetInputPin(1);
			check(BoolPin.PinCategory == EVoxelPinCategory::Boolean);
			if (BoolPin.NumLinkedTo() == 0)
			{
				const bool bValue = FVoxelPinCategory::ConvertDefaultValue(EVoxelPinCategory::Boolean, BoolPin.GetDefaultValue()).Get<bool>();

				auto& InputPin = Node->GetInputPin(0);
				auto& OutputPin = Node->GetOutputPin(bValue ? 0 : 1);

				// Safe because no default value for execs
				if (OutputPin.NumLinkedTo() > 0)
				{
					check(OutputPin.NumLinkedTo() == 1);
					auto& LinkedToOutput = OutputPin.GetLinkedTo(0);
					FVoxelGraphCompilerHelpers::MovePin(InputPin, LinkedToOutput);
				}
			
				Node->BreakAllLinks();
				Compiler.RemoveNode(Node);

				bChanged = true;
			}
		}
	}
}

void FVoxelRemoveConstantSwitchesPass::Apply(FVoxelGraphCompiler& Compiler, bool& bChanged)
{
	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		if (Node->IsA<FVoxelSwitchCompilationNode>())
		{
			auto& BoolPin = Node->GetInputPin(2);
			check(BoolPin.PinCategory == EVoxelPinCategory::Boolean);
			if (BoolPin.NumLinkedTo() == 0)
			{
				const bool bValue = FVoxelPinCategory::ConvertDefaultValue(EVoxelPinCategory::Boolean, BoolPin.GetDefaultValue()).Get<bool>();

				auto& InputPin = Node->GetInputPin(bValue ? 0 : 1);
				auto& OutputPin = Node->GetOutputPin(0);

				FVoxelGraphCompilerHelpers::MovePin(InputPin, OutputPin.GetLinkedToArray());
			
				Node->BreakAllLinks();
				Compiler.RemoveNode(Node);

				bChanged = true;
			}
		}
	}
}

void FVoxelRemoveIfsWithSameTargetPass::Apply(FVoxelGraphCompiler& Compiler, bool& bChanged)
{
	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		if (Node->IsA<FVoxelIfCompilationNode>())
		{
			auto& OutputPinA = Node->GetOutputPin(0);
			auto& OutputPinB = Node->GetOutputPin(1);
			if (OutputPinA.NumLinkedTo() > 0 && OutputPinB.NumLinkedTo() > 0)
			{
				check(OutputPinA.NumLinkedTo() == 1 && OutputPinB.NumLinkedTo() == 1);
				auto& LinkedToA = OutputPinA.GetLinkedTo(0);
				auto& LinkedToB = OutputPinB.GetLinkedTo(0);
				if (&LinkedToA == &LinkedToB)
				{
					auto& InputPin = Node->GetInputPin(0);
					for (auto& LinkedToInput : InputPin.IterateLinkedTo())
					{
						LinkedToInput.LinkTo(LinkedToA);
					}
					Node->BreakAllLinks();
					Compiler.RemoveNode(Node);

					bChanged = true;
				}
			}
		}
	}
}

void FVoxelReplaceConstantPureNodesPass::Apply(FVoxelGraphCompiler& Compiler, bool& bChanged)
{
	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		if (!Node->IsExecNode() && Node->IsPureNode())
		{
			bool bRemove = true;
			for (auto& Pin : Node->IteratePins<EVoxelPinIter::Input>())
			{
				if (Pin.NumLinkedTo() > 0)
				{
					bRemove = false;
					break;
				}
			}
			if (bRemove)
			{
				auto ComputeNode = Node->GetComputeNode();
				check(ComputeNode.IsValid());
				if (!ensureVoxelGraphImpl(ComputeNode->Type == EVoxelComputeNodeType::Data, Node, Compiler.ErrorReporter))
				{
					return;
				}
				check(ComputeNode->Type == EVoxelComputeNodeType::Data);
				const auto DataComputeNode = StaticCastSharedPtr<FVoxelDataComputeNode>(ComputeNode);

				FVoxelNodeType InputBuffer[MAX_VOXELNODE_PINS];
				FVoxelNodeType OutputBuffer[MAX_VOXELNODE_PINS];
				for (int32 Index = 0; Index < Node->GetInputCount(); Index++)
				{
					auto& Pin = Node->GetInputPin(Index);
					InputBuffer[Index] = FVoxelPinCategory::ConvertDefaultValue(Pin.PinCategory, Pin.GetDefaultValue());
				}

				DataComputeNode->Compute(InputBuffer, OutputBuffer, FVoxelContext::EmptyContext);

				for (int32 Index = 0; Index < Node->GetOutputCount(); Index++)
				{
					auto& Pin = Node->GetOutputPin(Index);
					FString NewDefaultValue = FVoxelPinCategory::ToString(Pin.PinCategory, OutputBuffer[Index]);
					for (auto& LinkedTo : Pin.IterateLinkedTo())
					{
						LinkedTo.SetDefaultValue(NewDefaultValue);
					}
				}

				Node->BreakAllLinks();
				Compiler.RemoveNode(Node);
				bChanged = true;
			}
		}
	}
}
