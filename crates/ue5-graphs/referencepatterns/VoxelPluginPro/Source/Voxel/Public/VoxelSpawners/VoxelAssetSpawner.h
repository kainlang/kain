// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelIntBox.h"
#include "VoxelSpawners/VoxelBasicSpawner.h"
#include "VoxelGenerators/VoxelGeneratorPicker.h"
#include "VoxelAssetSpawner.generated.h"

class UVoxelAssetSpawner;
class UVoxelTransformableGenerator;
class FVoxelTransformableGeneratorInstance;
class FVoxelAssetSpawnerProxy;

template<typename T>
class TVoxelDataItemWrapper;

struct FVoxelAssetItem;

class VOXEL_API FVoxelAssetSpawnerProxyResult : public FVoxelSpawnerProxyResult
{
public:
	explicit FVoxelAssetSpawnerProxyResult(const FVoxelAssetSpawnerProxy& Proxy);

	void Init(TArray<FMatrix>&& InMatrices, TArray<int32>&& InGeneratorsIndices);

	//~ Begin FVoxelSpawnerProxyResult override
	virtual void CreateImpl() override;
	virtual void DestroyImpl() override;

	virtual void SerializeImpl(FArchive& Ar, FVoxelSpawnersSaveVersion::Type Version) override;

	virtual uint32 GetAllocatedSize() override;
	//~ End FVoxelSpawnerProxyResult override

private:
	FVoxelIntBox Bounds;
	TArray<FMatrix> Matrices;
	TArray<int32> GeneratorsIndices;
	
	TArray<TVoxelWeakPtr<TVoxelDataItemWrapper<FVoxelAssetItem>>> Items;
};

class VOXEL_API FVoxelAssetSpawnerProxy : public FVoxelBasicSpawnerProxy
{
public:
	const TArray<TVoxelSharedPtr<FVoxelTransformableGeneratorInstance>> Generators;
	const FVoxelIntBox GeneratorLocalBounds;
	const int32 Priority;
	const bool bRoundAssetPosition;
	
	FVoxelAssetSpawnerProxy(UVoxelAssetSpawner* Spawner, FVoxelSpawnerManager& Manager);
	virtual ~FVoxelAssetSpawnerProxy();

	//~ Begin FVoxelSpawnerProxy Interface
	virtual TUniquePtr<FVoxelSpawnerProxyResult> ProcessHits(
		const FVoxelIntBox& Bounds,
		const TArray<FVoxelSpawnerHit>& Hits,
		const FVoxelConstDataAccelerator& Accelerator) const override;
	virtual void PostSpawn() override {}
	//~ End FVoxelSpawnerProxy Interface
};

UCLASS()
class VOXEL_API UVoxelAssetSpawner : public UVoxelBasicSpawner
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings")
	FVoxelTransformableGeneratorPicker Generator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings")
	FVoxelIntBox GeneratorLocalBounds = FVoxelIntBox(-25, 25);

	// The voxel world seeds will be sent to the generator.
	// Add the names of the seeds you want to be randomized here
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings")
	TArray<FName> Seeds;

	// All generators are created at begin play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings", meta = (ClampMin = 1))
	int32 NumberOfDifferentSeedsToUse = 1;

	// Priority of the spawned assets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings")
	bool bRoundAssetPosition = false;
	
public:
	//~ Begin UVoxelSpawner Interface
#if WITH_EDITOR
	virtual bool NeedsToRebuild(UObject* Object, const FPropertyChangedEvent& PropertyChangedEvent) override { return Object == Generator.GetObject(); }
#endif
	virtual TVoxelSharedRef<FVoxelSpawnerProxy> GetSpawnerProxy(FVoxelSpawnerManager& Manager) override;
	virtual FString GetDebugInfo() const override;
	//~ End UVoxelSpawner Interface
};