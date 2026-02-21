// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMExtendedEditorStyle.h"

#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleRegistry.h"

#define LOCTEXT_NAMESPACE "SMExtendedEditorStyle"

// See SlateEditorStyle.cpp
#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)
#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( FSMEditorStyle::InResources( RelativePath, ".png" ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( FSMEditorStyle::InResources( RelativePath, ".png" ), __VA_ARGS__ )

TSharedPtr<FSlateStyleSet> FSMExtendedEditorStyle::StyleSetInstance = nullptr;

static const FVector2D Icon16x16(16.0f, 16.0f);
static const FVector2D Icon40x40(40.0f, 40.0f);
static const FVector2D Icon64x64(64.0f, 64.0f);
static const FVector2D Icon128x128(128.0f, 128.0f);

void FSMExtendedEditorStyle::Initialize()
{
	// Only init once.
	if (StyleSetInstance.IsValid())
	{
		return;
	}

	StyleSetInstance = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	StyleSetInstance->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSetInstance->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	SetGraphStyles();
	SetBrushes();
	SetIcons();

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSetInstance.Get());
}

void FSMExtendedEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSetInstance.Get());
	ensure(StyleSetInstance.IsUnique());
	StyleSetInstance.Reset();
}

void FSMExtendedEditorStyle::SetGraphStyles()
{
	FTextBlockStyle GraphNodeTextProperty = FTextBlockStyle(NormalText)
		.SetFont(DEFAULT_FONT("Regular", 12))
		.SetColorAndOpacity(FLinearColor(208.0f / 255.0f, 227.0f / 255.0f, 242.0f / 255.0f));
	GraphNodeTextProperty.Font.OutlineSettings.OutlineSize = 1;
	
	StyleSetInstance->Set("SMExtendedEditor.Graph.Property.Text", GraphNodeTextProperty);

	const FButtonStyle GraphNodeButtonProperty = FButtonStyle()
		.SetNormal(BOX_BRUSH("Brushes/Button", FVector2D(32, 32), 8.0f / 32.0f))
		.SetHovered(BOX_BRUSH("Brushes/Button_Hovered", FVector2D(32, 32), 8.0f / 32.0f))
		.SetPressed(BOX_BRUSH("Brushes/Button_Pressed", FVector2D(32, 32), 8.0f / 32.0f))
		.SetDisabled(BOX_BRUSH("Brushes/Button_Disabled", 8.0f / 32.0f))
		.SetNormalPadding(FMargin(0, 0, 0, 0))
		.SetPressedPadding(FMargin(2, 3, 2, 1));

	StyleSetInstance->Set("SMExtendedEditor.Graph.Property.Button", GraphNodeButtonProperty);
}

void FSMExtendedEditorStyle::SetBrushes()
{
}

void FSMExtendedEditorStyle::SetIcons()
{
}

#undef DEFAULT_FONT
#undef IMAGE_BRUSH
#undef BOX_BRUSH

#undef LOCTEXT_NAMESPACE
