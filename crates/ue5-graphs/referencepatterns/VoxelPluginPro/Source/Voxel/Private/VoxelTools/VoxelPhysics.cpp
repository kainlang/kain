// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTools/VoxelPhysics.h"
#include "VoxelTools/VoxelPhysicsPartSpawner.h"
#include "VoxelTools/VoxelToolHelpers.h"
#include "VoxelData/VoxelData.h"
#include "VoxelData/VoxelDataAccelerator.h"
#include "VoxelRender/IVoxelLODManager.h"
#include "VoxelGenerators/VoxelEmptyGenerator.h"
#include "VoxelDebug/VoxelDebugUtilities.h"

#include "VoxelWorld.h"
#include "VoxelUtilities/VoxelIntVectorUtilities.h"

#include "Async/Async.h"
#include "DrawDebugHelpers.h"

FORCEINLINE uint32 GetIndex(const FVoxelIntBox& Box, const FIntVector& Size, int32 X, int32 Y, int32 Z)
{
	checkVoxelSlow(Box.Contains(X, Y, Z));
	checkVoxelSlow(Box.Size() == Size);
	return (X - Box.Min.X) + (Y - Box.Min.Y) * Size.X + (Z - Box.Min.Z) * Size.X * Size.Y;
}
FORCEINLINE uint32 GetIndex(const FVoxelIntBox& Box, const FIntVector& Size, const FIntVector& P)
{
	return GetIndex(Box, Size, P.X, P.Y, P.Z);
}

template<typename T1, typename T2, typename T3>
inline void CreateParts(
	TArray<FIntVector>& Queue, 
	TArray<FIntVector>& FloatingPoints,
	TArray<bool>& Visited,
	const TArray<FVoxelValue>& Values,
	const TArray<FVoxelMaterial>& Materials,
	const FVoxelIntBox& Bounds,
	const FIntVector& Size,
	T1 InitNewPart, 
	T2 AddVoxel, 
	T3 FinishPart)
{
	VOXEL_ASYNC_FUNCTION_COUNTER();
	
	while (FloatingPoints.Num() > 0)
	{
		const FIntVector NewPartCenter = FloatingPoints.Pop(UE_505_SWITCH(false, EAllowShrinking::No));
		const int32 NewPartCenterIndex = GetIndex(Bounds, Size, NewPartCenter);
		if (Visited[NewPartCenterIndex])
		{
			continue;
		}
		Visited[NewPartCenterIndex] = true;
		Queue.Reset();

		auto NewPart = InitNewPart(NewPartCenter);

		ensure(!Values[NewPartCenterIndex].IsEmpty());
		AddVoxel(NewPart, FIntVector(0), Values[NewPartCenterIndex], Materials[NewPartCenterIndex]);
		Queue.Add(NewPartCenter);

		while (Queue.Num() > 0)
		{
			const FIntVector QueuePosition = Queue.Pop(UE_505_SWITCH(false, EAllowShrinking::No));
			checkVoxelSlow(Bounds.Contains(QueuePosition));
			checkVoxelSlow(Visited[GetIndex(Bounds, Size, QueuePosition)] && !Values[GetIndex(Bounds, Size, QueuePosition)].IsEmpty());

			const auto Lambda = [&](const int32 LocalX, const int32 LocalY, const int32 LocalZ)
			{
				const FIntVector Position(QueuePosition.X + LocalX, QueuePosition.Y + LocalY, QueuePosition.Z + LocalZ);
				if (!Bounds.Contains(Position)) return;

				const int32 Index = GetIndex(Bounds, Size, Position);
				if (Visited[Index]) return;

				Visited[Index] = true;
				const auto Value = Values[Index];
				AddVoxel(NewPart, Position - NewPartCenter, Value, Materials[Index]);
				
				if (!Value.IsEmpty())
				{
					Queue.Add(Position);
				}
			};
			Lambda(+0, +0, -1);
			Lambda(+0, +0, +1);
			Lambda(+0, -1, +0);
			Lambda(+0, +1, +0);
			Lambda(-1, +0, +0);
			Lambda(+1, +0, +0);
		}

		FinishPart(NewPart);
	}
}

