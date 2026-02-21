// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMContentEditorStyle.h"
#include "Configuration/SMEditorStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "SMContentEditorStyle"

// See SlateEditorStyle.cpp
#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)
#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( FSMEditorStyle::InResources(RelativePath, ".png"), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( FSMEditorStyle::InResources(RelativePath, ".png"), __VA_ARGS__ )

TSharedPtr<FSlateStyleSet> FSMContentEditorStyle::StyleSetInstance = nullptr;

static const FVector2D Icon16x16(16.0f, 16.0f);
static const FVector2D Icon40x40(40.0f, 40.0f);
static const FVector2D Icon64x64(64.0f, 64.0f);
static const FVector2D Icon128x128(128.0f, 128.0f);

void FSMContentEditorStyle::Initialize()
{
	// Only init once.
	if(StyleSetInstance.IsValid())
	{
		return;
	}

	StyleSetInstance = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	StyleSetInstance->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSetInstance->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));
	
	SetIcons();

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSetInstance.Get());
}

void FSMContentEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSetInstance.Get());
	ensure(StyleSetInstance.IsUnique());
	StyleSetInstance.Reset();
}

void FSMContentEditorStyle::SetIcons()
{
	StyleSetInstance->Set("InstallContentIcon", new IMAGE_BRUSH(TEXT("Icons/InstallContentIcon_16"), Icon16x16));
}

#undef DEFAULT_FONT
#undef IMAGE_BRUSH
#undef BOX_BRUSH

#undef LOCTEXT_NAMESPACE
