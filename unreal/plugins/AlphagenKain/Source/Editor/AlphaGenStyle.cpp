// Copyright 2026 K-Studio. All Rights Reserved.

#include "AlphaGenStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FAlphaGenStyle::StyleInstance = nullptr;

void FAlphaGenStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FAlphaGenStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

void FAlphaGenStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

FName FAlphaGenStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("AlphaGenStyle"));
	return StyleSetName;
}

const ISlateStyle& FAlphaGenStyle::Get()
{
	return *StyleInstance;
}

TSharedRef<FSlateStyleSet> FAlphaGenStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	
	// Get plugin resources directory
	FString PluginBaseDir = IPluginManager::Get().FindPlugin(TEXT("AlphaGen"))->GetBaseDir();
	FString ResourcesDir = PluginBaseDir / TEXT("Resources");
	
	Style->SetContentRoot(ResourcesDir);
	
	// Define all icon sizes for various UI contexts
	
	// 16x16 - Menu items, small UI elements
	FString Icon16Path = ResourcesDir / TEXT("Icon16.png");
	Style->Set("AlphaGen.MenuIcon", new FSlateImageBrush(
		Icon16Path,
		FVector2D(16.0f, 16.0f)
	));
	Style->Set("AlphaGen.Icon16", new FSlateImageBrush(
		Icon16Path,
		FVector2D(16.0f, 16.0f)
	));
	
	// 20x20 - Compact toolbar buttons
	FString Icon20Path = ResourcesDir / TEXT("Icon20.png");
	Style->Set("AlphaGen.SmallIcon", new FSlateImageBrush(
		Icon20Path,
		FVector2D(20.0f, 20.0f)
	));
	Style->Set("AlphaGen.Icon20", new FSlateImageBrush(
		Icon20Path,
		FVector2D(20.0f, 20.0f)
	));
	
	// 40x40 - Main toolbar icon (most visible)
	FString Icon40Path = ResourcesDir / TEXT("Icon40.png");
	Style->Set("AlphaGen.ToolbarIcon", new FSlateImageBrush(
		Icon40Path,
		FVector2D(40.0f, 40.0f)
	));
	Style->Set("AlphaGen.Icon40", new FSlateImageBrush(
		Icon40Path,
		FVector2D(40.0f, 40.0f)
	));
	
	// 64x64 - Medium displays
	FString Icon64Path = ResourcesDir / TEXT("Icon64.png");
	Style->Set("AlphaGen.MediumIcon", new FSlateImageBrush(
		Icon64Path,
		FVector2D(64.0f, 64.0f)
	));
	Style->Set("AlphaGen.Icon64", new FSlateImageBrush(
		Icon64Path,
		FVector2D(64.0f, 64.0f)
	));
	
	// 128x128 - Plugin browser, large displays
	FString Icon128Path = ResourcesDir / TEXT("Icon128.png");
	Style->Set("AlphaGen.LargeIcon", new FSlateImageBrush(
		Icon128Path,
		FVector2D(128.0f, 128.0f)
	));
	Style->Set("AlphaGen.Icon128", new FSlateImageBrush(
		Icon128Path,
		FVector2D(128.0f, 128.0f)
	));
	
	// 256x256 - High-DPI displays, marketing
	FString Icon256Path = ResourcesDir / TEXT("Icon256.png");
	Style->Set("AlphaGen.ExtraLargeIcon", new FSlateImageBrush(
		Icon256Path,
		FVector2D(256.0f, 256.0f)
	));
	Style->Set("AlphaGen.Icon256", new FSlateImageBrush(
		Icon256Path,
		FVector2D(256.0f, 256.0f)
	));
	
	return Style;
}
