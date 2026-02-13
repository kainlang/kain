// Copyright 2026 K-Studio. All Rights Reserved.

#include "Widgets/SAlphaPreviewWidget.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"

void SAlphaPreviewWidget::Construct(const FArguments& InArgs)
{
	CurrentTexture = InArgs._InitialTexture;
	
	// Configure the brush for texture rendering
	TextureBrush.DrawAs = ESlateBrushDrawType::Image;
	TextureBrush.Tiling = ESlateBrushTileType::NoTile;
	TextureBrush.ImageSize = FVector2D(256.0f, 256.0f);
}

void SAlphaPreviewWidget::SetTexture(UTexture2D* InTexture)
{
	CurrentTexture = InTexture;
	
	if (CurrentTexture)
	{
		TextureBrush.SetResourceObject(CurrentTexture);
		TextureBrush.ImageSize = FVector2D(CurrentTexture->GetSizeX(), CurrentTexture->GetSizeY());
	}
	else
	{
		TextureBrush.SetResourceObject(nullptr);
	}
	
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 SAlphaPreviewWidget::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	// Draw background checkerboard pattern (indicates transparency)
	const FSlateBrush* CheckerBrush = FCoreStyle::Get().GetBrush("Checkerboard");
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		CheckerBrush,
		ESlateDrawEffect::None,
		FLinearColor::White
	);
	
	LayerId++;
	
	// Draw texture if available
	if (CurrentTexture)
	{
		FVector2D TextureSize = TextureBrush.ImageSize * ZoomLevel;
		FVector2D GeometrySize = AllottedGeometry.GetLocalSize();
		
		// Center the texture with pan offset
		FVector2D Position = (GeometrySize - TextureSize) * 0.5f + PanOffset;
		
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f(TextureSize), FSlateLayoutTransform(FVector2f(Position))),
			&TextureBrush,
			ESlateDrawEffect::None,
			FLinearColor::White
		);
		
		LayerId++;
	}
	else
	{
		// Draw placeholder text centered in the widget
		const FText PlaceholderText = FText::FromString(TEXT("No Alpha Generated"));
		
		FSlateFontInfo FontInfo = FCoreStyle::Get().GetFontStyle("NormalFont");
		FontInfo.Size = 14;
		
		// Simple centered text without measuring
		FVector2D GeometrySize = AllottedGeometry.GetLocalSize();
		FVector2D TextPosition = GeometrySize * 0.5f - FVector2D(80.0f, 7.0f); // Approximate centering
		
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f(160.0f, 14.0f), FSlateLayoutTransform(FVector2f(TextPosition))),
			PlaceholderText,
			FontInfo,
			ESlateDrawEffect::None,
			FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)
		);
		
		LayerId++;
	}
	
	return LayerId;
}

FReply SAlphaPreviewWidget::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	float Delta = MouseEvent.GetWheelDelta();
	float ZoomFactor = 1.1f;
	
	if (Delta > 0)
	{
		ZoomLevel *= ZoomFactor;
	}
	else
	{
		ZoomLevel /= ZoomFactor;
	}
	
	// Clamp zoom level
	ZoomLevel = FMath::Clamp(ZoomLevel, 0.1f, 10.0f);
	
	Invalidate(EInvalidateWidgetReason::Paint);
	
	return FReply::Handled();
}

FReply SAlphaPreviewWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		bIsPanning = true;
		LastMousePosition = MouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	
	return FReply::Unhandled();
}

FReply SAlphaPreviewWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		bIsPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	
	return FReply::Unhandled();
}

FReply SAlphaPreviewWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bIsPanning)
	{
		FVector2D CurrentMousePosition = MouseEvent.GetScreenSpacePosition();
		FVector2D Delta = CurrentMousePosition - LastMousePosition;
		
		PanOffset += Delta;
		LastMousePosition = CurrentMousePosition;
		
		Invalidate(EInvalidateWidgetReason::Paint);
		
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

FCursorReply SAlphaPreviewWidget::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (bIsPanning)
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
	}
	
	return FCursorReply::Cursor(EMouseCursor::Crosshairs);
}
