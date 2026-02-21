// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/VoxelSpawnerManager.h"
#include "VoxelSpawners/VoxelSpawner.h"
#include "VoxelSpawners/VoxelSpawnerEmbreeRayHandler.h"
#include "VoxelSpawners/VoxelSpawnerRayHandler.h"
#include "VoxelSpawners/VoxelMeshSpawner.h"
#include "VoxelSpawners/VoxelEmptySpawner.h"
#include "VoxelSpawners/HitGenerators/VoxelRayHitGenerator.h"
#include "VoxelSpawners/HitGenerators/VoxelHeightHitGenerator.h"

#include "VoxelEvents/VoxelEventManager.h"

#include "VoxelData/VoxelDataIncludes.h"

#include "VoxelRender/IVoxelRenderer.h"

#include "VoxelDebug/VoxelDebugManager.h"
#include "VoxelDebug/VoxelDebugUtilities.h"

#include "IVoxelPool.h"
#include "VoxelAsyncWork.h"
#include "VoxelCancelCounter.h"
#include "VoxelUtilities/VoxelIntVectorUtilities.h"
#include "VoxelMessages.h"
#include "VoxelPriorityHandler.h"
#include "VoxelUtilities/VoxelSerializationUtilities.h"
#include "VoxelUtilities/VoxelThreadingUtilities.h"
#include "VoxelWorld.h"
#include "VoxelUtilities/VoxelGeneratorUtilities.h"

#include "Async/Async.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/LargeMemoryWriter.h"

DEFINE_VOXEL_MEMORY_STAT(STAT_VoxelSpawnerManagerMemory);

