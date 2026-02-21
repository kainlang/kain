// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/VoxelNodeType.h"
#include "Runtime/VoxelComputeNode.h"

class FVoxelGraphStatsRecorderBase
{
public:
	static constexpr double Threshold = 1e-4;
	
	struct FNodeStats
	{
		double EstimatedCppTime = 0;
		double VirtualMachineTime = 0;

		FNodeStats& operator+=(const FNodeStats& Other)
		{
			EstimatedCppTime += Other.EstimatedCppTime;
			VirtualMachineTime += Other.VirtualMachineTime;
			return *this;
		}
	};
	struct FScope
	{
	};

	const auto& GetStats() const { return Stats; }
	
protected:
	TMap<FVoxelComputeNode*, FNodeStats> Stats;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelGraphStatsRecorder : public FVoxelGraphStatsRecorderBase
{
public:
	FORCEINLINE FScope MakeDataScope(FVoxelDataComputeNode* Node, FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context)
	{
		auto& NodeStats = Stats.FindOrAdd(Node);
		ensureVoxelSlowNoSideEffects(NodeStats.EstimatedCppTime == 0);
		ensureVoxelSlowNoSideEffects(NodeStats.VirtualMachineTime == 0);
		
		NodeStats.EstimatedCppTime = Node->ComputeStats(Threshold, true, Inputs, Context);
		NodeStats.VirtualMachineTime = Node->ComputeStats(Threshold, false, Inputs, Context);
		
		return {};
	}
	FORCEINLINE FScope MakeSetterScope(FVoxelExecComputeNode* ExecNode, FVoxelNodeInputBuffer Inputs, const FVoxelContext& Context)
	{
		return {};
	}
};

class FVoxelGraphStatsRangeRecorder : public FVoxelGraphStatsRecorderBase
{
public:
	FORCEINLINE FScope MakeDataScope(FVoxelDataComputeNode* Node, FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context)
	{
		auto& NodeStats = Stats.FindOrAdd(Node);
		
		ensureVoxelSlowNoSideEffects(NodeStats.EstimatedCppTime == 0);
		NodeStats.EstimatedCppTime = Node->ComputeRangeStats(Threshold, true, Inputs, Context);
		NodeStats.VirtualMachineTime = Node->ComputeRangeStats(Threshold, false, Inputs, Context);

		return {};
	}
	FORCEINLINE FScope MakeSetterScope(FVoxelExecComputeNode* ExecNode, FVoxelNodeInputRangeBuffer Inputs, const FVoxelContextRange& Context)
	{
		return {};
	}
	
	FORCEINLINE FScope MakeIfScope(FVoxelExecComputeNode* ExecNode)
	{
		return {};
	}
};
