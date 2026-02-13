// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "AlphaGenVersionCompat.h"

// Generator type enum matching the shader defines
enum class EAlphaGenType : uint8
{
	Radial = 0,
	Circle = 1,
	Square = 2,
	Diamond = 3,
	Perlin = 4,
	Voronoi = 5,
	Bricks = 6,
	Dots = 7,
	SeamlessNoise = 8,
	Crosshatch = 9,
	Waves = 10,
	Checkerboard = 11,
	Hexagon = 12,
	Tears = 13,
	Scratches = 14,
	Splatter = 15,
	Cracks = 16,
	Cells = 17,
	Grunge = 18,
	Fibers = 19,
	Caustics = 20
};

// Base shader class with common parameters
class FAlphaGenShaderBase : public FGlobalShader
{
public:
	DECLARE_INLINE_TYPE_LAYOUT(FAlphaGenShaderBase, NonVirtual);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
		SHADER_PARAMETER(FUintVector2, TextureSize)
		SHADER_PARAMETER(FVector4f, ProceduralParams)  // Falloff/Scale, EdgeSoftness/Octaves, Seed, Unused
		SHADER_PARAMETER(FVector4f, ProceduralParams2) // Width, Height, Mortar/Spacing, Unused
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

// Individual compute shader declarations for each generator type
class FAlphaGenRadialCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenRadialCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenRadialCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenCircleCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenCircleCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenCircleCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenSquareCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenSquareCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenSquareCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenDiamondCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenDiamondCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenDiamondCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenPerlinCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenPerlinCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenPerlinCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenVoronoiCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenVoronoiCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenVoronoiCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenBricksCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenBricksCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenBricksCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenDotsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenDotsCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenDotsCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenSeamlessNoiseCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenSeamlessNoiseCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenSeamlessNoiseCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenCrosshatchCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenCrosshatchCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenCrosshatchCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenWavesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenWavesCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenWavesCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenCheckerboardCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenCheckerboardCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenCheckerboardCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenHexagonCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenHexagonCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenHexagonCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenTearsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenTearsCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenTearsCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenScratchesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenScratchesCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenScratchesCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenSplatterCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenSplatterCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenSplatterCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenCracksCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenCracksCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenCracksCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenCellsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenCellsCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenCellsCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenGrungeCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenGrungeCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenGrungeCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenFibersCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenFibersCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenFibersCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FAlphaGenCausticsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaGenCausticsCS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaGenCausticsCS, FGlobalShader);
	using FParameters = FAlphaGenShaderBase::FParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

// ============================================================================
// FILTER SYSTEM - Post-processing passes applied after generation
// ============================================================================

enum class EAlphaGenFilterType : uint8
{
	Blur = 0,
	Sharpen = 1,
	Invert = 2,
	Dilate = 3,
	Erode = 4,
	Threshold = 5,
	Posterize = 6,
	Pixelate = 7,
	DomainWarp = 8,
	Spherize = 9,
	EdgeDetect = 10,
	Levels = 11,
	Contrast = 12,
	Spiral = 13
};

// Filter shader base with input/output texture ping-pong
class FAlphaGenFilterBase
{
public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, FilterInputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, FilterInputSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, FilterOutputTexture)
		SHADER_PARAMETER(FVector4f, FilterParams)
		SHADER_PARAMETER(FVector4f, FilterParams2)
		SHADER_PARAMETER(FUintVector2, FilterTextureSize)
	END_SHADER_PARAMETER_STRUCT()
	
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// Filter shader declarations - all use the same parameter struct
#define DECLARE_FILTER_SHADER(ShaderName) \
class ShaderName : public FGlobalShader \
{ \
public: \
	DECLARE_GLOBAL_SHADER(ShaderName); \
	SHADER_USE_PARAMETER_STRUCT(ShaderName, FGlobalShader); \
	using FParameters = FAlphaGenFilterBase::FParameters; \
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenFilterBase::ShouldCompilePermutation(Parameters); } \
};

