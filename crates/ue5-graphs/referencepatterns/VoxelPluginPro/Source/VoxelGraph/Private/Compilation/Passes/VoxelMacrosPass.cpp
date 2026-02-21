// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMacrosPass.h"
#include "VoxelNodes/VoxelGraphMacro.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Compilation/Passes/VoxelReplaceLocalVariablesPass.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelGraphErrorReporter.h"

inline void ReplaceMacro(FVoxelGraphCompiler& Compiler, FVoxelMacroCompilationNode* MacroCompilationNode, bool bClearMessages)
{	
	const UVoxelGraphMacroNode* const MacroNode = CastChecked<const UVoxelGraphMacroNode>(&MacroCompilationNode->Node);
	UVoxelGraphMacro* const MacroGraph = MacroNode->Macro;
	auto& ErrorReporter = Compiler.ErrorReporter;
	
	check(MacroGraph);
	check(MacroGraph->InputNode && MacroGraph->OutputNode);
	check(MacroNode->InputPins.Num() == MacroGraph->InputNode->OutputPins.Num() && MacroNode->OutputPins.Num() == MacroGraph->OutputNode->InputPins.Num());

	static TSet<const UVoxelGraphMacroNode*> StackMacros;

	if (StackMacros.Contains(MacroNode))
	{
		FString Error;
		Error += "Recursive macros detected! Macros in stack: ";
		for (auto& StackMacro : StackMacros)
		{
			Error += "\n\t" + StackMacro->Macro->GetName();
		}
		ErrorReporter.AddError(Error);
		ErrorReporter.AddNodeToSelect(MacroCompilationNode);
		ErrorReporter.AddMessageToNode(MacroCompilationNode, "recursive macros detected", EVoxelGraphNodeMessageType::Error);
		return;
	}
	
	StackMacros.Add(MacroNode);	
	{
		TMap<UVoxelNode*, FVoxelCompilationNode*> NodesMap;

		// Merge macro nodes
		{
			if (bClearMessages)
			{
				FVoxelGraphErrorReporter::ClearCompilationMessages(MacroGraph);
			}

			auto LocalReporter = MakeShared<FVoxelGraphErrorReporter>(Compiler.ErrorReporter, MacroGraph->GetName());
			FVoxelGraphCompiler LocalCompiler(LocalReporter);
			MacroGraph->AllNodes.RemoveAll([](UVoxelNode* Node) {return !IsValid(Node); });
			NodesMap = LocalCompiler.InitFromNodes(MacroGraph->AllNodes, nullptr, 0);
			for (auto& Node : LocalCompiler.GetAllNodes())
			{
				Node->SourceNodes.Append(MacroCompilationNode->SourceNodes);
			}
			LocalCompiler.ApplyPass<FVoxelReplaceLocalVariablesPass>(); // Apply it here to keep local refs and avoid UVoxelNode* collisions
			LocalCompiler.ApplyPass<FVoxelGraphInlineMacrosPass>(bClearMessages); // Apply pass AFTER rename/SourceNode
			Compiler.AppendAndClear(LocalCompiler);

			if (ErrorReporter.HasError())
			{
				ErrorReporter.AddMessageToNode(MacroNode, "errors in macro", EVoxelGraphNodeMessageType::Error);
			}
		}
		if (ErrorReporter.HasError())
		{
			StackMacros.Remove(MacroNode);
			return;
		}

		FVoxelCompilationNode* const InputNode = NodesMap.FindChecked(MacroGraph->InputNode);
		FVoxelCompilationNode* const OutputNode = NodesMap.FindChecked(MacroGraph->OutputNode);

		MacroCompilationNode->InputNode = InputNode;
		MacroCompilationNode->OutputNode = OutputNode;
	
		// Link pins
		for (auto& MacroNodePin : MacroCompilationNode->IteratePins<EVoxelPinIter::All>())
		{
			int32 PinIndex = MacroNodePin.Index;
			bool bIsInput = MacroNodePin.Direction == EVoxelPinDirection::Input;
			auto& InternalNodePin = bIsInput ? InputNode->GetInputPin(PinIndex) : OutputNode->GetOutputPin(PinIndex);
			check(MacroNodePin.Direction == InternalNodePin.Direction);

			// Copy the default values the user set
			if (bIsInput)
			{
				InternalNodePin.SetDefaultValue(MacroNodePin.GetDefaultValue());
			}

			// Always destroy all links when inside a macro: we don't want the preview nodes
			InternalNodePin.BreakAllLinks();
			for (auto& LinkedToPin : MacroNodePin.IterateLinkedTo())
			{
				InternalNodePin.LinkTo(LinkedToPin);
			}
			MacroNodePin.BreakAllLinks();
		}

		if (Compiler.FirstNode == MacroCompilationNode)
		{
			Compiler.FirstNode = InputNode;
			// Input node pin index is the same
		}

		Compiler.RemoveNode(MacroCompilationNode);
	}
	StackMacros.Remove(MacroNode);
}


void FVoxelGraphInlineMacrosPass::Apply(FVoxelGraphCompiler& Compiler, bool bClearMessages)
{
	// The nodes array will be changed by the macro nodes
	for (auto& Node : Compiler.GetAllNodesCopy())
	{
		if (auto* MacroNode = CastVoxel<FVoxelMacroCompilationNode>(Node))
		{
			ReplaceMacro(Compiler, MacroNode, bClearMessages);
		}
		if (Compiler.ErrorReporter.HasError())
		{
			return;
		}
	}
	for (auto& Node : Compiler.GetAllNodes())
	{
		check(!Node->IsA<FVoxelMacroCompilationNode>());
	}
}

void FVoxelGraphReplaceMacroInputOutputsPass::Apply(FVoxelGraphCompiler& Compiler)
{
	for (auto& Node : Compiler.GetAllNodesCopy())
	{
		if (auto* MacroInputOutputNode = CastVoxel<FVoxelMacroInputOuputCompilationNode>(Node))
		{
			check(Node->GetInputCount() == Node->GetOutputCount());
			for (int32 Index = 0; Index < MacroInputOutputNode->GetInputCount(); Index++)
			{
				auto& InputPin = MacroInputOutputNode->GetInputPin(Index);
				auto& OutputPin = MacroInputOutputNode->GetOutputPin(Index);
				auto* Passthrough = FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, InputPin);
				MacroInputOutputNode->Passthroughs.Add(Passthrough);

				auto& PassthroughOutputPin = Passthrough->GetOutputPin(0);
				check(PassthroughOutputPin.NumLinkedTo() == 1);
				PassthroughOutputPin.BreakAllLinks();
				for (auto& LinkedToPin : OutputPin.IterateLinkedTo())
				{
					PassthroughOutputPin.LinkTo(LinkedToPin);
				}
				OutputPin.BreakAllLinks();
			}

			if (Compiler.FirstNode == MacroInputOutputNode)
			{
				Compiler.FirstNode = MacroInputOutputNode->Passthroughs[Compiler.FirstNodePinIndex];
				Compiler.FirstNodePinIndex = 0; // Only one input for passthroughs
			}

			MacroInputOutputNode->CheckIsNotLinked(Compiler.ErrorReporter);
			Compiler.RemoveNode(MacroInputOutputNode);
		}
	}
}
