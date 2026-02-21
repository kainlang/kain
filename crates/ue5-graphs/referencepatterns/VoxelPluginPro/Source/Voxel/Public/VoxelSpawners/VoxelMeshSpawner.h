// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelSpawners/VoxelBasicSpawner.h"
#include "VoxelSpawners/VoxelInstancedMeshSettings.h"
#include "VoxelSpawners/VoxelSpawnerMatrix.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "VoxelMeshSpawner.generated.h"

class FVoxelConstDataAccelerator;
class FVoxelMeshSpawnerProxy;
class FVoxelMeshSpawnerGroupProxy;
struct FVoxelInstancedMeshInstancesRef;
class UStaticMesh;
class UVoxelMeshSpawnerBase;
class UVoxelMeshSpawnerGroup;
class UVoxelHierarchicalInstancedStaticMeshComponent;
enum class EVoxelMeshSpawnerInstanceRandom : uint8;

class VOXEL_API FVoxelMeshSpawnerProxyResult : public FVoxelSpawnerProxyResult
{
public:
	explicit FVoxelMeshSpawnerProxyResult(const FVoxelMeshSpawnerProxy& Proxy);
	~FVoxelMeshSpawnerProxyResult();

	void Init(const FVoxelIntBox& InBounds, FVoxelSpawnerTransforms&& InTransforms);

	const FVoxelSpawnerTransforms& GetTransforms() const { return Transforms; }
	
	//~ Begin FVoxelSpawnerProxyResult override
	virtual void CreateImpl() override;
	virtual void DestroyImpl() override;

	virtual void SerializeImpl(FArchive& Ar, FVoxelSpawnersSaveVersion::Type Version) override;
	
	virtual uint32 GetAllocatedSize() override;
	//~ End FVoxelSpawnerProxyResult override

private:
	FVoxelIntBox Bounds;
	FVoxelSpawnerTransforms Transforms;
	
	TUniquePtr<FVoxelInstancedMeshInstancesRef> InstancesRef;

	void ApplyRemovedIndices();
};

class VOXEL_API FVoxelMeshSpawnerProxy : public FVoxelBasicSpawnerProxy
{
public:
	const FVoxelInstancedMeshAndActorSettings InstanceSettings;
	const EVoxelMeshSpawnerInstanceRandom InstanceRandom;
	const FName ColorOutputName;
	const bool bAlwaysSpawnActor;
	const FVector FloatingDetectionOffset;

	FVoxelMeshSpawnerProxy(
		UVoxelMeshSpawnerBase* Spawner, 
		TWeakObjectPtr<UStaticMesh> Mesh, 
		const TMap<int32, UMaterialInterface*>& SectionsMaterials, 
		FVoxelSpawnerManager& Manager, 
		uint32 Seed);

	//~ Begin FVoxelSpawnerProxy Interface
	virtual TUniquePtr<FVoxelSpawnerProxyResult> ProcessHits(
		const FVoxelIntBox& Bounds,
		const TArray<FVoxelSpawnerHit>& Hits,
		const FVoxelConstDataAccelerator& Accelerator) const override;
	virtual void PostSpawn() override {}
	//~ End FVoxelSpawnerProxy Interface
};

class VOXEL_API FVoxelMeshSpawnerGroupProxy : public FVoxelSpawnerProxy
{
public:
	const TArray<TVoxelSharedPtr<FVoxelMeshSpawnerProxy>> Proxies;
	
	FVoxelMeshSpawnerGroupProxy(UVoxelMeshSpawnerGroup* Spawner, FVoxelSpawnerManager& Manager);

	//~ Begin FVoxelSpawnerProxy Interface
	virtual TUniquePtr<FVoxelSpawnerProxyResult> ProcessHits(
		const FVoxelIntBox& Bounds,
		const TArray<FVoxelSpawnerHit>& Hits,
		const FVoxelConstDataAccelerator& Accelerator) const override;
	virtual void PostSpawn() override {}
	//~ End FVoxelSpawnerProxy Interface
};

UENUM()
enum class EVoxelMeshSpawnerInstanceRandom : uint8
{
	// Random number
	// Use GetVoxelSpawnerActorInstanceRandom to get it
	// Will have the same value in the spawned actor as in the instance
	Random,
	// Get the voxel material in the shader
	// Use GetVoxelMaterialFromInstanceRandom
	VoxelMaterial,
	// Get a voxel graph output color in the shader
	// Use GetColorFromInstanceRandom
	ColorOutput
};

UCLASS(Abstract)
class VOXEL_API UVoxelMeshSpawnerBase : public UVoxelBasicSpawner
{
	GENERATED_BODY()

public:
	// What to send through InstanceRandom
	// Check enum values tooltips
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings")
	EVoxelMeshSpawnerInstanceRandom InstanceRandom = EVoxelMeshSpawnerInstanceRandom::Random;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings")
	FName ColorOutputName;
	
	// Actor to spawn to replace the instanced mesh. After spawn, the SetStaticMesh event will be called on the actor with Mesh as argument
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Settings", meta = (ShowOnlyInnerProperties))
	FVoxelSpawnerActorSettings ActorSettings;

	// Will always spawn an actor instead of an instanced mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Actor Settings")
	bool bAlwaysSpawnActor = false;

public:
	UPROPERTY(EditAnywhere, Category = "Instance Settings", meta = (ShowOnlyInnerProperties))
	FVoxelInstancedMeshSettings InstancedMeshSettings;
	
public:
	// In local space. Increase this if your foliage is enabling physics too soon. In cm
	UPROPERTY(EditAnywhere, Category = "Placement - Offset")
	FVector FloatingDetectionOffset = FVector(0, 0, -10);

protected:
	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface
};

UCLASS()
class VOXEL_API UVoxelMeshSpawner : public UVoxelMeshSpawnerBase
{
	GENERATED_BODY()

public:
	// Mesh to spawn. Can be left to null if AlwaysSpawnActor is true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	// Per mesh section
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "General Settings")
	TMap<int32, TObjectPtr<UMaterialInterface>> MaterialsOverrides;
	
public:
	//~ Begin UVoxelSpawner Interface
#if WITH_EDITOR
	virtual bool NeedsToRebuild(UObject* Object, const FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual TVoxelSharedRef<FVoxelSpawnerProxy> GetSpawnerProxy(FVoxelSpawnerManager& Manager) override;
	virtual FString GetDebugInfo() const override;
	//~ End UVoxelSpawner Interface
};

UCLASS()
class VOXEL_API UVoxelMeshSpawnerGroup : public UVoxelMeshSpawnerBase
{
	GENERATED_BODY()

public:
	// Meshes to spawn. Can be left to null if AlwaysSpawnActor is true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings")
	TArray<TObjectPtr<UStaticMesh>> Meshes;
	
public:
#if WITH_EDITOR
	virtual bool NeedsToRebuild(UObject* Object, const FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ Begin UVoxelSpawner Interface
	virtual TVoxelSharedRef<FVoxelSpawnerProxy> GetSpawnerProxy(FVoxelSpawnerManager& Manager) override;
	virtual float GetDistanceBetweenInstancesInVoxel() const override;
	virtual FString GetDebugInfo() const override;
	//~ End UVoxelSpawner Interface
};