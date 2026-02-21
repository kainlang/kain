// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Compilation/VoxelCompilationNode.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelAxisDependencies.h"
#include "VoxelMinimal.h"

class FVoxelComputeNodeTree;

template<typename InClass>
class TVoxelDefaultCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::Default, InClass>
{
public:
	using TVoxelCompilationNode<EVoxelCompilationNodeType::Default, InClass>::TVoxelCompilationNode;

	virtual FString GetClassName() const override { return "Default"; }
};

class FVoxelDefaultCompilationNode : public TVoxelDefaultCompilationNode<FVoxelDefaultCompilationNode>
{
public:
	using TVoxelDefaultCompilationNode::TVoxelDefaultCompilationNode;
};

class FVoxelDefaultPureCompilationNode : public TVoxelDefaultCompilationNode<FVoxelDefaultPureCompilationNode>
{
public:
	using TVoxelDefaultCompilationNode::TVoxelDefaultCompilationNode;

	virtual bool IsPureNode() const override { return true; }
	virtual FString GetClassName() const override { return "DefaultPure"; }
};

class FVoxelPassthroughCompilationNode : public FVoxelCompilationNode
{
public:
	using FVoxelCompilationNode::FVoxelCompilationNode;

	static EVoxelCompilationNodeType StaticType()
	{
		return EVoxelCompilationNodeType::Passthrough;
	}
	virtual FString GetClassName() const override
	{
		return "Passthrough";
	}

	template<EVoxelCompilationNodeType InType, typename InClass, typename InParent>
	friend class TVoxelCompilationNode;
};

class FVoxelSetterCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::Setter, FVoxelSetterCompilationNode>
{
public:
	uint32 OutputIndex = -1;

	using TVoxelCompilationNode::TVoxelCompilationNode;
	
	static void CopyProperties(const FVoxelSetterCompilationNode& From, FVoxelSetterCompilationNode& To)
	{
		To.OutputIndex = From.OutputIndex;
	}
	virtual void Check(FVoxelGraphErrorReporter& ErrorReporter) const override
	{
		ensureVoxelGraph(GetInputCount() > 1 && GetOutputCount() == 1, this);
	}
	virtual FString GetClassName() const override
	{
		return "Setter";
	}
};

class FVoxelAxisDependenciesCompilationNode : public TVoxelDefaultCompilationNode<FVoxelAxisDependenciesCompilationNode>
{
public:
	uint8 DefaultDependencies = -1;
	
	using TVoxelDefaultCompilationNode::TVoxelDefaultCompilationNode;
	
	static void CopyProperties(const FVoxelAxisDependenciesCompilationNode& From, FVoxelAxisDependenciesCompilationNode& To)
	{
		To.DefaultDependencies = From.DefaultDependencies;
	}
	virtual uint8 GetDefaultAxisDependencies() const override
	{
		return DefaultDependencies;
	}
	virtual FString GetClassName() const override
	{
		return "Axis Dependencies";
	}
};

class FVoxelFunctionSeparatorCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::FunctionSeparator, FVoxelFunctionSeparatorCompilationNode>
{
public:
	int32 FunctionId = -1;

	using TVoxelCompilationNode::TVoxelCompilationNode;

	static void CopyProperties(const FVoxelFunctionSeparatorCompilationNode& OldNode, FVoxelFunctionSeparatorCompilationNode& NewNode)
	{
		NewNode.FunctionId = OldNode.FunctionId;
	}
	virtual FString GetClassName() const override { return "Function Separator"; }
};

class FVoxelFunctionInitCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::FunctionInit, FVoxelFunctionInitCompilationNode>
{
public:
	int32 FunctionId = -1;

	using TVoxelCompilationNode::TVoxelCompilationNode;
	FVoxelFunctionInitCompilationNode(const FVoxelFunctionSeparatorCompilationNode& Separator);

	static void CopyProperties(const FVoxelFunctionInitCompilationNode& OldNode, FVoxelFunctionInitCompilationNode& NewNode)
	{
		NewNode.FunctionId = OldNode.FunctionId;
	}
	virtual void Check(FVoxelGraphErrorReporter& ErrorReporter) const override
	{
		ensureVoxelGraph(GetInputCount() == 0, this);
	}
	virtual uint8 GetDefaultAxisDependencies() const override
	{
		// Functions init depend on X, else their outputs are cached
		return EVoxelAxisDependenciesFlags::X;
	}
	virtual FString GetClassName() const override
	{
		return "FunctionInit";
	}

	virtual TVoxelSharedPtr<FVoxelComputeNode> GetComputeNode() const override;
};

class FVoxelFunctionCallCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::FunctionCall, FVoxelFunctionCallCompilationNode>
{
public:
	int32 FunctionId = -1;
	
	using TVoxelCompilationNode::TVoxelCompilationNode;
	FVoxelFunctionCallCompilationNode(const FVoxelFunctionSeparatorCompilationNode& Separator);

	static void CopyProperties(const FVoxelFunctionCallCompilationNode& OldNode, FVoxelFunctionCallCompilationNode& NewNode)
	{
		NewNode.FunctionId = OldNode.FunctionId;
	}
	virtual void Check(FVoxelGraphErrorReporter& ErrorReporter) const override
	{
		ensureVoxelGraph(GetOutputCount() == 0, this);
	}
	virtual FString GetClassName() const override
	{
		return "FunctionCall";
	}

