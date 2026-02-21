// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Runtime/VoxelComputeNode.h"
#include "Compilation/VoxelCompilationNode.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelVariables.h"
#include "VoxelGraphGlobals.h"
#include "VoxelNode.h"

inline FString MakeUniqueName(const FString& InName)
{
	const auto Name = FVoxelVariable::SanitizeName(InName);
	
	static TMap<FName, int32> Names;
	static int32 CompilationId;
	check(CompilationId <= FVoxelGraphCompiler::GetCompilationId());
	if (CompilationId != FVoxelGraphCompiler::GetCompilationId())
	{
		CompilationId = FVoxelGraphCompiler::GetCompilationId();
		Names.Empty();
	}

	int32& Id = Names.FindOrAdd(*Name);
	return Name + "_" + FString::FromInt(Id++);
}

inline TArray<bool> GetCacheOutputs(const FVoxelCompilationNode& CompilationNode)
{
	const auto NodeDependencies = FVoxelAxisDependencies::GetVoxelAxisDependenciesFromFlag(CompilationNode.Dependencies);
	TArray<bool> Result;
	for(auto& OutputPin : CompilationNode.IteratePins<EVoxelPinIter::Output>())
	{
		bool bCache = false;
		// Iterate linked to pin, and see if at least one has different dependencies (and hence we need to be stored in the cache)
		for (auto& OtherPin : OutputPin.IterateLinkedTo())
		{
			const auto OtherNodeDependencies = FVoxelAxisDependencies::GetVoxelAxisDependenciesFromFlag(OtherPin.Node.Dependencies);
			const bool bOtherIsExecNode = OtherPin.Node.IsExecNode();
			ensure(int32(NodeDependencies) <= int32(OtherNodeDependencies) || bOtherIsExecNode);

			// If other is an exec node, always cache unless we are in XYZ
			if (NodeDependencies != OtherNodeDependencies || (bOtherIsExecNode && NodeDependencies != EVoxelAxisDependencies::XYZ))
			{
				bCache = true;
				break;
			}
		}
		Result.Add(bCache);
	}
	return Result;
}

FVoxelComputeNode::FVoxelComputeNode(EVoxelComputeNodeType Type, const UVoxelNode& Node, const FVoxelCompilationNode& CompilationNode)
	: Type(Type)
	, InputCount(CompilationNode.GetInputCountWithoutExecs())
	, OutputCount(CompilationNode.GetOutputCountWithoutExecs())
	, InputIds(CompilationNode.GetInputIds())
	, OutputIds(CompilationNode.GetOutputIds())
	, CacheOutputs(GetCacheOutputs(CompilationNode))
	, PrettyName(CompilationNode.GetPrettyName())
	, UniqueName(*MakeUniqueName(PrettyName))
	, Node(&Node)
	, SourceNodes(CompilationNode.SourceNodes)
	, Dependencies(FVoxelAxisDependencies::GetVoxelAxisDependenciesFromFlag(CompilationNode.Dependencies))
{
	check(InputCount < MAX_VOXELNODE_PINS);
	check(OutputCount < MAX_VOXELNODE_PINS);

	check(InputIds.Num() == InputCount);
	check(OutputIds.Num() == OutputCount);

	for (auto& InputPin : CompilationNode.IteratePins<EVoxelPinIter::Input>())
	{
		if (InputPin.PinCategory != EVoxelPinCategory::Exec)
		{
			InputsCategories.Add(InputPin.PinCategory);
		}
	}
	for (auto& OutputPin : CompilationNode.IteratePins<EVoxelPinIter::Output>())
	{
		if (OutputPin.PinCategory != EVoxelPinCategory::Exec)
		{
			OutputsCategories.Add(OutputPin.PinCategory);
		}
	}
	check(InputsCategories.Num() == InputCount);
	check(OutputsCategories.Num() == OutputCount);

	check(&CompilationNode.Node == &Node);

	for (auto& Pin : CompilationNode.IteratePins<EVoxelPinIter::Input>())
	{
		if (Pin.PinCategory != EVoxelPinCategory::Exec)
		{
			const FString& DefaultValue = Pin.GetDefaultValue();
			DefaultValues.Add(FVoxelPinCategory::ConvertDefaultValue(Pin.PinCategory, DefaultValue));
			DefaultRangeValues.Add(FVoxelPinCategory::ConvertRangeDefaultValue(Pin.PinCategory, DefaultValue));
			DefaultValueStrings.Add(FVoxelPinCategory::ConvertStringDefaultValue(Pin.PinCategory, DefaultValue));
		}
	}
}

