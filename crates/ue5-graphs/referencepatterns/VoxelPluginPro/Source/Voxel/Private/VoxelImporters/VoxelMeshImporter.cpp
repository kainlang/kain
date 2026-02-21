// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelImporters/VoxelMeshImporter.h"

#include "VoxelAssets/VoxelDataAsset.h"
#include "VoxelAssets/VoxelDataAssetData.inl"
#include "VoxelUtilities/VoxelMathUtilities.h"
#include "VoxelUtilities/VoxelExampleUtilities.h"
#include "VoxelUtilities/VoxelDistanceFieldUtilities.h"
#include "VoxelMessages.h"

#include "SDFGen/makelevelset3.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetRenderingLibrary.h"

static void GetMergedSectionFromStaticMesh(
	UStaticMesh* InMesh, 
	int32 LODIndex, 
	TArray<FVector>& Vertices, 
	TArray<uint32>& Indices, 
	TArray<FVector2D>& UVs)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(InMesh->GetRenderData()) || !ensure(InMesh->GetRenderData()->LODResources.IsValidIndex(LODIndex))) return;

	const FStaticMeshLODResources& LODResources = InMesh->GetRenderData()->LODResources[LODIndex];
	const FRawStaticIndexBuffer& IndexBuffer = LODResources.IndexBuffer;
	const FPositionVertexBuffer& PositionVertexBuffer = LODResources.VertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& StaticMeshVertexBuffer = LODResources.VertexBuffers.StaticMeshVertexBuffer;
	const int32 NumTextureCoordinates = StaticMeshVertexBuffer.GetNumTexCoords();

	ensure(IndexBuffer.GetNumIndices() % 3 == 0);

	const auto Get = [](auto& Array, auto Index) -> auto&
	{
#if VOXEL_DEBUG
		return Array[Index];
#else
		return Array.GetData()[Index];
#endif
	};
	
	if (!FPlatformProperties::RequiresCookedData() || InMesh->bAllowCPUAccess)
	{
		{
			VOXEL_SCOPE_COUNTER("Copy Vertices from CPU");
			Vertices.SetNumUninitialized(PositionVertexBuffer.GetNumVertices());
			for (uint32 Index = 0; Index < PositionVertexBuffer.GetNumVertices(); Index++)
			{
				Get(Vertices, Index) = FVector(PositionVertexBuffer.VertexPosition(Index));
			}
		}
		{
			VOXEL_SCOPE_COUNTER("Copy Triangles from CPU");
			Indices.SetNumUninitialized(IndexBuffer.GetNumIndices());
			for (int32 Index = 0; Index < IndexBuffer.GetNumIndices(); Index++)
			{
				Get(Indices, Index) = IndexBuffer.GetIndex(Index);
			}
		}
		if (NumTextureCoordinates > 0)
		{
			VOXEL_SCOPE_COUNTER("Copy UVs from CPU");
			UVs.SetNumUninitialized(StaticMeshVertexBuffer.GetNumVertices());
			for (uint32 Index = 0; Index < StaticMeshVertexBuffer.GetNumVertices(); Index++)
			{
				Get(UVs, Index) = FVector2D(StaticMeshVertexBuffer.GetVertexUV(Index, 0));
			}
		}
	}
	else
	{
		LOG_VOXEL(Log, TEXT("Extracting mesh data from GPU for %s"), *InMesh->GetName());
		
		ENQUEUE_RENDER_COMMAND(VoxelDistanceFieldCompute)([&](FRHICommandListImmediate& RHICmdList)
		{
			{
				VOXEL_SCOPE_COUNTER("Copy Vertices from GPU");
				Vertices.SetNumUninitialized(PositionVertexBuffer.GetNumVertices());
				const int32 NumBytes = PositionVertexBuffer.GetNumVertices() * PositionVertexBuffer.GetStride();
				
				void* BufferData = RHICmdList.LockBuffer(PositionVertexBuffer.VertexBufferRHI, 0, NumBytes, EResourceLockMode::RLM_ReadOnly);
				FMemory::Memcpy(Vertices.GetData(), BufferData, NumBytes);
				RHICmdList.UnlockBuffer(PositionVertexBuffer.VertexBufferRHI);
			}
			{
				VOXEL_SCOPE_COUNTER("Copy Triangles from GPU");
				Indices.SetNumUninitialized(IndexBuffer.GetNumIndices());
				
				const bool bIs32Bit = IndexBuffer.Is32Bit();
				const int32 NumBytes = IndexBuffer.GetNumIndices() * (bIs32Bit ? sizeof(uint32) : sizeof(uint16));
				
				void* BufferData = RHICmdList.LockBuffer(IndexBuffer.IndexBufferRHI, 0, NumBytes, EResourceLockMode::RLM_ReadOnly);
				if (bIs32Bit)
				{
					FMemory::Memcpy(Indices.GetData(), BufferData, NumBytes);
				}
				else
				{
					TArray<uint16> Indices16;
					Indices16.SetNumUninitialized(IndexBuffer.GetNumIndices());
					FMemory::Memcpy(Indices16.GetData(), BufferData, NumBytes);
					for (int32 Index = 0; Index < Indices16.Num(); Index++)
					{
						Get(Indices, Index) = Get(Indices16, Index);
					}
				}
				RHICmdList.UnlockBuffer(IndexBuffer.IndexBufferRHI);
			}
			if (NumTextureCoordinates > 0)
			{
				VOXEL_SCOPE_COUNTER("Copy UVs from GPU");
				UVs.SetNumUninitialized(StaticMeshVertexBuffer.GetNumVertices());

				const bool bFullPrecision = StaticMeshVertexBuffer.GetUseFullPrecisionUVs();
				const int32 NumBytes = StaticMeshVertexBuffer.GetNumVertices() * (bFullPrecision ? sizeof(FVector2D) : sizeof(FVector2DHalf));
				
				void* BufferData = RHICmdList.LockBuffer(StaticMeshVertexBuffer.TexCoordVertexBuffer.VertexBufferRHI, 0, NumBytes, EResourceLockMode::RLM_ReadOnly);
				if (bFullPrecision)
				{
					FMemory::Memcpy(UVs.GetData(), BufferData, NumBytes);
				}
				else
				{
					TArray<FVector2DHalf> UVsHalf;
					UVsHalf.SetNumUninitialized(StaticMeshVertexBuffer.GetNumVertices());
					FMemory::Memcpy(UVsHalf.GetData(), BufferData, NumBytes);
					for (int32 Index = 0; Index < UVsHalf.Num(); Index++)
					{
						Get(UVs, Index) = Get(UVsHalf, Index);
					}
				}
				RHICmdList.UnlockBuffer(StaticMeshVertexBuffer.TexCoordVertexBuffer.VertexBufferRHI);
			}
		});
		
		FRenderCommandFence Fence;
		Fence.BeginFence();
		Fence.Wait();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelMeshImporterSettings::FVoxelMeshImporterSettings()
{
	ColorsMaterial = FVoxelExampleUtilities::LoadExampleObject<UMaterialInterface>(TEXT("/Voxel/Examples/Importers/Chair/VoxelExample_M_Chair_Emissive_Color"));
	UVsMaterial = FVoxelExampleUtilities::LoadExampleObject<UMaterialInterface>(TEXT("/Voxel/Examples/Importers/Chair/VoxelExample_M_Chair_Emissive_UVs"));
}

FVoxelMeshImporterSettings::FVoxelMeshImporterSettings(const FVoxelMeshImporterSettingsBase& Base)
	: FVoxelMeshImporterSettingsBase(Base)
{
	bImportColors = false;
	bImportUVs = false;
}

void UVoxelMeshImporterLibrary::CreateMeshDataFromStaticMesh(UStaticMesh* StaticMesh, FVoxelMeshImporterInputData& Data)
{
	VOXEL_FUNCTION_COUNTER();

	check(StaticMesh);
	Data.Vertices.Reset();
	Data.Triangles.Reset();
	Data.UVs.Reset();
	
	const int32 LOD = 0;
	TArray<uint32> Indices;

	GetMergedSectionFromStaticMesh(StaticMesh, LOD, Data.Vertices, Indices, Data.UVs);
	
	const auto Get = [](auto& Array, auto Index) -> auto&
	{
#if VOXEL_DEBUG
		return Array[Index];
#else
		return Array.GetData()[Index];
#endif
	};

	for (uint32 Index : Indices)
	{
		if (Index >= uint32(Data.Vertices.Num()))
		{
			FVoxelMessages::Error(FUNCTION_ERROR("Invalid index buffer"));
			Data = {};
			return;
		}
	}
	
	ensure(Indices.Num() % 3 == 0);
	Data.Triangles.SetNumUninitialized(Indices.Num() / 3);
	for (int32 Index = 0; Index < Data.Triangles.Num(); Index++)
	{
		Get(Data.Triangles, Index) = FIntVector(
			Get(Indices, 3 * Index + 0),
			Get(Indices, 3 * Index + 1),
			Get(Indices, 3 * Index + 2));
	}
}

bool UVoxelMeshImporterLibrary::ConvertMeshToVoxels(
	UObject* WorldContextObject,
	const FVoxelMeshImporterInputData& Mesh,
	const FTransform& Transform,
	const FVoxelMeshImporterSettings& Settings,
	FVoxelMeshImporterRenderTargetCache& RenderTargetCache,
	FVoxelDataAssetData& OutAsset,
	FIntVector& OutOffset,
	int32& OutNumLeaks)
{
	VOXEL_FUNCTION_COUNTER();

	if (RenderTargetCache.LastRenderedRenderTargetSize != Settings.RenderTargetSize)
	{
		RenderTargetCache.LastRenderedRenderTargetSize = Settings.RenderTargetSize;
		RenderTargetCache.LastRenderedColorsMaterial = nullptr;
		RenderTargetCache.LastRenderedUVsMaterial = nullptr;
		RenderTargetCache.ColorsRenderTarget = nullptr;
		RenderTargetCache.UVsRenderTarget = nullptr;
	}

	TVoxelTexture<FColor> ColorTexture;
	TVoxelTexture<FColor> UVTexture;
	if (Settings.bImportColors)
	{
		if (!Settings.ColorsMaterial)
		{
			FVoxelMessages::Error(FUNCTION_ERROR("PaintColors = true but ColorsMaterial = nullptr"));
			return false;
		}
		if (!RenderTargetCache.ColorsRenderTarget || Settings.ColorsMaterial != RenderTargetCache.LastRenderedColorsMaterial)
		{
			RenderTargetCache.LastRenderedColorsMaterial = Settings.ColorsMaterial;
			FVoxelTextureUtilities::ClearCache(RenderTargetCache.ColorsRenderTarget);
			RenderTargetCache.ColorsRenderTarget = CreateTextureFromMaterial(
				WorldContextObject,
				Settings.ColorsMaterial,
				Settings.RenderTargetSize,
				Settings.RenderTargetSize);
		}
		ColorTexture = FVoxelTextureUtilities::CreateFromTexture_Color(RenderTargetCache.ColorsRenderTarget);
	}
	if (Settings.bImportUVs)
	{
		if (!Settings.UVsMaterial)
		{
			FVoxelMessages::Error(FUNCTION_ERROR("PaintUVChannels = true but UVChannelsMaterial = nullptr"));
			return false;
		}
		if (!RenderTargetCache.UVsRenderTarget || Settings.UVsMaterial != RenderTargetCache.LastRenderedUVsMaterial)
		{
			RenderTargetCache.LastRenderedUVsMaterial = Settings.UVsMaterial;
			FVoxelTextureUtilities::ClearCache(RenderTargetCache.UVsRenderTarget);
			RenderTargetCache.UVsRenderTarget = CreateTextureFromMaterial(
				WorldContextObject,
				Settings.UVsMaterial,
				Settings.RenderTargetSize,
				Settings.RenderTargetSize);
		}
		UVTexture = FVoxelTextureUtilities::CreateFromTexture_Color(RenderTargetCache.UVsRenderTarget);
	}
	
	OutNumLeaks = 0;
	
	TArray<FVector> Vertices;
	Vertices.Reserve(Mesh.Vertices.Num());
	FBox Box(ForceInit);
	for (auto& Vertex : Mesh.Vertices)
	{
		const auto NewVertex = Transform.TransformPosition(Vertex);
		Vertices.Add(NewVertex);
		Box += NewVertex;
	}
	Box = Box.ExpandBy(Settings.VoxelSize);

	const FVector SizeFloat = Box.GetSize() / Settings.VoxelSize;
	const FIntVector Size(FVoxelUtilities::CeilToInt(SizeFloat));
	const FVector Origin = Box.Min;

	if (int64(Size.X) * int64(Size.Y) * int64(Size.Z) >= MAX_int32)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Converted assets would have more than 2B voxels! Abording"));
		return false;
	}
	if (Size.X * Size.Y * Size.Z == 0)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Size = 0! Abording"));
		return false;
	}

	FMakeLevelSet3InData InData;
	{
		InData.Vertices = Vertices;
		InData.Triangles = Mesh.Triangles;
		InData.UVs = Mesh.UVs;

		InData.Origin = Origin;
		InData.Size = Size;

		InData.bExportPositions = false;
		InData.bExportUVs = false;

		if (Settings.bImportColors)
		{
			InData.ColorTextures.Add(ColorTexture);
		}
		if (Settings.bImportUVs)
		{
			InData.ColorTextures.Add(UVTexture);
		}
	}

	FMakeLevelSet3OutData OutData;
	MakeLevelSet3(Settings, InData, OutData);
	
	OutOffset = FVoxelUtilities::RoundToInt(Box.Min / Settings.VoxelSize);
	OutNumLeaks = OutData.NumLeaks;
	
	const bool bHasMaterials = Settings.bImportColors || Settings.bImportUVs;
	OutAsset.SetSize(Size, bHasMaterials);

	for (int32 X = 0; X < Size.X; X++)
	{
		for (int32 Y = 0; Y < Size.Y; Y++)
		{
			for (int32 Z = 0; Z < Size.Z; Z++)
			{
				OutAsset.SetValue(X, Y, Z, FVoxelValue(OutData.Phi(X, Y, Z)));
				if (bHasMaterials)
				{
					FVoxelMaterial Material(ForceInit);
					if (Settings.bImportColors)
					{
						const FColor Color = OutData.Colors[0](X, Y, Z);
						Material.SetColor(Color);
					}
					if (Settings.bImportUVs)
					{
						const FColor Color = OutData.Colors.Last()(X, Y, Z);
						Material.SetU(uint8(0), Color.R);
						Material.SetV(uint8(0), Color.G);
						Material.SetU(uint8(1), Color.B);
						Material.SetV(uint8(1), Color.A);
					}
					OutAsset.SetMaterial(X, Y, Z, Material);
				}
			}
		}
	}

	return true;
}

