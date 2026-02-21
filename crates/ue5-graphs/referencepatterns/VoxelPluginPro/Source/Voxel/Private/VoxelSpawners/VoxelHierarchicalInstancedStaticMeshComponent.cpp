// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/VoxelHierarchicalInstancedStaticMeshComponent.h"
#include "VoxelSpawners/VoxelInstancedMeshManager.h"
#include "VoxelSpawners/VoxelHISMBuildTask.h"
#include "VoxelDebug/VoxelDebugManager.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelMinimal.h"
#include "IVoxelPool.h"
#include "VoxelVector.h"

#include "Async/Async.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

#include "Engine/InstancedStaticMesh.h"

#if VOXEL_ENGINE_VERSION >= 504
#include "InstancedStaticMesh/ISMInstanceUpdateChangeSet.h"
#endif

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Voxel HISM Num Instances"), STAT_VoxelHISMComponent_NumInstances, STATGROUP_VoxelCounters);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Voxel HISM Num Physics Bodies"), STAT_VoxelHISMComponent_NumPhysicsBodies, STATGROUP_VoxelCounters);
DECLARE_DWORD_COUNTER_STAT(TEXT("Voxel HISM Num Floating Mesh Checked"), STAT_VoxelHISMComponent_NumFloatingMeshChecked, STATGROUP_VoxelCounters);
DEFINE_VOXEL_MEMORY_STAT(STAT_VoxelHISMMemory);

static TAutoConsoleVariable<int32> CVarShowHISMCollisions(
	TEXT("voxel.spawners.ShowCollisions"),
	0,
	TEXT("If true, will show a debug point on HISM instances with collisions"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarBVHDebugDepth(
	TEXT("voxel.spawners.BVHDebugLevel"),
	0,
	TEXT("If above or equal to 1, debug BVH nodes at that depth, 1 being the root"),
	ECVF_Default);

static const FMatrix EmptyMatrix = FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector).ToMatrixWithScale();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelHierarchicalInstancedStaticMeshComponent::UVoxelHierarchicalInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if !UE_BUILD_SHIPPING
	PrimaryComponentTick.bCanEverTick = true;
#endif

	Voxel_UpdateAllocatedMemory();
}

