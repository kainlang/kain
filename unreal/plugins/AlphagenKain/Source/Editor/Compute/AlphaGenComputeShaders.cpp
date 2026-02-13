// Copyright 2026 K-Studio. All Rights Reserved.

#include "Compute/AlphaGenComputeShaders.h"
#include "AlphaGenVersionCompat.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "TextureResource.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "DataDrivenShaderPlatformInfo.h"

// Shader compilation permission
bool FAlphaGenShaderBase::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return FDataDrivenShaderPlatformInfo::GetMaxFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5;
}

// All shader classes use the same permission check
bool FAlphaGenRadialCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenCircleCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenSquareCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenDiamondCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenPerlinCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenVoronoiCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenBricksCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenDotsCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenSeamlessNoiseCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenCrosshatchCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenWavesCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenCheckerboardCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenHexagonCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenTearsCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenScratchesCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenSplatterCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenCracksCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenCellsCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenGrungeCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenFibersCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }
bool FAlphaGenCausticsCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return FAlphaGenShaderBase::ShouldCompilePermutation(Parameters); }

// Shader implementations linking to USF entry points
IMPLEMENT_GLOBAL_SHADER(FAlphaGenRadialCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateRadialCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenCircleCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateCircleCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenSquareCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateSquareCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenDiamondCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateDiamondCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenPerlinCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GeneratePerlinCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenVoronoiCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateVoronoiCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenBricksCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateBricksCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenDotsCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateDotsCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenSeamlessNoiseCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateSeamlessNoiseCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenCrosshatchCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateCrosshatchCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenWavesCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateWavesCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenCheckerboardCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateCheckerboardCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenHexagonCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateHexagonCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenTearsCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateTearsCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenScratchesCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateScratchesCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenSplatterCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateSplatterCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenCracksCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateCracksCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenCellsCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateCellsCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenGrungeCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateGrungeCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenFibersCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateFibersCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenCausticsCS, "/Plugin/AlphaGen/Private/AlphaGenProcedural.usf", "GenerateCausticsCS", SF_Compute);

// Filter shader implementations - all use same parameter struct, pointing to AlphaGenFilters.usf
IMPLEMENT_GLOBAL_SHADER(FAlphaGenBlurCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "BlurCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenSharpenCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "SharpenCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenInvertCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "InvertCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenDilateCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "DilateCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenErodeCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "ErodeCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenThresholdCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "ThresholdCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenPosterizeCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "PosterizeCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenPixelateCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "PixelateCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenDomainWarpCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "DomainWarpCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenSpherizeCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "SpherizeCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenEdgeDetectCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "EdgeDetectCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenLevelsCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "LevelsCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenContrastCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "ContrastCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenSpiralCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "SpiralCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAlphaGenCopyCS, "/Plugin/AlphaGen/Private/AlphaGenFilters.usf", "CopyCS", SF_Compute);