void UVoxelMeshImporterLibrary::ConvertMeshToDistanceField(
	const FVoxelMeshImporterInputData& Mesh,
	const FTransform& Transform,
	const FVoxelMeshImporterSettingsBase& Settings,
	float BoxExtension,
	TArray<float>& OutDistanceField,
	TArray<FVector3f>& OutSurfacePositions,
	FIntVector& OutSize,
	FIntVector& OutOffset,
	int32& OutNumLeaks,
	bool bMultiThreaded,
	int32 MaxPasses_Debug)
{
	VOXEL_FUNCTION_COUNTER();
	
	TArray<FVector> Vertices;
	Vertices.Reserve(Mesh.Vertices.Num());
	FBox Box(ForceInit);
	for (auto& Vertex : Mesh.Vertices)
	{
		const auto NewVertex = Transform.TransformPosition(Vertex);
		Vertices.Add(NewVertex);
		Box += NewVertex;
	}
	Box = Box.ExpandBy(Settings.VoxelSize * BoxExtension);

	const FVector SizeFloat = Box.GetSize() / Settings.VoxelSize;
	const FIntVector Size(FVoxelUtilities::CeilToInt(SizeFloat));
	const FVector Origin = Box.Min;

	if (int64(Size.X) * int64(Size.Y) * int64(Size.Z) >= MAX_int32)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Converted assets would have more than 2B voxels! Abording"));
		return;
	}
	if (Size.X * Size.Y * Size.Z == 0)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Size = 0! Abording"));
		return;
	}

	FMakeLevelSet3InData InData;
	{
		InData.Vertices = Vertices;
		InData.Triangles = Mesh.Triangles;
		InData.UVs = Mesh.UVs;

		InData.Origin = Origin;
		InData.Size = Size;

		InData.bExportPositions = true;
		InData.bExportUVs = false;
	}

	auto SettingsCopy = Settings;
	SettingsCopy.DistanceDivisor = 1;
	SettingsCopy.ExactBand = 0.f; // No need for anything bigger
	
	FMakeLevelSet3OutData OutData;
	MakeLevelSet3(SettingsCopy, InData, OutData);
	
	OutDistanceField = MoveTemp(OutData.Phi.Data);
	OutSurfacePositions = MoveTemp(OutData.Positions.Data);

	OutSize = Size;
	OutOffset = FVoxelUtilities::RoundToInt(Box.Min / Settings.VoxelSize);
	OutNumLeaks = OutData.NumLeaks;

	// Propagate distances
	FVoxelDistanceFieldUtilities::JumpFlood(Size, OutSurfacePositions, bMultiThreaded, MaxPasses_Debug);
	FVoxelDistanceFieldUtilities::GetDistancesFromSurfacePositions(Size, OutSurfacePositions, OutDistanceField);
}