UVoxelHierarchicalInstancedStaticMeshComponent::~UVoxelHierarchicalInstancedStaticMeshComponent()
{
	DEC_VOXEL_MEMORY_STAT_BY(STAT_VoxelHISMMemory, Voxel_AllocatedMemory);
	DEC_DWORD_STAT_BY(STAT_VoxelHISMComponent_NumInstances, Voxel_UnbuiltMatrices.Num());
	ensure(Voxel_InstanceBodies.Num() == 0);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_Init(
	TVoxelWeakPtr<IVoxelPool> Pool, 
	TVoxelWeakPtr<FVoxelInstancedMeshManager> InstancedMeshManager, 
	float VoxelSize,
	const FIntVector& VoxelPosition,
	const FVoxelInstancedMeshAndActorSettings& MeshAndActorSettings)
{
#if VOXEL_ENGINE_VERSION >= 504
	// Tell the manager to not perform any tracking, and, indeed assert that none of the delta functions are ever called.
	PrimitiveInstanceDataManager.SetMode(FPrimitiveInstanceDataManager::EMode::ExternalLegacyData);
#endif

	Voxel_Pool = Pool;
	Voxel_InstancedMeshManager = InstancedMeshManager;
	Voxel_VoxelSize = VoxelSize;
	Voxel_VoxelPosition = VoxelPosition;
	Voxel_MeshAndActorSettings = MeshAndActorSettings;
	
	auto& MeshSettings = MeshAndActorSettings.MeshSettings;
	
	Voxel_BuildDelay = MeshSettings.BuildDelay;
	bDisableCollision = true;

#define EQ(X) X = MeshSettings.X;
	EQ(bAffectDynamicIndirectLighting)
	EQ(bAffectDistanceFieldLighting)
	EQ(bCastShadowAsTwoSided)
	EQ(bReceivesDecals)
	EQ(bUseAsOccluder)
	EQ(BodyInstance)
	EQ(LightingChannels)
	EQ(bRenderCustomDepth)
	EQ(CustomDepthStencilValue)
#undef EQ
	InstanceStartCullDistance = MeshSettings.CullDistance.Min;
	InstanceEndCullDistance = MeshSettings.CullDistance.Max;
	CastShadow = MeshSettings.bCastShadow;
	bCastDynamicShadow = MeshSettings.bCastShadow;
	BodyInstance = MeshSettings.BodyInstance;
	SetCustomNavigableGeometry(MeshSettings.CustomNavigableGeometry);	

	SetStaticMesh(MeshAndActorSettings.Mesh.Get());
	
	for (auto& It : MeshAndActorSettings.GetSectionsMaterials())
	{
		SetMaterial(It.Key, It.Value);
	}

	Voxel_SetRelativeLocation();
}

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_SetRelativeLocation()
{
	VOXEL_FUNCTION_COUNTER();

	const auto InstancedMeshManager = Voxel_InstancedMeshManager.Pin();
	if (ensure(InstancedMeshManager.IsValid()))
	{
		const FVoxelInstancedMeshManagerSettings& Settings = InstancedMeshManager->Settings;
		SetRelativeLocation(FVector(Voxel_VoxelPosition + *Settings.WorldOffset) * Settings.VoxelSize, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelWeakPtr<FVoxelInstancesSection> UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_AppendTransforms(const TArray<FVoxelSpawnerMatrix>& InTransforms, const FVoxelIntBox& InBounds)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(GetStaticMesh())) return nullptr;
	if (!ensure(InTransforms.Num() > 0)) return nullptr;
	
	INC_DWORD_STAT_BY(STAT_VoxelHISMComponent_NumInstances, InTransforms.Num());

	const int32 FirstInstanceIndex = Voxel_UnbuiltMatrices.Num();

	const auto NewSection = MakeVoxelShared<FVoxelInstancesSection>();
	NewSection->StartIndex = FirstInstanceIndex;
	NewSection->Num = InTransforms.Num();
	Voxel_Sections.Emplace(NewSection);

	{
		VOXEL_SCOPE_COUNTER("Append");
		Voxel_UnbuiltMatrices.Append(InTransforms);
	}
	
	Voxel_PendingNewInstancesBounds.Add(InBounds);

	Voxel_ScheduleBuildTree();
	Voxel_UpdateAllocatedMemory();

	return NewSection;
}

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_RemoveSection(const TVoxelWeakPtr<FVoxelInstancesSection>& Section)
{
	VOXEL_FUNCTION_COUNTER();

	const auto PinnedSection = Section.Pin();
	if (!ensure(PinnedSection.IsValid())) return;

	const int32 StartIndex = PinnedSection->StartIndex;
	const int32 Num = PinnedSection->Num;
	
	if (!ensure(Voxel_UnbuiltMatrices.IsValidIndex(StartIndex)) ||
		!ensure(StartIndex + Num <= Voxel_UnbuiltMatrices.Num())) return;

	if (!ensure(Voxel_Sections.Remove(PinnedSection) == 1)) return;

	ensure(PinnedSection.IsUnique());
	
	const auto Fix = [&](int32& Index)
	{
		ensure(Voxel_UnbuiltMatrices.IsValidIndex(Index));
		if (Index >= StartIndex)
		{
			if (Index < StartIndex + Num)
			{
				Index = -1;
			}
			else
			{
				Index -= Num;
			}
		}
	};

	{
		VOXEL_SCOPE_COUNTER("Fix Existing Sections");
		for (auto& ExistingSection : Voxel_Sections)
		{
			Fix(ExistingSection->StartIndex);
		}
	}
	
	{
		VOXEL_SCOPE_COUNTER("Fix UnbuiltInstancesToClear");
		for (auto& Index : Voxel_UnbuiltInstancesToClear)
		{
			Fix(Index);
		}
		Voxel_UnbuiltInstancesToClear.RemoveSwap(-1);
	}

	{
		VOXEL_SCOPE_COUNTER("Fix bodies");
		for (auto& It : Voxel_InstanceBodies)
		{
			for (auto& Body : It.Value)
			{
				Fix(Body->InstanceBodyIndex);
				if (Body->InstanceBodyIndex == -1)
				{
					DEC_DWORD_STAT(STAT_VoxelHISMComponent_NumPhysicsBodies);
					Body->TermBody();
					delete Body;
					Body = nullptr;
				}
			}
			It.Value.RemoveSwap(nullptr);
		}
	}

	{
		VOXEL_SCOPE_COUNTER("RemoveAt");
		Voxel_UnbuiltMatrices.RemoveAt(StartIndex, Num);
	}
	DEC_DWORD_STAT_BY(STAT_VoxelHISMComponent_NumInstances, Num);

	if (Voxel_UnbuiltMatrices.Num() == 0)
	{
		ensure(Voxel_UnbuiltInstancesToClear.Num() == 0);
		// Having 0 instances creates a whole bunch of issues, so add one
		Voxel_UnbuiltMatrices.Add(FVoxelSpawnerMatrix(EmptyMatrix));
	}

	// Generate a dummy ID to order deletions with tasks
	const uint64 UniqueIdForOrder = FVoxelHISMBuildTask::GetUniqueId();
	Voxel_Mappings.DeletionsStack.Add({ UniqueIdForOrder, StartIndex, Num });

#if VOXEL_DEBUG && 0
	{
		VOXEL_SCOPE_COUNTER("Debug");
		for (int32 UnbuiltIndex = 0; UnbuiltIndex < Voxel_UnbuiltMatrices.Num(); UnbuiltIndex++)
		{
			const int32 BuiltIndex = Voxel_Mappings.GetBuiltIndex(UnbuiltIndex);
			if (BuiltIndex == -1) continue;
			ensure(Voxel_UnbuiltMatrices[UnbuiltIndex] == Voxel_BuiltMatrices[BuiltIndex]);
		}
		for (int32 BuildIndex = 0; BuildIndex < Voxel_BuiltMatrices.Num(); BuildIndex++)
		{
			const int32 UnbuiltIndex = Voxel_Mappings.GetUnbuiltIndex(BuildIndex);
			if (UnbuiltIndex == -1) continue;
			ensure(Voxel_UnbuiltMatrices[UnbuiltIndex] == Voxel_BuiltMatrices[BuildIndex]);
		}
	}
#endif

	Voxel_ScheduleBuildTree();
	Voxel_UpdateAllocatedMemory();
}

bool UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_IsEmpty() const
{
	return Voxel_UnbuiltMatrices.Num() == 0 || (Voxel_UnbuiltMatrices.Num() == 1 && Voxel_UnbuiltMatrices[0] == FVoxelSpawnerMatrix(EmptyMatrix));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_StartBuildTree()
{
	VOXEL_FUNCTION_COUNTER();

	ensure((Voxel_TaskUniqueId != 0) == Voxel_TaskCancelCounterPtr.IsValid());
	
	if (!ensure(GetStaticMesh())) return;
	if (!ensure(Voxel_UnbuiltMatrices.Num() > 0)) return;

	auto Pool = Voxel_Pool.Pin();
	if (!ensure(Pool.IsValid())) return;

	if (Voxel_TaskCancelCounterPtr.IsValid())
	{
		Voxel_TaskCancelCounterPtr->Increment();
		Voxel_TaskCancelCounterPtr.Reset();
	}
	
	auto* Task = new FVoxelHISMBuildTask(this, Voxel_UnbuiltMatrices);
	Voxel_TaskUniqueId = Task->UniqueId;
	Voxel_TaskCancelCounterPtr = Task->CancelCounter;
	Pool->QueueTask(EVoxelTaskType::HISMBuild, Task);

	ensure(!Voxel_TaskIdToNewInstancesBounds.Contains(Voxel_TaskUniqueId));
	// Not true when removing instances ensure(Voxel_PendingNewInstancesBounds.Num() > 0);
	Voxel_TaskIdToNewInstancesBounds.Add(Voxel_TaskUniqueId, MoveTemp(Voxel_PendingNewInstancesBounds));

	Voxel_UpdateAllocatedMemory();
}

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_FinishBuilding(FVoxelHISMBuiltData& BuiltData)
{
	VOXEL_FUNCTION_COUNTER();

	ensure((Voxel_TaskUniqueId != 0) == Voxel_TaskCancelCounterPtr.IsValid());

	if (Voxel_TaskUniqueId == 0)
	{
		// Task is too late, newer one already completed
		ensure(!Voxel_TaskIdToNewInstancesBounds.Contains(BuiltData.UniqueId));
		ensure(Voxel_UnbuiltInstancesToClear.Num() == 0);
		return;
	}

	ensure(Voxel_TaskUniqueId >= BuiltData.UniqueId); // Else the task time traveled
	const bool bIsLatestTask = BuiltData.UniqueId == Voxel_TaskUniqueId;

	for (int32 UnbuiltIndex : Voxel_UnbuiltInstancesToClear)
	{
		if (!BuiltData.InstancesToBuiltInstances.IsValidIndex(UnbuiltIndex))
		{
			ensure(!bIsLatestTask);
			continue;
		}
		const int32 BuiltIndex = BuiltData.InstancesToBuiltInstances[UnbuiltIndex];
		BuiltData.InstanceBuffer->SetInstance(BuiltIndex, FMatrix44f(EmptyMatrix), 0);
		BuiltData.BuiltInstancesMatrices[BuiltIndex] = FVoxelSpawnerMatrix(EmptyMatrix);
	}
	
	if (bIsLatestTask)
	{
		Voxel_TaskUniqueId = 0;
		Voxel_TaskCancelCounterPtr.Reset();
		Voxel_UnbuiltInstancesToClear.Reset();
	}
	else
	{
		// Do not reset the task id nor cancel it as this is not the right one yet
	}

	const int32 NumInstances = BuiltData.InstanceBuffer->GetNumInstances();
	check(NumInstances > 0);
	check(NumInstances == BuiltData.BuiltInstancesMatrices.Num());
	check(NumInstances == BuiltData.InstancesToBuiltInstances.Num());
	check(NumInstances == BuiltData.BuiltInstancesToInstances.Num());
	// Not true with timer delays ensure(!bIsLatestTask || NumInstances == Voxel_UnbuiltMatrices.Num());

	Voxel_BuiltMatrices = MoveTemp(BuiltData.BuiltInstancesMatrices);

	Voxel_Mappings.InstancesToBuiltInstances = MoveTemp(BuiltData.InstancesToBuiltInstances);
	Voxel_Mappings.BuiltInstancesToInstances = MoveTemp(BuiltData.BuiltInstancesToInstances);

	// Remove any old deletions
	// < safe as we are generating a dummy id when deleting
	Voxel_Mappings.DeletionsStack.RemoveAll([&](auto& Deletion) { return Deletion.TaskUniqueId < BuiltData.UniqueId; });

#if VOXEL_ENGINE_VERSION >= 504
	{
		VOXEL_SCOPE_COUNTER("ReleasePerInstanceRenderData");
		UInstancedStaticMeshComponent::ClearInstances();
	}
	{
		VOXEL_SCOPE_COUNTER("UpdateFromPreallocatedData");
		TArray<FInstancedStaticMeshInstanceData> InstanceData = *reinterpret_cast<TArray<FInstancedStaticMeshInstanceData>*>(&Voxel_BuiltMatrices);
		{
			VOXEL_SCOPE_COUNTER("AcceptPrebuiltTree");
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			AcceptPrebuiltTree(BuiltData.ClusterTree, BuiltData.OcclusionLayerNum, NumInstances, BuiltData.InstanceBuffer.Get());
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		RecreateRenderState_Concurrent();
	}
#else
	constexpr bool bRequireCPUAccess = true;
	if (!PerInstanceRenderData.IsValid() || PerInstanceRenderData->InstanceBuffer.RequireCPUAccess != bRequireCPUAccess || true)
	{
		UInstancedStaticMeshComponent::ClearInstances();

		if (ensure(PerInstanceRenderData.IsValid()))
		{
			VOXEL_SCOPE_COUNTER("ReleasePerInstanceRenderData");
			ReleasePerInstanceRenderData();
		}

		VOXEL_SCOPE_COUNTER("InitPerInstanceRenderData");
		InitPerInstanceRenderData(true, BuiltData.InstanceBuffer.Get(), bRequireCPUAccess);
	}
	else
	{
		VOXEL_SCOPE_COUNTER("UpdateFromPreallocatedData");
		PerInstanceRenderData->UpdateFromPreallocatedData(*BuiltData.InstanceBuffer);
	}

	TArray<FInstancedStaticMeshInstanceData> InstanceData = *reinterpret_cast<TArray<FInstancedStaticMeshInstanceData>*>(&Voxel_BuiltMatrices);
	{
		VOXEL_SCOPE_COUNTER("AcceptPrebuiltTree");
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		AcceptPrebuiltTree(InstanceData, BuiltData.ClusterTree, BuiltData.OcclusionLayerNum, NumInstances);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	
	// AcceptPrebuiltTree does not move data for non nanite proxies
	if (!ShouldCreateNaniteProxy())
	{
		ensure(PerInstanceSMData.Num() == 0);
		PerInstanceSMData = InstanceData;
	}
	else
	{
		ensure(PerInstanceSMData.Num() == NumInstances);
	}

#endif

	for (auto It = Voxel_TaskIdToNewInstancesBounds.CreateIterator(); It; ++It)
	{
		if (It.Key() <= BuiltData.UniqueId)
		{
			for (auto& Chunk : It.Value())
			{
				Voxel_RefreshPhysics(Chunk);
			}
			It.RemoveCurrent();
		}
	}

	Voxel_UpdateAllocatedMemory();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_EnablePhysics(const FVoxelIntBox Chunk)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(!Voxel_InstanceBodies.Contains(Chunk))) return;
	auto& Bodies = Voxel_InstanceBodies.Add(Chunk);

	Voxel_EnablePhysicsImpl(Chunk, Bodies);

	Voxel_UpdateAllocatedMemory();
}

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_DisablePhysics(const FVoxelIntBox Chunk)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(Voxel_InstanceBodies.Contains(Chunk))) return;
	auto Bodies = Voxel_InstanceBodies.FindAndRemoveChecked(Chunk);
	
	Voxel_DisablePhysicsImpl(Bodies);

	Voxel_UpdateAllocatedMemory();
}

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_RefreshPhysics(const FVoxelIntBox& BoundsToUpdate)
{
	VOXEL_FUNCTION_COUNTER();

	for (auto& It : Voxel_InstanceBodies)
	{
		const auto& Chunk = It.Key;
		auto& Bodies = It.Value;
		if (Chunk.Intersect(BoundsToUpdate))
		{
			Voxel_DisablePhysicsImpl(Bodies);
			ensure(Bodies.Num() == 0);
			Voxel_EnablePhysicsImpl(Chunk, Bodies);
		}
	}

	Voxel_UpdateAllocatedMemory();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSpawnerTransforms UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_RemoveInstancesInArea(
	const FVoxelIntBox& VoxelBounds,
	const FVoxelConstDataAccelerator* Accelerator,
	EVoxelSpawnerActorSpawnType SpawnType)
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelSpawnerTransforms OutTransforms;
	OutTransforms.TransformsOffset = Voxel_VoxelPosition;

	check(SpawnType == EVoxelSpawnerActorSpawnType::All || Accelerator);
	
	TArray<int32> BuiltIndicesToClear;
	FVoxelIntBoxWithValidity BoundsToUpdate;

	const auto Lambda = [&](const int32 BuiltIndex)
	{
		if (Voxel_Mappings.GetUnbuiltIndex(BuiltIndex) == -1)
		{
			// Deleted
			return;
		}
		
		INC_DWORD_STAT_BY(STAT_VoxelHISMComponent_NumFloatingMeshChecked, 1);

		const FVoxelSpawnerMatrix Matrix = Voxel_BuiltMatrices[BuiltIndex];
		const FTransform LocalInstanceTransform = FTransform(Matrix.GetCleanMatrix());
		
		if (LocalInstanceTransform.GetScale3D().IsNearlyZero())
		{
			return;
		}

		const FVoxelVector GlobalVoxelPosition = Voxel_GetGlobalVoxelPosition(Matrix);
		if (!VoxelBounds.Contains(FVoxelIntBox(GlobalVoxelPosition)))
		{
			return;
		}

		if (SpawnType == EVoxelSpawnerActorSpawnType::All || Accelerator->GetFloatValue(GlobalVoxelPosition.X, GlobalVoxelPosition.Y, GlobalVoxelPosition.Z, 0) > 0)
		{
			OutTransforms.Matrices.Add(Matrix);
			BuiltIndicesToClear.Add(BuiltIndex);
			BoundsToUpdate += FVoxelIntBox(GlobalVoxelPosition);
		}
	};
	if (!ensure(ClusterTreePtr.IsValid())) return {};
	Voxel_IterateInstancesInBounds(*ClusterTreePtr, Voxel_VoxelBoundsToLocal(VoxelBounds), Lambda);

	if (BuiltIndicesToClear.Num() > 0)
	{
		Voxel_SetInstancesScaleToZero(BuiltIndicesToClear);
		Voxel_RemoveInstancesFromSections(BuiltIndicesToClear);
		Voxel_RefreshPhysics(BoundsToUpdate.GetBox());
	}
	
	return OutTransforms;
}

bool UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_RemoveInstanceByIndex(int32 InstanceIndex, FVoxelSpawnerTransform& OutTransform)
{
	VOXEL_FUNCTION_COUNTER();
	
	const int32 BuiltIndex = Voxel_Mappings.GetBuiltIndex(InstanceIndex);
	if (BuiltIndex == -1)
	{
		return false;
	}

	const auto Matrix = Voxel_BuiltMatrices[BuiltIndex];

	OutTransform.TransformOffset = Voxel_VoxelPosition;
	OutTransform.Matrix = Matrix;

	Voxel_SetInstancesScaleToZero({ BuiltIndex });
	Voxel_RemoveInstancesFromSections({ BuiltIndex });

	const FVoxelIntBox MatrixBounds = FVoxelIntBox(Voxel_GetGlobalVoxelPosition(Matrix));
	
	for (auto& It : Voxel_InstanceBodies)
	{
		const auto& Chunk = It.Key;
		auto& Bodies = It.Value;
		if (Chunk.Intersect(MatrixBounds))
		{
			for (int32 BodyIndex = 0; BodyIndex < Bodies.Num(); BodyIndex++)
			{
				auto* Body = Bodies[BodyIndex];
				if (Body->InstanceBodyIndex == InstanceIndex)
				{
					DEC_DWORD_STAT(STAT_VoxelHISMComponent_NumPhysicsBodies);
					Body->TermBody();
					delete Body;
					Bodies.RemoveAtSwap(BodyIndex);
					return true;
				}
			}
		}
	}

#if VOXEL_DEBUG
	for (auto& It : Voxel_InstanceBodies)
	{
		for (auto* Body : It.Value)
		{
			ensure(Body->InstanceBodyIndex != InstanceIndex);
		}
	}
#endif
	
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelHierarchicalInstancedStaticMeshComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	VOXEL_FUNCTION_COUNTER();
	
	ensure((Voxel_TaskUniqueId != 0) == Voxel_TaskCancelCounterPtr.IsValid());
	
	Voxel_TaskUniqueId = 0;
	if (Voxel_TaskCancelCounterPtr.IsValid())
	{
		Voxel_TaskCancelCounterPtr->Increment();
		Voxel_TaskCancelCounterPtr.Reset();
	}
	
	DEC_DWORD_STAT_BY(STAT_VoxelHISMComponent_NumInstances, Voxel_UnbuiltMatrices.Num());

	// Free up memory
	Voxel_UnbuiltMatrices.Empty();
	Voxel_BuiltMatrices.Empty();
	Voxel_Mappings.InstancesToBuiltInstances.Empty();
	Voxel_Mappings.BuiltInstancesToInstances.Empty();
	Voxel_Mappings.DeletionsStack.Empty();
	Voxel_UnbuiltInstancesToClear.Empty();
	Voxel_TaskIdToNewInstancesBounds.Empty();
	Voxel_PendingNewInstancesBounds.Empty();
	
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

bool UVoxelHierarchicalInstancedStaticMeshComponent::ShouldCreatePhysicsState() const
{
	return IsRegistered() && !IsBeingDestroyed() && GetStaticMesh() && !GetStaticMesh()->IsCompiling() && (bAlwaysCreatePhysicsState || IsCollisionEnabled());
}

void UVoxelHierarchicalInstancedStaticMeshComponent::OnDestroyPhysicsState()
{
	VOXEL_FUNCTION_COUNTER();
	
	Super::OnDestroyPhysicsState();

	for (auto& It : Voxel_InstanceBodies)
	{
		Voxel_DisablePhysicsImpl(It.Value);
	}
	Voxel_InstanceBodies.Empty();
}

inline void DrawBVH(UWorld* World, const FColor& Color, const TArray<FClusterNode>& ClusterTree, int32 Depth, int32 Index = 0)
{
	if (ClusterTree.Num() == 0) return;
	
	const auto& Node = ClusterTree[Index];
	if (Depth <= 0 || Node.FirstChild == -1)
	{
		const FBox Box(Node.BoundMin, Node.BoundMax);
		DrawDebugBox(World, Box.GetCenter(), Box.GetExtent(), Color, false, World->GetDeltaSeconds() * 1.5f);
		return;
	}

	for (int32 ChildIndex = Node.FirstChild; ChildIndex <= Node.LastChild; ChildIndex++)
	{
		DrawBVH(World, Color, ClusterTree, Depth - 1, ChildIndex);
	}
}

void UVoxelHierarchicalInstancedStaticMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	VOXEL_FUNCTION_COUNTER();
	
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CVarShowHISMCollisions.GetValueOnGameThread())
	{
		if (!GetStaticMesh()) return;
		
		const FBox MeshBox = GetStaticMesh()->GetBounds().GetBox();
		for (auto& It : Voxel_InstanceBodies)
		{
			for (auto* Body : It.Value)
			{
				const FBox BodyBox = MeshBox.TransformBy(Body->GetUnrealWorldTransform().GetScaled(Body->Scale3D));
				DrawDebugBox(
					GetWorld(),
					BodyBox.GetCenter(),
					BodyBox.GetExtent(),
					FColor::Red,
					false,
					DeltaTime * 2);
			}
		}
	}

	if (CVarBVHDebugDepth.GetValueOnGameThread() && ClusterTreePtr.IsValid())
	{
		const int32 Depth = CVarBVHDebugDepth.GetValueOnGameThread() - 1;
		DrawBVH(GetWorld(), FColor(FVoxelUtilities::MurmurHash64(uint64(this))), *ClusterTreePtr, Depth);
	}

	if (CVarRandomColorPerHISM.GetValueOnGameThread() && !Voxel_DebugMaterial)
	{
		Voxel_DebugMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Voxel/MaterialHelpers/Debug_UsePerInstanceRandomAsColor"));
		for (int32 Index = 0; Index < GetNumMaterials(); Index++)
		{
			SetMaterial(Index, Voxel_DebugMaterial);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int32 UVoxelHierarchicalInstancedStaticMeshComponent::FVoxelMappings::GetBuiltIndex(int32 UnbuiltIndex) const
{
	// Reverse back the changes
	for (int32 StackIndex = DeletionsStack.Num() - 1; StackIndex >= 0; StackIndex--)
	{
		auto& Deletion = DeletionsStack[StackIndex];
		if (Deletion.StartIndex <= UnbuiltIndex) // If we were affected by the deletion
		{
			UnbuiltIndex += Deletion.Num; // Translate back
		}
	}
	if (InstancesToBuiltInstances.IsValidIndex(UnbuiltIndex))
	{
		return InstancesToBuiltInstances[UnbuiltIndex];
	}
	else
	{
		return -1;
	}
}

int32 UVoxelHierarchicalInstancedStaticMeshComponent::FVoxelMappings::GetUnbuiltIndex(const int32 BuiltIndex) const
{
	if (!BuiltInstancesToInstances.IsValidIndex(BuiltIndex))
	{
		return -1;
	}
	
	int32 UnbuiltIndex = BuiltInstancesToInstances[BuiltIndex];
	
	// The UnbuiltIndex is the one of the last tree build: need to apply deletions to it
	for (int32 StackIndex = 0; StackIndex < DeletionsStack.Num(); StackIndex++)
	{
		auto& Deletion = DeletionsStack[StackIndex];
		if (Deletion.StartIndex <= UnbuiltIndex) 
		{
			if (UnbuiltIndex < Deletion.StartIndex + Deletion.Num)
			{
				// Got deleted
				return -1;
			}

			// Apply the deletion
			UnbuiltIndex -= Deletion.Num;
		}
	}
	
	// ensureVoxelSlowNoSideEffects(GetBuiltIndex(UnbuiltIndex) == BuiltIndex);
	return UnbuiltIndex;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_UpdateAllocatedMemory()
{
	VOXEL_FUNCTION_COUNTER();
	
	DEC_VOXEL_MEMORY_STAT_BY(STAT_VoxelHISMMemory, Voxel_AllocatedMemory);
	
	Voxel_AllocatedMemory = 0;
	Voxel_AllocatedMemory += Voxel_UnbuiltMatrices.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_BuiltMatrices.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_Mappings.InstancesToBuiltInstances.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_Mappings.BuiltInstancesToInstances.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_Mappings.DeletionsStack.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_UnbuiltInstancesToClear.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_TaskIdToNewInstancesBounds.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_PendingNewInstancesBounds.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_InstanceBodies.GetAllocatedSize();

	INC_VOXEL_MEMORY_STAT_BY(STAT_VoxelHISMMemory, Voxel_AllocatedMemory);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_ScheduleBuildTree()
{
	VOXEL_FUNCTION_COUNTER();
	
	if (Voxel_BuildDelay <= 0)
	{
		Voxel_StartBuildTree();
	}
	else
	{
		auto& TimerManager = GetWorld()->GetTimerManager();
		TimerManager.SetTimer(
			Voxel_TimerHandle,
			this,
			&UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_StartBuildTree,
			Voxel_BuildDelay,
			false);
	}
}

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_RemoveInstancesFromSections(const TArray<int32>& BuiltIndices)
{
	VOXEL_FUNCTION_COUNTER();

#if VOXEL_DEBUG && 0
	for (int32 Index = 0; Index < Voxel_Sections.Num() - 1; Index++)
	{
		ensure(Voxel_Sections[Index]->StartIndex < Voxel_Sections[Index + 1]->StartIndex);
		ensure(Voxel_Sections[Index]->StartIndex + Voxel_Sections[Index]->Num == Voxel_Sections[Index + 1]->StartIndex);
	}
	ensure(Voxel_UnbuiltMatrices.Num() == 0 || Voxel_Sections.Last()->StartIndex + Voxel_Sections.Last()->Num == Voxel_UnbuiltMatrices.Num());
#endif
	
	for (int32 BuiltIndex : BuiltIndices)
	{
		const int32 UnbuiltIndex = Voxel_Mappings.GetUnbuiltIndex(BuiltIndex);
		if (!ensure(UnbuiltIndex != -1)) continue;

		const int32 SectionIndex = Algo::UpperBoundBy(Voxel_Sections, UnbuiltIndex, [](auto& Section) { return Section->StartIndex; });
		
		auto& Section = *Voxel_Sections[SectionIndex - 1];
		
		if (!ensure(Section.StartIndex <= UnbuiltIndex && UnbuiltIndex < Section.StartIndex + Section.Num)) continue;

		Section.RemovedIndices.Add(UnbuiltIndex - Section.StartIndex);
	}
}

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_SetInstancesScaleToZero(const TArray<int32>& BuiltIndices)
{
	VOXEL_FUNCTION_COUNTER();

#if VOXEL_ENGINE_VERSION >= 504
	class FProxyAccessor : public FISMCInstanceDataSceneProxyLegacyReordered
	{
	public:
		using FISMCInstanceDataSceneProxyLegacyReordered::FISMCInstanceDataSceneProxyLegacyReordered;
		using FISMCInstanceDataSceneProxyLegacyReordered::ExternalLegacyData;
	};
	const TSharedPtr<FProxyAccessor> Proxy = StaticCastSharedPtr<FProxyAccessor>(GetOrCreateInstanceDataSceneProxy());
	const bool bIsProxyValid =
		ensure(Proxy) &&
		ensure(Proxy->ExternalLegacyData);

#else
	const auto InstanceBuffer = PerInstanceRenderData.IsValid() ? PerInstanceRenderData->InstanceBuffer_GameThread : nullptr;
	ensure(InstanceBuffer.IsValid() || Voxel_TaskUniqueId > 0);
#endif

	for (int32 BuiltIndex : BuiltIndices)
	{
		const int32 UnbuiltIndex = Voxel_Mappings.GetUnbuiltIndex(BuiltIndex);
		check(UnbuiltIndex >= 0);

#if VOXEL_ENGINE_VERSION >= 504
		if (bIsProxyValid)
		{
			Proxy->ExternalLegacyData->NullifyInstance(BuiltIndex);
		}
#else
		if (InstanceBuffer.IsValid())
		{
			InstanceBuffer->SetInstance(BuiltIndex, FMatrix44f(EmptyMatrix), 0);
		}
		if (PerInstanceSMData.IsValidIndex(BuiltIndex))
		{
			PerInstanceSMData[BuiltIndex] = EmptyMatrix;
		}
#endif

		ensure(Voxel_BuiltMatrices[BuiltIndex] == Voxel_UnbuiltMatrices[UnbuiltIndex]);
		Voxel_BuiltMatrices[BuiltIndex] = FVoxelSpawnerMatrix(EmptyMatrix);
		Voxel_UnbuiltMatrices[UnbuiltIndex] = FVoxelSpawnerMatrix(EmptyMatrix);

		if (Voxel_TaskUniqueId > 0)
		{
			Voxel_UnbuiltInstancesToClear.Add(UnbuiltIndex);
		}
	}

#if VOXEL_ENGINE_VERSION >= 504
	if (bIsProxyValid)
	{
		TUniquePtr<FStaticMeshInstanceData> BuiltInstanceData = MakeUnique<FStaticMeshInstanceData>();
		Swap(*BuiltInstanceData, *Proxy->ExternalLegacyData);
		PrimitiveInstanceDataManager.MarkForRebuildFromLegacy(MoveTemp(BuiltInstanceData), InstanceReorderTable, TArray<TRefCountPtr<HHitProxy>>());
	}

	MarkRenderStateDirty();
#else
	if (InstanceBuffer.IsValid())
	{
		ENQUEUE_RENDER_COMMAND(UVoxelHierarchicalInstancedStaticMeshComponent_UpdateBuffer)(
			[PerInstanceRenderData = PerInstanceRenderData](FRHICommandListImmediate& RHICmdList)
			{
				PerInstanceRenderData->InstanceBuffer.InstanceData = PerInstanceRenderData->InstanceBuffer_GameThread;
				PerInstanceRenderData->InstanceBuffer.UpdateRHI(UE_503_ONLY(RHICmdList));
			}
		);
		MarkRenderStateDirty();
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_EnablePhysicsImpl(const FVoxelIntBox& Chunk, TArray<FBodyInstance*>& OutBodies) const
{
	VOXEL_FUNCTION_COUNTER();
	ensure(bPhysicsStateCreated);
	
	// We want this function to be const
	auto* const Component = const_cast<UPrimitiveComponent*>(static_cast<const UPrimitiveComponent*>(this));
	
	UBodySetup* const BodySetup = Component->GetBodySetup();
	if (!ensure(BodySetup)) return;

	const FVoxelIntBox LocalBounds = Voxel_VoxelBoundsToLocal(Chunk);
	
	TArray<FTransform> Transforms;
	const auto Lambda = [&](const int32 BuiltIndex)
	{
		const FTransform LocalInstanceTransform = FTransform(Voxel_BuiltMatrices[BuiltIndex].GetCleanMatrix());
		const FTransform GlobalInstanceTransform = LocalInstanceTransform * GetComponentTransform();

		if (GlobalInstanceTransform.GetScale3D().IsNearlyZero() ||
			!LocalBounds.ContainsFloat(LocalInstanceTransform.GetTranslation()))
		{
			return;
		}

		FBodyInstance* Instance = new FBodyInstance();
		INC_DWORD_STAT(STAT_VoxelHISMComponent_NumPhysicsBodies);

		Instance->CopyBodyInstancePropertiesFrom(&BodyInstance);
		Instance->bAutoWeld = false;

		// Make sure we never enable bSimulatePhysics for ISMComps
		Instance->bSimulatePhysics = false;

		// Set the body index to the UNBUILT index
		const int32 UnbuiltIndex = Voxel_Mappings.GetUnbuiltIndex(BuiltIndex);
		if (UnbuiltIndex == -1)
		{
			// Deleted
			return;
		}
		Instance->InstanceBodyIndex = UnbuiltIndex;
		ensure(Voxel_UnbuiltMatrices[Instance->InstanceBodyIndex] == Voxel_BuiltMatrices[BuiltIndex]);

		OutBodies.Add(Instance);
		Transforms.Add(GlobalInstanceTransform);
	};
	
	if (!ensure(ClusterTreePtr.IsValid())) return;
	Voxel_IterateInstancesInBounds(*ClusterTreePtr, LocalBounds, Lambda);

	if (OutBodies.Num() == 0) return;

	VOXEL_SCOPE_COUNTER("InitStaticBodies");
	FBodyInstance::InitStaticBodies(OutBodies, Transforms, BodySetup, Component, GetWorld()->GetPhysicsScene());
}

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_DisablePhysicsImpl(TArray<FBodyInstance*>& Bodies) const
{
	VOXEL_FUNCTION_COUNTER();
	
	for (auto* Body : Bodies)
	{
		DEC_DWORD_STAT(STAT_VoxelHISMComponent_NumPhysicsBodies);
		Body->TermBody();
		delete Body;
	}
	Bodies.Reset();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if VOXEL_ENGINE_VERSION >= 504
void UVoxelHierarchicalInstancedStaticMeshComponent::BuildTreeAnyThread(
	TArray<FMatrix>& InstanceTransforms,
	TArray<float>& InstanceCustomDataFloats,
	int32 NumCustomDataFloats,
	const FBox& MeshBox,
	TArray<FClusterNode>& OutClusterTree,
	TArray<int32>& OutSortedInstances,
	TArray<int32>& OutInstanceReorderTable,
	int32& OutOcclusionLayerNum,
	int32 MaxInstancesPerLeaf,
	bool InGenerateInstanceScalingRange)
{
	check(MaxInstancesPerLeaf > 0);

	// do grass need this?
	float DensityScaling = 1.0f;
	int32 InstancingRandomSeed = 1;

	FClusterBuilder Builder(InstanceTransforms, InstanceCustomDataFloats, NumCustomDataFloats, MeshBox, MaxInstancesPerLeaf, DensityScaling, InstancingRandomSeed, InGenerateInstanceScalingRange);
	Builder.BuildTree();
	OutOcclusionLayerNum = Builder.Result->OutOcclusionLayerNum;

	OutClusterTree = MoveTemp(Builder.Result->Nodes);
	OutInstanceReorderTable = MoveTemp(Builder.Result->InstanceReorderTable);
	OutSortedInstances = MoveTemp(Builder.Result->SortedInstances);
}

void UVoxelHierarchicalInstancedStaticMeshComponent::AcceptPrebuiltTree(TArray<FClusterNode>& InClusterTree, int32 InOcclusionLayerNumNodes, int32 InNumBuiltRenderInstances, FStaticMeshInstanceData* InSharedInstanceBufferData)
{
	checkSlow(IsInGameThread());

	QUICK_SCOPE_CYCLE_COUNTER(STAT_UGrassInstancedStaticMeshComponent_AcceptPrebuiltTree);

	// this is only for prebuild data, already in the correct order
	check(!PerInstanceSMData.Num());

	NumBuiltInstances = 0;
	TranslatedInstanceSpaceOrigin = FVector::Zero();
	NumBuiltRenderInstances = InNumBuiltRenderInstances;
	check(NumBuiltRenderInstances);
	UnbuiltInstanceBounds.Init();
	UnbuiltInstanceBoundsList.Empty();
	ClusterTreePtr = MakeShareable(new TArray<FClusterNode>);
	InstanceReorderTable.Empty();
	SortedInstances.Empty();
	OcclusionLayerNumNodes = InOcclusionLayerNumNodes;
	BuiltInstanceBounds = GetClusterTreeBounds(InClusterTree, FVector::Zero());
	InstanceCountToRender = InNumBuiltRenderInstances;

	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid =
		// make sure we have instances
		NumBuiltRenderInstances > 0 &&
		// make sure we have an actual staticmesh
		GetStaticMesh() &&
		GetStaticMesh()->HasValidRenderData();

	if (bMeshIsValid)
	{
		*ClusterTreePtr = MoveTemp(InClusterTree);

		PostBuildStats();

	}
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UGrassInstancedStaticMeshComponent_AcceptPrebuiltTree_Mark);

	check(PerInstanceSMData.Num() == 0 || InSharedInstanceBufferData->GetNumInstances() == PerInstanceSMData.Num());

	TUniquePtr<FStaticMeshInstanceData> BuiltInstanceData = MakeUnique<FStaticMeshInstanceData>();
	// TODO: Implement move semantics for FStaticMeshInstanceData!
	Swap(*BuiltInstanceData, *InSharedInstanceBufferData);
	PrimitiveInstanceDataManager.MarkForRebuildFromLegacy(MoveTemp(BuiltInstanceData), InstanceReorderTable, TArray<TRefCountPtr<HHitProxy>>());

	MarkRenderStateDirty();
}

void UVoxelHierarchicalInstancedStaticMeshComponent::BuildComponentInstanceData(ERHIFeatureLevel::Type FeatureLevel, FInstanceUpdateComponentDesc& OutData)
{
	if (PrimitiveInstanceDataManager.GetMode() != FPrimitiveInstanceDataManager::EMode::ExternalLegacyData)
	{
		return Super::BuildComponentInstanceData(FeatureLevel, OutData);
	}

	OutData.PrimitiveLocalToWorld = GetRenderMatrix();
	OutData.PrimitiveMaterialDesc = GetUsedMaterialPropertyDesc(FeatureLevel);
	OutData.Flags = MakeInstanceDataFlags(OutData.PrimitiveMaterialDesc.bAnyMaterialHasPerInstanceRandom, OutData.PrimitiveMaterialDesc.bAnyMaterialHasPerInstanceCustomData);
	OutData.Flags.bHasPerInstanceDynamicData = false;
	OutData.StaticMeshBounds = GetStaticMesh()->GetBounds();
	OutData.NumProxyInstances = InstanceCountToRender;
	OutData.NumSourceInstances = 0;
	OutData.NumCustomDataFloats = NumCustomDataFloats;

#if WITH_EDITOR
	// TODO: Do we want these for this path?
	OutData.Flags.bHasPerInstanceEditorData = false;
#endif
	OutData.BuildChangeSet = [&](FISMInstanceUpdateChangeSet &ChangeSet)
	{
		// Cancel update as there is no source data
		check(ChangeSet.GetTransformDelta().IsEmpty());
		check(ChangeSet.GetCustomDataDelta().IsEmpty());

		BuildInstanceDataDeltaChangeSetCommon(ChangeSet);
		check(GetTranslatedInstanceSpaceOrigin().IsNearlyZero());
		check(PerInstanceSMData.IsEmpty());
		check(PerInstancePrevTransform.IsEmpty());
		// The reorder table is always empty in this path because the HISM is populated in AcceptPrebuiltTree using instances in the already sorted order.
		check(InstanceReorderTable.IsEmpty());
		check(ChangeSet.LegacyInstanceReorderTable.IsEmpty());
	};
}
#endif
