// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelAxisDependencies.h"
#include "VoxelGraphGeneratorHelpers.h"
#include "Runtime/VoxelNodeType.h"
#include "Runtime/VoxelGraphVMUtils.h"
#include "Runtime/VoxelCompiledGraphs.h"

class UVoxelGraphGenerator;

class VOXELGRAPH_API FVoxelGraphGeneratorInstance : public TVoxelGraphGeneratorInstanceHelper<FVoxelGraphGeneratorInstance, UVoxelGraphGenerator>
{
public:
	class FTarget
	{
	public:
		const FVoxelGraph& Graph;
		mutable FVoxelGraphVMComputeBuffers Buffers;
		
		struct FBufferX {};
		struct FBufferXY {};
		struct FOutput
		{
			FVoxelGraphVMOutputBuffers* const GraphOutputs;

			void Init(const FVoxelGraphOutputsInit& Init)
			{
				GraphOutputs->MaterialBuilder.Clear();
				GraphOutputs->MaterialBuilder.SetMaterialConfig(Init.MaterialConfig);
			}
			
			template<typename T, uint32 Index>
			T Get() const
			{
				return GraphOutputs->Buffer[Index].Get<T>();
			}
			template<typename T, uint32 Index>
			void Set(T Value)
			{
				GraphOutputs->Buffer[Index].Get<T>() = Value;
			}
		};
		
		inline FBufferX GetBufferX() const { return {}; }
		inline FBufferXY GetBufferXY() const { return {}; }
		inline FOutput GetOutputs() const { return { &Buffers.GraphOutputs }; }

		inline void ComputeX(const FVoxelContext& Context, FBufferX& BufferX) const
		{
			Compute(EVoxelFunctionAxisDependencies::X, Context);
		}
		inline void ComputeXYWithCache(const FVoxelContext& Context, const FBufferX& BufferX, FBufferXY& BufferXY) const
		{
			Compute(EVoxelFunctionAxisDependencies::XYWithCache, Context);
		}
		inline void ComputeXYZWithCache(const FVoxelContext& Context, const FBufferX& BufferX, const FBufferXY& BufferXY, FOutput& Outputs) const
		{
			Compute(EVoxelFunctionAxisDependencies::XYZWithCache, Context);
		}
		inline void ComputeXYZWithoutCache(const FVoxelContext& Context, FOutput& Outputs) const
		{
			Compute(EVoxelFunctionAxisDependencies::XYZWithoutCache, Context);
		}

	private:
		VOXELGRAPH_API void Compute(EVoxelFunctionAxisDependencies Dependencies, const FVoxelContext& Context) const;
	};

	class FRangeTarget
	{
	public:
		const FVoxelGraph& Graph;
		mutable FVoxelGraphVMComputeRangeBuffers Buffers;
		
		struct FBufferX {};
		struct FBufferXY {};
		struct FOutput
		{
			FVoxelGraphVMRangeOutputBuffers* const GraphOutputs;

			void Init(const FVoxelGraphOutputsInit& Init)
			{
			}
			
			template<typename T, uint32 Index>
			TVoxelRange<T> Get() const
			{
				return GraphOutputs->Buffer[Index].Get<T>();
			}
			template<typename T, uint32 Index>
			void Set(TVoxelRange<T> Value)
			{
				GraphOutputs->Buffer[Index].Get<T>() = Value;
			}
		};

		inline FBufferX GetBufferX() const { return {}; }
		inline FBufferXY GetBufferXY() const { return {}; }
		inline FOutput GetOutputs() const { return { &Buffers.GraphOutputs }; }

		VOXELGRAPH_API void ComputeXYZWithoutCache(const FVoxelContextRange& Context, FOutput& Outputs) const;
	};

public:
	FVoxelGraphGeneratorInstance(
		const TVoxelSharedRef<FVoxelCompiledGraphs>& Graphs,
		UVoxelGraphGenerator& Generator,
		const TMap<FName, uint32>& FloatOutputs,
		const TMap<FName, uint32>& Int32Outputs,
		const TMap<FName, uint32>& ColorOutputs);
	~FVoxelGraphGeneratorInstance();

	//~ Begin TVoxelGraphGeneratorInstanceHelper Interface
	void InitGraph(const FVoxelGeneratorInit& InitStruct) override final;
	
	template<uint32... InPermutation>
	inline auto GetTarget() const
	{
		static_assert(FVoxelGraphPermutation::IsSorted<InPermutation...>(), "");
		static_assert(sizeof...(InPermutation) == 1 || 
			!FVoxelGraphPermutation::Contains<InPermutation...>(FVoxelGraphOutputsIndices::RangeAnalysisIndex), "");

		auto& Graph = Graphs->GetFast(FVoxelGraphPermutation::Hash<InPermutation...>());
		return FTarget{ *Graph, FVoxelGraphVMComputeBuffers(GetVariablesBuffer(Graph)) };
	}
	template<uint32... InPermutation>
	inline auto GetRangeTarget() const
	{
		static_assert(FVoxelGraphPermutation::IsSorted<InPermutation...>(), "");
		static_assert(FVoxelGraphPermutation::Contains<InPermutation...>(FVoxelGraphOutputsIndices::RangeAnalysisIndex), "");

		auto& Graph = Graphs->GetFast(FVoxelGraphPermutation::Hash<InPermutation...>());
		return FRangeTarget{ *Graph, FVoxelGraphVMComputeRangeBuffers(GetRangeVariablesBuffer(Graph)) };
	}
	//~ End TVoxelGraphGeneratorInstanceHelper Interface

	inline UVoxelGraphGenerator* GetOwner() const
	{
		return Generator.Get();
	}

public:
	template<uint32... InPermutation>
	TVoxelSharedPtr<const FVoxelGraph> GetGraph() const
	{
		static_assert(FVoxelGraphPermutation::IsSorted<InPermutation...>(), "");
		return Graphs->GetFast(FVoxelGraphPermutation::Hash<InPermutation...>());
	}
	
	FVoxelNodeType* GetVariablesBuffer(const TVoxelWeakPtr<const FVoxelGraph>& Graph) const;
	FVoxelNodeRangeType* GetRangeVariablesBuffer(const TVoxelWeakPtr<const FVoxelGraph>& Graph) const;

private:
	const TWeakObjectPtr<UVoxelGraphGenerator> Generator;
	const TVoxelSharedRef<FVoxelCompiledGraphs> Graphs;

	TMap<TVoxelWeakPtr<const FVoxelGraph>, TArray<FVoxelNodeType>> Variables;
	TMap<TVoxelWeakPtr<const FVoxelGraph>, TArray<FVoxelNodeRangeType>> RangeVariables;

	struct FThreadVariables : TThreadSingleton<FThreadVariables>
	{
		TMap<TVoxelWeakPtr<const FVoxelGraph>, TArray<FVoxelNodeType>> Map;
	};
	struct FThreadRangeVariables : TThreadSingleton<FThreadRangeVariables>
	{
		TMap<TVoxelWeakPtr<const FVoxelGraph>, TArray<FVoxelNodeRangeType>> Map;
	};
};

template<>
inline FVoxelMaterial FVoxelGraphGeneratorInstance::FTarget::FOutput::Get<FVoxelMaterial, FVoxelGraphOutputsIndices::MaterialIndex>() const
{
	return GraphOutputs->MaterialBuilder.Build();
}
