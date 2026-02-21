// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppIds.h"
#include "Runtime/VoxelGraphFunction.h"
#include "VoxelGraphConstants.h"
#include "VoxelAxisDependencies.h"
#include "VoxelGraphErrorReporter.h"

void FVoxelCppConstructor::GetCode(FString& OutCode) const
{
	for (auto& Line : Lines)
	{
		OutCode.Append(Line);
		OutCode.Append("\n");
	}
}

void FVoxelCppConstructor::AddOtherConstructor(const FVoxelCppConstructor& Other)
{
	for (auto& Line : Other.Lines)
	{
		AddLineInternal(Line);
	}
	ensure(Other.CurrentIndent == 0);
	ensure(Other.CurrentScope == &Other.MainScope);
}

void FVoxelCppConstructor::AddFunctionCall(const FVoxelGraphFunctionInfo& Info, const TArray<FString>& Args)
{
	FString Line = Info.GetFunctionName();
	
	Line += "(";
	if (Info.FunctionType == EVoxelFunctionType::Init)
	{
		Line += FVoxelCppIds::InitStruct;
	}
	else
	{
		check(Info.FunctionType == EVoxelFunctionType::Compute);
		Line += FVoxelCppIds::Context;
		if (Info.Dependencies == EVoxelFunctionAxisDependencies::X ||
			Info.Dependencies == EVoxelFunctionAxisDependencies::XYWithoutCache || // Still need to compute X variables
			Info.Dependencies == EVoxelFunctionAxisDependencies::XYWithCache ||
			Info.Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache)
		{
			Line += ", " + FVoxelCppIds::GetCacheName(EVoxelAxisDependencies::X);
		}
		if (Info.Dependencies == EVoxelFunctionAxisDependencies::XYWithoutCache ||
			Info.Dependencies == EVoxelFunctionAxisDependencies::XYWithCache ||
			Info.Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache)
		{
			Line += ", " + FVoxelCppIds::GetCacheName(EVoxelAxisDependencies::XY);
		}
		if (Info.Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache ||
			Info.Dependencies == EVoxelFunctionAxisDependencies::XYZWithoutCache)
		{
			Line += ", " + FVoxelCppIds::GraphOutputs;
		}
	}
	for (auto& Arg : Args)
	{
		Line += ", " + Arg;
	}
	Line += ");";
	AddLine(Line);
}

void FVoxelCppConstructor::AddFunctionDeclaration(const FVoxelGraphFunctionInfo& Info, const TArray<FString>& Args)
{
	FString Line = "void " + Info.GetFunctionName();
	Line += "(";
	if (Info.FunctionType == EVoxelFunctionType::Init)
	{
		Line += "const FVoxelGeneratorInit& " + FVoxelCppIds::InitStruct;
	}
	else
	{
		check(Info.FunctionType == EVoxelFunctionType::Compute);
		Line += "const " + GetContextTypeString() + "& " + FVoxelCppIds::Context;

		const auto Dependencies = Info.Dependencies;

		if (Dependencies == EVoxelFunctionAxisDependencies::X ||
			Dependencies == EVoxelFunctionAxisDependencies::XYWithoutCache || // Still need to compute X variables
			Dependencies == EVoxelFunctionAxisDependencies::XYWithCache ||
			Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache)
		{
			Line += ", ";
			if (Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache)
			{
				Line += "const ";
			}
			Line += FVoxelCppIds::GetCacheType(EVoxelAxisDependencies::X) + "& " + FVoxelCppIds::GetCacheName(EVoxelAxisDependencies::X);
		}

		if (Dependencies == EVoxelFunctionAxisDependencies::XYWithoutCache ||
			Dependencies == EVoxelFunctionAxisDependencies::XYWithCache ||
			Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache)
		{
			Line += ", ";
			if (Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache)
			{
				Line += "const ";
			}
			Line += FVoxelCppIds::GetCacheType(EVoxelAxisDependencies::XY) + "& " + FVoxelCppIds::GetCacheName(EVoxelAxisDependencies::XY);
		}

		if (Info.Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache || Info.Dependencies == EVoxelFunctionAxisDependencies::XYZWithoutCache)
		{
			Line += ", " + FVoxelCppIds::GraphOutputsType + "& " + FVoxelCppIds::GraphOutputs;
		}
	}
	for (auto& Arg : Args)
	{
		Line += ", " + Arg;
	}
	Line += ")";
	if (Info.FunctionType != EVoxelFunctionType::Init)
	{
		Line += " const";
	}
	AddLine(Line);
}

///////////////////////////////////////////////////////////////////////////////

void FVoxelCppConstructor::AddVariable(int32 Id, const FString& Value)
{
	check(Id >= 0 && !CurrentScope->Variables.Contains(Id));
	CurrentScope->Variables.Add(Id, Value);
}

bool FVoxelCppConstructor::HasVariable(int32 Id)
{
	return CurrentScope->GetVariable(Id) != nullptr;
}

bool FVoxelCppConstructor::CurrentScopeHasVariable(int32 Id)
{
	return CurrentScope->Variables.Contains(Id);
}

FString FVoxelCppConstructor::GetVariable(int32 Id, const FVoxelComputeNode* Node)
{
	check(Id >= 0);
	if (auto* Variable = CurrentScope->GetVariable(Id))
	{
		return *Variable;
	}
	else
	{
		ErrorReporter.AddInternalError("Invalid Id in GetVariable.");
		ErrorReporter.AddMessageToNode(Node, "INTERNAL ERROR: Invalid Id in GetVariable", EVoxelGraphNodeMessageType::Error, true);
		return "ERROR" + FString::FromInt(Id);
	}
}

void FVoxelCppConstructor::StartScope()
{
	CurrentScope = CurrentScope->GetChild();
}

void FVoxelCppConstructor::EndScope()
{
	CurrentScope = CurrentScope->GetParent();
	CurrentScope->RemoveChild();
}

bool FVoxelCppConstructor::IsNodeInit(FVoxelComputeNode* Node) const
{
	return CurrentScope->IsNodeInit(Node);
}

void FVoxelCppConstructor::SetNodeAsInit(FVoxelComputeNode* Node)
{
	CurrentScope->NodesAlreadyInit.Add(Node);
}

///////////////////////////////////////////////////////////////////////////////

FString FVoxelCppConstructor::GetTypeString(EVoxelPinCategory Category) const
{
	if (Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex))
	{
		return FVoxelPinCategory::GetRangeTypeString(Category);
	}
	else
	{
		return FVoxelPinCategory::GetTypeString(Category);
	}
}

FString FVoxelCppConstructor::GetTypeString(EVoxelDataPinCategory Category) const
{
	return GetTypeString(FVoxelPinCategory::DataPinToPin(Category));
}

FString FVoxelCppConstructor::GetContextTypeString() const
{
	if (Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex))
	{
		return "FVoxelContextRange";
	}
	else
	{
		return "FVoxelContext";
	}
}