UTexture2D* FAlphaGenComputeEngine::GenerateGPU(
	EAlphaGenType Type,
	int32 Size,
	float Param1,
	float Param2,
	float Param3,
	float PatternWidth,
	float PatternHeight,
	float PatternMortar,
	float PatternNoiseType)
{
	// Create output texture - bypass streaming pool to prevent eviction
	UTexture2D* OutputTexture = UTexture2D::CreateTransient(Size, Size, PF_R8G8B8A8);
	if (!OutputTexture)
	{
		return nullptr;
	}
	
	// Critical: Disable streaming to prevent pool eviction
	OutputTexture->NeverStream = true;
	OutputTexture->CompressionSettings = TC_VectorDisplacementmap;
	OutputTexture->SRGB = false;
	OutputTexture->Filter = TF_Bilinear;
	OutputTexture->LODGroup = TEXTUREGROUP_Pixels2D;
	OutputTexture->UpdateResource();
	
	// Wait for GPU initialization
	FlushRenderingCommands();
	
	FTextureResource* Resource = OutputTexture->GetResource();
	if (!Resource || !Resource->GetTexture2DRHI())
	{
		UE_LOG(LogTemp, Error, TEXT("AlphaGen: Failed to create output texture RHI"));
		return nullptr;
	}
	
	ALPHAGEN_TEXTURE2D_RHI_TYPE OutputRHI = ALPHAGEN_GET_TEXTURE2D_RHI(Resource);
	
	// Capture parameters for render thread
	int32 Width = Size;
	int32 Height = Size;
	FVector4f ProceduralParams(Param1, Param2, Param3, 0.0f);
	FVector4f ProceduralParams2(PatternWidth, PatternHeight, PatternMortar, PatternNoiseType);
	
	ENQUEUE_RENDER_COMMAND(AlphaGenCompute)(
		[OutputRHI, Type, Width, Height, ProceduralParams, ProceduralParams2](FRHICommandListImmediate& RHICmdList)
		{
			if (!ALPHAGEN_IS_VALID_RHI(OutputRHI))
			{
				return;
			}
			
			FRDGBuilder GraphBuilder(RHICmdList);
			
			// Create output RDG texture
			FRDGTextureDesc OutDesc = FRDGTextureDesc::Create2D(
				FIntPoint(Width, Height),
				PF_R8G8B8A8,
				FClearValueBinding::Black,
				TexCreate_UAV | TexCreate_ShaderResource | TexCreate_RenderTargetable
			);
			FRDGTextureRef OutputRDG = GraphBuilder.CreateTexture(OutDesc, TEXT("AlphaGenOutput"));
			
			// Calculate thread group count
			FIntVector GroupCount(FMath::DivideAndRoundUp(Width, 8), FMath::DivideAndRoundUp(Height, 8), 1);
			
			// Dispatch appropriate shader based on type
			switch (Type)
			{
				case EAlphaGenType::Radial:
				{
					TShaderMapRef<FAlphaGenRadialCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenRadialCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenRadialCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Radial"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Circle:
				{
					TShaderMapRef<FAlphaGenCircleCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenCircleCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenCircleCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Circle"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Square:
				{
					TShaderMapRef<FAlphaGenSquareCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenSquareCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenSquareCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Square"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Diamond:
				{
					TShaderMapRef<FAlphaGenDiamondCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenDiamondCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenDiamondCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Diamond"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Perlin:
				{
					TShaderMapRef<FAlphaGenPerlinCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenPerlinCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenPerlinCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Perlin"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Voronoi:
				{
					TShaderMapRef<FAlphaGenVoronoiCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenVoronoiCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenVoronoiCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Voronoi"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Bricks:
				{
					TShaderMapRef<FAlphaGenBricksCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenBricksCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenBricksCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Bricks"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Dots:
				{
					TShaderMapRef<FAlphaGenDotsCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenDotsCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenDotsCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Dots"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::SeamlessNoise:
				{
					TShaderMapRef<FAlphaGenSeamlessNoiseCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenSeamlessNoiseCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenSeamlessNoiseCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_SeamlessNoise"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Crosshatch:
				{
					TShaderMapRef<FAlphaGenCrosshatchCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenCrosshatchCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenCrosshatchCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Crosshatch"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Waves:
				{
					TShaderMapRef<FAlphaGenWavesCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenWavesCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenWavesCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Waves"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Checkerboard:
				{
					TShaderMapRef<FAlphaGenCheckerboardCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenCheckerboardCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenCheckerboardCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Checkerboard"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Hexagon:
				{
					TShaderMapRef<FAlphaGenHexagonCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenHexagonCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenHexagonCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Hexagon"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Tears:
				{
					TShaderMapRef<FAlphaGenTearsCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenTearsCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenTearsCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Tears"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Scratches:
				{
					TShaderMapRef<FAlphaGenScratchesCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenScratchesCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenScratchesCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Scratches"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Splatter:
				{
					TShaderMapRef<FAlphaGenSplatterCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenSplatterCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenSplatterCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Splatter"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Cracks:
				{
					TShaderMapRef<FAlphaGenCracksCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenCracksCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenCracksCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Cracks"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Cells:
				{
					TShaderMapRef<FAlphaGenCellsCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenCellsCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenCellsCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Cells"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Grunge:
				{
					TShaderMapRef<FAlphaGenGrungeCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenGrungeCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenGrungeCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Grunge"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Fibers:
				{
					TShaderMapRef<FAlphaGenFibersCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenFibersCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenFibersCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Fibers"), Shader, Params, GroupCount);
					break;
				}
				case EAlphaGenType::Caustics:
				{
					TShaderMapRef<FAlphaGenCausticsCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					FAlphaGenCausticsCS::FParameters* Params = GraphBuilder.AllocParameters<FAlphaGenCausticsCS::FParameters>();
					Params->OutputTexture = GraphBuilder.CreateUAV(OutputRDG);
					Params->TextureSize = FUintVector2(Width, Height);
					Params->ProceduralParams = ProceduralParams;
					Params->ProceduralParams2 = ProceduralParams2;
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_Caustics"), Shader, Params, GroupCount);
					break;
				}
			}
			
			// Extract result
			TRefCountPtr<IPooledRenderTarget> ExtractedOutput;
			GraphBuilder.QueueTextureExtraction(OutputRDG, &ExtractedOutput);
			
			// Execute the graph
			GraphBuilder.Execute();
			
			// Copy result to output texture
			if (ExtractedOutput.IsValid() && ALPHAGEN_IS_VALID_RHI(OutputRHI))
			{
				RHICmdList.Transition(FRHITransitionInfo(OutputRHI, ERHIAccess::Unknown, ERHIAccess::CopyDest));
				FRHICopyTextureInfo CopyInfo;
				RHICmdList.CopyTexture(ExtractedOutput->GetRHI(), OutputRHI, CopyInfo);
				RHICmdList.Transition(FRHITransitionInfo(OutputRHI, ERHIAccess::CopyDest, ERHIAccess::SRVGraphics));
			}
		}
	);
	
	// Wait for GPU to complete
	FlushRenderingCommands();
	
	return OutputTexture;
}

// ============================================================================
// FILTER CHAIN - Applies ALL filters in one RDG graph with ping-pong textures
// This avoids the copy corruption from individual filter applications
// ============================================================================

UTexture2D* FAlphaGenComputeEngine::ApplyFilterChain(
	UTexture2D* InputTexture,
	const FAlphaGenFilterChainParams& Params,
	UTexture2D* ExistingTexture)
{
	if (!InputTexture || !InputTexture->GetResource())
	{
		return nullptr;
	}
	
	// Early out if no filters active - just return the input directly
	if (!Params.HasActiveFilters())
	{
		return InputTexture;
	}
	
	// Ensure GPU resources are initialized
	InputTexture->UpdateResource();
	FlushRenderingCommands();
	
	FTextureResource* Resource = InputTexture->GetResource();
	if (!Resource)
	{
		UE_LOG(LogTemp, Warning, TEXT("AlphaGen: No texture resource for filter chain"));
		return InputTexture;
	}
	
	ALPHAGEN_TEXTURE2D_RHI_TYPE InputRHI = ALPHAGEN_GET_TEXTURE2D_RHI(Resource);
	if (!ALPHAGEN_IS_VALID_RHI(InputRHI))
	{
		UE_LOG(LogTemp, Warning, TEXT("AlphaGen: Invalid RHI resource for filter chain"));
		return InputTexture;
	}
	
	int32 Width = InputTexture->GetSizeX();
	int32 Height = InputTexture->GetSizeY();
	FIntPoint Size(Width, Height);
	
	// Reuse existing texture if valid, otherwise create new
	UTexture2D* OutputTexture = ExistingTexture;
	bool bCreateNew = true;
	
	if (OutputTexture && OutputTexture->IsValidLowLevel() && 
		OutputTexture->GetSizeX() == Width && OutputTexture->GetSizeY() == Height &&
		OutputTexture->GetPixelFormat() == PF_R8G8B8A8 &&
		OutputTexture->GetResource() && OutputTexture->GetResource()->GetTexture2DRHI())
	{
		bCreateNew = false;
	}
	
	if (bCreateNew)
	{
		// Create output texture using NewObject with RF_Transient to avoid streaming pool entirely
		// The streaming pool eviction was causing the gray background glitch when "TEXTURE STREAMING POOL OVER BUDGET" appeared
		OutputTexture = NewObject<UTexture2D>(GetTransientPackage(), NAME_None, RF_Transient);
		if (!OutputTexture)
		{
			UE_LOG(LogTemp, Error, TEXT("AlphaGen: Failed to create output texture for filter chain"));
			return InputTexture;
		}
		
		// Initialize platform data manually (bypasses streaming system)
		FTexturePlatformData* PlatformData = new FTexturePlatformData();
		PlatformData->SizeX = Width;
		PlatformData->SizeY = Height;
		PlatformData->PixelFormat = PF_R8G8B8A8;
		
		// Add single mip level
		FTexture2DMipMap* Mip = new FTexture2DMipMap();
		Mip->SizeX = Width;
		Mip->SizeY = Height;
		Mip->BulkData.Lock(LOCK_READ_WRITE);
		void* Data = Mip->BulkData.Realloc(Width * Height * 4);
		FMemory::Memzero(Data, Width * Height * 4);
		Mip->BulkData.Unlock();
		PlatformData->Mips.Add(Mip);
		
		OutputTexture->SetPlatformData(PlatformData);
		
		// Critical: Disable streaming to prevent pool eviction
		OutputTexture->NeverStream = true;
		OutputTexture->CompressionSettings = TC_VectorDisplacementmap; // No compression, keeps raw RGBA
		OutputTexture->SRGB = false;
		OutputTexture->Filter = TF_Bilinear;
		OutputTexture->LODGroup = TEXTUREGROUP_Pixels2D; // Persistent, non-streaming group
		OutputTexture->UpdateResource();
		FlushRenderingCommands();
	}
	
	FTextureResource* OutputResource = OutputTexture->GetResource();
	if (!OutputResource || !OutputResource->GetTexture2DRHI())
	{
		UE_LOG(LogTemp, Error, TEXT("AlphaGen: Failed to get output RHI for filter chain"));
		return InputTexture;
	}
	
	ALPHAGEN_TEXTURE2D_RHI_TYPE OutputRHI = ALPHAGEN_GET_TEXTURE2D_RHI(OutputResource);
	
	// Capture params for render thread (copy the struct)
	FAlphaGenFilterChainParams FilterParams = Params;
	
	ENQUEUE_RENDER_COMMAND(ApplyAlphaGenFilterChain)(
		[InputRHI, OutputRHI, Width, Height, Size, FilterParams](FRHICommandListImmediate& RHICmdList)
		{
			if (!ALPHAGEN_IS_VALID_RHI(InputRHI) || !ALPHAGEN_IS_VALID_RHI(OutputRHI))
			{
				UE_LOG(LogTemp, Warning, TEXT("AlphaGen: RHI resources invalid in filter chain"));
				return;
			}
			
			// Let RDG handle transitions for the input texture
			// (removed manual Unknown->SRVCompute transition - risky on DX12/Vulkan)
			
			FRDGBuilder GraphBuilder(RHICmdList);
			
			// Create TWO ping-pong textures - use Transparent like KSample, not Black
			FRDGTextureDesc PingPongDesc = FRDGTextureDesc::Create2D(
				Size,
				PF_R8G8B8A8,
				FClearValueBinding::Transparent,  // KSample pattern - more robust
				TexCreate_UAV | TexCreate_ShaderResource | TexCreate_RenderTargetable
			);
			FRDGTextureRef PingTexture = GraphBuilder.CreateTexture(PingPongDesc, TEXT("FilterPing"));
			FRDGTextureRef PongTexture = GraphBuilder.CreateTexture(PingPongDesc, TEXT("FilterPong"));
			
			// Register the input texture (RDG handles state for external textures)
			FRDGTextureRef InputRDG = GraphBuilder.RegisterExternalTexture(
				CreateRenderTarget(InputRHI, TEXT("FilterChainInput"))
			);
			
			// Use GetGroupCount like KSample for proper thread group calculation
			FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(Size, FIntPoint(8, 8));
			
			// Copy input to ping buffer using shader-based copy (more reliable than AddCopyTexturePass)
			// AddCopyTexturePass was causing issues between external and internal RDG textures
			{
				TShaderMapRef<FAlphaGenCopyCS> CopyShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
				FAlphaGenCopyCS::FParameters* CopyParams = GraphBuilder.AllocParameters<FAlphaGenCopyCS::FParameters>();
				CopyParams->FilterInputTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(InputRDG));
				CopyParams->FilterInputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				CopyParams->FilterOutputTexture = GraphBuilder.CreateUAV(PingTexture);
				CopyParams->FilterParams = FVector4f(0, 0, 0, 0);
				CopyParams->FilterParams2 = FVector4f(0, 0, 0, 0);
				CopyParams->FilterTextureSize = FUintVector2(Width, Height);
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("AlphaGen_CopyInput"), CopyShader, CopyParams, GroupCount);
			}
			
			// Now PingTexture has valid RGBA8 data (even if input was G8)
			FRDGTextureRef CurrentSource = PingTexture;
			FRDGTextureRef CurrentDest = PongTexture;
			bool bAnyFilterRan = true;  // We ran the format cast, so we have valid data
			
			// Helper macro to dispatch filter and swap ping-pong buffers
			#define ADD_FILTER_PASS(ShaderClass, EventName, P1, P2, P3, P4) \
			{ \
				TShaderMapRef<ShaderClass> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel)); \
				ShaderClass::FParameters* Params = GraphBuilder.AllocParameters<ShaderClass::FParameters>(); \
				Params->FilterInputTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(CurrentSource)); \
				Params->FilterInputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(); \
				Params->FilterOutputTexture = GraphBuilder.CreateUAV(CurrentDest); \
				Params->FilterParams = FVector4f(P1, P2, P3, P4); \
				Params->FilterParams2 = FVector4f(0, 0, 0, 0); \
				Params->FilterTextureSize = FUintVector2(Width, Height); \
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME(EventName), Shader, Params, GroupCount); \
				FRDGTextureRef Temp = CurrentSource; CurrentSource = CurrentDest; CurrentDest = Temp; \
				bAnyFilterRan = true; \
			}
			
			// Apply filters in order (matches the order in RefreshPreview)
			
			// 1. Levels 
			if (FilterParams.bLevelsEnabled && (FilterParams.LevelsBlack > 0.0f || FilterParams.LevelsWhite < 1.0f || FMath::Abs(FilterParams.LevelsGamma - 1.0f) > 0.01f))
			{
				ADD_FILTER_PASS(FAlphaGenLevelsCS, "AlphaGen_Levels", FilterParams.LevelsBlack, FilterParams.LevelsWhite, FilterParams.LevelsGamma, 0);
			}
			
			// 2. Contrast
			if (FMath::Abs(FilterParams.Contrast - 1.0f) > 0.01f)
			{
				ADD_FILTER_PASS(FAlphaGenContrastCS, "AlphaGen_Contrast", FilterParams.Contrast, 0, 0, 0);
			}
			
			// 3. Blur
			if (FilterParams.BlurRadius > 0.0f)
			{
				ADD_FILTER_PASS(FAlphaGenBlurCS, "AlphaGen_Blur", FilterParams.BlurRadius, 0, 0, 0);
			}
			
			// 4. Sharpen
			if (FilterParams.SharpenStrength > 0.0f)
			{
				ADD_FILTER_PASS(FAlphaGenSharpenCS, "AlphaGen_Sharpen", FilterParams.SharpenStrength, 0, 0, 0);
			}
			
			// 5. Dilate
			if (FilterParams.DilateRadius > 0.0f)
			{
				ADD_FILTER_PASS(FAlphaGenDilateCS, "AlphaGen_Dilate", FilterParams.DilateRadius, 0, 0, 0);
			}
			
			// 6. Erode
			if (FilterParams.ErodeRadius > 0.0f)
			{
				ADD_FILTER_PASS(FAlphaGenErodeCS, "AlphaGen_Erode", FilterParams.ErodeRadius, 0, 0, 0);
			}
			
			// 7. Posterize
			if (FilterParams.bPosterizeEnabled)
			{
				ADD_FILTER_PASS(FAlphaGenPosterizeCS, "AlphaGen_Posterize", FilterParams.PosterizeSteps, 0, 0, 0);
			}
			
			// 8. Threshold
			if (FilterParams.bThresholdEnabled)
			{
				ADD_FILTER_PASS(FAlphaGenThresholdCS, "AlphaGen_Threshold", FilterParams.Threshold, 0, 0, 0);
			}
			
			// 9. Pixelate
			if (FilterParams.PixelateSize > 1.0f)
			{
				ADD_FILTER_PASS(FAlphaGenPixelateCS, "AlphaGen_Pixelate", FilterParams.PixelateSize, 0, 0, 0);
			}
			
			// 10. Domain Warp
			if (FilterParams.DomainWarpStrength > 0.0f)
			{
				ADD_FILTER_PASS(FAlphaGenDomainWarpCS, "AlphaGen_DomainWarp", FilterParams.DomainWarpStrength, FilterParams.DomainWarpScale, 0, 0);
			}
			
			// 11. Spherize
			if (FMath::Abs(FilterParams.SpherizeAmount) > 0.01f)
			{
				ADD_FILTER_PASS(FAlphaGenSpherizeCS, "AlphaGen_Spherize", FilterParams.SpherizeAmount, 0, 0, 0);
			}
			
			// 12. Spiral
			if (FMath::Abs(FilterParams.SpiralTwist) > 0.01f)
			{
				ADD_FILTER_PASS(FAlphaGenSpiralCS, "AlphaGen_Spiral", FilterParams.SpiralTwist, 0, 0, 0);
			}
			
			// 13. Edge Detect
			if (FilterParams.EdgeDetectStrength > 0.0f)
			{
				ADD_FILTER_PASS(FAlphaGenEdgeDetectCS, "AlphaGen_EdgeDetect", FilterParams.EdgeDetectStrength, 0, 0, 0);
			}
			
			// 14. Invert (last)
			if (FilterParams.bInvert)
			{
				ADD_FILTER_PASS(FAlphaGenInvertCS, "AlphaGen_Invert", 0, 0, 0, 0);
			}
			
			#undef ADD_FILTER_PASS
			
			// Format cast always runs, so CurrentSource (Ping) always has valid RGBA8 data
			// No need for "no filter ran" fallback anymore
			
			// CurrentSource now holds the final result - extract it
			TRefCountPtr<IPooledRenderTarget> ExtractedResult;
			GraphBuilder.QueueTextureExtraction(CurrentSource, &ExtractedResult);
			
			// Execute entire filter chain
			GraphBuilder.Execute();
			
			// KSample-style SafeCopy with proper validity checks
			auto SafeCopy = [&](TRefCountPtr<IPooledRenderTarget> Src, ALPHAGEN_TEXTURE2D_RHI_TYPE Dst) {
				if (Src.IsValid() && ALPHAGEN_IS_VALID_RHI(Dst))
				{
					RHICmdList.Transition(FRHITransitionInfo(Dst, ERHIAccess::Unknown, ERHIAccess::CopyDest));
					FRHICopyTextureInfo CopyInfo;
					RHICmdList.CopyTexture(Src->GetRHI(), Dst, CopyInfo);
					RHICmdList.Transition(FRHITransitionInfo(Dst, ERHIAccess::CopyDest, ERHIAccess::SRVGraphics));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("AlphaGen: SafeCopy failed - invalid source or destination"));
				}
			};
			
			SafeCopy(ExtractedResult, OutputRHI);
		}
	);
	
	FlushRenderingCommands();
	
	return OutputTexture;
}

// ============================================================================
// LEGACY FILTER APPLICATION - Single filter (uses chain internally now)
// ============================================================================

UTexture2D* FAlphaGenComputeEngine::ApplyFilter(
	UTexture2D* InputTexture,
	EAlphaGenFilterType FilterType,
	float Param1,
	float Param2,
	float Param3,
	float Param4)
{
	if (!InputTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("AlphaGen: ApplyFilter called with null texture"));
		return nullptr;
	}
	
	// Make sure GPU resources are ready
	InputTexture->UpdateResource();
	FlushRenderingCommands();
	
	FTextureResource* Resource = InputTexture->GetResource();
	if (!Resource)
	{
		UE_LOG(LogTemp, Warning, TEXT("AlphaGen: No texture resource for filter"));
		return nullptr;
	}
	
	ALPHAGEN_TEXTURE2D_RHI_TYPE InputRHI = ALPHAGEN_GET_TEXTURE2D_RHI(Resource);
	if (!ALPHAGEN_IS_VALID_RHI(InputRHI))
	{
		UE_LOG(LogTemp, Warning, TEXT("AlphaGen: No RHI resource for filter"));
		return nullptr;
	}
	
	int32 Width = InputTexture->GetSizeX();
	int32 Height = InputTexture->GetSizeY();
	
	// Create the output texture that we'll eventually return to the caller
	// Bypass streaming pool to prevent eviction
	UTexture2D* OutputTexture = UTexture2D::CreateTransient(Width, Height, PF_R8G8B8A8);
	if (!OutputTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("AlphaGen: Failed to create output texture for filter"));
		return nullptr;
	}
	
	// Critical: Disable streaming to prevent pool eviction
	OutputTexture->NeverStream = true;
	OutputTexture->CompressionSettings = TC_VectorDisplacementmap;
	OutputTexture->SRGB = false;
	OutputTexture->Filter = TF_Bilinear;
	OutputTexture->LODGroup = TEXTUREGROUP_Pixels2D;
	OutputTexture->UpdateResource();
	FlushRenderingCommands();
	
	FTextureResource* OutputResource = OutputTexture->GetResource();
	if (!OutputResource || !OutputResource->GetTexture2DRHI())
	{
		UE_LOG(LogTemp, Error, TEXT("AlphaGen: Failed to get output texture RHI resource"));
		return nullptr;
	}
	
	ALPHAGEN_TEXTURE2D_RHI_TYPE OutputRHI = ALPHAGEN_GET_TEXTURE2D_RHI(OutputResource);
	
	// Pack filter parameters for the shader
	FVector4f FilterParams(Param1, Param2, Param3, Param4);
	FVector4f FilterParams2(0.0f, 0.0f, 0.0f, 0.0f);
	
	ENQUEUE_RENDER_COMMAND(ApplyAlphaGenFilter)(
		[InputRHI, OutputRHI, Width, Height, FilterType, FilterParams, FilterParams2](FRHICommandListImmediate& RHICmdList)
		{
			if (!ALPHAGEN_IS_VALID_RHI(InputRHI) || !ALPHAGEN_IS_VALID_RHI(OutputRHI))
			{
				UE_LOG(LogTemp, Warning, TEXT("AlphaGen: RHI resources became invalid before filter dispatch"));
				return;
			}
			
			// Transition input to SRV-readable state BEFORE building the graph
			RHICmdList.Transition(FRHITransitionInfo(InputRHI, ERHIAccess::Unknown, ERHIAccess::SRVGraphics));
			
			FRDGBuilder GraphBuilder(RHICmdList);
			
			// Register the external input texture - now guaranteed to be in SRV state
			FRDGTextureRef InputRDG = GraphBuilder.RegisterExternalTexture(
				CreateRenderTarget(InputRHI, TEXT("FilterInput"))
			);
			
			// Create an RDG-managed OUTPUT texture with UAV support for the compute shader
			FRDGTextureDesc FilterResultDesc = FRDGTextureDesc::Create2D(
				FIntPoint(Width, Height),
				PF_R8G8B8A8,
				FClearValueBinding::Black,
				TexCreate_UAV | TexCreate_ShaderResource | TexCreate_RenderTargetable
			);
			FRDGTextureRef FilterResultRDG = GraphBuilder.CreateTexture(FilterResultDesc, TEXT("FilterResult"));
			
			// Thread group dimensions for 8x8 compute dispatch
			FIntVector GroupCount(
				FMath::DivideAndRoundUp(Width, 8),
				FMath::DivideAndRoundUp(Height, 8),
				1
			);
			
			// Dispatch macro sets up all the shader parameters and adds the compute pass
			#define DISPATCH_FILTER(ShaderClass, EventName) \
			{ \
				TShaderMapRef<ShaderClass> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel)); \
				ShaderClass::FParameters* Params = GraphBuilder.AllocParameters<ShaderClass::FParameters>(); \
				Params->FilterInputTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(InputRDG)); \
				Params->FilterInputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(); \
				Params->FilterOutputTexture = GraphBuilder.CreateUAV(FilterResultRDG); \
				Params->FilterParams = FilterParams; \
				Params->FilterParams2 = FilterParams2; \
				Params->FilterTextureSize = FUintVector2(Width, Height); \
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME(EventName), Shader, Params, GroupCount); \
			}
			
			switch (FilterType)
			{
				case EAlphaGenFilterType::Blur:
					DISPATCH_FILTER(FAlphaGenBlurCS, "AlphaGen_Blur");
					break;
				case EAlphaGenFilterType::Sharpen:
					DISPATCH_FILTER(FAlphaGenSharpenCS, "AlphaGen_Sharpen");
					break;
				case EAlphaGenFilterType::Invert:
					DISPATCH_FILTER(FAlphaGenInvertCS, "AlphaGen_Invert");
					break;
				case EAlphaGenFilterType::Dilate:
					DISPATCH_FILTER(FAlphaGenDilateCS, "AlphaGen_Dilate");
					break;
				case EAlphaGenFilterType::Erode:
					DISPATCH_FILTER(FAlphaGenErodeCS, "AlphaGen_Erode");
					break;
				case EAlphaGenFilterType::Threshold:
					DISPATCH_FILTER(FAlphaGenThresholdCS, "AlphaGen_Threshold");
					break;
				case EAlphaGenFilterType::Posterize:
					DISPATCH_FILTER(FAlphaGenPosterizeCS, "AlphaGen_Posterize");
					break;
				case EAlphaGenFilterType::Pixelate:
					DISPATCH_FILTER(FAlphaGenPixelateCS, "AlphaGen_Pixelate");
					break;
				case EAlphaGenFilterType::DomainWarp:
					DISPATCH_FILTER(FAlphaGenDomainWarpCS, "AlphaGen_DomainWarp");
					break;
				case EAlphaGenFilterType::Spherize:
					DISPATCH_FILTER(FAlphaGenSpherizeCS, "AlphaGen_Spherize");
					break;
				case EAlphaGenFilterType::EdgeDetect:
					DISPATCH_FILTER(FAlphaGenEdgeDetectCS, "AlphaGen_EdgeDetect");
					break;
				case EAlphaGenFilterType::Levels:
					DISPATCH_FILTER(FAlphaGenLevelsCS, "AlphaGen_Levels");
					break;
				case EAlphaGenFilterType::Contrast:
					DISPATCH_FILTER(FAlphaGenContrastCS, "AlphaGen_Contrast");
					break;
				case EAlphaGenFilterType::Spiral:
					DISPATCH_FILTER(FAlphaGenSpiralCS, "AlphaGen_Spiral");
					break;
			}
			
			#undef DISPATCH_FILTER
			
			// Extract our RDG result so we can copy it after execution
			TRefCountPtr<IPooledRenderTarget> ExtractedResult;
			GraphBuilder.QueueTextureExtraction(FilterResultRDG, &ExtractedResult);
			
			// Run all the graph passes
			GraphBuilder.Execute();
			
			// Now copy from our extracted RDG texture to the final output texture
			if (ExtractedResult.IsValid())
			{
				RHICmdList.Transition(FRHITransitionInfo(OutputRHI, ERHIAccess::Unknown, ERHIAccess::CopyDest));
				FRHICopyTextureInfo CopyInfo;
				RHICmdList.CopyTexture(ExtractedResult->GetRHI(), OutputRHI, CopyInfo);
				RHICmdList.Transition(FRHITransitionInfo(OutputRHI, ERHIAccess::CopyDest, ERHIAccess::SRVGraphics));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AlphaGen: Filter result extraction failed"));
			}
		}
	);
	
	FlushRenderingCommands();
	
	return OutputTexture;
}