UVoxelMeshImporterInputData* UVoxelMeshImporterLibrary::CreateMeshDataFromStaticMesh(UStaticMesh* StaticMesh)
{
	VOXEL_FUNCTION_COUNTER();
	
	if (!StaticMesh)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Invalid StaticMesh"));
		return nullptr;
	}
	auto* Object = NewObject<UVoxelMeshImporterInputData>(GetTransientPackage());
	CreateMeshDataFromStaticMesh(StaticMesh, Object->Data);
	return Object;
}

UTextureRenderTarget2D* UVoxelMeshImporterLibrary::CreateTextureFromMaterial(
	UObject* WorldContextObject,
	UMaterialInterface* Material,
	int32 Width,
	int32 Height)
{
	VOXEL_FUNCTION_COUNTER();
	
	if (!WorldContextObject)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Invalid WorldContextObject"));
		return nullptr;
	}
	if (!Material)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Invalid Material"));
		return nullptr;
	}
	if (Width <= 0)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Width <= 0"));
		return nullptr;
	}
	if (Height <= 0)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Height <= 0"));
		return nullptr;
	}

	UTextureRenderTarget2D* RenderTarget2D = UKismetRenderingLibrary::CreateRenderTarget2D(WorldContextObject, Width, Height, ETextureRenderTargetFormat::RTF_RGBA8);
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(WorldContextObject, RenderTarget2D, Material);
	return RenderTarget2D;
}

