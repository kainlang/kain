// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "CppTranslation/VoxelCppConfig.h"
#include "CppTranslation/VoxelVariables.h"
#include "VoxelNodes/VoxelExposedNodes.h"
#include "VoxelGraphErrorReporter.h"

FVoxelCppConfig::FVoxelCppConfig(FVoxelGraphErrorReporter& ErrorReporter)
	: ErrorReporter(ErrorReporter)
{
}

void FVoxelCppConfig::AddExposedVariable(const TSharedRef<FVoxelExposedVariable>& Variable)
{
	const FName Name(*Variable->ExposedName);
	if (auto* ExistingVariablePtr = ExposedVariables.Find(Name))
	{
		const auto& ExistingVariable = *ExistingVariablePtr;
		if (ExistingVariable->Node != Variable->Node && !ExistingVariable->IsSameAs(*Variable, false))
		{
			ErrorReporter.AddError("Exposed variables from nodes A and B have the same name, but different settings!");
			ErrorReporter.AddMessageToNode(Variable->Node, "node A", EVoxelGraphNodeMessageType::Info);
			ErrorReporter.AddMessageToNode(ExistingVariable->Node, "node B", EVoxelGraphNodeMessageType::Info);
		}
	}
	else
	{
		ExposedVariables.Add(Name, Variable);
	}
}

void FVoxelCppConfig::AddInclude(const FVoxelCppInclude& Include)
{
	Includes.AddUnique(Include);
}

void FVoxelCppConfig::BuildExposedVariablesArray()
{
	ExposedVariablesArray.Reset();
	ExposedVariables.GenerateValueArray(ExposedVariablesArray);

	// Needs to match FVoxelGeneratorPickerCustomization::CustomizeChildren
	ExposedVariablesArray.Sort([](const TSharedRef<FVoxelExposedVariable>& A, const TSharedRef<FVoxelExposedVariable>& B)
	{
		if (A->Priority != B->Priority)
		{
			return A->Priority < B->Priority;
		}
		return A->ExposedName < B->ExposedName;
	});
}
