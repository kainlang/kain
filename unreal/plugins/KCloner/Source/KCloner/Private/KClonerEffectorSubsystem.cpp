// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerEffectorSubsystem.h"

void UKClonerEffectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bIsDirty = false;
}

void UKClonerEffectorSubsystem::Deinitialize()
{
	Super::Deinitialize();
	PointCloud.Effectors.Empty();
	KDTree.Reset();
}

void UKClonerEffectorSubsystem::RegisterEffector(UKClonerEffectorComponent* Effector)
{
	if (Effector && !PointCloud.Effectors.Contains(Effector))
	{
		PointCloud.Effectors.Add(Effector);
		bIsDirty = true;
	}
}

void UKClonerEffectorSubsystem::UnregisterEffector(UKClonerEffectorComponent* Effector)
{
	if (Effector)
	{
		int32 Removed = PointCloud.Effectors.Remove(Effector);
		if (Removed > 0)
		{
			bIsDirty = true;
		}
	}
}

void UKClonerEffectorSubsystem::RebuildIndex()
{
	// cleanup dead pointers before we rebuild
	// don't want to crash on a dangling ref
	for (int32 i = PointCloud.Effectors.Num() - 1; i >= 0; --i)
	{
		if (!PointCloud.Effectors[i] || !IsValid(PointCloud.Effectors[i]))
		{
			PointCloud.Effectors.RemoveAt(i);
		}
	}

	if (PointCloud.Effectors.Num() == 0)
	{
		KDTree.Reset();
		bIsDirty = false;
		return;
	}

	// build the KD-Tree
	// using nanoflann because typical unreal scans are too slow for 10k items
	KDTree = MakeUnique<FKClonerEffectorKDTree>(
		3 /*dim*/, 
		PointCloud, 
		nanoflann::KDTreeSingleIndexAdaptorParams(10 /* leaf max size */)
	);
	
	KDTree->buildIndex();
	bIsDirty = false;
}

TArray<UKClonerEffectorComponent*> UKClonerEffectorSubsystem::FindEffectorsNear(const FVector& Location, float Radius)
{
	if (bIsDirty || !KDTree)
	{
		RebuildIndex();
	}

	TArray<UKClonerEffectorComponent*> Results;
	
	if (!KDTree || PointCloud.Effectors.Num() == 0)
	{
		return Results;
	}

	float SearchPoint[3] = { (float)Location.X, (float)Location.Y, (float)Location.Z };
	// nanoflann wants squared radius for optimization
	float SearchRadiusSq = Radius * Radius;

	std::vector<nanoflann::ResultItem<unsigned int, float>> RetMatches;
	nanoflann::SearchParameters Params;

	// Radius search
	const size_t nMatches = KDTree->radiusSearch(&SearchPoint[0], SearchRadiusSq, RetMatches, Params);

	Results.Reserve(nMatches);
	for (size_t i = 0; i < nMatches; i++)
	{
		size_t Idx = static_cast<size_t>(RetMatches[i].first);
		if (PointCloud.Effectors.IsValidIndex(Idx))
		{
			Results.Add(PointCloud.Effectors[Idx]);
		}
	}
	
	// priority sorting - high priority effectors win
	Results.Sort([](const UKClonerEffectorComponent& A, const UKClonerEffectorComponent& B)
	{
		return A.Priority > B.Priority;
	});

	return Results;
}
