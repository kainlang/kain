// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

class SAlphaPreviewWidget;
class SEditableTextBox;

enum class EProceduralType : uint8
{
	Radial,
	Circle,
	Square,
	Diamond,
	Perlin,
	Voronoi,
	Bricks,
	Dots,
	SeamlessNoise,
	Crosshatch,
	Waves,
	Checkerboard,
	Hexagon,
	Tears,
	Scratches,
	Splatter,
	Cracks,
	Cells,
	Grunge,
	Fibers,
	Caustics
};

/**
 * SAlphaGenWidget - Procedural Alpha Texture Generator
 * Sleek 2-panel editor, pure generation (no post-processing)
 */
class SAlphaGenWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAlphaGenWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAlphaGenWidget();

private:
	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildPreviewPanel();
	TSharedRef<SWidget> BuildControlsPanel();
	TSharedRef<SWidget> BuildGeneratorSection();
	TSharedRef<SWidget> BuildParameterSection();
	TSharedRef<SWidget> BuildExportSection();
	
	TSharedRef<SWidget> BuildRadialParams();
	TSharedRef<SWidget> BuildShapeParams();
	TSharedRef<SWidget> BuildPerlinParams();
	TSharedRef<SWidget> BuildVoronoiParams();
	TSharedRef<SWidget> BuildBricksParams();
	TSharedRef<SWidget> BuildDotsParams();
	TSharedRef<SWidget> BuildSeamlessNoiseParams();
	TSharedRef<SWidget> BuildCrosshatchParams();
	TSharedRef<SWidget> BuildWavesParams();
	TSharedRef<SWidget> BuildCheckerboardParams();
	TSharedRef<SWidget> BuildHexagonParams();
	TSharedRef<SWidget> BuildTearsParams();
	TSharedRef<SWidget> BuildScratchesParams();
	TSharedRef<SWidget> BuildSplatterParams();
	TSharedRef<SWidget> BuildCracksParams();
	TSharedRef<SWidget> BuildCellsParams();
	TSharedRef<SWidget> BuildGrungeParams();
	TSharedRef<SWidget> BuildFibersParams();
	TSharedRef<SWidget> BuildCausticsParams();
	
	TSharedRef<SWidget> MakeSlider(const FText& Label, float Min, float Max, float* Value, bool bInteger = false);
	TSharedRef<SWidget> MakeSection(const FText& Title, TSharedRef<SWidget> Content);
	
	void OnTypeChanged(TSharedPtr<FString> NewType, ESelectInfo::Type);
	void OnExportToProject();
	void OnSaveToDisk();
	void RefreshPreview();
	void UpdateParameterPanel();

	int32 GetPreviewSize() const { return PreviewSize; }

private:
	EProceduralType CurrentType = EProceduralType::Radial;
	int32 PreviewSize = 512;
	int32 ExportSize = 1024;
	
	float ParamFalloff = 2.0f;
	float ParamScale = 0.8f;
	float ParamOctaves = 4.0f;
	float ParamSeed = 0.0f;
	float ParamNoiseType = 0.0f;
	float ParamEdgeSoftness = 0.02f;
	float ParamEdgeWidth = 0.05f;
	float ParamBrickWidth = 0.25f;
	float ParamBrickHeight = 0.1f;
	float ParamMortar = 0.02f;
	float ParamDotSize = 0.08f;
	float ParamSpacing = 0.15f;
	float ParamSeamlessNoiseType = 0.0f;
	
	// New generator params
	float ParamThickness = 0.1f;
	float ParamAngle = 0.785f; // 45 degrees
	float ParamFrequency = 8.0f;
	float ParamAmplitude = 0.5f;
	float ParamWaveType = 0.0f; // 0=horizontal, 1=vertical, 2=radial
	float ParamLength = 0.8f;
	float ParamDensity = 20.0f;
	float ParamSize = 0.3f;
	float ParamWidth = 0.1f;
	float ParamContrast = 2.0f;
	float ParamDetail = 1.0f;
	float ParamTime = 0.0f;
	
	FString ExportPath = TEXT("/Game/Alphas");
	FString ExportName = TEXT("Alpha");
	
	TSharedPtr<SAlphaPreviewWidget> PreviewWidget;
	TSharedPtr<STextComboBox> TypeCombo;
	TSharedPtr<SWidgetSwitcher> ParamSwitcher;
	TSharedPtr<SEditableTextBox> ExportPathBox;
	TSharedPtr<SEditableTextBox> ExportNameBox;
	TSharedPtr<STextComboBox> NoiseTypeCombo;
	TSharedPtr<STextComboBox> SeamlessNoiseTypeCombo;
	
	TArray<TSharedPtr<FString>> TypeOptions;
	TArray<TSharedPtr<FString>> NoiseTypeOptions;
	TArray<TSharedPtr<FString>> SeamlessNoiseTypeOptions;
	TArray<TSharedPtr<FString>> WaveTypeOptions;
	TArray<TSharedPtr<FString>> PreviewResOptions;
	TArray<TSharedPtr<FString>> ExportResOptions;
	
	class UTexture2D* PreviewTexture = nullptr;
};
