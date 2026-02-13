// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KClonerEffectorComponent.h"
#include "nanoflann.hpp"
#include "KClonerEffectorSubsystem.generated.h"

// Adapter for nanoflann to interface with UE4 types
struct FKClonerEffectorPointCloud
{
	TArray<UKClonerEffectorComponent*> Effectors;

	inline size_t kdtree_get_point_count() const { return Effectors.Num(); }

	inline float kdtree_get_pt(const size_t idx, const size_t dim) const
	{
		if (Effectors.IsValidIndex(idx) && Effectors[idx])
		{
			FVector Loc = Effectors[idx]->GetEffectorLocation();
			if (dim == 0) return Loc.X;
			if (dim == 1) return Loc.Y;
			return Loc.Z;
		}
		return 0.0f;
	}

	template <class BBOX>
	bool kdtree_get_bbox(BBOX& /*bb*/) const { return false; }
};

// Define the KDTree alias
typedef nanoflann::KDTreeSingleIndexAdaptor<
	nanoflann::L2_Simple_Adaptor<float, FKClonerEffectorPointCloud>,
	FKClonerEffectorPointCloud,
	3 /* dim */
> FKClonerEffectorKDTree;

/**
 * Subsystem to manage spatial indexing of K-Cloner Effectors.
 * Replaces O(N) world scans with O(log N) KD-tree lookups.
 */
UCLASS()
class KCLONER_API UKClonerEffectorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// API
	void RegisterEffector(UKClonerEffectorComponent* Effector);
	void UnregisterEffector(UKClonerEffectorComponent* Effector);
	
	/** Mark tree as needing rebuild (e.g. effector moved) */
	void MarkDirty() { bIsDirty = true; }

	/** Find all effectors within radius using KD-Tree */
	TArray<UKClonerEffectorComponent*> FindEffectorsNear(const FVector& Location, float Radius);

private:
	void RebuildIndex();

	FKClonerEffectorPointCloud PointCloud;
	TUniquePtr<FKClonerEffectorKDTree> KDTree;
	bool bIsDirty = false;
};
