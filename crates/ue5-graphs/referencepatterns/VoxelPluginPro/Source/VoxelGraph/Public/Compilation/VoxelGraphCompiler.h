// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelMinimal.h"

class FVoxelCompilationNode;
class UVoxelNode;
class UVoxelGraphGenerator;

class FVoxelGraphCompiler
{
public:
	FVoxelGraphErrorReporter& ErrorReporter;

	FVoxelCompilationNode* FirstNode = nullptr;
	int32 FirstNodePinIndex = -1;

	TMap<const FVoxelCompilationNode*, const FVoxelCompilationNode*> Parents;
	TMap<const FVoxelCompilationNode*, int32> DuplicateCounts;

	FVoxelGraphCompiler(const TSharedRef<FVoxelGraphErrorReporter>& ErrorReporter);
	FVoxelGraphCompiler(UVoxelGraphGenerator* Graph);
	
	TSharedRef<FVoxelGraphCompiler> Clone(const FString& ErrorPrefix, TMap<FVoxelCompilationNode*, FVoxelCompilationNode*>& OutOldNodesToNewNodes) const;
	TSharedRef<FVoxelGraphCompiler> Clone(const FString& ErrorPrefix) const;

	TMap<UVoxelNode*, FVoxelCompilationNode*> InitFromNodes(const TArray<UVoxelNode*>& Nodes, UVoxelNode* FirstNode, int32 FirstNodePinIndex);

public:
	inline const TSet<FVoxelCompilationNode*>& GetAllNodes() const { return Nodes; }
	inline TSet<FVoxelCompilationNode*> GetAllNodesCopy() const { return Nodes; }

public:
	FVoxelCompilationNode* AddNode(UVoxelNode* Node);
	FVoxelCompilationNode* AddNode(const TSharedPtr<FVoxelCompilationNode>& Ref, FVoxelCompilationNode* SourceNode = nullptr);
	void RemoveNode(FVoxelCompilationNode* Node);

	template<typename T, typename ...TArgs>
	inline void ApplyPass(TArgs&&... Args)
	{
		if (!ErrorReporter.HasError())
		{
#if CPUPROFILERTRACE_ENABLED
			// Static id is static for the template, so it's per class
			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(T::GetName());
#endif
			T::Apply(*this, Forward<TArgs>(Args)...);
			Check();
		}
	}

public:
	void Check() const;
	void AppendAndClear(FVoxelGraphCompiler& Other);

private:
	const TSharedRef<FVoxelGraphErrorReporter> ErrorReporterRef;
	TArray<TSharedPtr<FVoxelCompilationNode>> NodesRefs;
	TSet<FVoxelCompilationNode*> Nodes;

public:
	inline static int32 GetCompilationId()
	{
		return CompilationId;
	}
	inline static void IncreaseCompilationId()
	{
		CompilationId++;
	}

private:
	static int32 CompilationId;
};

struct FVoxelCompilationFunctionDescriptor
{
	int32 const FunctionId;
	FVoxelCompilationNode* const FirstNode;
	TSet<FVoxelCompilationNode*> Nodes;

	FVoxelCompilationFunctionDescriptor(
		int32 FunctionId,
		FVoxelCompilationNode* FirstNode)
		: FunctionId(FunctionId)
		, FirstNode(FirstNode)
	{
	}
};

// Required to avoid duplicating compute nodes
class FVoxelCreatedComputeNodes
{
public:
	FVoxelCreatedComputeNodes() = default;
	FVoxelCreatedComputeNodes(FVoxelCreatedComputeNodes const&) = delete;
	FVoxelCreatedComputeNodes& operator=(FVoxelCreatedComputeNodes const&) = delete;

	TVoxelSharedRef<FVoxelComputeNode> GetComputeNode(const FVoxelCompilationNode& Node);

private:
	TMap<const FVoxelCompilationNode*, TVoxelSharedPtr<FVoxelComputeNode>> Map;
};