	virtual TVoxelSharedPtr<FVoxelComputeNode> GetComputeNode() const override { check(false); return {}; }
	TVoxelSharedPtr<FVoxelComputeNode> GetComputeNode(EVoxelFunctionAxisDependencies FunctionDependencies) const;
};

class FVoxelFlowMergeCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::FlowMerge, FVoxelFlowMergeCompilationNode>
{
public:
	using TVoxelCompilationNode::TVoxelCompilationNode;

	virtual FString GetClassName() const override { return "FlowMerge"; }
};

class FVoxelIfCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::If, FVoxelIfCompilationNode>
{
public:
	using TVoxelCompilationNode::TVoxelCompilationNode;

	virtual FString GetClassName() const override { return "If"; }
};

class FVoxelRangeAnalysisConstantCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::RangeAnalysisConstant, FVoxelRangeAnalysisConstantCompilationNode>
{
public:
	using TVoxelCompilationNode::TVoxelCompilationNode;

	virtual FString GetClassName() const override { return "RangeAnalysisConstant"; }
};

class FVoxelGetRangeAnalysisCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::GetRangeAnalysis, FVoxelGetRangeAnalysisCompilationNode>
{
public:
	int32 VariablesBufferSize = -1;
	TVoxelSharedPtr<FVoxelComputeNodeTree> Tree;
	uint8 DefaultDependencies = 0;
	
	using TVoxelCompilationNode::TVoxelCompilationNode;

	virtual FString GetClassName() const override { return "GetRangeAnalysis"; }
	virtual uint8 GetDefaultAxisDependencies() const override { return DefaultDependencies; }
	
	static void CopyProperties(const FVoxelGetRangeAnalysisCompilationNode& From, FVoxelGetRangeAnalysisCompilationNode& To)
	{
		To.VariablesBufferSize = From.VariablesBufferSize;
		To.Tree = From.Tree;
	}
};

class FVoxelSmartMinMaxCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::SmartMinMax, FVoxelSmartMinMaxCompilationNode>
{
public:
	bool bIsMin = false;
	FVoxelCompilationNode* OutputPassthrough = nullptr;

	using TVoxelCompilationNode::TVoxelCompilationNode;

	virtual FString GetClassName() const override { return "SmartMinMax"; }
	
	static void CopyProperties(const FVoxelSmartMinMaxCompilationNode& From, FVoxelSmartMinMaxCompilationNode& To)
	{
		To.bIsMin = From.bIsMin;
		To.OutputPassthrough = From.OutputPassthrough;
	}
};

class FVoxelMacroCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::Macro, FVoxelMacroCompilationNode>
{
public:
	FVoxelCompilationNode* InputNode = nullptr;
	FVoxelCompilationNode* OutputNode = nullptr;

	using TVoxelCompilationNode::TVoxelCompilationNode;
	
	static void CopyProperties(const FVoxelMacroCompilationNode& From, FVoxelMacroCompilationNode& To)
	{
		To.InputNode = From.InputNode;
		To.OutputNode = From.OutputNode;
	}
};

struct FVoxelMacroInputOuputCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::MacroInputOutput, FVoxelMacroInputOuputCompilationNode>
{
public:
	TArray<FVoxelCompilationNode*> Passthroughs;

	using TVoxelCompilationNode::TVoxelCompilationNode;
	
	static void CopyProperties(const FVoxelMacroInputOuputCompilationNode& From, FVoxelMacroInputOuputCompilationNode& To)
	{
		To.Passthroughs = From.Passthroughs;
	}
};

class FVoxelLocalVariableDeclarationCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::LocalVariableDeclaration, FVoxelLocalVariableDeclarationCompilationNode>
{
	using TVoxelCompilationNode::TVoxelCompilationNode;
};

class FVoxelLocalVariableUsageCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::LocalVariableUsage, FVoxelLocalVariableUsageCompilationNode>
{
public:
	FVoxelCompilationNode* Passthrough = nullptr; // For preview

	using TVoxelCompilationNode::TVoxelCompilationNode;

	static void CopyProperties(const FVoxelLocalVariableUsageCompilationNode& From, FVoxelLocalVariableUsageCompilationNode& To)
	{
		To.Passthrough = From.Passthrough;
	}
};

class FVoxelBiomeMergeCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::BiomeMerge, FVoxelBiomeMergeCompilationNode>
{
public:
	TArray<FVoxelCompilationNode*> OutputPassthroughs; // For preview
	float Tolerance;

	using TVoxelCompilationNode::TVoxelCompilationNode;

	virtual FString GetClassName() const override { return "BiomeMerge"; }

	static void CopyProperties(const FVoxelBiomeMergeCompilationNode& From, FVoxelBiomeMergeCompilationNode& To)
	{
		To.OutputPassthroughs = From.OutputPassthroughs;
		To.Tolerance = From.Tolerance;
	}
};

class FVoxelCompileTimeConstantCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::CompileTimeConstant, FVoxelCompileTimeConstantCompilationNode>
{
public:
	FName Name;

	using TVoxelCompilationNode::TVoxelCompilationNode;

	virtual FString GetClassName() const override { return "CompileTimeConstant"; }

	static void CopyProperties(const FVoxelCompileTimeConstantCompilationNode& From, FVoxelCompileTimeConstantCompilationNode& To)
	{
		To.Name = From.Name;
	}
};

class FVoxelSwitchCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::Switch, FVoxelSwitchCompilationNode>
{
public:
	using TVoxelCompilationNode::TVoxelCompilationNode;

	virtual FString GetClassName() const override { return "Switch"; }
	virtual bool IsPureNode() const override { return true; }
};
