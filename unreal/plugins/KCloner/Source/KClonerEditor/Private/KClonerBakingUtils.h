// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UKClonerData;
class AKClonerActor;
class USkeletalMesh;
class UAnimSequence;
class UStaticMesh;
class UGeometryCache;

/**
 * Options for Static Mesh baking
 */
struct FKClonerStaticMeshBakeOptions
{
	// Generate per-LOD meshes
	bool bGenerateLODs = true;
	
	// LOD screen sizes
	TArray<float> LODScreenSizes = { 1.0f, 0.5f, 0.25f };
	
	// LOD polygon reduction percentages
	TArray<float> LODReductionPercent = { 1.0f, 0.5f, 0.25f };
	
	// Generate simple collision
	bool bGenerateCollision = true;
	
	// Generate distance field for mesh
	bool bGenerateDistanceField = true;
	
	// Merge distance for vertex welding
	float MergeDistance = 0.01f;
};

/**
 * Options for Alembic export
 */
struct FKClonerAlembicExportOptions
{
	// Duration in seconds
	float Duration = 5.0f;
	
	// Frames per second
	float FrameRate = 30.0f;
	
	// Export path (full path including filename)
	FString ExportPath;
	
	// Export materials
	bool bExportMaterials = true;
	
	// Export as single mesh or per-instance
	bool bMergeToSingleMesh = true;
	
	// Archive type
	bool bOgawa = true; // true = Ogawa, false = HDF5
};

/**
 * Options for Geometry Cache baking
 */
struct FKClonerGeometryCacheBakeOptions
{
	// Duration in seconds
	float Duration = 5.0f;
	
	// Frames per second
	float FrameRate = 30.0f;
	
	// Compression codec
	enum class ECompression
	{
		None,
		Lossless,
		Lossy
	};
	ECompression Compression = ECompression::Lossless;
	
	// Quantization bits (for lossy)
	int32 QuantizationBits = 16;
	
	// Optimize for playback
	bool bOptimizeForPlayback = true;
};

/**
 * Result of a baking operation
 */
struct FKClonerBakeResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	UObject* ResultAsset = nullptr;
	int32 VertexCount = 0;
	int32 TriangleCount = 0;
	int32 InstanceCount = 0;
	float BakeDuration = 0.0f;
};

/**
 * Complete baking utilities for K-Cloner
 */
class KCLONEREDITOR_API FKClonerBakingUtils
{
public:
	//===========================================================================
	// STATIC MESH BAKING
	//===========================================================================
	
	/**
	 * Merges all cloner instances into a single Static Mesh asset.
	 * Each instance's transform is baked into vertex positions.
	 */
	static FKClonerBakeResult BakeToStaticMesh(
		AKClonerActor* ClonerActor,
		const FString& PackageName,
		const FKClonerStaticMeshBakeOptions& Options = FKClonerStaticMeshBakeOptions());
	
	//===========================================================================
	// ALEMBIC EXPORT
	//===========================================================================
	
	/**
	 * Exports cloner animation to Alembic (.abc) file.
	 * Samples modifier animation over time and writes to disk.
	 */
	static FKClonerBakeResult ExportToAlembic(
		AKClonerActor* ClonerActor,
		const FKClonerAlembicExportOptions& Options);
	
	//===========================================================================
	// GEOMETRY CACHE BAKING
	//===========================================================================
	
	/**
	 * Bakes cloner animation to UE Geometry Cache asset.
	 * Creates a playable vertex animation cache.
	 */
	static FKClonerBakeResult BakeToGeometryCache(
		AKClonerActor* ClonerActor,
		const FString& PackageName,
		const FKClonerGeometryCacheBakeOptions& Options = FKClonerGeometryCacheBakeOptions());
	
	//===========================================================================
	// SKELETAL MESH (existing)
	//===========================================================================
	
	static USkeletalMesh* BakeToSkeletalMesh(AKClonerActor* ClonerActor, const FString& PackageName);
	static UAnimSequence* BakeToAnimSequence(AKClonerActor* ClonerActor, USkeletalMesh* TargetMesh, const FString& PackageName, float Duration, float FrameRate = 30.0f);

private:
	//===========================================================================
	// INTERNAL HELPERS
	//===========================================================================
	
	/** Collect all instance transforms at a given time */
	static TArray<FTransform> SampleInstanceTransforms(AKClonerActor* ClonerActor, float Time);
	
	/** Merge multiple mesh descriptions with transforms */
	static bool MergeMeshInstances(
		const FMeshDescription& SourceMesh,
		const TArray<FTransform>& Transforms,
		FMeshDescription& OutMergedMesh);
	
	/** Write Alembic file using the Alembic library */
	static bool WriteAlembicFile(
		const FString& FilePath,
		const TArray<TArray<FVector>>& FramePositions,
		const TArray<TArray<FVector>>& FrameNormals,
		const TArray<int32>& Indices,
		const TArray<FVector2D>& UVs,
		float FrameRate);
};