void UVoxelPhysicsTools::RemoveFloatingParts(
	FVoxelData& Data,
	const FVoxelIntBox& Bounds,
	const int32 MinParts,
	const bool bCreateData,
	const bool bCreateVoxels,
	const bool bDebug,
	const TWeakObjectPtr<AVoxelWorld> DebugWorld,
	FVoxelRemoveFloatingPartsResult& OutResult)
{
	VOXEL_TOOL_FUNCTION_COUNTER(Bounds.Count());

	const FIntVector Size = Bounds.Size();

	// TODO bit array
	TArray<bool> Visited;
	Visited.SetNumZeroed(Size.X * Size.Y * Size.Z);

	TArray<FIntVector> FloatingPoints;

	TArray<FIntVector> Queue;
	Queue.Reserve(Size.X * Size.Y * Size.Z);

	TArray<FVoxelValue> Values;
	TArray<FVoxelMaterial> Materials;
	{
		FVoxelPromotableReadScopeLock Lock(Data, Bounds, "SearchForFloatingBlocks");

		Values = Data.GetValues(Bounds);

		// Fill Visited array
		{
			VOXEL_ASYNC_SCOPE_COUNTER("Fill Visited array");
			
			// Add borders
			for (int32 X = Bounds.Min.X; X < Bounds.Max.X; X++)
			{
				for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; Y++)
				{
					Queue.Emplace(X, Y, Bounds.Min.Z);
					Queue.Emplace(X, Y, Bounds.Max.Z - 1);
				}
				for (int32 Z = Bounds.Min.Z; Z < Bounds.Max.Z; Z++)
				{
					Queue.Emplace(X, Bounds.Min.Y, Z);
					Queue.Emplace(X, Bounds.Max.Y - 1, Z);
				}
			}
			for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; Y++)
			{
				for (int32 Z = Bounds.Min.Z; Z < Bounds.Max.Z; Z++)
				{
					Queue.Emplace(Bounds.Min.X, Y, Z);
					Queue.Emplace(Bounds.Max.X - 1, Y, Z);
				}
			}

			while (Queue.Num() > 0)
			{
				const FIntVector Position = Queue.Pop(UE_505_SWITCH(false, EAllowShrinking::No));

				if (Bounds.Contains(Position))
				{
					const int32 Index = GetIndex(Bounds, Size, Position);
					if (!Visited[Index] && !Values[Index].IsEmpty())
					{
						Visited[Index] = true;
						FVoxelUtilities::AddImmediateNeighborsToArray(Position, Queue);
					}
				}
			}
		}

		{
			VOXEL_ASYNC_SCOPE_COUNTER("Remove not visited voxels");
			FVoxelMutableDataAccelerator OctreeAccelerator(Data, Bounds);
			for (int32 Z = Bounds.Min.Z; Z < Bounds.Max.Z; Z++)
			{
				for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; Y++)
				{
					for (int32 X = Bounds.Min.X; X < Bounds.Max.X; X++)
					{
						const int32 Index = GetIndex(Bounds, Size, X, Y, Z);

						if (Visited[Index]) continue;
						
						if (Values[Index].IsEmpty()) continue;

						// Write lock is expensive, so we avoid calling it until we really have to
						if (!Lock.IsPromoted())
						{
							Lock.Promote();
							Materials.SetNumUninitialized(Size.X * Size.Y * Size.Z);
						}

						Materials[Index] = OctreeAccelerator.GetMaterial(X, Y, Z, 0);
						OctreeAccelerator.SetValue(X, Y, Z, FVoxelValue::Empty());

						FloatingPoints.Emplace(X, Y, Z);
						OutResult.BoxToUpdate += FIntVector(X, Y, Z);
					}
				}
			}
		}
	}

	if (FloatingPoints.Num() < MinParts)
	{
		return;
	}

	if (bDebug)
	{
		AsyncTask(ENamedThreads::GameThread, [FloatingPoints, Bounds, DebugWorld]()
		{
			if (!DebugWorld.IsValid()) return;

			UVoxelDebugUtilities::DrawDebugIntBox(DebugWorld.Get(), Bounds, 10);

			for (auto& Point : FloatingPoints)
			{
				DrawDebugPoint(
					DebugWorld->GetWorld(),
					DebugWorld->LocalToGlobal(Point),
					10,
					FColor::Red,
					false,
					10);
			}
		});
	}

	{
		VOXEL_ASYNC_SCOPE_COUNTER("Create Parts");

		// Need to find all connected points to every floating point to create different parts
		
		ensure(!(bCreateData && bCreateVoxels));

		if (bCreateData)
		{
			const uint8 Depth = FVoxelUtilities::GetDepthFromSize<DATA_CHUNK_SIZE>(Size.GetMax());
			const auto Generator = MakeVoxelShared<FVoxelEmptyGeneratorInstance>(1, Data.Generator);

			struct FNewPart
			{
				const FIntVector NewPartCenter;
				const TVoxelSharedRef<FVoxelData> NewPartData;
				TUniquePtr<FVoxelWriteScopeLock> Lock;
				TUniquePtr<FVoxelMutableDataAccelerator> OctreeAccelerator;

				FNewPart(
					const FIntVector& NewPartCenter,
					uint8 InDepth,
					const TVoxelSharedRef<FVoxelGeneratorInstance>& InGenerator,
					bool bEnableMultiplayer,
					bool bEnableUndoRedo)
					: NewPartCenter(NewPartCenter)
					, NewPartData(FVoxelData::Create(FVoxelDataSettings(InDepth, InGenerator, bEnableMultiplayer, bEnableUndoRedo)))
					, Lock(MakeUnique<FVoxelWriteScopeLock>(*NewPartData, FVoxelIntBox::Infinite, STATIC_FNAME("FloatingBlocks")))  // Make checks happy
					, OctreeAccelerator(MakeUnique<FVoxelMutableDataAccelerator>(*NewPartData, FVoxelIntBox::Infinite))
				{
				}
			};

			const auto InitNewPart = [Depth, &Generator, &Data](const FIntVector& NewPartCenter)
			{
				return FNewPart(NewPartCenter, Depth, Generator, Data.bEnableMultiplayer, Data.bEnableUndoRedo);
			};
			const auto AddVoxel = [](FNewPart& NewPart, const FIntVector& Position, const FVoxelValue& Value, const FVoxelMaterial& Material)
			{
				if (Value.IsEmpty())
				{
					// Just copy the value (to have a great looking part)
					NewPart.OctreeAccelerator->SetValue(Position, Value);
				}
				else
				{
					// Copy the value & add it to the queue
					NewPart.OctreeAccelerator->SetValue(Position, Value);
					NewPart.OctreeAccelerator->SetMaterial(Position, Material);
				}
			};
			const auto FinishPart = [&OutResult](FNewPart& NewPart)
			{
				OutResult.Parts.Add({ NewPart.NewPartCenter, NewPart.NewPartData, {} });
			};

			CreateParts(Queue, FloatingPoints, Visited, Values, Materials, Bounds, Size, InitNewPart, AddVoxel, FinishPart);
		}

		if (bCreateVoxels)
		{
			const auto InitNewPart = [](const FIntVector& NewPartCenter)
			{
				return FVoxelFloatingPart{ NewPartCenter };
			};
			const auto AddVoxel = [](FVoxelFloatingPart& NewPart, const FIntVector& Position, const FVoxelValue& Value, const FVoxelMaterial& Material)
			{
				if (!Value.IsEmpty())
				{
					NewPart.Voxels.Add({ Position, Value.ToFloat(), Material });
				}
			};
			const auto FinishPart = [&OutResult](FVoxelFloatingPart& NewPart)
			{
				OutResult.Parts.Add(MoveTemp(NewPart));
			};

			CreateParts(Queue, FloatingPoints, Visited, Values, Materials, Bounds, Size, InitNewPart, AddVoxel, FinishPart);
		}
	}
}