UVoxelGraphGenerator* FVoxelComputeNode::GetGraph() const
{
	if (SourceNodes.Num() > 0)
	{
		if (SourceNodes.Last().IsValid())
		{
			return SourceNodes.Last()->Graph;
		}
	}
	return nullptr;
}

void FVoxelComputeNode::DeclareOutput(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo, int32 OutputIndex) const
{
	const EVoxelPinCategory Category = GetOutputCategory(OutputIndex);
	if (VariableInfo.IsInit() != (Category == EVoxelPinCategory::Seed))
	{
		// Only declare seeds when init
		return;
	}

	const int32 Id = GetOutputId(OutputIndex);
	const FString VariableName = Id == -1 ? GetUnusedOutputName(OutputIndex) : FVoxelCppIds::GetVariableName(Id);
	const FString Declaration =
		Constructor.GetTypeString(Category) + " "
		+ VariableName + "; "
		+ FString::Printf(TEXT("// %s output %d"), *PrettyName, OutputIndex);

	if (Id == -1)
	{
		ensure(!VariableInfo.IsFunctionParameter());
		if (!VariableInfo.IsStructDeclaration()) 
		{
			Constructor.AddLine(Declaration);
		}
	}
	else if (VariableInfo.IsInit())
	{
		if (!Constructor.CurrentScopeHasVariable(Id))
		{
			ensureAlways(!Constructor.HasVariable(Id));
			Constructor.AddLine(Declaration);
			Constructor.AddVariable(Id, VariableName);
		}
	}
	else if (VariableInfo.IsStructDeclaration())
	{
		if (CacheOutputs[OutputIndex])
		{
			const auto StructDependencies = VariableInfo.GetStructDeclarationDependencies();
			check(StructDependencies != EVoxelAxisDependencies::XYZ);
			if (StructDependencies == Dependencies)
			{
				Constructor.AddLine(Declaration);
				Constructor.AddVariable(Id, FVoxelCppIds::GetCacheName(Dependencies) + "." + VariableName);
			}
		}
	}
	else if (VariableInfo.IsFunctionParameter())
	{
		Constructor.AddVariable(Id, VariableName);
	}
	else
	{
		// Hack to not have buffers in XYZWithoutCache
		// check IsConstant as else GetFunctionDependencies would crash
		if (!VariableInfo.IsConstant() && VariableInfo.GetFunctionDependencies() == EVoxelFunctionAxisDependencies::XYZWithoutCache)
		{
			if (!Constructor.CurrentScopeHasVariable(Id))
			{
				Constructor.AddLine(Declaration);
				Constructor.AddVariable(Id, VariableName);
			}
		}
		else
		{
			if (CacheOutputs[OutputIndex])
			{
				ensureAlways(Constructor.HasVariable(Id));
				ensureAlways(Constructor.HasVariable(Id) && Constructor.GetVariable(Id, nullptr) == FVoxelCppIds::GetCacheName(Dependencies) + "." + VariableName);
			}
			else
			{
				ensureAlways(!Constructor.HasVariable(Id));
				Constructor.AddLine(Declaration);
				Constructor.AddVariable(Id, VariableName);
			}
		}
	}
}

void FVoxelComputeNode::DeclareOutputs(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo) const
{
	for (int32 OutputIndex = 0; OutputIndex < OutputCount; OutputIndex++)
	{
		DeclareOutput(Constructor, VariableInfo, OutputIndex);
	}
}

