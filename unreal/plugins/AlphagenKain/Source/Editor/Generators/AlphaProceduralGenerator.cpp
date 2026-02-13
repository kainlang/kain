// Copyright 2026 K-Studio. All Rights Reserved.

#include "Generators/AlphaProceduralGenerator.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"

// Include all Kain-generated shader headers
#include "GenerateRadial.h"
#include "GenerateCircle.h"
#include "GenerateSquare.h"
#include "GenerateDiamond.h"
#include "GeneratePerlin.h"
#include "GenerateVoronoi.h"
#include "GenerateBricks.h"
#include "GenerateDots.h"
#include "GenerateSeamlessNoise.h"
#include "GenerateCrosshatch.h"
#include "GenerateWaves.h"
#include "GenerateCheckerboard.h"
#include "GenerateHexagon.h"
#include "GenerateTears.h"
#include "GenerateScratches.h"
#include "GenerateSplatter.h"
#include "GenerateCracks.h"
#include "GenerateCells.h"
#include "GenerateGrunge.h"
#include "GenerateFibers.h"
#include "GenerateCaustics.h"

// Helper to safely get a param with a default value
static float GetParam(const TMap<FString, float>& Params, const FString& Key, float Default)
{
	const float* Value = Params.Find(Key);
	return Value ? *Value : Default;
}

