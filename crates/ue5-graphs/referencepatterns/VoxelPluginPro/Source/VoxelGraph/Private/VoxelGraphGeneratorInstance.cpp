// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Runtime/VoxelGraphGeneratorInstance.h"
#include "Runtime/Recorders/VoxelGraphEmptyRecorder.h"
#include "Runtime/VoxelGraphVMUtils.h"
#include "Runtime/VoxelGraph.h"
#include "Runtime/VoxelGraph.inl"

#include "VoxelGraphGenerator.h"
#include "VoxelGraphConstants.h"
#include "VoxelContext.h"
#include "VoxelMessages.h"

#include "Async/Async.h"

void FVoxelCompiledGraphs::Compact()
{
	ensure(FastAccess.Num() == 0);
	FastAccess.Reset();
	
	Graphs.Compact();
	for (auto& It : Graphs)
	{
		const uint32 Key = FVoxelGraphPermutation::Hash(It.Key);
		ensure(!FastAccess.Contains(Key));
		FastAccess.Add(Key, It.Value);
	}
	FastAccess.Compact();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FVoxelGraphGeneratorInstance::FTarget::Compute(EVoxelFunctionAxisDependencies Dependencies, const FVoxelContext& Context) const
{
	FVoxelGraphEmptyRecorder EmptyRecorder;
	Graph.Compute(Context, Buffers, Dependencies, EmptyRecorder);
}

void FVoxelGraphGeneratorInstance::FRangeTarget::ComputeXYZWithoutCache(const FVoxelContextRange& Context, FOutput& Outputs) const
{
	FVoxelGraphEmptyRangeRecorder EmptyRecorder;
	Graph.ComputeRange(Context, Buffers, EmptyRecorder);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T, typename ReturnType>
inline ReturnType StaticSwitch(uint32 Index)
{
	switch (Index)
	{
#if !INTELLISENSE_PARSER
	static_assert(MAX_VOXELGRAPH_OUTPUTS == 256, "Need to update switch");
#define  C0(I) case (I): return T::template Get<I, ReturnType>();
#define  C1(I) C0(2 * (I)) C0(2 * (I) + 1)
#define  C2(I) C1(2 * (I)) C1(2 * (I) + 1)
#define  C3(I) C2(2 * (I)) C2(2 * (I) + 1)
#define  C4(I) C3(2 * (I)) C3(2 * (I) + 1)
#define  C5(I) C4(2 * (I)) C4(2 * (I) + 1)
#define  C6(I) C5(2 * (I)) C5(2 * (I) + 1)
#define  C7(I) C6(2 * (I)) C6(2 * (I) + 1)
#define  C8(I) C7(2 * (I)) C7(2 * (I) + 1)
#define  C9(I) C8(2 * (I)) C8(2 * (I) + 1)
#define C10(I) C9(2 * (I)) C9(2 * (I) + 1)
	C8(0);
#undef C0
#undef C1
#undef C2
#undef C3
#undef C4
#undef C5
#undef C6
#undef C7
#undef C8
#undef C9
#undef C10
#endif
	default: check(false); return nullptr;
	}
}

template<typename Accessor, typename PtrType>
inline TMap<FName, PtrType> GetCustomOutputsPtrMap(const TMap<FName, uint32>& Map)
{
	TMap<FName, PtrType> Result;
	for (auto& It : Map)
	{
		Result.Emplace(It.Key, StaticSwitch<Accessor, PtrType>(It.Value));
	}
	return Result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FVoxelGraphGeneratorInstance::FVoxelGraphGeneratorInstance(
	const TVoxelSharedRef<FVoxelCompiledGraphs>& Graphs,
	UVoxelGraphGenerator& Generator,
	const TMap<FName, uint32>& FloatOutputs,
	const TMap<FName, uint32>& Int32Outputs,
	const TMap<FName, uint32>& ColorOutputs)
	: TVoxelGraphGeneratorInstanceHelper(
		FloatOutputs,
		Int32Outputs,
		ColorOutputs,
		{
		GetCustomOutputsPtrMap<NoTransformAccessor<v_flt>, TOutputFunctionPtr<v_flt>>(FloatOutputs),
		GetCustomOutputsPtrMap<NoTransformAccessor<int32>, TOutputFunctionPtr<int32>>(Int32Outputs),
		GetCustomOutputsPtrMap<NoTransformAccessor<FColor>, TOutputFunctionPtr<FColor>>(ColorOutputs),
		GetCustomOutputsPtrMap<NoTransformRangeAccessor<v_flt>, TRangeOutputFunctionPtr<v_flt>>(FloatOutputs),
		},
		{
		GetCustomOutputsPtrMap<WithTransformAccessor<v_flt>, TOutputFunctionPtr_Transform<v_flt>>(FloatOutputs),
		GetCustomOutputsPtrMap<WithTransformAccessor<int32>, TOutputFunctionPtr_Transform<int32>>(Int32Outputs),
		GetCustomOutputsPtrMap<WithTransformAccessor<FColor>, TOutputFunctionPtr_Transform<FColor>>(ColorOutputs),
		GetCustomOutputsPtrMap<WithTransformRangeAccessor<v_flt>, TRangeOutputFunctionPtr_Transform<v_flt>>(FloatOutputs),
		},
		Generator)
	, Generator(&Generator)
	, Graphs(Graphs)
{

}

FVoxelGraphGeneratorInstance::~FVoxelGraphGeneratorInstance()
{
}

void FVoxelGraphGeneratorInstance::InitGraph(const FVoxelGeneratorInit& InitStruct)
{
	TArray<FVoxelGraphSeed> SeedVariables;
	for (auto& It : Graphs->GetGraphsMap())
	{
		auto& Graph = It.Value;
		SeedVariables.SetNumUninitialized(Graph->VariablesBufferSize, UE_505_SWITCH(false, EAllowShrinking::No));
		FVoxelGraphVMInitBuffers Buffers(SeedVariables.GetData());
		Graph->Init(InitStruct, Buffers);
	}

	// Compute constants
	for (auto& It : Graphs->GetGraphsMap())
	{
		auto& Graph = It.Value;
		if (It.Key.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex))
		{
			auto& CurrentVariables = RangeVariables.FindOrAdd(Graph);
			CurrentVariables.SetNumUninitialized(Graph->VariablesBufferSize);
			FVoxelGraphVMComputeRangeBuffers Buffers(CurrentVariables.GetData());
			Graph->ComputeRangeConstants(Buffers);
		}
		else
		{
			auto& CurrentVariables = Variables.FindOrAdd(Graph);
			CurrentVariables.SetNumUninitialized(Graph->VariablesBufferSize);
			FVoxelGraphVMComputeBuffers Buffers(CurrentVariables.GetData());
			Graph->ComputeConstants(Buffers);
		}
	}
}

FVoxelNodeType* FVoxelGraphGeneratorInstance::GetVariablesBuffer(const TVoxelWeakPtr<const FVoxelGraph>& Graph) const
{
	auto& BuffersMap = FThreadVariables::Get().Map;

	auto* BufferPtr = BuffersMap.Find(Graph);
	if (BufferPtr)
	{
		// Fast path
		return BufferPtr->GetData();
	}

	// Cleanup
	BuffersMap.Remove(nullptr);
	// Add new one
	auto& Buffer = BuffersMap.Add(Graph);
	// Copy data
	Buffer = Variables.FindChecked(Graph);
	return Buffer.GetData();
}

FVoxelNodeRangeType* FVoxelGraphGeneratorInstance::GetRangeVariablesBuffer(const TVoxelWeakPtr<const FVoxelGraph>& Graph) const
{
	auto& BuffersMap = FThreadRangeVariables::Get().Map;

	auto* BufferPtr = BuffersMap.Find(Graph);
	if (BufferPtr)
	{
		// Fast path
		return BufferPtr->GetData();
	}

	// Cleanup
	BuffersMap.Remove(nullptr);
	// Add new one
	auto& Buffer = BuffersMap.Add(Graph);
	// Copy data
	Buffer = RangeVariables.FindChecked(Graph);
	return Buffer.GetData();
}
