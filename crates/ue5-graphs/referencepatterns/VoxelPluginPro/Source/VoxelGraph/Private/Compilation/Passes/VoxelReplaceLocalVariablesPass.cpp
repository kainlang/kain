// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelReplaceLocalVariablesPass.h"
#include "Compilation/VoxelCompilationNode.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "VoxelNodes/VoxelLocalVariables.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelGraphGenerator.h"

void FVoxelReplaceLocalVariablesPass::Apply(FVoxelGraphCompiler& Compiler)
{
	TMap<const UVoxelLocalVariableDeclaration*, FVoxelCompilationNode*> DeclarationPassthroughs;
	TMap<const UVoxelLocalVariableUsage*, FVoxelCompilationNode*> UsagePassthroughs;

	for (auto* CompilationNode : Compiler.GetAllNodesCopy())
	{
		if (auto* Declaration = CastVoxel<FVoxelLocalVariableDeclarationCompilationNode>(CompilationNode))
		{
			auto* Passthrough = FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, Declaration->GetInputPin(0));

			auto* VoxelNode = CastChecked<UVoxelLocalVariableDeclaration>(&Declaration->Node);
			check(!DeclarationPassthroughs.Contains(VoxelNode));
			DeclarationPassthroughs.Add(VoxelNode, Passthrough);

			Declaration->BreakAllLinks();
			Compiler.RemoveNode(Declaration);
		}
		else if (auto* Usage = CastVoxel<FVoxelLocalVariableUsageCompilationNode>(CompilationNode))
		{
			auto* Passthrough = FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, Usage->GetOutputPin(0));
			Usage->Passthrough = Passthrough;

			auto* UsageDeclaration = CastChecked<UVoxelLocalVariableUsage>(&Usage->Node);
			check(!UsagePassthroughs.Contains(UsageDeclaration));
			UsagePassthroughs.Add(UsageDeclaration, Passthrough);

			Usage->BreakAllLinks();
			Compiler.RemoveNode(Usage);
		}
	}

	if (Compiler.ErrorReporter.HasError())
	{
		return;
	}

	for (auto& It : UsagePassthroughs)
	{
		auto* Output = It.Key;
		auto* OutputPassthrough = It.Value;
		UVoxelLocalVariableDeclaration* Declaration = Output->Declaration;
		check(Declaration);
		auto* DeclarationPassthrough = DeclarationPassthroughs.FindChecked(Declaration);
		
		auto& OutputPin = DeclarationPassthrough->GetOutputPin(0);
		auto& InputPin = OutputPassthrough->GetInputPin(0);
		check(InputPin.NumLinkedTo() == 0); // Only input pin, as output pin can be linked to multiple local variable outputs
		OutputPin.LinkTo(InputPin);
	}

	if (Compiler.ErrorReporter.HasError())
	{
		return;
	}

	for (auto* Node : Compiler.GetAllNodes())
	{
		check(!CastVoxel<FVoxelLocalVariableDeclarationCompilationNode>(Node) && !CastVoxel<FVoxelLocalVariableUsageCompilationNode>(Node));
	}
}