UTexture2D* FAlphaProceduralGenerator::Generate(
	EProceduralType Type,
	int32 Size,
	const TMap<FString, float>& Params)
{
	// Clamp size like Rust: params.size.max(16).min(2048)
	// We allow up to 4096 for higher quality
	Size = FMath::Clamp(Size, 16, 4096);
	
	// Create output texture
	UTexture2D* OutputTexture = UTexture2D::CreateTransient(Size, Size, PF_R8G8B8A8);
	if (!OutputTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("AlphaGen: Failed to create output texture"));
		return nullptr;
	}
	
	OutputTexture->NeverStream = true;
	OutputTexture->CompressionSettings = TC_VectorDisplacementmap;
	OutputTexture->SRGB = false;
	OutputTexture->Filter = TF_Bilinear;
	OutputTexture->LODGroup = TEXTUREGROUP_Pixels2D;
	OutputTexture->UpdateResource();
	FlushRenderingCommands();
	
	FTextureResource* Resource = OutputTexture->GetResource();
	if (!Resource || !Resource->GetTexture2DRHI())
	{
		UE_LOG(LogTemp, Error, TEXT("AlphaGen: Failed to get texture RHI"));
		return nullptr;
	}
	
	FTexture2DRHIRef OutputRHI = Resource->GetTexture2DRHI();
	
	// Extract params for each generator type
	float Param1, Param2, Param3;
	float PatternX, PatternY, PatternZ, PatternW;
	
	// Dispatch to render thread with RDG
	ENQUEUE_RENDER_COMMAND(AlphaGenKainDispatch)(
		[OutputRHI, Type, Size, Params](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			
			FRDGTextureDesc OutDesc = FRDGTextureDesc::Create2D(
				FIntPoint(Size, Size),
				PF_R8G8B8A8,
				FClearValueBinding::Black,
				TexCreate_UAV | TexCreate_ShaderResource
			);
			FRDGTextureRef OutputRDG = GraphBuilder.CreateTexture(OutDesc, TEXT("AlphaGenKainOutput"));
			
			FIntVector GroupCount(FMath::DivideAndRoundUp(Size, 8), FMath::DivideAndRoundUp(Size, 8), 1);
			FUintVector2 TexSize(Size, Size);
			
			switch (Type)
			{
				case EProceduralType::Radial:
				{
					float falloff = GetParam(Params, TEXT("falloff"), 2.0f);
					AddPass_GenerateRadial(GraphBuilder, TexSize, falloff, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Circle:
				case EProceduralType::Square:
				case EProceduralType::Diamond:
				{
					float scale = GetParam(Params, TEXT("scale"), 0.8f);
					float softness = GetParam(Params, TEXT("softness"), 0.02f);
					
					if (Type == EProceduralType::Circle)
						AddPass_GenerateCircle(GraphBuilder, TexSize, scale, softness, OutputRDG, GroupCount);
					else if (Type == EProceduralType::Square)
						AddPass_GenerateSquare(GraphBuilder, TexSize, scale, softness, OutputRDG, GroupCount);
					else
						AddPass_GenerateDiamond(GraphBuilder, TexSize, scale, softness, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Perlin:
				{
					float scale = GetParam(Params, TEXT("scale"), 4.0f);
					float octaves = GetParam(Params, TEXT("octaves"), 4.0f);
					float seed = GetParam(Params, TEXT("seed"), 0.0f);
					AddPass_GeneratePerlin(GraphBuilder, TexSize, scale, octaves, seed, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Voronoi:
				{
					float scale = GetParam(Params, TEXT("scale"), 8.0f);
					float edge_width = GetParam(Params, TEXT("edge_width"), 0.05f);
					float seed = GetParam(Params, TEXT("seed"), 0.0f);
					AddPass_GenerateVoronoi(GraphBuilder, TexSize, scale, edge_width, seed, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Bricks:
				{
					float width = GetParam(Params, TEXT("width"), 0.25f);
					float height = GetParam(Params, TEXT("height"), 0.1f);
					float mortar = GetParam(Params, TEXT("mortar"), 0.02f);
					AddPass_GenerateBricks(GraphBuilder, TexSize, width, height, mortar, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Dots:
				{
					float dot_size = GetParam(Params, TEXT("dot_size"), 0.08f);
					float spacing = GetParam(Params, TEXT("spacing"), 0.15f);
					AddPass_GenerateDots(GraphBuilder, TexSize, dot_size, spacing, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::SeamlessNoise:
				{
					float scale = GetParam(Params, TEXT("scale"), 4.0f);
					float octaves = GetParam(Params, TEXT("octaves"), 4.0f);
					float seed = GetParam(Params, TEXT("seed"), 0.0f);
					AddPass_GenerateSeamlessNoise(GraphBuilder, TexSize, scale, octaves, seed, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Crosshatch:
				{
					float scale = GetParam(Params, TEXT("scale"), 16.0f);
					float thickness = GetParam(Params, TEXT("thickness"), 0.1f);
					float angle = GetParam(Params, TEXT("angle"), 0.785f);
					AddPass_GenerateCrosshatch(GraphBuilder, TexSize, scale, thickness, angle, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Waves:
				{
					float frequency = GetParam(Params, TEXT("frequency"), 8.0f);
					float amplitude = GetParam(Params, TEXT("amplitude"), 0.5f);
					float wave_type = GetParam(Params, TEXT("wave_type"), 0.0f);
					AddPass_GenerateWaves(GraphBuilder, TexSize, frequency, amplitude, wave_type, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Checkerboard:
				{
					float scale = GetParam(Params, TEXT("scale"), 8.0f);
					AddPass_GenerateCheckerboard(GraphBuilder, TexSize, scale, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Hexagon:
				{
					float scale = GetParam(Params, TEXT("scale"), 8.0f);
					float edge_thickness = GetParam(Params, TEXT("edge_thickness"), 0.1f);
					float seed = GetParam(Params, TEXT("seed"), 0.0f);
					AddPass_GenerateHexagon(GraphBuilder, TexSize, scale, edge_thickness, seed, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Tears:
				{
					float scale = GetParam(Params, TEXT("scale"), 8.0f);
					float length = GetParam(Params, TEXT("length"), 0.8f);
					float seed = GetParam(Params, TEXT("seed"), 0.0f);
					AddPass_GenerateTears(GraphBuilder, TexSize, scale, length, seed, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Scratches:
				{
					float density = GetParam(Params, TEXT("density"), 20.0f);
					float length = GetParam(Params, TEXT("length"), 0.5f);
					float seed = GetParam(Params, TEXT("seed"), 0.0f);
					AddPass_GenerateScratches(GraphBuilder, TexSize, density, length, seed, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Splatter:
				{
					float scale = GetParam(Params, TEXT("scale"), 4.0f);
					float size = GetParam(Params, TEXT("size"), 0.3f);
					float seed = GetParam(Params, TEXT("seed"), 0.0f);
					AddPass_GenerateSplatter(GraphBuilder, TexSize, scale, size, seed, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Cracks:
				{
					float scale = GetParam(Params, TEXT("scale"), 8.0f);
					float width = GetParam(Params, TEXT("width"), 0.1f);
					float seed = GetParam(Params, TEXT("seed"), 0.0f);
					AddPass_GenerateCracks(GraphBuilder, TexSize, scale, width, seed, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Cells:
				{
					float scale = GetParam(Params, TEXT("scale"), 8.0f);
					float contrast = GetParam(Params, TEXT("contrast"), 2.0f);
					float seed = GetParam(Params, TEXT("seed"), 0.0f);
					AddPass_GenerateCells(GraphBuilder, TexSize, scale, contrast, seed, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Grunge:
				{
					float scale = GetParam(Params, TEXT("scale"), 4.0f);
					float detail = GetParam(Params, TEXT("detail"), 1.0f);
					float seed = GetParam(Params, TEXT("seed"), 0.0f);
					AddPass_GenerateGrunge(GraphBuilder, TexSize, scale, detail, seed, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Fibers:
				{
					float scale = GetParam(Params, TEXT("scale"), 16.0f);
					float angle = GetParam(Params, TEXT("angle"), 0.0f);
					float seed = GetParam(Params, TEXT("seed"), 0.0f);
					AddPass_GenerateFibers(GraphBuilder, TexSize, scale, angle, seed, OutputRDG, GroupCount);
					break;
				}
				case EProceduralType::Caustics:
				{
					float scale = GetParam(Params, TEXT("scale"), 4.0f);
					float time = GetParam(Params, TEXT("time"), 0.0f);
					float seed = GetParam(Params, TEXT("seed"), 0.0f);
					AddPass_GenerateCaustics(GraphBuilder, TexSize, scale, time, seed, OutputRDG, GroupCount);
					break;
				}
			}
			
			// Extract to output texture
			TRefCountPtr<IPooledRenderTarget> ExtractedOutput;
			GraphBuilder.QueueTextureExtraction(OutputRDG, &ExtractedOutput);
			GraphBuilder.Execute();
			
			if (ExtractedOutput.IsValid())
			{
				RHICmdList.Transition(FRHITransitionInfo(OutputRHI, ERHIAccess::Unknown, ERHIAccess::CopyDest));
				FRHICopyTextureInfo CopyInfo;
				RHICmdList.CopyTexture(ExtractedOutput->GetRHI(), OutputRHI, CopyInfo);
				RHICmdList.Transition(FRHITransitionInfo(OutputRHI, ERHIAccess::CopyDest, ERHIAccess::SRVGraphics));
			}
		}
	);
	
	FlushRenderingCommands();
	return OutputTexture;
}

// Direct port from Rust procedural.rs: generate_radial
void FAlphaProceduralGenerator::GenerateRadial(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params)
{
	const float Falloff = Params.Contains(TEXT("falloff")) ? Params[TEXT("falloff")] : 2.0f;
	const float Center = Size / 2.0f;
	const float MaxDist = Center;
	
	for (int32 Y = 0; Y < Size; Y++)
	{
		for (int32 X = 0; X < Size; X++)
		{
			float DX = X - Center;
			float DY = Y - Center;
			float Dist = FMath::Sqrt(DX * DX + DY * DY);
			
			float T = FMath::Min(Dist / MaxDist, 1.0f);
			float Value = FMath::Max(1.0f - FMath::Pow(T, Falloff), 0.0f);
			
			Pixels[Y * Size + X] = static_cast<uint8>(Value * 255.0f);
		}
	}
}

// Direct port from Rust procedural.rs: generate_circle
void FAlphaProceduralGenerator::GenerateCircle(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params)
{
	const float EdgeSoftness = Params.Contains(TEXT("softness")) ? Params[TEXT("softness")] : 0.02f;
	const float Center = Size / 2.0f;
	const float Radius = Center * 0.95f;
	
	for (int32 Y = 0; Y < Size; Y++)
	{
		for (int32 X = 0; X < Size; X++)
		{
			float DX = X - Center;
			float DY = Y - Center;
			float Dist = FMath::Sqrt(DX * DX + DY * DY);
			
			float Edge = (Radius - Dist) / (Radius * EdgeSoftness);
			float Value = FMath::Clamp(Edge, 0.0f, 1.0f);
			
			Pixels[Y * Size + X] = static_cast<uint8>(Value * 255.0f);
		}
	}
}

// Direct port from Rust procedural.rs: generate_square
void FAlphaProceduralGenerator::GenerateSquare(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params)
{
	const float EdgeSoftness = Params.Contains(TEXT("softness")) ? Params[TEXT("softness")] : 0.02f;
	const int32 Padding = static_cast<int32>(Size * 0.05f);
	
	for (int32 Y = 0; Y < Size; Y++)
	{
		for (int32 X = 0; X < Size; X++)
		{
			bool bInBounds = X >= Padding && X < Size - Padding 
			              && Y >= Padding && Y < Size - Padding;
			
			if (bInBounds)
			{
				int32 DX = FMath::Min(X - Padding, Size - Padding - 1 - X);
				int32 DY = FMath::Min(Y - Padding, Size - Padding - 1 - Y);
				float EdgeDist = static_cast<float>(FMath::Min(DX, DY));
				float SoftnessPixels = FMath::Max(Size * EdgeSoftness, 1.0f);
				float Value = FMath::Min(EdgeDist / SoftnessPixels, 1.0f);
				
				Pixels[Y * Size + X] = static_cast<uint8>(Value * 255.0f);
			}
		}
	}
}

// Direct port from Rust procedural.rs: generate_diamond
void FAlphaProceduralGenerator::GenerateDiamond(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params)
{
	const float EdgeSoftness = Params.Contains(TEXT("softness")) ? Params[TEXT("softness")] : 0.05f;
	const float Center = Size / 2.0f;
	const float Radius = Center * 0.95f;
	
	for (int32 Y = 0; Y < Size; Y++)
	{
		for (int32 X = 0; X < Size; X++)
		{
			float DX = FMath::Abs(X - Center);
			float DY = FMath::Abs(Y - Center);
			float ManhattanDist = DX + DY;
			
			float Edge = (Radius - ManhattanDist) / (Radius * EdgeSoftness);
			float Value = FMath::Clamp(Edge, 0.0f, 1.0f);
			
			Pixels[Y * Size + X] = static_cast<uint8>(Value * 255.0f);
		}
	}
}

// Noise helper functions - ported from proceduralcommon.ush

float FAlphaProceduralGenerator::Hash21(FVector2f P)
{
	FVector3f P3 = FVector3f(
		FMath::Frac(P.X * 0.1031f),
		FMath::Frac(P.Y * 0.1031f),
		FMath::Frac(P.X * 0.1031f)
	);
	float Dot = P3.X * (P3.Y + 33.33f) + P3.Y * (P3.Z + 33.33f) + P3.Z * (P3.X + 33.33f);
	P3.X += Dot; P3.Y += Dot; P3.Z += Dot;
	return FMath::Frac((P3.X + P3.Y) * P3.Z);
}

FVector2f FAlphaProceduralGenerator::Hash22(FVector2f P)
{
	FVector3f P3 = FVector3f(
		FMath::Frac(P.X * 0.1031f),
		FMath::Frac(P.Y * 0.1030f),
		FMath::Frac(P.X * 0.0973f)
	);
	float Dot = P3.X * (P3.Y + 33.33f) + P3.Y * (P3.Z + 33.33f) + P3.Z * (P3.X + 33.33f);
	P3.X += Dot; P3.Y += Dot; P3.Z += Dot;
	return FVector2f(
		FMath::Frac((P3.X + P3.Y) * P3.Z),
		FMath::Frac((P3.X + P3.Z) * P3.Y)
	);
}

float FAlphaProceduralGenerator::Grad2(FVector2f P)
{
	FVector2f I = FVector2f(FMath::FloorToFloat(P.X), FMath::FloorToFloat(P.Y));
	FVector2f F = FVector2f(FMath::Frac(P.X), FMath::Frac(P.Y));
	
	// Quintic interpolation curve
	FVector2f U = F * F * F * (F * (F * 6.0f - 15.0f) + 10.0f);
	
	// Four corner gradients
	float A = Hash21(I + FVector2f(0.0f, 0.0f));
	float B = Hash21(I + FVector2f(1.0f, 0.0f));
	float C = Hash21(I + FVector2f(0.0f, 1.0f));
	float D = Hash21(I + FVector2f(1.0f, 1.0f));
	
	// Bilinear interpolation
	return FMath::Lerp(FMath::Lerp(A, B, U.X), FMath::Lerp(C, D, U.X), U.Y);
}

float FAlphaProceduralGenerator::FBM(FVector2f P, int32 Octaves, float Lacunarity, float Persistence)
{
	float Value = 0.0f;
	float Amplitude = 0.5f;
	float Frequency = 1.0f;
	float MaxValue = 0.0f;
	
	for (int32 I = 0; I < Octaves; I++)
	{
		Value += Amplitude * (Grad2(P * Frequency) * 2.0f - 1.0f);
		MaxValue += Amplitude;
		Amplitude *= Persistence;
		Frequency *= Lacunarity;
	}
	
	return (Value / MaxValue) * 0.5f + 0.5f; // Normalize to 0-1
}

// Direct port from Rust procedural.rs: generate_perlin
void FAlphaProceduralGenerator::GeneratePerlin(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params)
{
	const float Scale = Params.Contains(TEXT("scale")) ? Params[TEXT("scale")] : 4.0f;
	const int32 Octaves = static_cast<int32>(Params.Contains(TEXT("octaves")) ? Params[TEXT("octaves")] : 4.0f);
	const float Seed = Params.Contains(TEXT("seed")) ? Params[TEXT("seed")] : 0.0f;
	
	for (int32 Y = 0; Y < Size; Y++)
	{
		for (int32 X = 0; X < Size; X++)
		{
			FVector2f UV = FVector2f(
				static_cast<float>(X) / Size,
				static_cast<float>(Y) / Size
			);
			
			FVector2f P = UV * Scale + Seed;
			float Value = FBM(P, Octaves, 2.0f, 0.5f);
			
			Pixels[Y * Size + X] = static_cast<uint8>(FMath::Clamp(Value, 0.0f, 1.0f) * 255.0f);
		}
	}
}

// Direct port from Rust procedural.rs: generate_voronoi
void FAlphaProceduralGenerator::GenerateVoronoi(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params)
{
	const int32 CellCount = static_cast<int32>(Params.Contains(TEXT("cells")) ? Params[TEXT("cells")] : 16.0f);
	const float EdgeWidth = Params.Contains(TEXT("edge_width")) ? Params[TEXT("edge_width")] : 0.1f;
	const uint64 Seed = static_cast<uint64>(Params.Contains(TEXT("seed")) ? Params[TEXT("seed")] : 42.0f);
	
	// Generate cell centers using simple LCG random
	TArray<FVector2f> Centers;
	Centers.Reserve(CellCount);
	uint64 RandState = Seed;
	
	auto NextRandom = [&RandState]() -> float
	{
		RandState = RandState * 6364136223846793005ULL + 1442695040888963407ULL;
		return static_cast<float>((RandState >> 33) & 0x7FFFFFFF) / static_cast<float>(0x7FFFFFFF);
	};
	
	for (int32 I = 0; I < CellCount; I++)
	{
		Centers.Add(FVector2f(NextRandom() * Size, NextRandom() * Size));
	}
	
	for (int32 Y = 0; Y < Size; Y++)
	{
		for (int32 X = 0; X < Size; X++)
		{
			FVector2f P = FVector2f(static_cast<float>(X), static_cast<float>(Y));
			
			// Find two closest centers
			TArray<float> Dists;
			Dists.Reserve(CellCount);
			
			for (const FVector2f& Center : Centers)
			{
				FVector2f Delta = P - Center;
				Dists.Add(FMath::Sqrt(Delta.X * Delta.X + Delta.Y * Delta.Y));
			}
			
			Dists.Sort();
			
			float D1 = Dists[0];
			float D2 = Dists.Num() > 1 ? Dists[1] : D1 + 10.0f;
			
			// Edge detection based on distance difference
			float Edge = (D2 - D1) / (Size * EdgeWidth);
			float Value = FMath::Clamp(Edge, 0.0f, 1.0f);
			
			Pixels[Y * Size + X] = static_cast<uint8>(Value * 255.0f);
		}
	}
}

// Direct port from Rust procedural.rs: generate_bricks
void FAlphaProceduralGenerator::GenerateBricks(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params)
{
	const float BrickWidthRatio = Params.Contains(TEXT("width")) ? Params[TEXT("width")] : 0.25f;
	const float BrickHeightRatio = Params.Contains(TEXT("height")) ? Params[TEXT("height")] : 0.1f;
	const float MortarRatio = Params.Contains(TEXT("mortar")) ? Params[TEXT("mortar")] : 0.02f;
	
	const int32 BW = static_cast<int32>(Size * BrickWidthRatio);
	const int32 BH = static_cast<int32>(Size * BrickHeightRatio);
	const int32 MW = static_cast<int32>(Size * MortarRatio);
	
	// Prevent division by zero
	if (BW <= 0 || BH <= 0 || MW <= 0)
	{
		return;
	}
	
	for (int32 Y = 0; Y < Size; Y++)
	{
		for (int32 X = 0; X < Size; X++)
		{
			int32 Row = Y / (BH + MW);
			int32 Offset = (Row % 2 == 1) ? BW / 2 : 0;
			
			int32 BX = (X + Offset) % (BW + MW);
			int32 BY = Y % (BH + MW);
			
			bool bInBrick = BX < BW && BY < BH;
			
			if (bInBrick)
			{
				// Slight variation within brick
				float EdgeX = static_cast<float>(FMath::Min(BX, BW - 1 - BX)) / MW;
				float EdgeY = static_cast<float>(FMath::Min(BY, BH - 1 - BY)) / MW;
				float Edge = FMath::Min(FMath::Min(EdgeX, EdgeY), 1.0f);
				
				Pixels[Y * Size + X] = static_cast<uint8>(Edge * 255.0f);
			}
		}
	}
}

// Direct port from Rust procedural.rs: generate_dots
void FAlphaProceduralGenerator::GenerateDots(TArray<uint8>& Pixels, int32 Size, const TMap<FString, float>& Params)
{
	const float DotSizeRatio = Params.Contains(TEXT("dot_size")) ? Params[TEXT("dot_size")] : 0.08f;
	const float SpacingRatio = Params.Contains(TEXT("spacing")) ? Params[TEXT("spacing")] : 0.15f;
	
	const float DotRadius = Size * DotSizeRatio / 2.0f;
	const float CellSize = Size * SpacingRatio;
	
	// Prevent issues with tiny cell sizes
	if (CellSize < 1.0f)
	{
		return;
	}
	
	for (int32 Y = 0; Y < Size; Y++)
	{
		for (int32 X = 0; X < Size; X++)
		{
			// Find nearest dot center
			float CellX = (FMath::FloorToFloat(X / CellSize) + 0.5f) * CellSize;
			float CellY = (FMath::FloorToFloat(Y / CellSize) + 0.5f) * CellSize;
			
			float DX = X - CellX;
			float DY = Y - CellY;
			float Dist = FMath::Sqrt(DX * DX + DY * DY);
			
			float Value = FMath::Max(1.0f - Dist / DotRadius, 0.0f);
			
			Pixels[Y * Size + X] = static_cast<uint8>(Value * 255.0f);
		}
	}
}

UTexture2D* FAlphaProceduralGenerator::CreateTextureFromPixels(const TArray<uint8>& Pixels, int32 Size)
{
	// Create transient texture - bypass streaming pool to prevent eviction
	UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_G8);
	
	if (!Texture)
	{
		UE_LOG(LogTemp, Error, TEXT("AlphaGen: Failed to create transient texture"));
		return nullptr;
	}
	
	// Critical: Disable streaming to prevent pool eviction (gray background fix)
	Texture->NeverStream = true;
	
	// Configure texture settings for grayscale alpha usage
	Texture->CompressionSettings = TC_Grayscale;
	Texture->SRGB = false;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->LODGroup = TEXTUREGROUP_Pixels2D; // Non-streaming group
	
	// Lock and write pixel data
	void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, Pixels.GetData(), Pixels.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	
	// Update the texture resource
	Texture->UpdateResource();
	
	return Texture;
}