TArray<FString> FVoxelComputeNode::GetInputsNamesCppImpl(FVoxelCppConstructor& Constructor, bool bOnlySeeds) const
{
	TArray<FString> Inputs;
	Inputs.SetNum(InputCount);
	for (int32 InputIndex = 0; InputIndex < InputCount; InputIndex++)
	{
		const bool bIsSeed = GetInputCategory(InputIndex) == EVoxelPinCategory::Seed;
		if ((bOnlySeeds && !bIsSeed) || (!bOnlySeeds && bIsSeed))
		{
			continue;
		}

		const int32 Id = GetInputId(InputIndex);
		if (Id == -1)
		{
			Inputs[InputIndex] = Constructor.GetTypeString(GetInputCategory(InputIndex)) + "(" + GetDefaultValueString(InputIndex) + ")";
		}
		else
		{
			Inputs[InputIndex] =  Constructor.GetVariable(Id, this);
		}
	}
	return Inputs;
}

TArray<FString> FVoxelComputeNode::GetOutputsNamesCpp(FVoxelCppConstructor& Constructor) const
{
	TArray<FString> Outputs;
	Outputs.SetNum(OutputCount);
	for (int32 OutputIndex = 0; OutputIndex < OutputCount; OutputIndex++)
	{
		const int32 Id = GetOutputId(OutputIndex);
		if (Id == -1)
		{
			Outputs[OutputIndex] = GetUnusedOutputName(OutputIndex);
		}
		else
		{
			Outputs[OutputIndex] = Constructor.GetVariable(Id, this);
		}
	}
	return Outputs;
}

void FVoxelComputeNode::GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const
{
	// Needed for forward decl
}

FString FVoxelComputeNode::GetUnusedOutputName(int32 OutputIndex) const
{
	check(GetOutputId(OutputIndex) == -1);
	return UniqueName.ToString() + "_Temp_" + FString::FromInt(OutputIndex);
}

///////////////////////////////////////////////////////////////////////////////

void FVoxelDataComputeNode::CallInitCpp(FVoxelCppConstructor& Constructor)
{
	if (!Constructor.IsNodeInit(this))
	{
		DeclareOutputs(Constructor, FVoxelVariableAccessInfo::Init());
		InitCpp(GetSeedInputsNamesCpp(Constructor), Constructor);
		Constructor.SetNodeAsInit(this);
	}
}

void FVoxelDataComputeNode::CallComputeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo) const
{
	DeclareOutputs(Constructor, VariableInfo);
	ComputeCpp(GetInputsNamesCpp(Constructor), GetOutputsNamesCpp(Constructor), Constructor);
}

void FVoxelDataComputeNode::CallComputeRangeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo) const
{
	DeclareOutputs(Constructor, VariableInfo);
	ComputeRangeCpp(GetInputsNamesCpp(Constructor), GetOutputsNamesCpp(Constructor), Constructor);
}

///////////////////////////////////////////////////////////////////////////////

void FVoxelSeedComputeNode::CallInitCpp(FVoxelCppConstructor& Constructor)
{
	if (!Constructor.IsNodeInit(this))
	{
		DeclareOutputs(Constructor, FVoxelVariableAccessInfo::Init());
		InitCpp(GetSeedInputsNamesCpp(Constructor), GetOutputsNamesCpp(Constructor), Constructor);
		Constructor.SetNodeAsInit(this);
	}
}

///////////////////////////////////////////////////////////////////////////////

void FVoxelSetterComputeNode::CallComputeSetterNodeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo, const TArray<FString>& GraphOutputs) const
{
	check(
		VariableInfo.GetFunctionDependencies() == EVoxelFunctionAxisDependencies::XYZWithoutCache ||
		VariableInfo.GetFunctionDependencies() == EVoxelFunctionAxisDependencies::XYZWithCache);
	ComputeSetterNodeCpp(Constructor, GetInputsNamesCpp(Constructor), GraphOutputs);
}

void FVoxelSetterComputeNode::CallComputeRangeSetterNodeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo, const TArray<FString>& GraphOutputs) const
{
	check(
		VariableInfo.GetFunctionDependencies() == EVoxelFunctionAxisDependencies::XYZWithoutCache ||
		VariableInfo.GetFunctionDependencies() == EVoxelFunctionAxisDependencies::XYZWithCache);
	ComputeRangeSetterNodeCpp(Constructor, GetInputsNamesCpp(Constructor), GraphOutputs);
}

