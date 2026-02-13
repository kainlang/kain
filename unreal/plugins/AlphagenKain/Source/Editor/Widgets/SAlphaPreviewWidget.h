// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UTexture2D;

/**
 * SAlphaPreviewWidget
 * 
 * Displays a zoomable, pannable preview of an alpha texture.
 * Renders grayscale with optional colorization.
 */
class SAlphaPreviewWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAlphaPreviewWidget)
		: _InitialTexture(nullptr)
	{}
		SLATE_ARGUMENT(UTexture2D*, InitialTexture)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	
	/** Set the texture to preview */
	void SetTexture(UTexture2D* InTexture);
	
	/** Get current texture */
	UTexture2D* GetTexture() const { return CurrentTexture; }
	
	/** Get current zoom level */
	float GetZoom() const { return ZoomLevel; }
	
	/** Set zoom level */
	void SetZoom(float NewZoom) { ZoomLevel = FMath::Clamp(NewZoom, 0.1f, 10.0f); Invalidate(EInvalidateWidgetReason::Paint); }
	
	/** Reset view to default */
	void ResetView() { ZoomLevel = 1.0f; PanOffset = FVector2D::ZeroVector; Invalidate(EInvalidateWidgetReason::Paint); }
	
	// SWidget interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	/** The texture being previewed */
	UTexture2D* CurrentTexture = nullptr;
	
	/** Zoom level (1.0 = 100%) */
	float ZoomLevel = 1.0f;
	
	/** Pan offset */
	FVector2D PanOffset = FVector2D::ZeroVector;
	
	/** Is middle mouse button dragging */
	bool bIsPanning = false;
	
	/** Last mouse position for drag calculation */
	FVector2D LastMousePosition = FVector2D::ZeroVector;
	
	/** Brush for rendering the texture */
	mutable FSlateBrush TextureBrush;
};
