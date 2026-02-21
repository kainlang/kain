// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelIntBox.h"
#include "VoxelMinimal.h"
#include "VoxelSaveStruct.h"
#include "VoxelSpawner.generated.h"

class FVoxelConstDataAccelerator;
class FVoxelSpawnerManager;
class FVoxelSpawnerProxy;
class FVoxelData;
class AVoxelSpawnerActor;
class UVoxelSpawner;

namespace FVoxelSpawnersSaveVersion
{
	enum Type : int32
	{
		BeforeCustomVersionWasAdded,
		SHARED_PlaceableItemsInSave,
		SHARED_AssetItemsImportValueMaterials,
		SHARED_DataAssetScale,
		SHARED_RemoveVoxelGrass,
		SHARED_DataAssetTransform,
		SHARED_RemoveEnableVoxelSpawnedActorsEnableVoxelGrass,
		SHARED_FoliagePaint,
		SHARED_ValueConfigFlagAndSaveGUIDs,
		SHARED_SingleValues,
		SHARED_NoVoxelMaterialInHeightmapAssets,
		SHARED_FixMissingMaterialsInHeightmapAssets,
		SHARED_AddUserFlagsToSaves,
		StoreSpawnerMatricesRelativeToComponent,
		SHARED_StoreMaterialChannelsIndividuallyAndRemoveFoliage,
		
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
}

struct VOXEL_API FVoxelSpawnersSaveImpl
{	
	FVoxelSpawnersSaveImpl() = default;

	bool Serialize(FArchive& Ar);

	bool operator==(const FVoxelSpawnersSaveImpl& Other) const
	{
		return Guid == Other.Guid;
	}
	
private:
	// Version of FVoxelSpawnerSave, not of the compressed data!
	int32 Version;
	FGuid Guid;
	TArray<uint8> CompressedData;