void UVoxelMeshImporterLibrary::ConvertMeshToVoxels(
	UObject* WorldContextObject,
	UVoxelMeshImporterInputData* Mesh,
	FTransform Transform,
	bool bSubtractive,
	FVoxelMeshImporterSettings Settings,
	FVoxelMeshImporterRenderTargetCache& RenderTargetCache,
	UVoxelDataAsset*& Asset,
	int32& NumLeaks)
{
	VOXEL_FUNCTION_COUNTER();
	
	if (!Mesh)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Invalid Mesh"));
		return;
	}

	if (bSubtractive)
	{
		Settings.DistanceDivisor *= -1;
	}

	FIntVector PositionOffset{ ForceInit };

	const auto Data = MakeVoxelShared<FVoxelDataAssetData>();
	if (!ConvertMeshToVoxels(WorldContextObject, Mesh->Data, Transform, Settings, RenderTargetCache, *Data, PositionOffset, NumLeaks))
	{
		return;
	}
	
	Asset = NewObject<UVoxelDataAsset>(GetTransientPackage());
	Asset->bSubtractiveAsset = bSubtractive;
	Asset->PositionOffset = PositionOffset;
	Asset->SetData(Data);
}

void UVoxelMeshImporterLibrary::ConvertMeshToVoxels_NoMaterials(
	UObject* WorldContextObject, 
	UVoxelMeshImporterInputData* Mesh, 
	FTransform Transform, 
	bool bSubtractive, 
	FVoxelMeshImporterSettingsBase Settings, 
	UVoxelDataAsset*& Asset, 
	int32& NumLeaks)
{
	FVoxelMeshImporterRenderTargetCache RenderTargetCache;
	ConvertMeshToVoxels(WorldContextObject, Mesh, Transform, bSubtractive, FVoxelMeshImporterSettings(Settings), RenderTargetCache, Asset, NumLeaks);

	ensure(!RenderTargetCache.ColorsRenderTarget);
	ensure(!RenderTargetCache.UVsRenderTarget);
	ensure(!RenderTargetCache.LastRenderedColorsMaterial);
	ensure(!RenderTargetCache.LastRenderedUVsMaterial);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AVoxelMeshImporter::AVoxelMeshImporter()
{
#if WITH_EDITOR
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");

	StaticMesh = FVoxelExampleUtilities::LoadExampleObject<UStaticMesh>(TEXT("/Voxel/Examples/Importers/Chair/VoxelExample_SM_Chair"));
	MeshComponent->SetStaticMesh(StaticMesh);
	MeshComponent->SetRelativeScale3D(FVector(100.f));
	RootComponent = MeshComponent;

	PrimaryActorTick.bCanEverTick = true;
#endif
}

void AVoxelMeshImporter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if (GetWorld()->WorldType != EWorldType::Editor)
	{
		Destroy();
	}

	if (StaticMesh)
	{
		if (CachedStaticMesh != StaticMesh)
		{
			CachedStaticMesh = StaticMesh;

			TArray<uint32> Indices;
			TArray<FVector2D> UVs;
			CachedVertices.Reset();
			GetMergedSectionFromStaticMesh(StaticMesh, 0, CachedVertices, Indices, UVs);
		}

		// TODO: Use PostEditMove
		const FTransform Transform = GetTransform();
		if (CachedTransform.ToMatrixWithScale() != Transform.ToMatrixWithScale())
		{
			CachedTransform = Transform;

			CachedBox = FBox(ForceInit);
			for (auto& Vertex : CachedVertices)
			{
				CachedBox += Transform.TransformPosition(Vertex);
			}
			CachedBox = CachedBox.ExpandBy(Settings.VoxelSize);

			InitMaterialInstance();
			MaterialInstance->SetVectorParameterValue("Offset", CachedBox.Min);
		}

		UpdateSizes();
	}
}

