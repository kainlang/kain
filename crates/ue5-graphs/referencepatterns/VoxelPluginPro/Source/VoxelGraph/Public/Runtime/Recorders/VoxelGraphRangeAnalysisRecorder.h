// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/VoxelNodeType.h"
#include "Runtime/VoxelComputeNode.h"

class FVoxelGraphRangeAnalysisRecorder
{
public:
	struct FScope
	{
		FVoxelDataComputeNode& Node;
		FVoxelNodeOutputBuffer Source;
		TArray<FVoxelNodeType>& Destination;

		FScope(FVoxelDataComputeNode& Node, FVoxelNodeOutputBuffer Source, TArray<FVoxelNodeType>& Destination)
			: Node(Node)
			, Source(Source)
			, Destination(Destination)
		{
		}
		~FScope()
		{
			Destination.Empty(Node.OutputCount);
			Destination.SetNumZeroed(Node.OutputCount);
			for (int32 OutputIndex = 0; OutputIndex < Node.OutputCount; OutputIndex++)
			{
				Destination[OutputIndex] = Source[OutputIndex];
			}
		}
	};
	struct FEmptyScope
	{
	};
	
	FORCEINLINE FScope MakeDataScope(FVoxelDataComputeNode* Node, FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context)
	{
		ensureVoxelSlowNoSideEffects(!NodesOutputs.Contains(Node));
		return FScope(*Node, Outputs, NodesOutputs.Add(Node));
	}
	FORCEINLINE FEmptyScope MakeSetterScope(FVoxelExecComputeNode* ExecNode, FVoxelNodeInputBuffer Inputs, const FVoxelContext& Context)
	{
		return {};
	}

	const auto& GetNodesOutputs() const { return NodesOutputs; }

private:
	TMap<FVoxelComputeNode*, TArray<FVoxelNodeType>> NodesOutputs;
};

class FVoxelGraphStatsRangeAnalysisRangeRecorder
{
public:
	struct FScope
	{
		FVoxelComputeNode& Node;
		FVoxelGraphStatsRangeAnalysisRangeRecorder& Recorder;

		FScope(FVoxelComputeNode& Node, FVoxelGraphStatsRangeAnalysisRangeRecorder& Recorder)
			: Node(Node)
			, Recorder(Recorder)
		{
		}
		~FScope()
		{
			auto& RangeFailStatus = FVoxelRangeFailStatus::Get();
			if (RangeFailStatus.HasFailed() || RangeFailStatus.HasWarning())
			{
				auto* Message = RangeFailStatus.GetMessage();
				if (!ensure(Message)) return;

				FString PrettyMessage;
				if (RangeFailStatus.HasFailed())
				{
					PrettyMessage = FString::Printf(TEXT("range analysis failed:\n%s"), Message);
				}
				else
				{
					check(RangeFailStatus.HasWarning());
					PrettyMessage = FString::Printf(TEXT("range analysis warning:\n%s"), Message);

					// Clear the warning
					FVoxelRangeFailStatus::Get().Reset();
				}
				
				Recorder.NodesRangeMessages.FindOrAdd(&Node).Add(PrettyMessage);
			}
		}
	};
	
	struct FDataScope : FScope
	{
		FVoxelNodeOutputRangeBuffer OutputBuffer;

		FDataScope(FVoxelDataComputeNode& Node, FVoxelGraphStatsRangeAnalysisRangeRecorder& Recorder, FVoxelNodeOutputRangeBuffer OutputBuffer)
			: FScope(Node, Recorder)
			, OutputBuffer(OutputBuffer)
		{
		}
		~FDataScope()
		{
			ensureVoxelSlowNoSideEffects(!Recorder.NodesOutputs.Contains(&Node));
			auto& Destination = Recorder.NodesOutputs.Add(&Node);

			Destination.Empty(Node.OutputCount);
			Destination.SetNum(Node.OutputCount);
			for (int32 OutputIndex = 0; OutputIndex < Node.OutputCount; OutputIndex++)
			{
				Destination[OutputIndex] = OutputBuffer[OutputIndex];
			}
		}
	};

	struct FEmptyScope
	{
	};
	
	FORCEINLINE FDataScope MakeDataScope(FVoxelDataComputeNode* Node, FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context)
	{
		return FDataScope(*Node, *this, Outputs);
	}
	FORCEINLINE FEmptyScope MakeSetterScope(FVoxelExecComputeNode* ExecNode, FVoxelNodeInputRangeBuffer Inputs, const FVoxelContextRange& Context)
	{
		return {};
	}
	
	FORCEINLINE FScope MakeIfScope(FVoxelExecComputeNode* ExecNode)
	{
		return FScope(*ExecNode, *this);
	}

	const auto& GetNodesOutputs() const { return NodesOutputs; }
	const auto& GetNodesRangeMessages() const { return NodesRangeMessages; }

private:
	TMap<FVoxelComputeNode*, TArray<FVoxelNodeRangeType>> NodesOutputs;
	TMap<FVoxelComputeNode*, TArray<FString>> NodesRangeMessages;

	 friend FScope;
};
