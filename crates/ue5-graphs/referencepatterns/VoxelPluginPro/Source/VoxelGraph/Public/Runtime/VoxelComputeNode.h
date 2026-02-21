// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/VoxelNodeType.h"
#include "VoxelAxisDependencies.h"
#include "VoxelPinCategory.h"
#include "VoxelGraphGlobals.h"

class UVoxelNode;
class UVoxelGraphGenerator;
class FVoxelCppConstructor;
class FVoxelCppConfig;
class FVoxelCompilationNode;
class FVoxelGraphFunction;
class FVoxelVariable;
struct FVoxelContext;
struct FVoxelGeneratorInit;
struct FVoxelContextRange;
struct FVoxelGraphVMOutputBuffers;
struct FVoxelGraphVMRangeOutputBuffers;

enum class EVoxelVariableType : uint8
{
	Constant,
	Init,
	Compute
};

struct FVoxelVariableAccessInfo
{
public:
	inline bool IsInit() const
	{
		return bIsInit;
	}
	inline bool IsConstant() const
	{
		return bIsConstant;
	}
	inline bool IsStructDeclaration() const
	{
		return bIsStructDeclaration;
	}
	inline EVoxelAxisDependencies GetStructDeclarationDependencies() const
	{
		check(bIsStructDeclaration);
		return StructDependencies;
	}
	inline bool IsFunctionParameter() const
	{
		return bIsFunctionParameter;
	}

	inline EVoxelFunctionAxisDependencies GetFunctionDependencies() const
	{
		check(FunctionDependencies != EVoxelFunctionAxisDependencies(-1));
		return FunctionDependencies;
	}

public:
	inline static FVoxelVariableAccessInfo Init()
	{
		FVoxelVariableAccessInfo Info;
		Info.bIsInit = true;
		return Info;
	}
	inline static FVoxelVariableAccessInfo Constant()
	{
		FVoxelVariableAccessInfo Info;
		Info.bIsConstant = true;
		return Info;
	}
	inline static FVoxelVariableAccessInfo Compute(EVoxelFunctionAxisDependencies FunctionDependencies)
	{
		FVoxelVariableAccessInfo Info;
		Info.FunctionDependencies = FunctionDependencies;
		return Info;
	}
	inline static FVoxelVariableAccessInfo StructDeclaration(EVoxelAxisDependencies StructDependencies)
	{
		FVoxelVariableAccessInfo Info;
		Info.bIsStructDeclaration = true;
		Info.StructDependencies = StructDependencies;
		return Info;
	}
	inline static FVoxelVariableAccessInfo FunctionParameter()
	{
		FVoxelVariableAccessInfo Info;
		Info.bIsFunctionParameter = true;
		return Info;
	}

private:
	bool bIsInit = false;
	bool bIsConstant = false;
	bool bIsStructDeclaration = false;
	bool bIsFunctionParameter = false;
	EVoxelFunctionAxisDependencies FunctionDependencies = EVoxelFunctionAxisDependencies(-1);

	// Only valid when used to declare structs
	EVoxelAxisDependencies StructDependencies = EVoxelAxisDependencies(-1);

	FVoxelVariableAccessInfo() = default;
};

enum class EVoxelComputeNodeType : uint8
{
	Data,
	Seed,
	Exec
};

class VOXELGRAPH_API FVoxelComputeNode
{
public:
	const EVoxelComputeNodeType Type;
	const int32 InputCount;
	const int32 OutputCount;
	const TArray<int32, TInlineAllocator<16>> InputIds;
	const TArray<int32, TInlineAllocator<16>> OutputIds;
	const TArray<bool> CacheOutputs;

public:
	const FString PrettyName;
	const FName UniqueName;
	const TWeakObjectPtr<const UVoxelNode> Node;
	// First are deeper in the callstack
	const TArray<TWeakObjectPtr<const UVoxelNode>> SourceNodes;
	const EVoxelAxisDependencies Dependencies;

	virtual ~FVoxelComputeNode() = default;

public:
	UVoxelGraphGenerator* GetGraph() const;
	
	inline FString GetDefaultValueString(int32 Index) const { return DefaultValueStrings[Index]; }

	FORCEINLINE int32 GetInputId(int32 Index) const { checkVoxelGraph(0 <= Index && Index < InputCount); return InputIds.GetData()[Index]; }
	FORCEINLINE int32 GetOutputId(int32 Index) const { checkVoxelGraph(0 <= Index && Index < OutputCount); return OutputIds.GetData()[Index]; }