#if WITH_EDITOR
void AVoxelMeshImporter::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	MeshComponent->SetStaticMesh(StaticMesh);
	InitMaterialInstance();
	MaterialInstance->SetScalarParameterValue("VoxelSize", Settings.VoxelSize);
	UpdateSizes();
}
#endif

void AVoxelMeshImporter::InitMaterialInstance()
{
	if (MaterialInstance)
	{
		return;
	}
	auto* Material = LoadObject<UMaterial>(nullptr, TEXT("Material'/Voxel/MaterialHelpers/MeshImporterMaterial.MeshImporterMaterial'"));
	MaterialInstance = UMaterialInstanceDynamic::Create(Material, GetTransientPackage());
	MeshComponent->SetMaterial(0, MaterialInstance);
	MaterialInstance->SetScalarParameterValue("VoxelSize", Settings.VoxelSize); // To have it on start
}

void AVoxelMeshImporter::UpdateSizes()
{
	const FVector SizeFloat = CachedBox.GetSize() / Settings.VoxelSize;
	SizeX = FMath::CeilToInt(SizeFloat.X);
	SizeY = FMath::CeilToInt(SizeFloat.Y);
	SizeZ = FMath::CeilToInt(SizeFloat.Z);
	NumberOfVoxels = SizeX * SizeY * SizeZ;
	const bool bHasMaterials = Settings.bImportColors || Settings.bImportUVs;
	SizeInMB = double(NumberOfVoxels) * (sizeof(FVoxelValue) + (bHasMaterials ? sizeof(FVoxelMaterial) : 0)) / double(1 << 20);
}