// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"

class AKClonerActor;
class UMaterialInterface;

/**
 * VAT (Vertex Animation Texture) precision levels
 */
UENUM()
enum class EKClonerVATPrecision : uint8
{
	Low		UMETA(DisplayName = "Low (8-bit)"),
	High	UMETA(DisplayName = "High (16-bit)"),
	Ultra	UMETA(DisplayName = "Ultra (32-bit HDR)")
};

/**
 * Result of VAT baking operation
 */
struct FKClonerVATResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	
	// Generated assets
	UTexture2D* PositionTexture = nullptr;
	UTexture2D* NormalTexture = nullptr;
	UTexture2D* RotationTexture = nullptr;
	UStaticMesh* OutputMesh = nullptr;
	UMaterialInterface* VATMaterial = nullptr;
	
	// Metadata for shader
	FVector BoundsMin = FVector::ZeroVector;
	FVector BoundsSize = FVector::OneVector;
	int32 TotalFrames = 0;
	int32 InstanceCount = 0;
	float FrameRate = 30.0f;
	int32 RowsPerFrame = 1;
};

/**
 * Options for VAT baking
 */
struct FKClonerVATOptions
{
	// Duration in seconds to capture
	float Duration = 5.0f;
	
	// Frames per second to sample
	float FrameRate = 30.0f;
	
	// Texture precision
	EKClonerVATPrecision Precision = EKClonerVATPrecision::High;
	
	// Max texture dimensions
	int32 MaxWidth = 4096;
	int32 MaxHeight = 4096;
	
	// Whether to bake normals (false = position only, lighter)
	bool bBakeNormals = true;
	
	// Whether to bake rotation per instance
	bool bBakeRotation = true;
	
	// UV channel to store instance ID mapping
	int32 UVChannel = 1;
	
	// Output package path (auto-generated if empty)
	FString PackagePath;
};

/**
 * Utility class for baking K-Cloner animations to Vertex Animation Textures
 */
class KCLONEREDITOR_API FKClonerVATUtils
{
public:
	/**
	 * Main entry point: Bakes a K-Cloner actor's modifier animation to VAT textures.
	 * 
	 * @param ClonerActor	The cloner actor to bake
	 * @param Options		Baking options
	 * @return				Result containing generated textures, mesh, and metadata
	 */
	static FKClonerVATResult BakeToVAT(AKClonerActor* ClonerActor, const FKClonerVATOptions& Options);

private:
	/**
	 * Samples all instance transforms at a specific time
	 */
	static TArray<FTransform> SampleInstanceTransforms(AKClonerActor* ClonerActor, float Time);
	
	/**
	 * Creates the position texture from sampled transforms
	 */
	static UTexture2D* CreatePositionTexture(
		const TArray<TArray<FTransform>>& FrameData,
		int32 InstanceCount,
		int32 TotalFrames,
		const FVector& BoundsMin,
		const FVector& BoundsSize,
		EKClonerVATPrecision Precision,
		int32 MaxWidth,
		int32 MaxHeight,
		const FString& PackagePath);
	
	/**
	 * Creates the rotation texture from sampled transforms
	 */
	static UTexture2D* CreateRotationTexture(
		const TArray<TArray<FTransform>>& FrameData,
		int32 InstanceCount,
		int32 TotalFrames,
		EKClonerVATPrecision Precision,
		int32 MaxWidth,
		int32 MaxHeight,
		const FString& PackagePath);
	
	/**
	 * Creates a static mesh with UV1 containing instance ID lookup
	 */
	static UStaticMesh* CreateOutputMesh(
		AKClonerActor* ClonerActor,
		int32 UVChannel,
		const FString& PackagePath);
	
	/**
	 * Calculates texture dimensions to fit all data
	 */
	static FIntPoint CalculateTextureDimensions(
		int32 InstanceCount,
		int32 TotalFrames,
		int32 MaxWidth,
		int32 MaxHeight,
		int32& OutRowsPerFrame);
	
	/**
	 * Gets the pixel format for the given precision
	 */
	static EPixelFormat GetPixelFormat(EKClonerVATPrecision Precision);
	
	/**
	 * Gets the texture source format for the given precision
	 */
	static ETextureSourceFormat GetTextureSourceFormat(EKClonerVATPrecision Precision);
	
	/**
	 * Normalizes a position to 0-1 range within bounds
	 */
	static FVector NormalizePosition(const FVector& Position, const FVector& BoundsMin, const FVector& BoundsSize);
	
	/**
	 * Encodes a quaternion rotation to 0-1 range
	 */
	static FVector4 EncodeRotation(const FQuat& Rotation);

public:
	/**
	 * Creates a complete VAT material instance with all parameters properly configured.
	 * This is the auto-material generation feature.
	 * 
	 * @param PositionTexture	Baked position texture
	 * @param RotationTexture	Baked rotation texture (optional)
	 * @param BoundsMin			Minimum bounds from bake
	 * @param BoundsSize		Size of bounds from bake
	 * @param TotalFrames		Total animation frames
	 * @param FrameRate			Animation frame rate
	 * @param PackagePath		Where to save the material
	 * @return					Created material instance
	 */
	static UMaterialInstanceDynamic* CreateVATMaterial(
		UTexture2D* PositionTexture,
		UTexture2D* RotationTexture,
		const FVector& BoundsMin,
		const FVector& BoundsSize,
		int32 TotalFrames,
		float FrameRate,
		UObject* Outer);

	/**
	 * Creates and saves a VAT material asset to disk.
	 */
	static UMaterialInterface* CreateAndSaveVATMaterial(
		UTexture2D* PositionTexture,
		UTexture2D* RotationTexture,
		const FVector& BoundsMin,
		const FVector& BoundsSize,
		int32 TotalFrames,
		float FrameRate,
		const FString& PackagePath,
		int32 InstanceCount);

	/**
	 * Gets or creates the base VAT material from plugin content.
	 */
	static UMaterial* GetOrCreateBaseVATMaterial();
};