	FORCEINLINE bool IsOutputUsed(int32 Index) const { return GetOutputId(Index) != -1; }

	EVoxelPinCategory GetInputCategory(int32 Index) const { return InputsCategories[Index]; }
	EVoxelPinCategory GetOutputCategory(int32 Index) const { return OutputsCategories[Index]; }

	void DeclareOutput(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo, int32 OutputIndex) const;
	void DeclareOutputs(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo) const;

	TArray<FString> GetInputsNamesCpp(FVoxelCppConstructor& Constructor) const { return GetInputsNamesCppImpl(Constructor, false); }
	TArray<FString> GetOutputsNamesCpp(FVoxelCppConstructor& Constructor) const;

	TArray<FString> GetSeedInputsNamesCpp(FVoxelCppConstructor& Constructor) const { return GetInputsNamesCppImpl(Constructor, true); }

public:
	template<typename T>
	T GetDefaultValue(int32 Index) const;
	template<typename T>
	void CopyVariablesToInputs(T* RESTRICT Variables, T* RESTRICT InputBuffer) const;
	template<typename T>
	void CopyOutputsToVariables(T* RESTRICT OutputBuffer, T* RESTRICT Variables) const;
	
public:
	void CallSetupCpp(FVoxelCppConfig& Config) { check(!bIsConstructorSetup); bIsConstructorSetup = true; SetupCpp(Config); }

protected:
	FVoxelComputeNode(EVoxelComputeNodeType Type, const UVoxelNode& Node, const FVoxelCompilationNode& CompilationNode);

	//~ Begin FVoxelComputeNode Interface
public:
	virtual void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const;
protected:
	virtual void SetupCpp(FVoxelCppConfig& Config) const {}
	//~ End FVoxelComputeNode Interface

private:
	TArray<FVoxelNodeType> DefaultValues;
	TArray<FVoxelNodeRangeType> DefaultRangeValues;
	TArray<FString> DefaultValueStrings;
	TArray<EVoxelPinCategory> InputsCategories;
	TArray<EVoxelPinCategory> OutputsCategories;
	
	bool bIsConstructorSetup = false;
	
	TArray<FString> GetInputsNamesCppImpl(FVoxelCppConstructor& Constructor, bool bOnlySeeds) const;
	FString GetUnusedOutputName(int32 OutputIndex) const;
};

template<>
inline FVoxelGraphSeed FVoxelComputeNode::GetDefaultValue<FVoxelGraphSeed>(int32 Index) const { return DefaultValues[Index].Get<int32>(); }

template<>
inline FVoxelNodeType FVoxelComputeNode::GetDefaultValue<FVoxelNodeType>(int32 Index) const { checkVoxelGraph(DefaultValues.IsValidIndex(Index)); return DefaultValues.GetData()[Index]; }

template<>
inline FVoxelNodeRangeType FVoxelComputeNode::GetDefaultValue<FVoxelNodeRangeType>(int32 Index) const { return DefaultRangeValues[Index]; }

template<typename T>
inline void FVoxelComputeNode::CopyVariablesToInputs(T* RESTRICT Variables, T* RESTRICT InputBuffer) const
{
	for (int32 InputIndex = 0; InputIndex < InputCount; InputIndex++)
	{
		int32 Id = GetInputId(InputIndex);
		if (Id == -1)
		{
			checkVoxelGraph(0 <= InputIndex && InputIndex < MAX_VOXELNODE_PINS);
			InputBuffer[InputIndex] = GetDefaultValue<T>(InputIndex);
		}
		else
		{
			checkVoxelGraph(0 <= InputIndex && InputIndex < MAX_VOXELNODE_PINS);
			InputBuffer[InputIndex] = Variables[Id];
		}
	}
}