TArray<TScriptInterface<IVoxelPhysicsPartSpawnerResult>> UVoxelPhysicsTools::SpawnFloatingPartsAndUpdateWorld(
	IVoxelPhysicsPartSpawner& PartSpawner,
	AVoxelWorld* const World,
	FVoxelRemoveFloatingPartsResult&& RemoveFloatingPartsResult)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	TArray<TScriptInterface<IVoxelPhysicsPartSpawnerResult>> Results;
	
	TArray<TVoxelSharedPtr<FSimpleDelegate>> OnWorldUpdateDoneDelegates;
	for (auto& NewPart : RemoveFloatingPartsResult.Parts)
	{
		TVoxelSharedPtr<FSimpleDelegate> OnWorldUpdateDone;
		const auto Result = PartSpawner.SpawnPart(OnWorldUpdateDone, World, MoveTemp(NewPart.Data), MoveTemp(NewPart.Voxels), NewPart.PartCenter);
		if (Result)
		{
			Results.Add(Result);
		}
		if (OnWorldUpdateDone.IsValid())
		{
			OnWorldUpdateDoneDelegates.Add(OnWorldUpdateDone);
		}
	}

	const auto Finish = [OnWorldUpdateDoneDelegates]()
	{
		for (auto& Delegate : OnWorldUpdateDoneDelegates)
		{
			if (ensure(Delegate.IsValid()))
			{
				Delegate->ExecuteIfBound();
			}
		}
	};

	if (ensure(RemoveFloatingPartsResult.BoxToUpdate.IsValid()))
	{
		World->GetLODManager().UpdateBounds_OnAllFinished(RemoveFloatingPartsResult.BoxToUpdate.GetBox(), FSimpleDelegate::CreateLambda(Finish));
	}

	return Results;
}

