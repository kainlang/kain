// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SAlphaGenWidget.h" // For EProceduralType

class UTexture2D;

/**
 * FAlphaProceduralGenerator
 * 
 * Static class for generating procedural alpha textures.
 * Ported from K-OS DCC Rust procedural.rs implementation.
 * 
 * Supports generation of:
 * - Radial falloff (soft brush)
 * - Hard shapes (circle, square, diamond)
 * - Noise patterns (perlin, voronoi)
 * - Tiling patterns (bricks, dots)
 */
class FAlphaProceduralGenerator
{
public:
	/**
	 * Generate a procedural alpha texture
	 * 
	 * @param Type The procedural type to generate
	 * @param Size Output texture size (clamped to 16-4096)
	 * @param Params Generator-specific parameters (matching Rust HashMap<String, f32>)
	 * @return Generated UTexture2D or nullptr on failure
	 */
	static UTexture2D* Generate(
		EProceduralType Type,
		int32 Size,
		const TMap<FString, float>& Params
	);

private:
	// Individual generators matching Rust implementations
	
	/** Radial falloff - standard soft brush */
	static void GenerateRadial(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params);
	
	/** Hard circle with optional edge softness */
	static void GenerateCircle(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params);
	
	/** Square shape with optional edge softness */
	static void GenerateSquare(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params);
	
	/** Diamond shape (rotated square) */
	static void GenerateDiamond(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params);
	
	/** Perlin noise with octaves (fractal brownian motion) */
	static void GeneratePerlin(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params);
	
	/** Voronoi cells for scales/cracks patterns */
	static void GenerateVoronoi(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params);
	
	/** Brick/tile pattern */
	static void GenerateBricks(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params);
	
	/** Dot pattern (polka dots) */
	static void GenerateDots(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params);
	
	// Noise helper functions (ported from proceduralcommon.ush / Rust)
	
	/** 2D hash function */
	static float Hash21(FVector2f P);
	
	/** 2D hash returning 2D vector */
	static FVector2f Hash22(FVector2f P);
	
	/** 2D gradient noise */
	static float Grad2(FVector2f P);
	
	/** Fractal Brownian Motion */
	static float FBM(FVector2f P, int32 Octaves, float Lacunarity, float Persistence);
	
	/** Create texture from grayscale pixel data */
	static UTexture2D* CreateTextureFromPixels(const TArray<uint8>& Pixels, int32 Size);
};
