// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "AlphaTexturePool.generated.h"

#include "EAlphaSource.h"
#include "FAlphaInfo.h"

/**
 * Loaded alpha texture with GPU resources (matches Rust AlphaTexture struct)
 */
struct FAlphaTexture
{
	/** The GPU texture */
	TWeakObjectPtr<UTexture2D> Texture;
	
	/** Texture dimensions */
	int32 Width = 0;
	int32 Height = 0;
	
	/** Human-readable name */
	FString Name;
	
	/** Source info */
	EAlphaSource Source = EAlphaSource::Procedural;
	
	/** For file sources - the original path */
	FString SourcePath;
	
	/** For procedural - the generator type */
	FString GeneratorType;
	
	/** For procedural - the parameters used */
	TMap<FString, float> GeneratorParams;
	
	/** Preview thumbnail texture */
	TWeakObjectPtr<UTexture2D> ThumbnailTexture;
	
	/** Is this texture valid */
	bool IsValid() const { return Texture.IsValid(); }
};

/**
 * UAlphaTexturePool
 * 
 * Singleton subsystem for managing alpha textures.
 * Ported from K-OS DCC alpha_pool.rs implementation.
 */
UCLASS()
class UAlphaTexturePool : public UObject
{
	GENERATED_BODY()
	
public:
	/** Get the singleton instance */
	static UAlphaTexturePool* Get();
	
	/** Load an alpha from file */
	int64 LoadFromFile(const FString& Path, const FString& Name = TEXT(""));
	
	/** Load an alpha from raw bytes (PNG/TGA/etc data) */
	int64 LoadFromBytes(const TArray<uint8>& Bytes, const FString& Name, EAlphaSource Source = EAlphaSource::Embedded);
	
	/** Load from an already-created texture */
	int64 LoadFromTexture(UTexture2D* Texture, const FString& Name, EAlphaSource Source = EAlphaSource::Procedural);
	
	/** Get an alpha texture by handle */
	FAlphaTexture* Get(int64 Handle);
	
	/** Get alpha info by handle */
	FAlphaInfo GetInfo(int64 Handle) const;
	
	/** List all loaded alphas */
	TArray<FAlphaInfo> List() const;
	
	/** Dispose an alpha texture */
	bool Dispose(int64 Handle);
	
	/** Clear all loaded alphas */
	void Clear();
	
private:
	/** Loaded textures by handle (matching Rust HashMap structure) */
	TMap<int64, FAlphaTexture> Textures;
	
	/** Metadata for each texture */
	TMap<int64, FAlphaInfo> Metadata;
	
	/** Next handle to assign */
	int64 NextHandle = 1;
	
	/** Whether the pool is initialized */
	bool bInitialized = false;
	
	/** Generate a 64x64 preview thumbnail */
	UTexture2D* GenerateThumbnail(UTexture2D* Source);
	
	/** Singleton instance */
	static UAlphaTexturePool* Instance;
};
