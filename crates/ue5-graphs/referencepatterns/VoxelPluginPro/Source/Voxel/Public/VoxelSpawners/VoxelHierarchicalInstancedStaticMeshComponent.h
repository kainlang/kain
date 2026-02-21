// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"
#include "VoxelEnums.h"
#include "VoxelIntBox.h"
#include "VoxelSpawners/VoxelSpawnerMatrix.h"
#include "VoxelSpawners/VoxelInstancedMeshSettings.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "VoxelHierarchicalInstancedStaticMeshComponent.generated.h"

struct FVoxelSpawnerTransform;
struct FVoxelSpawnerTransforms;
struct FVoxelHISMBuiltData;
struct FVoxelInstancedMeshAndActorSettings;
class IVoxelPool;
class FVoxelConstDataAccelerator;
class FVoxelInstancedMeshManager;

DECLARE_VOXEL_MEMORY_STAT(TEXT("Voxel HISM Memory"), STAT_VoxelHISMMemory, STATGROUP_VoxelMemory, VOXEL_API);

struct FVoxelInstancesSection
{
	int32 StartIndex = -1;
	int32 Num = -1;

	// Between 0 and Num - 1
	TSet<int32> RemovedIndices;
};

// Need to prefix names with Voxel to avoid collisions with normal HISM
UCLASS()
class VOXEL_API UVoxelHierarchicalInstancedStaticMeshComponent : public UHierarchicalInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	// How long to wait for new instances before triggering a new cull tree/render update
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	float Voxel_BuildDelay = 0.5f;

private:
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Voxel_DebugMaterial;
		
public:
	UVoxelHierarchicalInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer);
	~UVoxelHierarchicalInstancedStaticMeshComponent();
	
public:
	void Voxel_Init(
		TVoxelWeakPtr<IVoxelPool> Pool,
		TVoxelWeakPtr<FVoxelInstancedMeshManager> InstancedMeshManager,
		float VoxelSize,
		const FIntVector& VoxelPosition,
		const FVoxelInstancedMeshAndActorSettings& MeshAndActorSettings);

	void Voxel_SetRelativeLocation();

	FIntVector Voxel_GetVoxelPosition() const
	{
		return Voxel_VoxelPosition;
	}
	const FVoxelInstancedMeshAndActorSettings& Voxel_GetMeshAndActorSettings() const
	{
		return Voxel_MeshAndActorSettings;
	}

private:
	TVoxelWeakPtr<IVoxelPool> Voxel_Pool;
	TVoxelWeakPtr<FVoxelInstancedMeshManager> Voxel_InstancedMeshManager;
	float Voxel_VoxelSize = 0;
	FIntVector Voxel_VoxelPosition;
	FVoxelInstancedMeshAndActorSettings Voxel_MeshAndActorSettings;

	friend class FVoxelHISMBuildTask;

public:
	TVoxelWeakPtr<FVoxelInstancesSection> Voxel_AppendTransforms(const TArray<FVoxelSpawnerMatrix>& InTransforms, const FVoxelIntBox& Bounds);
	void Voxel_RemoveSection(const TVoxelWeakPtr<FVoxelInstancesSection>& Section);

	bool Voxel_IsEmpty() const;

	void Voxel_StartBuildTree();
	void Voxel_FinishBuilding(FVoxelHISMBuiltData& BuiltData);

	void Voxel_EnablePhysics(FVoxelIntBox Chunk);
	void Voxel_DisablePhysics(FVoxelIntBox Chunk);
	void Voxel_RefreshPhysics(const FVoxelIntBox& BoundsToUpdate);

	FVoxelSpawnerTransforms Voxel_RemoveInstancesInArea(
		const FVoxelIntBox& VoxelBounds, 
		const FVoxelConstDataAccelerator* Accelerator, 
		EVoxelSpawnerActorSpawnType SpawnType);
	bool Voxel_RemoveInstanceByIndex(int32 InstanceIndex, FVoxelSpawnerTransform& OutTransform);
	
	int32 Voxel_GetNumInstances() const
	{
		return Voxel_UnbuiltMatrices.Num();
	}

public:
	template<typename T1, typename T2>
	inline void Voxel_IterateInstancesInBounds(const TArray<FClusterNode>& ClusterTree, const T1& Bounds, T2 Lambda) const
	{
		VOXEL_FUNCTION_COUNTER();
		Voxel_IterateInstancesInBoundsImpl(ClusterTree, Bounds, Lambda, 0);
	}
	template<typename T1, typename T2>
	void Voxel_IterateInstancesInBoundsImpl(const TArray<FClusterNode>& ClusterTree, const T1& Bounds, T2 Lambda, const int32 Index) const
	{
		struct FHelper
		{
			inline static bool ContainsBounds(const FVoxelIntBox& A, const FBox& B) { return A.ContainsTemplate(B); }
			inline static bool ContainsBounds(const FBox& A, const FBox& B) { return A.IsInside(B); }
		};

		if (ClusterTree.Num() == 0)
		{
			return;
		}
		auto& Tree = ClusterTree[Index];
		const auto TreeBounds = FBox(Tree.BoundMin, Tree.BoundMax);
		if (Bounds.Intersect(TreeBounds))
		{
			if (FHelper::ContainsBounds(Bounds, TreeBounds) || Tree.FirstChild < 0)
			{
				for (int32 Instance = Tree.FirstInstance; Instance <= Tree.LastInstance; Instance++)
				{
					Lambda(Instance);
				}
			}
			else
			{
				for (int32 Child = Tree.FirstChild; Child <= Tree.LastChild; Child++)
				{
					Voxel_IterateInstancesInBoundsImpl(ClusterTree, Bounds, Lambda, Child);
				}
			}
		}
	}

