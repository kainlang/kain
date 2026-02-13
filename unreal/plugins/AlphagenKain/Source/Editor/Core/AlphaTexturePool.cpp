// Copyright 2026 K-Studio. All Rights Reserved.

#include "Core/AlphaTexturePool.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "ImageUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UAlphaTexturePool* UAlphaTexturePool::Instance = nullptr;

UAlphaTexturePool* UAlphaTexturePool::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UAlphaTexturePool>();
		Instance->AddToRoot(); // Prevent garbage collection
	}
	return Instance;
}

int64 UAlphaTexturePool::LoadFromFile(const FString& Path, const FString& InName)
{
	// Read file bytes
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *Path))
	{
		UE_LOG(LogTemp, Error, TEXT("AlphaPool: Failed to read file: %s"), *Path);
		return 0;
	}
	
	// Determine name from filename if not provided
	FString Name = InName;
	if (Name.IsEmpty())
	{
		Name = FPaths::GetBaseFilename(Path);
	}
	
	// Create FAlphaSource::File info
	int64 Handle = LoadFromBytes(FileData, Name, EAlphaSource::File);
	
	if (Handle > 0)
	{
		// Store the source path
		if (FAlphaTexture* Tex = Get(Handle))
		{
			Tex->SourcePath = Path;
		}
	}
	
	return Handle;
}

int64 UAlphaTexturePool::LoadFromBytes(const TArray<uint8>& Bytes, const FString& Name, EAlphaSource Source)
{
	// Use image wrapper to decode
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	
	// Try PNG first, then TGA, then JPEG
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	
	if (!ImageWrapper->SetCompressed(Bytes.GetData(), Bytes.Num()))
	{
		ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
		if (!ImageWrapper->SetCompressed(Bytes.GetData(), Bytes.Num()))
		{
			UE_LOG(LogTemp, Error, TEXT("AlphaPool: Failed to decode image: %s"), *Name);
			return 0;
		}
	}
	
	// Get raw image data
	TArray<uint8> RawData;
	if (!ImageWrapper->GetRaw(ERGBFormat::Gray, 8, RawData))
	{
		// Try getting as BGRA and convert
		if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
		{
			UE_LOG(LogTemp, Error, TEXT("AlphaPool: Failed to get raw image data: %s"), *Name);
			return 0;
		}
		
		// Convert BGRA to grayscale
		int32 PixelCount = RawData.Num() / 4;
		TArray<uint8> GrayData;
		GrayData.SetNumUninitialized(PixelCount);
		
		for (int32 I = 0; I < PixelCount; I++)
		{
			// Simple luminance calculation
			float R = RawData[I * 4 + 2] / 255.0f;
			float G = RawData[I * 4 + 1] / 255.0f;
			float B = RawData[I * 4 + 0] / 255.0f;
			float Gray = 0.2126f * R + 0.7152f * G + 0.0722f * B;
			GrayData[I] = static_cast<uint8>(FMath::Clamp(Gray * 255.0f, 0.0f, 255.0f));
		}
		
		RawData = MoveTemp(GrayData);
	}
	
	int32 Width = ImageWrapper->GetWidth();
	int32 Height = ImageWrapper->GetHeight();
	
	// Create texture - bypass streaming pool to prevent eviction
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_G8);
	if (!Texture)
	{
		UE_LOG(LogTemp, Error, TEXT("AlphaPool: Failed to create texture: %s"), *Name);
		return 0;
	}
	
	// Critical: Disable streaming to prevent pool eviction (gray background fix)
	Texture->NeverStream = true;
	
	// Configure texture
	Texture->CompressionSettings = TC_Grayscale;
	Texture->SRGB = false;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->LODGroup = TEXTUREGROUP_Pixels2D; // Non-streaming group
	
	// Upload data
	void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();
	
	return LoadFromTexture(Texture, Name, Source);
}

int64 UAlphaTexturePool::LoadFromTexture(UTexture2D* Texture, const FString& Name, EAlphaSource Source)
{
	if (!Texture)
	{
		return 0;
	}
	
	// Assign handle
	int64 Handle = NextHandle++;
	
	// Create texture entry
	FAlphaTexture& Entry = Textures.Add(Handle);
	Entry.Texture = Texture;
	Entry.Width = Texture->GetSizeX();
	Entry.Height = Texture->GetSizeY();
	Entry.Name = Name;
	Entry.Source = Source;
	Entry.ThumbnailTexture = GenerateThumbnail(Texture);
	
	// Create metadata entry
	FAlphaInfo& Info = Metadata.Add(Handle);
	Info.handle = Handle;
	Info.name = Name;
	Info.width = Entry.Width;
	Info.height = Entry.Height;
	Info.source = Source;
	// Preview base64 would be generated here if needed
	
	UE_LOG(LogTemp, Log, TEXT("AlphaPool: Loaded '%s' as handle %lld (%dx%d)"), *Name, Handle, Entry.Width, Entry.Height);
	
	return Handle;
}

FAlphaTexture* UAlphaTexturePool::Get(int64 Handle)
{
	return Textures.Find(Handle);
}

FAlphaInfo UAlphaTexturePool::GetInfo(int64 Handle) const
{
	const FAlphaInfo* Info = Metadata.Find(Handle);
	return Info ? *Info : FAlphaInfo();
}

TArray<FAlphaInfo> UAlphaTexturePool::List() const
{
	TArray<FAlphaInfo> Result;
	Metadata.GenerateValueArray(Result);
	return Result;
}

bool UAlphaTexturePool::Dispose(int64 Handle)
{
	bool bRemoved = Textures.Remove(Handle) > 0;
	Metadata.Remove(Handle);
	
	if (bRemoved)
	{
		UE_LOG(LogTemp, Log, TEXT("AlphaPool: Disposed handle %lld"), Handle);
	}
	
	return bRemoved;
}

void UAlphaTexturePool::Clear()
{
	Textures.Empty();
	Metadata.Empty();
	
	UE_LOG(LogTemp, Log, TEXT("AlphaPool: Cleared all alphas"));
}

UTexture2D* UAlphaTexturePool::GenerateThumbnail(UTexture2D* Source)
{
	if (!Source)
	{
		return nullptr;
	}
	
	// For now, just return the source texture as thumbnail
	// A proper implementation would downscale to 64x64
	// This would require reading back texture data and resampling
	
	return Source;
}