template<typename T>
inline void FVoxelComputeNode::CopyOutputsToVariables(T* RESTRICT OutputBuffer, T* RESTRICT Variables) const
{
	for (int32 OutputIndex = 0; OutputIndex < OutputCount; OutputIndex++)
	{
		int32 Id = GetOutputId(OutputIndex);
		if (Id != -1) // Can be -1 if unused
		{
			checkVoxelGraph(0 <= OutputIndex && OutputIndex < MAX_VOXELNODE_PINS);
			Variables[Id] = OutputBuffer[OutputIndex];
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELGRAPH_API FVoxelDataComputeNode : public FVoxelComputeNode
{
public:
	FVoxelDataComputeNode(const UVoxelNode& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelComputeNode(EVoxelComputeNodeType::Data, Node, CompilationNode)
	{
	}

	using FComputeFunction = void(FVoxelDataComputeNode::*)(FVoxelNodeInputBuffer, FVoxelNodeOutputBuffer, const FVoxelContext&) const;
	FComputeFunction ComputeFunctionPtr = nullptr;

	//~ Begin FVoxelDataComputeNode Interface
	virtual void Init(FVoxelGraphSeed Inputs[], const FVoxelGeneratorInit& InitStruct) {}
	virtual void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const {}

	virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const = 0;
	virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const = 0;
	virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const = 0;
	virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const = 0;
	
	virtual double ComputeStats(double Threshold, bool bSimulateCpp, FVoxelNodeInputBuffer Inputs, const FVoxelContext& Context) const = 0;
	virtual double ComputeRangeStats(double Threshold, bool bSimulateCpp, FVoxelNodeInputRangeBuffer Inputs, const FVoxelContextRange& Context) const = 0;
	
	virtual void CacheFunctionPtr() = 0;
	//~ End FVoxelDataComputeNode Interface

	void CallInitCpp        (FVoxelCppConstructor& Constructor);
	void CallComputeCpp     (FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo) const;
	void CallComputeRangeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo) const;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELGRAPH_API FVoxelSeedComputeNode : public FVoxelComputeNode
{
public:
	FVoxelSeedComputeNode(const UVoxelNode& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelComputeNode(EVoxelComputeNodeType::Seed, Node, CompilationNode)
	{
	}

	//~ Begin FVoxelSeedComputeNode Interface
	virtual void Init(FVoxelGraphSeed Inputs[], FVoxelGraphSeed Outputs[], const FVoxelGeneratorInit& InitStruct) {};
	virtual void InitCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const {}
	//~ End FVoxelSeedComputeNode Interface

	void CallInitCpp(FVoxelCppConstructor& Constructor);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

enum class EVoxelComputeNodeExecType : uint8
{
	Passthrough,
	If,
	Setter,
	FunctionCall,
	FunctionInit
};

class VOXELGRAPH_API FVoxelExecComputeNode : public FVoxelComputeNode
{
public:
	const EVoxelComputeNodeExecType ExecType;

	FVoxelExecComputeNode(EVoxelComputeNodeExecType ExecType, const UVoxelNode& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelComputeNode(EVoxelComputeNodeType::Exec, Node, CompilationNode)
		, ExecType(ExecType)
	{
	}

	//~ Begin FVoxelExecComputeNode Interface
	// Used only for materials node to check that the right config is used
	virtual void Init(const FVoxelGeneratorInit& InitStruct) {};
	//~ End FVoxelExecComputeNode Interface
};

template<EVoxelComputeNodeExecType T>
class TVoxelExecComputeNodeHelper : public FVoxelExecComputeNode
{
public:
	TVoxelExecComputeNodeHelper(const UVoxelNode& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelExecComputeNode(T, Node, CompilationNode)
	{
	}
};

using FVoxelPassthroughComputeNode = TVoxelExecComputeNodeHelper<EVoxelComputeNodeExecType::Passthrough>;

class VOXELGRAPH_API FVoxelSetterComputeNode : public TVoxelExecComputeNodeHelper<EVoxelComputeNodeExecType::Setter>
{
public:
	using TVoxelExecComputeNodeHelper::TVoxelExecComputeNodeHelper;

	//~ Begin FVoxelSetterComputeNode Interface
	virtual void ComputeSetterNode(FVoxelNodeInputBuffer Inputs, FVoxelGraphVMOutputBuffers& GraphOutputs) const = 0;
	virtual void ComputeRangeSetterNode(FVoxelNodeInputRangeBuffer Inputs, FVoxelGraphVMRangeOutputBuffers& GraphOutputs) const = 0;
	virtual void ComputeSetterNodeCpp(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, const TArray<FString>& GraphOutputs) const = 0;
	virtual void ComputeRangeSetterNodeCpp(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, const TArray<FString>& GraphOutputs) const = 0;
	//~ End FVoxelSetterComputeNode Interface

	void CallComputeSetterNodeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo, const TArray<FString>& GraphOutputs) const;
	void CallComputeRangeSetterNodeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo, const TArray<FString>& GraphOutputs) const;
};