public:
	//~ Begin UActorComponent Interface
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual bool ShouldCreatePhysicsState() const override;
	virtual void OnDestroyPhysicsState() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

private:
	// Stored relative to Voxel_VoxelPosition
	TArray<FVoxelSpawnerMatrix> Voxel_UnbuiltMatrices;
	TArray<FVoxelSpawnerMatrix> Voxel_BuiltMatrices;

	TArray<TVoxelSharedPtr<FVoxelInstancesSection>> Voxel_Sections;

	struct FVoxelMappings
	{
		TArray<int32> InstancesToBuiltInstances;
		TArray<int32> BuiltInstancesToInstances;

		// We need to store a stack of the deletions that happened since the last tree build
		struct FDeletion
		{
			uint64 TaskUniqueId = 0; // Id of the task started right after that deletion
			int32 StartIndex = -1;
			int32 Num = -1;
		};
		TArray<FDeletion> DeletionsStack;

		int32 GetBuiltIndex(int32 UnbuiltIndex) const;
		int32 GetUnbuiltIndex(int32 BuiltIndex) const;
	};
	FVoxelMappings Voxel_Mappings;

	// Unbuilt instances indices to clear when task finishes
	TArray<int32> Voxel_UnbuiltInstancesToClear;
	
	uint64 Voxel_TaskUniqueId = 0;
	TVoxelSharedPtr<FThreadSafeCounter> Voxel_TaskCancelCounterPtr;

	FTimerHandle Voxel_TimerHandle;

	// Instance bounds to rebuild physics on when the respective tasks will be done
	TMap<uint64, TArray<FVoxelIntBox>> Voxel_TaskIdToNewInstancesBounds;
	// Instances bounds to rebuild physics on when the next task will be done
	TArray<FVoxelIntBox> Voxel_PendingNewInstancesBounds;
	
	TMap<FVoxelIntBox, TArray<FBodyInstance*>> Voxel_InstanceBodies;

	uint32 Voxel_AllocatedMemory = 0;

	void Voxel_UpdateAllocatedMemory();

	void Voxel_ScheduleBuildTree();
	void Voxel_RemoveInstancesFromSections(const TArray<int32>& BuiltIndices);
	void Voxel_SetInstancesScaleToZero(const TArray<int32>& BuiltIndices);
	
	// These 2 do not modify Voxel_InstanceBodies. Used by RefreshPhysics.
	void Voxel_EnablePhysicsImpl(const FVoxelIntBox& Chunk, TArray<FBodyInstance*>& OutBodies) const;
	void Voxel_DisablePhysicsImpl(TArray<FBodyInstance*>& Bodies) const;

	FORCEINLINE FVoxelIntBox Voxel_VoxelBoundsToLocal(const FVoxelIntBox& VoxelBounds) const
	{
		return VoxelBounds.Translate(-Voxel_VoxelPosition).Scale(Voxel_VoxelSize);
	}
	FORCEINLINE FVoxelVector Voxel_GetGlobalVoxelPosition(const FVoxelSpawnerMatrix& Matrix) const
	{
		const FVector LocalVoxelPosition = (FTransform(Matrix.GetCleanMatrix()).GetTranslation() + Matrix.GetPositionOffset()) / Voxel_VoxelSize;
		return FVoxelVector(Voxel_VoxelPosition) + FVoxelVector(LocalVoxelPosition);
	}

public:
#if VOXEL_ENGINE_VERSION >= 504
	static void BuildTreeAnyThread(TArray<FMatrix>& InstanceTransforms, TArray<float>& InstanceCustomDataFloats, int32 NumCustomDataFloats, const FBox& MeshBox, TArray<FClusterNode>& OutClusterTree, TArray<int32>& OutSortedInstances, TArray<int32>& OutInstanceReorderTable, int32& OutOcclusionLayerNum, int32 MaxInstancesPerLeaf, bool InGenerateInstanceScalingRange);
	void AcceptPrebuiltTree(TArray<FClusterNode>& InClusterTree, int32 InOcclusionLayerNumNodes, int32 InNumBuiltRenderInstances, FStaticMeshInstanceData* InSharedInstanceBufferData);
	virtual void BuildComponentInstanceData(ERHIFeatureLevel::Type FeatureLevel, FInstanceUpdateComponentDesc& OutData) override final;
	virtual int32 GetNumRenderInstances() const override { return NumBuiltRenderInstances; }
#endif
};