void UVoxelPhysicsTools::ApplyVoxelPhysics(
	UObject* WorldContextObject,
	FLatentActionInfo LatentInfo,
	TArray<TScriptInterface<IVoxelPhysicsPartSpawnerResult>>& OutResults,
	AVoxelWorld* World,
	FVoxelIntBox Bounds,
	TScriptInterface<IVoxelPhysicsPartSpawner> PartSpawner,
	int32 MinParts,
	bool bDebug,
	bool bHideLatentWarnings)
{
	VOXEL_FUNCTION_COUNTER();
	CHECK_VOXELWORLD_IS_CREATED_VOID();

	const bool bCreateData = PartSpawner && PartSpawner->NeedData();
	const bool bCreateVoxels = PartSpawner && PartSpawner->NeedVoxels();

	const auto PartSpawnerUnsafe = PartSpawner.GetObject();
	const auto PartSpawnerWeakPtr = MakeWeakObjectPtr(PartSpawner.GetObject());
	const auto VoxelWorldWeakPtr = MakeWeakObjectPtr(World);
	
	using FWork = TVoxelLatentActionAsyncWork_WithWorld_WithValue<FVoxelRemoveFloatingPartsResult>;
	
	FVoxelToolHelpers::StartAsyncLatentActionImpl<FWork>(
		WorldContextObject,
		LatentInfo,
		World,
		FUNCTION_FNAME,
		bHideLatentWarnings,
		[&]()
		{
			return new FWork(
				FUNCTION_FNAME,
				World,
				[=](FVoxelData& Data, FVoxelRemoveFloatingPartsResult& Result)
				{
					RemoveFloatingParts(
						Data,
						Bounds,
						MinParts,
						bCreateData,
						bCreateVoxels,
						bDebug,
						VoxelWorldWeakPtr,
						Result);
				});
		},
		[=, WeakWorldContextObject = MakeWeakObjectPtr(WorldContextObject), &OutResults]
		(FWork& Work)
		{
			if (!VoxelWorldWeakPtr.IsValid()) return;

			if (PartSpawnerUnsafe != PartSpawnerWeakPtr.Get())
			{
				ensure(!PartSpawnerWeakPtr.IsValid());
				FVoxelMessages::Error(FUNCTION_ERROR("Part spawner was deleted by garbage collection before the async function ended. To avoid this, store a ref to the spawner in your BP"));
				return;
			}

			TArray<TScriptInterface<IVoxelPhysicsPartSpawnerResult>> Results;
		
			auto* IPartSpawner = Cast<IVoxelPhysicsPartSpawner>(PartSpawnerWeakPtr.Get());
			if (PartSpawnerWeakPtr.IsValid() && ensure(IPartSpawner) && Work.Value.Parts.Num() > 0)
			{
				Results = SpawnFloatingPartsAndUpdateWorld(*IPartSpawner, VoxelWorldWeakPtr.Get(), MoveTemp(Work.Value));
			}
		
			if (WeakWorldContextObject.IsValid())
			{
				OutResults = MoveTemp(Results);
			}
		});
}