	friend class FVoxelSpawnerManager;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelSpawnerHit
{
	// Relative to chunk bounds.min
	FVector LocalPosition;
	FVector Normal;

	FVoxelSpawnerHit() = default;
	FVoxelSpawnerHit(const FVector& LocalPosition, const FVector& Normal)
		: LocalPosition(LocalPosition)
		, Normal(Normal)
	{
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DECLARE_VOXEL_MEMORY_STAT(TEXT("Voxel Spawner Results Memory"), STAT_VoxelSpawnerResults, STATGROUP_VoxelMemory, VOXEL_API);

enum class EVoxelSpawnerProxyType : uint8
{
	Invalid      = 0,
	// Special spawner: any proxy can return an empty spawner
	EmptySpawner = 1,
	AssetSpawner = 2,
	MeshSpawner  = 3,
	SpawnerGroup = 4,
};

inline const TCHAR* ToString(EVoxelSpawnerProxyType ProxyType)
{
	switch (ProxyType)
	{
	case EVoxelSpawnerProxyType::Invalid: return TEXT("Invalid");
	case EVoxelSpawnerProxyType::EmptySpawner: return TEXT("EmptySpawner");
	case EVoxelSpawnerProxyType::AssetSpawner: return TEXT("AssetSpawner");
	case EVoxelSpawnerProxyType::MeshSpawner: return TEXT("MeshSpawner");
	case EVoxelSpawnerProxyType::SpawnerGroup: return TEXT("SpawnerGroup");
	default: check(false); return TEXT("");;
	}
}

class VOXEL_API FVoxelSpawnerProxyResult : public TVoxelSharedFromThis<FVoxelSpawnerProxyResult>
{
private:
	uint32 AllocatedSize = 0;
	bool bCreated = false;
	bool bDirty = false;
	bool bCanBeSaved = true;
	bool bCanBeDespawned = true;
	
public:
	const EVoxelSpawnerProxyType Type;
	const FVoxelSpawnerProxy& Proxy;

public:
	explicit FVoxelSpawnerProxyResult(EVoxelSpawnerProxyType Type, const FVoxelSpawnerProxy& Proxy);
	virtual ~FVoxelSpawnerProxyResult();

	FVoxelSpawnerProxyResult(const FVoxelSpawnerProxyResult&) = delete;
	FVoxelSpawnerProxyResult& operator=(const FVoxelSpawnerProxyResult&) = delete;

public:
	void Create();
	void Destroy();
	void SerializeProxy(FArchive& Ar, FVoxelSpawnersSaveVersion::Type Version);

	static TVoxelSharedRef<FVoxelSpawnerProxyResult> CreateFromType(EVoxelSpawnerProxyType Type, FVoxelSpawnerProxy& Proxy);

	inline bool IsCreated() const
	{
		return bCreated;
	}
	
	inline bool IsDirty() const
	{
		return bDirty;
	}
	inline void MarkDirty()
	{
		bDirty = true;
	}

	inline bool CanBeSaved() const
	{
		return bCanBeSaved;
	}
	inline void SetCanBeSaved(bool bNewCanBeSaved)
	{
		bCanBeSaved = bNewCanBeSaved;
	}

	inline bool CanBeDespawned() const
	{
		return bCanBeDespawned;
	}
	inline void SetCanBeDespawned(bool bNewCanBeDespawned)
	{
		bCanBeDespawned = bNewCanBeDespawned;
	}
	
	inline bool NeedsToBeSaved() const
	{
		return IsDirty() && CanBeSaved();
	}

protected:
	//~ Begin FVoxelSpawnerProxyResult Interface
	// Creates rendering. Called on the game thread.
	virtual void CreateImpl() = 0;
	// Destroys rendering. Called on the game thread.
	virtual void DestroyImpl() = 0;

	virtual void SerializeImpl(FArchive& Ar, FVoxelSpawnersSaveVersion::Type Version) = 0;

	virtual uint32 GetAllocatedSize() = 0;
	//~ End FVoxelSpawnerProxyResult Interface

private:
	void UpdateStats();
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXEL_API FVoxelSpawnerProxy : public TVoxelSharedFromThis<FVoxelSpawnerProxy>
{
public:
	FVoxelSpawnerManager& Manager;

	const EVoxelSpawnerProxyType Type;
	const uint32 SpawnerSeed;
	

	// Seed can be 0
	FVoxelSpawnerProxy(UVoxelSpawner* Spawner, FVoxelSpawnerManager& Manager, EVoxelSpawnerProxyType Type, uint32 Seed);
	virtual ~FVoxelSpawnerProxy() = default;

	// Both of these functions must be called only from the Manager or recursively!
	
	//~ Begin FVoxelSpawnerProxy Interface
	virtual TUniquePtr<FVoxelSpawnerProxyResult> ProcessHits(
		const FVoxelIntBox& Bounds, 
		const TArray<FVoxelSpawnerHit>& Hits,
		const FVoxelConstDataAccelerator& Accelerator) const = 0;
	virtual void PostSpawn() = 0; // Called right after every spawner is created
	//~ End FVoxelSpawnerProxy Interface
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UCLASS(Abstract)
class VOXEL_API UVoxelSpawner : public UObject
{
	GENERATED_BODY()

public:
	// Average distance between the instances, in voxels
	// Num Instances = Area in voxels / Square(DistanceBetweenInstancesInVoxel)
	// Not a density because the values would be too small to store in a float
	UPROPERTY(EditAnywhere, Category = "General Settings", meta = (ClampMin = 0))
	float DistanceBetweenInstancesInVoxel = 10;

	// Use this if you create the spawner at runtime
	UPROPERTY(Transient)
	uint32 SeedOverride = 0;
	
public:
#if WITH_EDITOR
	virtual bool NeedsToRebuild(UObject* Object, const FPropertyChangedEvent& PropertyChangedEvent) { return false; }
#endif
	
	virtual TVoxelSharedRef<FVoxelSpawnerProxy> GetSpawnerProxy(FVoxelSpawnerManager& Manager);
	// All added spawners MUST be valid. Returns success
	virtual bool GetSpawners(TSet<UVoxelSpawner*>& OutSpawners);
	virtual float GetDistanceBetweenInstancesInVoxel() const { return DistanceBetweenInstancesInVoxel; }
	virtual FString GetDebugInfo() const { return {}; }
};

USTRUCT(BlueprintType, Category = Voxel)
struct VOXEL_API FVoxelSpawnersSave
#if CPP
	: public TVoxelSaveStruct<FVoxelSpawnersSaveImpl>
#endif
{	
	GENERATED_BODY()
};

DEFINE_VOXEL_SAVE_STRUCT(FVoxelSpawnersSave)