static TAutoConsoleVariable<int32> CVarShowVoxelSpawnerRays(
	TEXT("voxel.spawners.ShowRays"),
	0,
	TEXT("If true, will show the voxel spawner rays"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarShowVoxelSpawnerHits(
	TEXT("voxel.spawners.ShowHits"),
	0,
	TEXT("If true, will show the voxel spawner rays hits"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarShowVoxelSpawnerPositions(
	TEXT("voxel.spawners.ShowPositions"),
	0,
	TEXT("If true, will show the positions sent to spawners"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarShowChunks(
	TEXT("voxel.spawners.ShowChunks"),
	0,
	TEXT("If true, show the spawners chunks"),
	ECVF_Default);



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelSpawnerTask : public FVoxelAsyncWork
{
public:
	const TVoxelWeakPtr<FVoxelSpawnerManager> SpawnerManagerPtr;
	const FVoxelIntBox Bounds;
	const int32 GroupIndex;
	const TArray<int32> SpawnersToSpawn;
	const FVoxelCancelCounter CancelCounter;
	const FVoxelPriorityHandler PriorityHandler;

	FVoxelSpawnerTask(
		FVoxelSpawnerManager& SpawnerManager,
		const FVoxelIntBox& Bounds,
		int32 GroupIndex, 
		const TArray<int32>& SpawnersToSpawn,
		const FVoxelCancelCounter& CancelCounter)
		: FVoxelAsyncWork(STATIC_FNAME("Spawner Task"), SpawnerManager.Settings.PriorityDuration, true)
		, SpawnerManagerPtr(SpawnerManager.AsShared())
		, Bounds(Bounds)
		, GroupIndex(GroupIndex)
		, SpawnersToSpawn(SpawnersToSpawn)
		, CancelCounter(CancelCounter)
		, PriorityHandler(Bounds, SpawnerManager.Settings.Renderer->GetInvokersPositionsForPriorities())
	{
		SpawnerManager.TaskCounter.Increment();
		SpawnerManager.UpdateTaskCount();
	}

	virtual void DoWork() override
	{
		auto SpawnerManager = SpawnerManagerPtr.Pin();
		if (!SpawnerManager.IsValid()) return;

		SpawnerManager->SpawnGroup_AnyThread(*this);

		SpawnerManager->TaskCounter.Decrement();
		SpawnerManager->UpdateTaskCount();

		FVoxelUtilities::DeleteOnGameThread_AnyThread(SpawnerManager);
	}
	virtual uint32 GetPriority() const override
	{
		return PriorityHandler.GetPriority();
	}

private:
	~FVoxelSpawnerTask() = default;

	template<typename T>
	friend struct TVoxelAsyncWorkDelete;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSpawnerSettings::FVoxelSpawnerSettings(
	const AVoxelWorld* World,
	EVoxelPlayType PlayType,
	const TVoxelSharedRef<IVoxelPool>& Pool,
	const TVoxelSharedRef<FVoxelDebugManager>& DebugManager,
	const TVoxelSharedRef<FVoxelData>& Data,
	const TVoxelSharedRef<IVoxelLODManager>& LODManager,
	const TVoxelSharedRef<IVoxelRenderer>& Renderer,
	const TVoxelSharedRef<FVoxelInstancedMeshManager>& MeshManager,
	const TVoxelSharedRef<FVoxelEventManager>& EventManager)
	: VoxelWorldInterface(World)
	, Pool(Pool)
	, DebugManager(DebugManager)
	, Data(Data)
	, MeshManager(MeshManager)
	, EventManager(EventManager)
	, LODManager(LODManager)
	, Renderer(Renderer)
	, Config(PlayType == EVoxelPlayType::Game || World->bEnableFoliageInEditor ? World->SpawnerConfig : nullptr)
	, VoxelSize(World->VoxelSize)
	, PriorityDuration(World->PriorityDuration)
{
}

inline bool operator==(const FVoxelSpawnerConfigGroupBase& Lhs, const FVoxelSpawnerConfigGroupBase& Rhs)
{
	return
		Lhs.LOD == Rhs.LOD &&
		Lhs.GenerationDistanceInChunks == Rhs.GenerationDistanceInChunks &&
		Lhs.bInfiniteGenerationDistance == Rhs.bInfiniteGenerationDistance &&
		Lhs.SpawnerType == Rhs.SpawnerType;
}

inline uint32 GetTypeHash(const FVoxelSpawnerConfigGroupBase& Key)
{
	return HashCombine(HashCombine(HashCombine(
		GetTypeHash(Key.LOD),
		GetTypeHash(Key.GenerationDistanceInChunks)),
		GetTypeHash(Key.bInfiniteGenerationDistance)),
		GetTypeHash(Key.SpawnerType));
}
inline bool operator<(const FVoxelSpawnerConfigGroupBase& Lhs, const FVoxelSpawnerConfigGroupBase& Rhs)
{
	// Just to be deterministic
	return
		Lhs.LOD < Rhs.LOD &&
		Lhs.GenerationDistanceInChunks < Rhs.GenerationDistanceInChunks &&
		int32(Lhs.bInfiniteGenerationDistance) < int32(Rhs.bInfiniteGenerationDistance) &&
		int32(Lhs.SpawnerType) < int32(Rhs.SpawnerType);
}

TVoxelSharedRef<FVoxelSpawnerManager> FVoxelSpawnerManager::Create(const FVoxelSpawnerSettings& Settings)
{
	VOXEL_FUNCTION_COUNTER();

	struct FInitStruct
	{
		FVoxelSpawnerThreadSafeConfig ThreadSafeConfig;
		TSet<UVoxelSpawner*> Spawners;
	};
	
	const auto GetInitStruct = [=]() -> FInitStruct
	{
		if (!Settings.Config.IsValid())
		{
			return {};
		}
		
		UVoxelSpawnerConfig& Config = *Settings.Config;

		FInitStruct InitStruct;
		InitStruct.ThreadSafeConfig.WorldType = Config.WorldType;
		InitStruct.ThreadSafeConfig.FiveWayBlendSetup = Config.FiveWayBlendSetup;

		TMap<FVoxelSpawnerConfigGroupBase, TArray<FVoxelSpawnerConfigSpawnerWithRuntimeData>> Groups;
		
		const auto& Generator = *Settings.Data->Generator;
		
		// Fill Groups and Spawners
		for (const FVoxelSpawnerConfigSpawner& Spawner : Config.Spawners)
		{
			if (!Spawner.Spawner)
			{
				FVoxelMessages::Error("Spawner is null!", &Config);
				return {};
			}

			{
				const auto CheckFloatOutputExists = [&](FName Name)
				{
					if (!Generator.GetOutputsPtrMap<v_flt>().Contains(Name))
					{
						FVoxelMessages::Warning(
							FVoxelUtilities::GetMissingGeneratorOutputErrorString<v_flt>(Name, Generator),
							&Config);
					}
				};

				if (Spawner.SpawnerType == EVoxelSpawnerType::Height)
				{
					CheckFloatOutputExists(Spawner.HeightGraphOutputName_HeightOnly);
				}

				if (Spawner.Density.Type == EVoxelSpawnerDensityType::GeneratorOutput)
				{
					CheckFloatOutputExists(Spawner.Density.GeneratorOutputName);
				}
			}

			InitStruct.Spawners.Add(Spawner.Spawner);

			FVoxelSpawnerConfigSpawnerWithRuntimeData SpawnerWithRuntimeData{ Spawner };

			SpawnerWithRuntimeData.DistanceBetweenInstancesInVoxel = Spawner.Spawner->GetDistanceBetweenInstancesInVoxel();
			SpawnerWithRuntimeData.DebugName = Spawner.Spawner->GetName() + ": " + Spawner.Spawner->GetDebugInfo();

			Groups.FindOrAdd(FVoxelSpawnerConfigGroupBase{ Spawner.LOD, Spawner.GenerationDistanceInChunks, Spawner.bInfiniteGenerationDistance , Spawner.SpawnerType }).Add(SpawnerWithRuntimeData);
		}

		// Make the group creation deterministic
		Groups.KeySort([](auto& A, auto& B) { return A < B; });
		
		// Build groups
		for (const auto& It : Groups)
		{
			FVoxelSpawnerConfigGroup Group(It.Key);
			Group.Spawners = It.Value;
			InitStruct.ThreadSafeConfig.Groups.Add(Group);
		}

		return InitStruct;
	};

	const FInitStruct InitStruct = GetInitStruct();

	TVoxelSharedRef<FVoxelSpawnerManager> Manager = MakeShareable(new FVoxelSpawnerManager(Settings, InitStruct.ThreadSafeConfig));

	// Spawners config
	{
		TSet<UVoxelSpawner*> Spawners;

		// Gather all the spawners and the ones they reference
		{
			TArray<UVoxelSpawner*> QueuedSpawners = InitStruct.Spawners.Array();

			while (QueuedSpawners.Num() > 0)
			{
				UVoxelSpawner* Spawner = QueuedSpawners.Pop();

				bool bAlreadyInSet = false;
				Spawners.Add(Spawner, &bAlreadyInSet);
				if (bAlreadyInSet)
				{
					continue;
				}
				
				TSet<UVoxelSpawner*> NewSpawners;
				if (!Spawner->GetSpawners(NewSpawners))
				{
					continue;
				}
				
				QueuedSpawners.Append(NewSpawners.Array());
			}
		}

		// Create the proxies
		for (auto* Spawner : Spawners)
		{
			check(Spawner);
			Manager->SpawnersMap.Add(Spawner, Spawner->GetSpawnerProxy(*Manager));
		}

		// Call post spawn
		for (auto& It : Manager->SpawnersMap)
		{
			It.Value->PostSpawn();
		}
	}

	// Create GroupsData first
	for (const auto& Group : Manager->ThreadSafeConfig.Groups)
	{
		const int32 ChunkSize = RENDER_CHUNK_SIZE << Group.LOD;
	
		TArray<FVoxelSpawnerProxy*> Proxies;
		for (auto& Spawner : Group.Spawners)
		{
			Proxies.Add(Manager->SpawnersMap[Spawner.Spawner].Get());
		}
		Manager->GroupsData.Emplace(ChunkSize, Proxies);
	}

	// Then bind delegates. This separation is needed as BindEvent might fire the delegate now.
	for (int32 GroupIndex = 0; GroupIndex < Manager->ThreadSafeConfig.Groups.Num(); GroupIndex++)
	{
		const auto& Group = Manager->ThreadSafeConfig.Groups[GroupIndex];
		
		const int32 ChunkSize = RENDER_CHUNK_SIZE << Group.LOD;

		if (Group.bInfiniteGenerationDistance)
		{
			TArray<FVoxelIntBox> Children;
			const int32 MaxChildren = 1 << 20;
			if (!Settings.Data->WorldBounds.Subdivide(ChunkSize, Children, MaxChildren))
			{
				// If we have more than a million chunks, something went terribly wrong
				FVoxelMessages::Error(FString::Printf(TEXT("bInfiniteGenerationDistance = true: trying to spawn more than %d chunks. Abording"), MaxChildren), Settings.Config.Get());
				continue;
			}
			for (auto& Child : Children)
			{
				Manager->SpawnGroup_GameThread(Child, GroupIndex);
			}
		}
		else
		{
			// Bind generation delegates
			Settings.EventManager->BindEvent(
				true,
				ChunkSize,
				Group.GenerationDistanceInChunks,
				FChunkDelegate::CreateThreadSafeSP(Manager, &FVoxelSpawnerManager::SpawnGroup_GameThread, GroupIndex),
				FChunkDelegate::CreateThreadSafeSP(Manager, &FVoxelSpawnerManager::DestroyGroup_GameThread, GroupIndex));
		}
	}

	return Manager;
}

void FVoxelSpawnerManager::Destroy()
{
	for (auto& Group : GroupsData)
	{
		FScopeLock Lock(&Group.Section);

		for (auto& It : Group.ChunksData)
		{
			It.Value.CancelTasks();
		}
	}

	StopTicking();
}

FVoxelSpawnerManager::~FVoxelSpawnerManager()
{
	ensure(IsInGameThread());
}

FVoxelSpawnerManager::FVoxelSpawnerManager(const FVoxelSpawnerSettings& Settings, const FVoxelSpawnerThreadSafeConfig& ThreadSafeConfig)
	: Settings(Settings)
	, ThreadSafeConfig(ThreadSafeConfig)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelSharedPtr<FVoxelSpawnerProxy> FVoxelSpawnerManager::GetSpawner(UVoxelSpawner* Spawner) const
{
	return SpawnersMap.FindRef(Spawner);
}

void FVoxelSpawnerManager::Regenerate(const FVoxelIntBox& Bounds)
{
	VOXEL_FUNCTION_COUNTER();

	IterateResultsInBounds(
		Bounds,
		[&](TVoxelSharedPtr<FVoxelSpawnerProxyResult>& Result)
		{
			if (Result.IsValid() && !Result->NeedsToBeSaved())
			{
				if (Result->IsCreated())
				{
					Result->Destroy();
				}
				else
				{
					FScopeLock Lock(&CreateQueueSection);
					ensure(CreateQueue.RemoveSwap(Result) == 1);
				}
				Result.Reset();
				return true;
			}
			else
			{
				return false;
			}
		});
}

void FVoxelSpawnerManager::MarkDirty(const FVoxelIntBox& Bounds)
{
	VOXEL_FUNCTION_COUNTER();

	IterateResultsInBounds(
		Bounds,
		[](TVoxelSharedPtr<FVoxelSpawnerProxyResult>& Result)
		{
			if (Result.IsValid())
			{
				Result->MarkDirty();
			}
			return false;
		});
}

void FVoxelSpawnerManager::Serialize(FArchive& Ar, FVoxelSpawnersSaveVersion::Type Version)
{
	VOXEL_FUNCTION_COUNTER();

	if (Ar.IsSaving())
	{
		check(Version == FVoxelSpawnersSaveVersion::LatestVersion);

		struct FGuidInfo
		{
			FVoxelSpawnerProxy* Proxy = nullptr;
			TArray<FIntVector> Positions;
			TArray<TVoxelSharedPtr<FVoxelSpawnerProxyResult>> ProxyResults;
		};
		TMap<FGuid, FGuidInfo> GuidInfos;

		// Expand groups to get SpawnerGuid -> Results
		// Storing GUIDs is more future-proof than the group/spawner index
		for (int32 GroupIndex = 0; GroupIndex < GroupsData.Num(); GroupIndex++)
		{
			const FSpawnerGroupData& GroupData = GroupsData[GroupIndex];
			const FVoxelSpawnerConfigGroup& Group = ThreadSafeConfig.Groups[GroupIndex];

			FScopeLock Lock(&GroupData.Section);
			for (auto& It : GroupData.ChunksData)
			{
				const FIntVector& Key = It.Key;
				const FSpawnerGroupChunkData& ChunkData = It.Value;

				check(ChunkData.SpawnerProxiesResults.Num() == 0 || ChunkData.SpawnerProxiesResults.Num() == Group.Spawners.Num());
				for (int32 SpawnerIndex = 0; SpawnerIndex < ChunkData.SpawnerProxiesResults.Num(); SpawnerIndex++)
				{
					const TVoxelSharedPtr<FVoxelSpawnerProxyResult>& ProxyResult = ChunkData.SpawnerProxiesResults[SpawnerIndex];
					if (ProxyResult.IsValid() && ProxyResult->NeedsToBeSaved())
					{
						FGuidInfo& GuidInfo = GuidInfos.FindOrAdd(Group.Spawners[SpawnerIndex].Guid);

						ensure(!GuidInfo.Proxy || GuidInfo.Proxy == GroupData.SpawnerProxies[SpawnerIndex]);
						GuidInfo.Proxy = GroupData.SpawnerProxies[SpawnerIndex];
						check(GuidInfo.Proxy);
						
						ensure(ProxyResult->Type == GuidInfo.Proxy->Type || ProxyResult->Type == EVoxelSpawnerProxyType::EmptySpawner);

						GuidInfo.Positions.Add(Key);
						GuidInfo.ProxyResults.Add(ProxyResult);
					}
				}
			}
		}

		int32 NumGuid = GuidInfos.Num();
		Ar << NumGuid;

		// Now serialize each GUID
		for (auto& It : GuidInfos)
		{
			FGuid Guid = It.Key;
			FGuidInfo& GuidInfo = It.Value;

			// Create one archive per GUID to be safer
			FBufferArchive GuidArchive;
			{
				int32 NumProxyResults = GuidInfo.Positions.Num();
				check(GuidInfo.ProxyResults.Num() == NumProxyResults);
				check(NumProxyResults > 0);
				
				GuidArchive << NumProxyResults;
				GuidArchive.Serialize(GuidInfo.Positions.GetData(), NumProxyResults * sizeof(FIntVector));

				for (auto& Result : GuidInfo.ProxyResults)
				{
					check(Result->Type == GuidInfo.Proxy->Type || Result->Type == EVoxelSpawnerProxyType::EmptySpawner);
					GuidArchive << Result->Type;
					Result->SerializeProxy(GuidArchive, Version);
				}
			}

			Ar << Guid;

			int32 GuidArchiveNum = GuidArchive.Num();
			Ar << GuidArchiveNum;

			Ar.Serialize(GuidArchive.GetData(), GuidArchiveNum * sizeof(uint8));
		}
	}
	else
	{
		check(Ar.IsLoading());

		int32 NumGuid = -1;
		Ar << NumGuid;
		check(NumGuid >= 0);

		// Deserialize each GUID
		for (int32 GuidIndex = 0; GuidIndex < NumGuid; GuidIndex++)
		{
			FGuid Guid;
			TArray<uint8> GuidArchiveData;
			{
				Ar << Guid;

				int32 GuidArchiveNum = -1;
				Ar << GuidArchiveNum;
				check(GuidArchiveNum >= 0);

				GuidArchiveData.SetNumUninitialized(GuidArchiveNum);
				Ar.Serialize(GuidArchiveData.GetData(), GuidArchiveNum * sizeof(uint8));
			}

			int32 GroupIndex = -1;
			int32 SpawnerIndex = -1;

			// Find the Group Index & Spawner Index from the GUID by iterating all of them
			for (int32 LocalGroupIndex = 0; LocalGroupIndex < ThreadSafeConfig.Groups.Num(); LocalGroupIndex++)
			{
				const FVoxelSpawnerConfigGroup& Group = ThreadSafeConfig.Groups[LocalGroupIndex];
				for (int32 LocalSpawnerIndex = 0; LocalSpawnerIndex < Group.Spawners.Num(); LocalSpawnerIndex++)
				{
					if (Group.Spawners[LocalSpawnerIndex].Guid == Guid)
					{
						ensureMsgf(GroupIndex == -1, TEXT("Spawners with identical GUIDs"));
						GroupIndex = LocalGroupIndex;
						SpawnerIndex = LocalSpawnerIndex;
					}
				}
			}
			
			if (GroupIndex == -1)
			{
				FVoxelMessages::Error(FString::Printf(TEXT("Voxel Spawners Serialization: Loading: Spawner with GUID %s not found, skipping it"), *Guid.ToString()), Settings.VoxelWorldInterface.Get());
				ensure(false);
				continue;
			}

			check(GroupIndex >= 0);
			check(SpawnerIndex >= 0);

			const FVoxelSpawnerConfigGroup& Group = ThreadSafeConfig.Groups[GroupIndex];
			const FVoxelSpawnerConfigSpawnerWithRuntimeData& Spawner = Group.Spawners[SpawnerIndex];

			FSpawnerGroupData& GroupData = GroupsData[GroupIndex];
			FVoxelSpawnerProxy& Proxy = *GroupData.SpawnerProxies[SpawnerIndex];
			
			FScopeLock Lock(&GroupData.Section);
			
			TArray<FIntVector> Positions;
			TArray<TVoxelSharedPtr<FVoxelSpawnerProxyResult>> ProxyResults;

			// Load GUID archive
			{
				FMemoryReader GuidArchive(GuidArchiveData);
				
				int32 NumProxyResults = -1;
				GuidArchive << NumProxyResults;
				check(NumProxyResults > 0);

				Positions.SetNumUninitialized(NumProxyResults);
				GuidArchive.Serialize(Positions.GetData(), NumProxyResults * sizeof(FIntVector));

				ProxyResults.Reserve(NumProxyResults);
				for (int32 ResultIndex = 0; ResultIndex < NumProxyResults; ResultIndex++)
				{
					EVoxelSpawnerProxyType ProxyType = EVoxelSpawnerProxyType::Invalid;
					GuidArchive << ProxyType;
					check(ProxyType != EVoxelSpawnerProxyType::Invalid);

					if (ProxyType != EVoxelSpawnerProxyType::EmptySpawner && ProxyType != Proxy.Type)
					{
						FVoxelMessages::Error(FString::Printf(
							TEXT("Voxel Spawner Serialization: Spawner changed type, skipping it (was %s, is %s now). Spawner: %s; GUID: %s"),
							ToString(ProxyType),
							ToString(Proxy.Type),
							*Spawner.DebugName,
							*Guid.ToString()), Settings.VoxelWorldInterface.Get());
						ensure(false);
						break;
					}

					// Create result
					const TVoxelSharedRef<FVoxelSpawnerProxyResult> Result = FVoxelSpawnerProxyResult::CreateFromType(ProxyType, Proxy);
					Result->MarkDirty();
					Result->SerializeProxy(GuidArchive, Version);
					ProxyResults.Add(Result);
				}

				if (ProxyResults.Num() != NumProxyResults)
				{
					// Can only be caused if we broke because of the error
					continue;
				}

				ensure(GuidArchive.AtEnd());
			}
			check(Positions.Num() == ProxyResults.Num());

			// Load the results into the group data
			for (int32 ResultIndex = 0; ResultIndex < ProxyResults.Num(); ResultIndex++)
			{
				TArray<TVoxelSharedPtr<FVoxelSpawnerProxyResult>>& ExistingProxiesResults = GroupData.ChunksData.FindOrAdd(Positions[ResultIndex]).SpawnerProxiesResults;
				ExistingProxiesResults.SetNum(GroupData.SpawnerProxies.Num());

				TVoxelSharedPtr<FVoxelSpawnerProxyResult>& ExistingProxyResult = ExistingProxiesResults[SpawnerIndex];
				
				bool bIsCreated = false;
				if (ExistingProxyResult.IsValid())
				{
					bIsCreated = ExistingProxyResult->IsCreated();
					if (bIsCreated)
					{
						// Destroy existing result
						ExistingProxyResult->Destroy();
					}
					ExistingProxyResult.Reset();
				}
				
				ExistingProxyResult = ProxyResults[ResultIndex];

				if (bIsCreated)
				{
					// If existing result was created, create this one too
					ExistingProxyResult->Create();
				}
			}

			GroupData.UpdateStats();
		}
	}
}

void FVoxelSpawnerManager::SaveTo(FVoxelSpawnersSaveImpl& Save)
{
	VOXEL_FUNCTION_COUNTER();

	Save.Guid = FGuid::NewGuid();

	FLargeMemoryWriter Archive;
	
	int32 VoxelCustomVersion = FVoxelSpawnersSaveVersion::LatestVersion;
	Archive << VoxelCustomVersion;
	
	Serialize(Archive, FVoxelSpawnersSaveVersion::LatestVersion);

	FVoxelSerializationUtilities::CompressData(Archive, Save.CompressedData);
}

void FVoxelSpawnerManager::LoadFrom(const FVoxelSpawnersSaveImpl& Save)
{
	VOXEL_FUNCTION_COUNTER();

	TArray64<uint8> UncompressedData;
	FVoxelSerializationUtilities::DecompressData(Save.CompressedData, UncompressedData);

	FLargeMemoryReader Archive(UncompressedData.GetData(), UncompressedData.Num());

	int32 Version = -1;
	Archive << Version;
	check(Version >= 0);
	
	Serialize(Archive, FVoxelSpawnersSaveVersion::Type(Version));

	ensure(Archive.AtEnd() && !Archive.IsError());
}

bool FVoxelSpawnerManager::GetMeshSpawnerTransforms(const FGuid& SpawnerGuid, TArray<FVoxelSpawnerTransforms>& OutTransforms) const
{
	int32 GroupIndex;
	int32 SpawnerIndex;
	if (!FindSpawnerByGuid(SpawnerGuid, GroupIndex, SpawnerIndex))
	{
		return false;
	}

	auto& GroupData = GroupsData[GroupIndex];

	FScopeLock Lock(&GroupData.Section);
	for (auto& It : GroupData.ChunksData)
	{
		auto& Results = It.Value.SpawnerProxiesResults;
		if (!ensure(Results.IsValidIndex(SpawnerIndex)))
		{
			continue;
		}

		auto& Result = Results[SpawnerIndex];
		if (Result->Type == EVoxelSpawnerProxyType::EmptySpawner)
		{
			continue;
		}
		if (!ensure(Result->Type == EVoxelSpawnerProxyType::MeshSpawner))
		{
			return false;
		}

		auto& MeshResult = static_cast<const FVoxelMeshSpawnerProxyResult&>(*Result);
		OutTransforms.Add(MeshResult.GetTransforms());
	}

	return true;
}

int32 FVoxelSpawnerManager::GetTaskCount() const
{
	return TaskCounter.GetValue();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnerManager::Tick(float DeltaTime)
{
	VOXEL_FUNCTION_COUNTER();
	
	// TODO: time limit?
	FlushCreateQueue_GameThread();

	if (CVarShowChunks.GetValueOnGameThread())
	{
		int32 ColorCounter = 0;
		uint64 MessageCounter = OBJECT_LINE_ID();

		for (int32 GroupIndex = 0; GroupIndex < ThreadSafeConfig.Groups.Num(); GroupIndex++)
		{
			const auto& Group = ThreadSafeConfig.Groups[GroupIndex];
			const auto& GroupData = GroupsData[GroupIndex];

			FColor Color = FLinearColor::MakeFromHSV8(255.f * ColorCounter / ThreadSafeConfig.Groups.Num(), 255, 255).ToFColor(true);
			Color.A = 255;

			ColorCounter++;

			GEngine->AddOnScreenDebugMessage(MessageCounter++, DeltaTime * 1.5f, Color,
				FString::Printf(TEXT("LOD: %d (chunk size %d); Generation Distance: %d chunks (%d voxels); Infinite: %s"), 
					Group.LOD, 
					RENDER_CHUNK_SIZE << Group.LOD, 
					Group.GenerationDistanceInChunks,
					Group.GenerationDistanceInChunks * (RENDER_CHUNK_SIZE << Group.LOD),
					*LexToString(Group.bInfiniteGenerationDistance)));

			for (const auto& Spawner : Group.Spawners)
			{
				GEngine->AddOnScreenDebugMessage(MessageCounter++, DeltaTime * 1.5f, Color, TEXT("    ") + Spawner.DebugName);
			}

			for (auto& It : GroupData.ChunksData)
			{
				const FVoxelIntBox Bounds(It.Key, It.Key + GroupData.ChunkSize);
				UVoxelDebugUtilities::DrawDebugIntBox(Settings.VoxelWorldInterface.Get(), Bounds, DeltaTime * 1.5f, 0, Color);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnerManager::SpawnGroup_GameThread(FVoxelIntBox Bounds, int32 GroupIndex)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	SpawnedSpawnersBounds += Bounds;

	TArray<int32> SpawnersToSpawnAsync;
	TArray<TVoxelSharedPtr<FVoxelSpawnerProxyResult>> SpawnerProxiesResultsToCreateNow;

	FSpawnerGroupData& GroupData = GroupsData[GroupIndex];
	
	GroupData.Section.Lock();
	//////////////////////////////////////////////////////////////////////////////
	FSpawnerGroupChunkData& ChunkData = GroupData.ChunksData.FindOrAdd(GroupData.GetChunkKey(Bounds));
	const FVoxelCancelCounter CancelCounter(ChunkData.UpdateIndex);
	ChunkData.SpawnerProxiesResults.SetNum(GroupData.SpawnerProxies.Num());
	for (int32 SpawnerIndex = 0; SpawnerIndex < GroupData.SpawnerProxies.Num(); SpawnerIndex++)
	{
		const auto& SpawnerProxyResult = ChunkData.SpawnerProxiesResults[SpawnerIndex];
		ensure(!SpawnerProxyResult.IsValid() || &SpawnerProxyResult->Proxy == GroupData.SpawnerProxies[SpawnerIndex]);
		if (!SpawnerProxyResult.IsValid())
		{
			SpawnersToSpawnAsync.Add(SpawnerIndex);
		}
		else
		{
			ensure(!SpawnerProxyResult->IsCreated() || !SpawnerProxyResult->CanBeDespawned());
			if (SpawnerProxyResult->CanBeDespawned())
			{
				// If we cannot be despawned we weren't destroyed, and there is nothing to do
				SpawnerProxiesResultsToCreateNow.Add(SpawnerProxyResult);
			}
		}
	}
	GroupData.UpdateStats();
	//////////////////////////////////////////////////////////////////////////////
	GroupData.Section.Unlock();

	if (SpawnersToSpawnAsync.Num() > 0)
	{
		Settings.Pool->QueueTask(
			EVoxelTaskType::FoliageBuild,
			new FVoxelSpawnerTask(*this, Bounds, GroupIndex, SpawnersToSpawnAsync, CancelCounter));
	}

	for (auto& Result : SpawnerProxiesResultsToCreateNow)
	{
		Result->Create();
	}
}

void FVoxelSpawnerManager::DestroyGroup_GameThread(FVoxelIntBox Bounds, int32 GroupIndex)
{
	VOXEL_FUNCTION_COUNTER();

	FSpawnerGroupData& GroupData = GroupsData[GroupIndex];

	FScopeLock Lock(&GroupData.Section);
	const FIntVector ChunkKey = GroupData.GetChunkKey(Bounds);
	auto* ChunkData = GroupData.ChunksData.Find(ChunkKey);
	if (!ensure(ChunkData)) return;
	
	// Cancel existing tasks
	ChunkData->CancelTasks();
	
	// Destroy results that can be
	bool bCanFreeMemory = true;
	for (auto& Result : ChunkData->SpawnerProxiesResults)
	{
		// Can be invalid if the task was started but not completed
		if (!Result.IsValid()) continue;

		if (!Result->CanBeDespawned())
		{
			// Can't do anything
			bCanFreeMemory = false;
			continue;
		}

		if (Result->IsCreated())
		{
			Result->Destroy();
		}
		else
		{
			FScopeLock CreateQueueLock(&CreateQueueSection);
			ensure(CreateQueue.RemoveSwap(Result) == 1);
		}

		if (!Result->NeedsToBeSaved())
		{
			Result.Reset();
		}
		else
		{
			bCanFreeMemory = false;
		}
	}

	// Free up memory
	if (bCanFreeMemory)
	{
		GroupData.ChunksData.Remove(ChunkKey);
	}

	GroupData.UpdateStats();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnerManager::SpawnGroup_AnyThread(const FVoxelSpawnerTask& Task)
{
	VOXEL_ASYNC_FUNCTION_COUNTER();

	if (Task.CancelCounter.IsCanceled()) return;

	const FVoxelSpawnerConfigGroup& Group = ThreadSafeConfig.Groups[Task.GroupIndex];
	check(Task.Bounds.Size() == FIntVector(RENDER_CHUNK_SIZE << Group.LOD));
	
	const bool bShowDebugRays = CVarShowVoxelSpawnerRays.GetValueOnAnyThread() != 0;
	const bool bShowDebugHits = CVarShowVoxelSpawnerHits.GetValueOnAnyThread() != 0;
	
	TUniquePtr<FVoxelSpawnerRayHandler> RayHandler;
	if (Group.SpawnerType == EVoxelSpawnerType::Ray)
	{
		// Needs to be done before the Lock as it will lock the data too
		TArray<uint32> Indices;
		TArray<FVector> Vertices;
		Settings.Renderer->CreateGeometry_AnyThread(Group.LOD, Task.Bounds.Min, Indices, Vertices);
		
#if USE_EMBREE_VOXEL
		RayHandler = MakeUnique<FVoxelSpawnerEmbreeRayHandler>(bShowDebugRays || bShowDebugHits, MoveTemp(Indices), MoveTemp(Vertices));
#else
		LOG_VOXEL(Error, TEXT("Embree is required for ray spawners!"));
		return;
#endif

		if (!ensure(!RayHandler->HasError())) 
		{
			return;
		}
	}

	if (Task.CancelCounter.IsCanceled()) return;

	const FVoxelIntBox LockedBounds = Task.Bounds.Extend(2); // For neighbors: +1; For max included vs excluded: +1
	FVoxelReadScopeLock Lock(*Settings.Data, LockedBounds, FUNCTION_FNAME);
	const FVoxelConstDataAccelerator Accelerator(*Settings.Data, LockedBounds);

	if (Task.CancelCounter.IsCanceled()) return;
	
	const FVoxelHitGenerator::FParameters Parameters{
		Task.GroupIndex,
		Task.Bounds,
		Task.SpawnersToSpawn,
		Group,
		ThreadSafeConfig,
		Task.CancelCounter,
		Accelerator
	};

	TUniquePtr<FVoxelHitGenerator> HitGenerator;
	if (Group.SpawnerType == EVoxelSpawnerType::Height)
	{
		HitGenerator = MakeUnique<FVoxelHeightHitGenerator>(Parameters);
	}
	else
	{
		HitGenerator = MakeUnique<FVoxelRayHitGenerator>(Parameters, *RayHandler);
	}

	const auto HitsMap = HitGenerator->Generate();

	if (Task.CancelCounter.IsCanceled()) return;

	if (bShowDebugRays || bShowDebugHits)
	{
		RayHandler->ShowDebug(Settings.VoxelWorldInterface, Task.Bounds.Min, bShowDebugRays, bShowDebugHits);
	}

	if (Task.CancelCounter.IsCanceled()) return;

	ProcessHits(Task, Accelerator, Lock, HitsMap);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnerManager::ProcessHits(
	const FVoxelSpawnerTask& Task,
	const FVoxelConstDataAccelerator& Accelerator,
	FVoxelReadScopeLock& Lock,
	const TMap<int32, TArray<FVoxelSpawnerHit>>& HitsMap)
{
	VOXEL_ASYNC_FUNCTION_COUNTER();
	
	if (Task.CancelCounter.IsCanceled()) return;

	if (CVarShowVoxelSpawnerPositions.GetValueOnAnyThread() != 0)
	{
		VOXEL_ASYNC_SCOPE_COUNTER("Debug Hits");
		AsyncTask(ENamedThreads::GameThread, [Hits = HitsMap, VoxelWorld = Settings.VoxelWorldInterface, BoundsMin = Task.Bounds.Min]()
		{
			if (VoxelWorld.IsValid())
			{
				if (UWorld* World = VoxelWorld->GetWorld())
				{
					for (auto& It : Hits)
					{
						auto& Color = GColorList.GetFColorByIndex(FMath::RandRange(0, GColorList.GetColorsNum() - 1));
						for (auto& Hit : It.Value)
						{
							auto Position = VoxelWorld->LocalToGlobalFloat(BoundsMin + FVoxelVector(Hit.LocalPosition));
							DrawDebugPoint(World, Position, 5, Color, true, 1000.f);
							DrawDebugLine(World, Position, Position + 50 * Hit.Normal, Color, true, 1000.f);
						}
					}
				}
			}
		});
	}

	if (Task.CancelCounter.IsCanceled()) return;

	const FVoxelSpawnerConfigGroup& Group = ThreadSafeConfig.Groups[Task.GroupIndex];
	FSpawnerGroupData& GroupData = GroupsData[Task.GroupIndex];

	TMap<int32, TVoxelSharedPtr<FVoxelSpawnerProxyResult>> SpawnerProxiesResults;
	for (int32 SpawnerIndex : Task.SpawnersToSpawn)
	{
		if (Task.CancelCounter.IsCanceled()) return;

		const FVoxelSpawnerConfigSpawner& Spawner = Group.Spawners[SpawnerIndex];
		FVoxelSpawnerProxy& SpawnerProxy = *GroupData.SpawnerProxies[SpawnerIndex];
		
		const TArray<FVoxelSpawnerHit> EmptyHits; // Won't be in HitsMap if no hits
		auto& Hits = HitsMap.Contains(SpawnerIndex) ? HitsMap[SpawnerIndex] : EmptyHits;

		TVoxelSharedPtr<FVoxelSpawnerProxyResult> Result;
		
		if (Hits.Num() > 0)
		{
			TUniquePtr<FVoxelSpawnerProxyResult> UniqueResult = SpawnerProxy.ProcessHits(Task.Bounds, Hits, Accelerator);
			Result = TVoxelSharedPtr<FVoxelSpawnerProxyResult>(UniqueResult.Release());
			check(!Result.IsValid() || Result->Type == SpawnerProxy.Type);
		}

		if (!Result.IsValid())
		{
			// Need to create empty results that can be saved if the voxel data is modified
			Result = MakeVoxelShared<FVoxelEmptySpawnerProxyResult>(SpawnerProxy);
		}

		Result->SetCanBeSaved(Spawner.bSave);
		Result->SetCanBeDespawned(!Spawner.bDoNotDespawn);

		SpawnerProxiesResults.Add(SpawnerIndex, Result);
	}

	// Important: Unlock before locking GroupLock to avoid deadlocks
	Lock.Unlock();

	FScopeLock GroupLock(&GroupData.Section);
	if (Task.CancelCounter.IsCanceled()) return;

	auto& ChunkData = GroupData.ChunksData.FindChecked(GroupData.GetChunkKey(Task.Bounds));
	if (!ensure(ChunkData.SpawnerProxiesResults.Num() == GroupData.SpawnerProxies.Num())) return;
	
	// Atomically add to the queues and save to chunk data
	for (auto& It : SpawnerProxiesResults)
	{
		ensure(!ChunkData.SpawnerProxiesResults[It.Key].IsValid());
		ChunkData.SpawnerProxiesResults[It.Key] = It.Value;
	}
	{
		FScopeLock ScopeLock(&CreateQueueSection);
		for (auto& It : SpawnerProxiesResults)
		{
			CreateQueue.Add(It.Value);
		}
	}

	for (const auto& SpawnerProxyResult : ChunkData.SpawnerProxiesResults) ensure(SpawnerProxyResult.IsValid());
	ensure(!Task.CancelCounter.IsCanceled());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnerManager::UpdateTaskCount() const
{
	Settings.DebugManager->ReportFoliageTaskCount(TaskCounter.GetValue());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnerManager::FlushCreateQueue_GameThread()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	CreateQueueSection.Lock();
	const auto QueueData = MoveTemp(CreateQueue);
	CreateQueueSection.Unlock();
	
	for (auto& Result : QueueData)
	{
		check(Result.IsValid());
		Result->Create();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
void FVoxelSpawnerManager::IterateResultsInBounds(const FVoxelIntBox& InBounds, T ApplyToResult_ReturnsShouldRebuild)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (!SpawnedSpawnersBounds.IsValid()) return;

	// As we are iterating Bounds, we need it as small as possible
	const FVoxelIntBox Bounds = InBounds.Overlap(SpawnedSpawnersBounds.GetBox());

	for (int32 GroupIndex = 0; GroupIndex < GroupsData.Num(); GroupIndex++)
	{
		FSpawnerGroupData& GroupData = GroupsData[GroupIndex];
		TArray<FVoxelIntBox> ChunksToRebuild;
		{
			FScopeLock Lock(&GroupData.Section);

			const FVoxelIntBox KeyBounds = Bounds.MakeMultipleOfBigger(GroupData.ChunkSize);

			const auto IterateChunkData = [&](const FVoxelIntBox& ChunkBounds, FSpawnerGroupChunkData& ChunkData)
			{
				bool bNeedRebuild = false;
				for (TVoxelSharedPtr<FVoxelSpawnerProxyResult>& SpawnerProxyResult : ChunkData.SpawnerProxiesResults)
				{
					if (SpawnerProxyResult.IsValid())
					{
						bNeedRebuild |= ApplyToResult_ReturnsShouldRebuild(SpawnerProxyResult);
					}
				}
				
				if (bNeedRebuild)
				{
					ChunksToRebuild.Emplace(ChunkBounds);
					// If we are rebuilding, we need to cancel existing tasks
					ChunkData.CancelTasks();
				}
			};

			// Check if it's faster to iterate ChunksData directly
			if (FVoxelIntBox(KeyBounds.Min / GroupData.ChunkSize, KeyBounds.Max / GroupData.ChunkSize).Count() < GroupData.ChunksData.Num())
			{
				KeyBounds.Iterate(GroupData.ChunkSize, [&](int32 X, int32 Y, int32 Z)
				{
					const FVoxelIntBox ChunkBounds = FVoxelIntBox(FIntVector(X, Y, Z), FIntVector(X, Y, Z) + GroupData.ChunkSize);
					if (FSpawnerGroupChunkData* ChunkData = GroupData.ChunksData.Find(GroupData.GetChunkKey(ChunkBounds)))
					{
						IterateChunkData(ChunkBounds, *ChunkData);
					}
				});
			}
			else
			{
				for (auto& It : GroupData.ChunksData)
				{
					const FVoxelIntBox ChunkBounds(It.Key, It.Key + GroupData.ChunkSize);
					if (ChunkBounds.Intersect(Bounds))
					{
						IterateChunkData(ChunkBounds, It.Value);
					}
				}
			}
		}

		for (auto& Chunk : ChunksToRebuild)
		{
			SpawnGroup_GameThread(Chunk, GroupIndex);
		}
	}
}

bool FVoxelSpawnerManager::FindSpawnerByGuid(const FGuid& Guid, int32& GroupIndex, int32& SpawnerIndex) const
{
	for (GroupIndex = 0; GroupIndex < ThreadSafeConfig.Groups.Num(); GroupIndex++)
	{
		auto& Group = ThreadSafeConfig.Groups[GroupIndex];
		for (SpawnerIndex = 0; SpawnerIndex < Group.Spawners.Num(); SpawnerIndex++)
		{
			if (Group.Spawners[SpawnerIndex].Guid == Guid)
			{
				return true;
			}
		}
	}
	return false;
}