DECLARE_FILTER_SHADER(FAlphaGenBlurCS);
DECLARE_FILTER_SHADER(FAlphaGenSharpenCS);
DECLARE_FILTER_SHADER(FAlphaGenInvertCS);
DECLARE_FILTER_SHADER(FAlphaGenDilateCS);
DECLARE_FILTER_SHADER(FAlphaGenErodeCS);
DECLARE_FILTER_SHADER(FAlphaGenThresholdCS);
DECLARE_FILTER_SHADER(FAlphaGenPosterizeCS);
DECLARE_FILTER_SHADER(FAlphaGenPixelateCS);
DECLARE_FILTER_SHADER(FAlphaGenDomainWarpCS);
DECLARE_FILTER_SHADER(FAlphaGenSpherizeCS);
DECLARE_FILTER_SHADER(FAlphaGenEdgeDetectCS);
DECLARE_FILTER_SHADER(FAlphaGenLevelsCS);
DECLARE_FILTER_SHADER(FAlphaGenContrastCS);
DECLARE_FILTER_SHADER(FAlphaGenSpiralCS);
DECLARE_FILTER_SHADER(FAlphaGenCopyCS);  // Simple copy for non-destructive pipeline

#undef DECLARE_FILTER_SHADER

// Filter chain parameters - holds ALL filter settings for single-pass execution
struct FAlphaGenFilterChainParams
{
	// Levels & Contrast (applied first for proper range adjustment)
	bool bLevelsEnabled = false;
	float LevelsBlack = 0.0f;
	float LevelsWhite = 1.0f;
	float LevelsGamma = 1.0f;
	float Contrast = 1.0f;
	
	// Blur & Sharpen
	float BlurRadius = 0.0f;
	float SharpenStrength = 0.0f;
	
	// Morphology
	float DilateRadius = 0.0f;
	float ErodeRadius = 0.0f;
	
	// Quantize
	bool bPosterizeEnabled = false;
	float PosterizeSteps = 4.0f;
	bool bThresholdEnabled = false;
	float Threshold = 0.5f;
	float PixelateSize = 1.0f;
	
	// Distortion
	float DomainWarpStrength = 0.0f;
	float DomainWarpScale = 8.0f;
	float SpherizeAmount = 0.0f;
	float SpiralTwist = 0.0f;
	
	// Edge & Invert (applied last)
	float EdgeDetectStrength = 0.0f;
	bool bInvert = false;
	
	// Returns true if any filter is actually enabled/active
	bool HasActiveFilters() const
	{
		return bLevelsEnabled || FMath::Abs(Contrast - 1.0f) > 0.01f ||
			BlurRadius > 0.0f || SharpenStrength > 0.0f ||
			DilateRadius > 0.0f || ErodeRadius > 0.0f ||
			bPosterizeEnabled || bThresholdEnabled || PixelateSize > 1.0f ||
			DomainWarpStrength > 0.0f || FMath::Abs(SpherizeAmount) > 0.01f || FMath::Abs(SpiralTwist) > 0.01f ||
			EdgeDetectStrength > 0.0f || bInvert;
	}
};

// Compute engine interface for dispatching GPU generation
class ALPHAGENEDITOR_API FAlphaGenComputeEngine
{
public:
	// Generate an alpha texture on the GPU
	static UTexture2D* GenerateGPU(
		EAlphaGenType Type,
		int32 Size,
		float Param1,     // Falloff/Scale
		float Param2,     // EdgeSoftness/Octaves
		float Param3,     // Seed
		float PatternWidth = 0.25f,
		float PatternHeight = 0.1f,
		float PatternMortar = 0.02f,
		float PatternNoiseType = 0.0f  // Noise type for Perlin: 0=perlin, 1=simplex, 2=ridged, 3=billowy, 4=worley
	);
	
	// Apply entire filter chain in ONE RDG execution - no intermediate copies
	static UTexture2D* ApplyFilterChain(
		UTexture2D* InputTexture,
		const FAlphaGenFilterChainParams& Params,
		UTexture2D* ExistingTexture = nullptr
	);
	
	// Legacy single filter (kept for backwards compat, internally uses chain)
	static UTexture2D* ApplyFilter(
		UTexture2D* InputTexture,
		EAlphaGenFilterType FilterType,
		float Param1 = 0.0f,
		float Param2 = 0.0f,
		float Param3 = 0.0f,
		float Param4 = 0.0f
	);

private:
	// Internal RDG dispatch
	template<typename TShaderClass>
	static void DispatchGeneration(
		FRHICommandListImmediate& RHICmdList,
		ALPHAGEN_TEXTURE2D_RHI_TYPE OutputRHI,
		int32 Width, int32 Height,
		const FAlphaGenShaderBase::FParameters& Params
	);
};

