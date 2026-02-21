// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelContext.h"
#include "VoxelGraphGlobals.h"
#include "Runtime/VoxelComputeNode.h"

struct VOXELGRAPH_API FVoxelNodeHelpers
{
	template<typename T>
	static double ComputeStatsImplEx(const T* This, bool bSimulateCpp, int32 NumberOfLoops, FVoxelNodeInputBuffer Inputs, const FVoxelContext& Context)
	{
		FVoxelNodeType Outputs[MAX_VOXELNODE_PINS];

		const uint64 Start = FPlatformTime::Cycles64();
		if (bSimulateCpp)
		{
			for (int32 Index = 0; Index < NumberOfLoops; Index++)
			{
				This->T::Compute(Inputs, Outputs, Context);
			}
		}
		else
		{
			for (int32 Index = 0; Index < NumberOfLoops; Index++)
			{
				(This->*(static_cast<const FVoxelDataComputeNode*>(This)->ComputeFunctionPtr))(Inputs, Outputs, Context);
			}
		}
		const uint64 End = FPlatformTime::Cycles64();

		// Make sure it's not optimizing away the loop above
		{
			float Value = 0;
			for (int32 Index = 0; Index < MAX_VOXELNODE_PINS; Index++)
			{
				Value += Inputs[Index].Get<v_flt>();
				Value += Outputs[Index].Get<v_flt>();
			}
			if (Value == PI)
			{
				LOG_VOXEL(Log, TEXT("Win!"));
			}
		}

		return FPlatformTime::ToSeconds64(End - Start);
	}

	template<typename T>
	static double ComputeStatsImpl(const T* This, double Threshold, bool bSimulateCpp, FVoxelNodeInputBuffer Inputs, const FVoxelContext& Context)
	{
		double Duration;
		int32 NumberOfLoops = 1000;
		
		do
		{
			NumberOfLoops *= 10;
			Duration = ComputeStatsImplEx(This, bSimulateCpp, NumberOfLoops, Inputs, Context);
		} while (Duration < Threshold);

		return Duration / double(NumberOfLoops);
	}

public:
	template<typename T>
	static double ComputeRangeStatsImplEx(const T* This, bool bSimulateCpp, int32 NumberOfLoops, FVoxelNodeInputRangeBuffer Inputs, const FVoxelContextRange& Context)
	{
		FVoxelNodeRangeType Outputs[MAX_VOXELNODE_PINS];

		const uint64 Start = FPlatformTime::Cycles64();
		if (bSimulateCpp)
		{
			for (int32 Index = 0; Index < NumberOfLoops; Index++)
			{
				This->T::ComputeRange(Inputs, Outputs, Context);
			}
		}
		else
		{
			for (int32 Index = 0; Index < NumberOfLoops; Index++)
			{
				static_cast<const FVoxelDataComputeNode*>(This)->ComputeRange(Inputs, Outputs, Context);
			}
		}
		const uint64 End = FPlatformTime::Cycles64();

		// Make sure it's not optimizing away the loop above
		{
			float Value = 0;
			for (int32 Index = 0; Index < MAX_VOXELNODE_PINS; Index++)
			{
				Value += Inputs[Index].Get<v_flt>().Min;
				Value += Inputs[Index].Get<v_flt>().Max;
				Value += Outputs[Index].Get<v_flt>().Min;
				Value += Outputs[Index].Get<v_flt>().Max;
			}
			if (Value == PI)
			{
				LOG_VOXEL(Log, TEXT("Win!"));
			}
		}

		return FPlatformTime::ToSeconds64(End - Start);
	}

	template<typename T>
	static double ComputeRangeStatsImpl(const T* This, double Threshold, bool bSimulateCpp, FVoxelNodeInputRangeBuffer Inputs, const FVoxelContextRange& Context)
	{
		double Duration;
		int32 NumberOfLoops = 1000;
		
		do
		{
			NumberOfLoops *= 10;
			Duration = ComputeRangeStatsImplEx(This, bSimulateCpp, NumberOfLoops, Inputs, Context);
		} while (Duration < Threshold);

		return Duration / double(NumberOfLoops);
	}
	
public:
	static void ReplaceInputsOutputs(FString& S, const TArray<FString>& Inputs, const TArray<FString>& Outputs);
	static FString GetPrefixOpLoopString(const TArray<FString>& Inputs, const TArray<FString>& Outputs, int32 InputCount, const FString& Op);
	static FString GetInfixOpLoopString(const TArray<FString>& Inputs, const TArray<FString>& Outputs, int32 InputCount, const FString& Op);
};
