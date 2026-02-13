// Copyright 2026 K-Studio. All Rights Reserved.
// AlphaGen - Pure procedural alpha generator, no post-processing overhead

#include "Widgets/SAlphaGenWidget.h"
#include "Widgets/SAlphaPreviewWidget.h"
#include "Generators/AlphaProceduralGenerator.h"
#include "AlphaGenVersionCompat.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ObjectTools.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "SAlphaGenWidget"

namespace AlphaGenColors
{
	const FLinearColor DarkBg(0.02f, 0.02f, 0.025f, 1.0f);
	const FLinearColor PanelBg(0.05f, 0.05f, 0.055f, 1.0f);
	const FLinearColor AccentGreen(0.2f, 0.7f, 0.4f, 1.0f);
	const FLinearColor TextDim(0.5f, 0.5f, 0.55f, 1.0f);
	const FLinearColor TextBright(0.9f, 0.9f, 0.92f, 1.0f);
}

void SAlphaGenWidget::Construct(const FArguments& InArgs)
{
	TypeOptions.Add(MakeShared<FString>(TEXT("Radial")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Circle")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Square")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Diamond")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Perlin")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Voronoi")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Bricks")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Dots")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Seamless Noise")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Crosshatch")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Waves")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Checkerboard")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Hexagon")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Tears")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Scratches")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Splatter")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Cracks")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Cells")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Grunge")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Fibers")));
	TypeOptions.Add(MakeShared<FString>(TEXT("Caustics")));
	
	NoiseTypeOptions.Add(MakeShared<FString>(TEXT("Perlin")));
	NoiseTypeOptions.Add(MakeShared<FString>(TEXT("Simplex")));
	NoiseTypeOptions.Add(MakeShared<FString>(TEXT("Ridged")));
	NoiseTypeOptions.Add(MakeShared<FString>(TEXT("Billowy")));
	NoiseTypeOptions.Add(MakeShared<FString>(TEXT("Worley")));
	
	SeamlessNoiseTypeOptions.Add(MakeShared<FString>(TEXT("Standard")));
	SeamlessNoiseTypeOptions.Add(MakeShared<FString>(TEXT("Ridged")));
	SeamlessNoiseTypeOptions.Add(MakeShared<FString>(TEXT("Billowy")));
	
	WaveTypeOptions.Add(MakeShared<FString>(TEXT("Horizontal")));
	WaveTypeOptions.Add(MakeShared<FString>(TEXT("Vertical")));
	WaveTypeOptions.Add(MakeShared<FString>(TEXT("Radial")));
	
	PreviewResOptions.Add(MakeShared<FString>(TEXT("256")));
	PreviewResOptions.Add(MakeShared<FString>(TEXT("512")));
	PreviewResOptions.Add(MakeShared<FString>(TEXT("1024")));
	PreviewResOptions.Add(MakeShared<FString>(TEXT("2048")));
	
	ExportResOptions.Add(MakeShared<FString>(TEXT("64")));
	ExportResOptions.Add(MakeShared<FString>(TEXT("128")));
	ExportResOptions.Add(MakeShared<FString>(TEXT("256")));
	ExportResOptions.Add(MakeShared<FString>(TEXT("512")));
	ExportResOptions.Add(MakeShared<FString>(TEXT("1024")));
	ExportResOptions.Add(MakeShared<FString>(TEXT("2048")));
	ExportResOptions.Add(MakeShared<FString>(TEXT("4096")));
	ExportResOptions.Add(MakeShared<FString>(TEXT("8192")));

	ChildSlot
	[
		SNew(SBorder).BorderBackgroundColor(AlphaGenColors::DarkBg).Padding(0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight() [ BuildToolbar() ]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8, 4, 8, 8)
			[
				SNew(SSplitter).Orientation(Orient_Horizontal).PhysicalSplitterHandleSize(2.0f)
				+ SSplitter::Slot().Value(0.6f) [ BuildPreviewPanel() ]
				+ SSplitter::Slot().Value(0.4f) [ BuildControlsPanel() ]
			]
		]
	];
	
	RefreshPreview();
}

SAlphaGenWidget::~SAlphaGenWidget()
{
	if (PreviewTexture)
	{
		PreviewTexture->RemoveFromRoot();
		PreviewTexture = nullptr;
	}
}

TSharedRef<SWidget> SAlphaGenWidget::BuildToolbar()
{
	return SNew(SBorder).BorderBackgroundColor(AlphaGenColors::PanelBg).Padding(FMargin(16, 10))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(LOCTEXT("Title", "ALPHAGEN"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
			.ColorAndOpacity(AlphaGenColors::TextBright)
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f) [ SNullWidget::NullWidget ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
		[ SNew(STextBlock).Text(LOCTEXT("PreviewRes", "Preview")).ColorAndOpacity(AlphaGenColors::TextDim) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(80)
			[
				SNew(STextComboBox).OptionsSource(&PreviewResOptions)
				.InitiallySelectedItem(PreviewResOptions[1]) // 512
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> S, ESelectInfo::Type) {
					if (!S.IsValid()) return;
					PreviewSize = FCString::Atoi(**S);
					RefreshPreview();
				})
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16, 0, 8, 0)
		[ SNew(STextBlock).Text(LOCTEXT("Zoom", "Zoom")).ColorAndOpacity(AlphaGenColors::TextDim) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(100)
			[
				SNew(SSpinBox<float>).MinValue(0.1f).MaxValue(10.0f).Delta(0.1f)
				.Value_Lambda([this]() { return PreviewWidget.IsValid() ? PreviewWidget->GetZoom() : 1.0f; })
				.OnValueChanged_Lambda([this](float NewZoom) {
					if (PreviewWidget.IsValid())
					{
						PreviewWidget->SetZoom(NewZoom);
					}
				})
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
		[
			SNew(SButton).Text(LOCTEXT("ResetView", "Reset"))
			.OnClicked_Lambda([this]() {
				if (PreviewWidget.IsValid())
				{
					PreviewWidget->ResetView();
				}
				return FReply::Handled();
			})
		]
	];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildPreviewPanel()
{
	return SNew(SBorder).BorderBackgroundColor(AlphaGenColors::DarkBg)
	[
		SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[ SAssignNew(PreviewWidget, SAlphaPreviewWidget) ]
	];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildControlsPanel()
{
	return SNew(SBorder).BorderBackgroundColor(AlphaGenColors::PanelBg).Padding(12)
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight() [ BuildGeneratorSection() ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0) [ BuildParameterSection() ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0) [ BuildExportSection() ]
		]
	];
}

TSharedRef<SWidget> SAlphaGenWidget::MakeSection(const FText& Title, TSharedRef<SWidget> Content)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
		[ SNew(STextBlock).Text(Title).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10)).ColorAndOpacity(AlphaGenColors::TextDim) ]
		+ SVerticalBox::Slot().AutoHeight() [ Content ];
}

TSharedRef<SWidget> SAlphaGenWidget::MakeSlider(const FText& Label, float Min, float Max, float* Value, bool bInteger)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center)
		[ SNew(STextBlock).Text(Label).ColorAndOpacity(AlphaGenColors::TextDim) ]
		+ SHorizontalBox::Slot().FillWidth(0.6f)
		[
			SNew(SSpinBox<float>).MinValue(Min).MaxValue(Max).Value(*Value)
			.MinFractionalDigits(bInteger ? 0 : 2).MaxFractionalDigits(bInteger ? 0 : 2)
			.OnValueChanged_Lambda([this, Value, bInteger](float V) {
				*Value = bInteger ? FMath::RoundToFloat(V) : V;
				RefreshPreview();
			})
		];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildGeneratorSection()
{
	return MakeSection(LOCTEXT("Gen", "GENERATOR"),
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.3f).VAlign(VAlign_Center)
		[ SNew(STextBlock).Text(LOCTEXT("Type", "Type")).ColorAndOpacity(AlphaGenColors::TextDim) ]
		+ SHorizontalBox::Slot().FillWidth(0.7f)
		[
			SAssignNew(TypeCombo, STextComboBox).OptionsSource(&TypeOptions)
			.InitiallySelectedItem(TypeOptions[0])
			.OnSelectionChanged(this, &SAlphaGenWidget::OnTypeChanged)
		]
	);
}

TSharedRef<SWidget> SAlphaGenWidget::BuildParameterSection()
{
	return MakeSection(LOCTEXT("Params", "PARAMETERS"),
		SAssignNew(ParamSwitcher, SWidgetSwitcher).WidgetIndex(0)
		+ SWidgetSwitcher::Slot() [ BuildRadialParams() ]
		+ SWidgetSwitcher::Slot() [ BuildShapeParams() ]
		+ SWidgetSwitcher::Slot() [ BuildShapeParams() ]
		+ SWidgetSwitcher::Slot() [ BuildShapeParams() ]
		+ SWidgetSwitcher::Slot() [ BuildPerlinParams() ]
		+ SWidgetSwitcher::Slot() [ BuildVoronoiParams() ]
		+ SWidgetSwitcher::Slot() [ BuildBricksParams() ]
		+ SWidgetSwitcher::Slot() [ BuildDotsParams() ]
		+ SWidgetSwitcher::Slot() [ BuildSeamlessNoiseParams() ]
		+ SWidgetSwitcher::Slot() [ BuildCrosshatchParams() ]
		+ SWidgetSwitcher::Slot() [ BuildWavesParams() ]
		+ SWidgetSwitcher::Slot() [ BuildCheckerboardParams() ]
		+ SWidgetSwitcher::Slot() [ BuildHexagonParams() ]
		+ SWidgetSwitcher::Slot() [ BuildTearsParams() ]
		+ SWidgetSwitcher::Slot() [ BuildScratchesParams() ]
		+ SWidgetSwitcher::Slot() [ BuildSplatterParams() ]
		+ SWidgetSwitcher::Slot() [ BuildCracksParams() ]
		+ SWidgetSwitcher::Slot() [ BuildCellsParams() ]
		+ SWidgetSwitcher::Slot() [ BuildGrungeParams() ]
		+ SWidgetSwitcher::Slot() [ BuildFibersParams() ]
		+ SWidgetSwitcher::Slot() [ BuildCausticsParams() ]
	);
}

TSharedRef<SWidget> SAlphaGenWidget::BuildRadialParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Fall", "Falloff"), 0.5f, 10.0f, &ParamFalloff) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildShapeParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Size", "Size"), 0.1f, 1.0f, &ParamScale) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Soft", "Softness"), 0.0f, 0.5f, &ParamEdgeSoftness) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildPerlinParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center)
			[ SNew(STextBlock).Text(LOCTEXT("Noise", "Noise")).ColorAndOpacity(AlphaGenColors::TextDim) ]
			+ SHorizontalBox::Slot().FillWidth(0.6f)
			[
				SAssignNew(NoiseTypeCombo, STextComboBox).OptionsSource(&NoiseTypeOptions)
				.InitiallySelectedItem(NoiseTypeOptions[0])
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> S, ESelectInfo::Type) {
					if (!S.IsValid()) return;
					if (*S == TEXT("Perlin")) ParamNoiseType = 0;
					else if (*S == TEXT("Simplex")) ParamNoiseType = 1;
					else if (*S == TEXT("Ridged")) ParamNoiseType = 2;
					else if (*S == TEXT("Billowy")) ParamNoiseType = 3;
					else if (*S == TEXT("Worley")) ParamNoiseType = 4;
					RefreshPreview();
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Freq", "Frequency"), 1.0f, 64.0f, &ParamScale) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Oct", "Octaves"), 1.0f, 8.0f, &ParamOctaves, true) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Seed", "Seed"), 0.0f, 9999.0f, &ParamSeed) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildVoronoiParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Cells", "Cells"), 2.0f, 32.0f, &ParamScale) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Edge", "Edge"), 0.01f, 0.2f, &ParamEdgeWidth) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Seed", "Seed"), 0.0f, 9999.0f, &ParamSeed) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildBricksParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("BW", "Width"), 0.05f, 0.5f, &ParamBrickWidth) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("BH", "Height"), 0.02f, 0.3f, &ParamBrickHeight) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Mort", "Mortar"), 0.005f, 0.1f, &ParamMortar) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildDotsParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("DotS", "Size"), 0.01f, 0.2f, &ParamDotSize) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Space", "Spacing"), 0.05f, 0.4f, &ParamSpacing) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildSeamlessNoiseParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center)
			[ SNew(STextBlock).Text(LOCTEXT("Style", "Style")).ColorAndOpacity(AlphaGenColors::TextDim) ]
			+ SHorizontalBox::Slot().FillWidth(0.6f)
			[
				SAssignNew(SeamlessNoiseTypeCombo, STextComboBox).OptionsSource(&SeamlessNoiseTypeOptions)
				.InitiallySelectedItem(SeamlessNoiseTypeOptions[0])
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> S, ESelectInfo::Type) {
					if (!S.IsValid()) return;
					if (*S == TEXT("Standard")) ParamSeamlessNoiseType = 0;
					else if (*S == TEXT("Ridged")) ParamSeamlessNoiseType = 1;
					else if (*S == TEXT("Billowy")) ParamSeamlessNoiseType = 2;
					RefreshPreview();
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Freq", "Frequency"), 1.0f, 32.0f, &ParamScale) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Oct", "Octaves"), 1.0f, 8.0f, &ParamOctaves, true) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Seed", "Seed"), 0.0f, 9999.0f, &ParamSeed) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildCrosshatchParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Scale", "Scale"), 1.0f, 64.0f, &ParamScale) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Thick", "Thickness"), 0.01f, 0.5f, &ParamThickness) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Angle", "Angle"), 0.0f, 6.28f, &ParamAngle) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildWavesParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center)
			[ SNew(STextBlock).Text(LOCTEXT("Type", "Type")).ColorAndOpacity(AlphaGenColors::TextDim) ]
			+ SHorizontalBox::Slot().FillWidth(0.6f)
			[
				SNew(STextComboBox).OptionsSource(&WaveTypeOptions)
				.InitiallySelectedItem(WaveTypeOptions[0])
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> S, ESelectInfo::Type) {
					if (!S.IsValid()) return;
					if (*S == TEXT("Horizontal")) ParamWaveType = 0;
					else if (*S == TEXT("Vertical")) ParamWaveType = 1;
					else if (*S == TEXT("Radial")) ParamWaveType = 2;
					RefreshPreview();
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Freq", "Frequency"), 1.0f, 32.0f, &ParamFrequency) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Amp", "Amplitude"), 0.1f, 1.0f, &ParamAmplitude) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildCheckerboardParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Scale", "Scale"), 1.0f, 64.0f, &ParamScale) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildHexagonParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Scale", "Scale"), 1.0f, 64.0f, &ParamScale) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Edge", "Edge Thickness"), 0.0f, 0.5f, &ParamEdgeSoftness) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildTearsParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Scale", "Scale"), 1.0f, 32.0f, &ParamScale) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Length", "Length"), 0.1f, 1.0f, &ParamLength) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Seed", "Seed"), 0.0f, 9999.0f, &ParamSeed) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildScratchesParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Density", "Density"), 5.0f, 50.0f, &ParamDensity) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Length", "Length"), 0.1f, 1.0f, &ParamLength) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Seed", "Seed"), 0.0f, 9999.0f, &ParamSeed) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildSplatterParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Scale", "Scale"), 1.0f, 16.0f, &ParamScale) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Size", "Size"), 0.1f, 1.0f, &ParamSize) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Seed", "Seed"), 0.0f, 9999.0f, &ParamSeed) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildCracksParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Scale", "Scale"), 2.0f, 32.0f, &ParamScale) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Width", "Width"), 0.01f, 0.3f, &ParamWidth) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Seed", "Seed"), 0.0f, 9999.0f, &ParamSeed) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildCellsParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Scale", "Scale"), 2.0f, 32.0f, &ParamScale) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Contrast", "Contrast"), 0.5f, 5.0f, &ParamContrast) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Seed", "Seed"), 0.0f, 9999.0f, &ParamSeed) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildGrungeParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Scale", "Scale"), 1.0f, 16.0f, &ParamScale) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Detail", "Detail"), 0.0f, 2.0f, &ParamDetail) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Seed", "Seed"), 0.0f, 9999.0f, &ParamSeed) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildFibersParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Scale", "Scale"), 4.0f, 64.0f, &ParamScale) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Angle", "Angle"), 0.0f, 6.28f, &ParamAngle) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Seed", "Seed"), 0.0f, 9999.0f, &ParamSeed) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildCausticsParams()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Scale", "Scale"), 1.0f, 16.0f, &ParamScale) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Time", "Time"), 0.0f, 10.0f, &ParamTime) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeSlider(LOCTEXT("Seed", "Seed"), 0.0f, 9999.0f, &ParamSeed) ];
}

TSharedRef<SWidget> SAlphaGenWidget::BuildExportSection()
{
	return MakeSection(LOCTEXT("Export", "EXPORT"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.3f).VAlign(VAlign_Center)
			[ SNew(STextBlock).Text(LOCTEXT("Resolution", "Resolution")).ColorAndOpacity(AlphaGenColors::TextDim) ]
			+ SHorizontalBox::Slot().FillWidth(0.7f)
			[
				SNew(STextComboBox).OptionsSource(&ExportResOptions)
				.InitiallySelectedItem(ExportResOptions[4]) // 1024
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> S, ESelectInfo::Type) {
					if (!S.IsValid()) return;
					ExportSize = FCString::Atoi(**S);
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.3f).VAlign(VAlign_Center)
			[ SNew(STextBlock).Text(LOCTEXT("Path", "Path")).ColorAndOpacity(AlphaGenColors::TextDim) ]
			+ SHorizontalBox::Slot().FillWidth(0.7f)
			[
				SAssignNew(ExportPathBox, SEditableTextBox).Text(FText::FromString(ExportPath))
				.OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { ExportPath = T.ToString(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.3f).VAlign(VAlign_Center)
			[ SNew(STextBlock).Text(LOCTEXT("Name", "Name")).ColorAndOpacity(AlphaGenColors::TextDim) ]
			+ SHorizontalBox::Slot().FillWidth(0.7f)
			[
				SAssignNew(ExportNameBox, SEditableTextBox).Text(FText::FromString(ExportName))
				.OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { ExportName = T.ToString(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(0, 0, 4, 0)
			[
				SNew(SButton).HAlign(HAlign_Center).ButtonColorAndOpacity(AlphaGenColors::AccentGreen)
				.OnClicked_Lambda([this]() { OnExportToProject(); return FReply::Handled(); })
				[ SNew(STextBlock).Text(LOCTEXT("Exp", "Export to Project")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10)) ]
			]
			+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(4, 0, 0, 0)
			[
				SNew(SButton).HAlign(HAlign_Center)
				.OnClicked_Lambda([this]() { OnSaveToDisk(); return FReply::Handled(); })
				[ SNew(STextBlock).Text(LOCTEXT("Save", "Save to Disk")) ]
			]
		]
	);
}

void SAlphaGenWidget::OnTypeChanged(TSharedPtr<FString> NewType, ESelectInfo::Type)
{
	if (!NewType.IsValid()) return;
	FString T = *NewType;
	if (T == TEXT("Radial")) CurrentType = EProceduralType::Radial;
	else if (T == TEXT("Circle")) CurrentType = EProceduralType::Circle;
	else if (T == TEXT("Square")) CurrentType = EProceduralType::Square;
	else if (T == TEXT("Diamond")) CurrentType = EProceduralType::Diamond;
	else if (T == TEXT("Perlin")) CurrentType = EProceduralType::Perlin;
	else if (T == TEXT("Voronoi")) CurrentType = EProceduralType::Voronoi;
	else if (T == TEXT("Bricks")) CurrentType = EProceduralType::Bricks;
	else if (T == TEXT("Dots")) CurrentType = EProceduralType::Dots;
	else if (T == TEXT("Seamless Noise")) CurrentType = EProceduralType::SeamlessNoise;
	else if (T == TEXT("Crosshatch")) CurrentType = EProceduralType::Crosshatch;
	else if (T == TEXT("Waves")) CurrentType = EProceduralType::Waves;
	else if (T == TEXT("Checkerboard")) CurrentType = EProceduralType::Checkerboard;
	else if (T == TEXT("Hexagon")) CurrentType = EProceduralType::Hexagon;
	else if (T == TEXT("Tears")) CurrentType = EProceduralType::Tears;
	else if (T == TEXT("Scratches")) CurrentType = EProceduralType::Scratches;
	else if (T == TEXT("Splatter")) CurrentType = EProceduralType::Splatter;
	else if (T == TEXT("Cracks")) CurrentType = EProceduralType::Cracks;
	else if (T == TEXT("Cells")) CurrentType = EProceduralType::Cells;
	else if (T == TEXT("Grunge")) CurrentType = EProceduralType::Grunge;
	else if (T == TEXT("Fibers")) CurrentType = EProceduralType::Fibers;
	else if (T == TEXT("Caustics")) CurrentType = EProceduralType::Caustics;
	UpdateParameterPanel();
	RefreshPreview();
}

void SAlphaGenWidget::UpdateParameterPanel()
{
	if (ParamSwitcher.IsValid())
		ParamSwitcher->SetActiveWidgetIndex(static_cast<int32>(CurrentType));
}

void SAlphaGenWidget::RefreshPreview()
{
	TMap<FString, float> Params;
	Params.Add(TEXT("falloff"), ParamFalloff);
	Params.Add(TEXT("scale"), ParamScale);
	Params.Add(TEXT("octaves"), ParamOctaves);
	Params.Add(TEXT("seed"), ParamSeed);
	Params.Add(TEXT("noise_type"), ParamNoiseType);
	Params.Add(TEXT("softness"), ParamEdgeSoftness);
	Params.Add(TEXT("edge_width"), ParamEdgeWidth);
	Params.Add(TEXT("edge_thickness"), ParamEdgeSoftness); // For hexagon
	Params.Add(TEXT("width"), ParamBrickWidth);
	Params.Add(TEXT("height"), ParamBrickHeight);
	Params.Add(TEXT("mortar"), ParamMortar);
	Params.Add(TEXT("dot_size"), ParamDotSize);
	Params.Add(TEXT("spacing"), ParamSpacing);
	Params.Add(TEXT("seamless_noise_type"), ParamSeamlessNoiseType);
	Params.Add(TEXT("thickness"), ParamThickness);
	Params.Add(TEXT("angle"), ParamAngle);
	Params.Add(TEXT("frequency"), ParamFrequency);
	Params.Add(TEXT("amplitude"), ParamAmplitude);
	Params.Add(TEXT("wave_type"), ParamWaveType);
	Params.Add(TEXT("length"), ParamLength);
	Params.Add(TEXT("density"), ParamDensity);
	Params.Add(TEXT("size"), ParamSize);
	Params.Add(TEXT("width"), ParamWidth);
	Params.Add(TEXT("contrast"), ParamContrast);
	Params.Add(TEXT("detail"), ParamDetail);
	Params.Add(TEXT("time"), ParamTime);
	
	UTexture2D* Generated = FAlphaProceduralGenerator::Generate(CurrentType, PreviewSize, Params);
	
	if (Generated != PreviewTexture)
	{
		if (PreviewTexture) PreviewTexture->RemoveFromRoot();
		PreviewTexture = Generated;
		if (PreviewTexture) PreviewTexture->AddToRoot();
	}
	
	if (PreviewWidget.IsValid() && PreviewTexture)
		PreviewWidget->SetTexture(PreviewTexture);
}

void SAlphaGenWidget::OnExportToProject()
{
	if (ExportPath.IsEmpty()) ExportPath = TEXT("/Game/Alphas");
	if (ExportName.IsEmpty()) ExportName = TEXT("Alpha");
	
	// Generate at export resolution
	TMap<FString, float> Params;
	Params.Add(TEXT("falloff"), ParamFalloff);
	Params.Add(TEXT("scale"), ParamScale);
	Params.Add(TEXT("octaves"), ParamOctaves);
	Params.Add(TEXT("seed"), ParamSeed);
	Params.Add(TEXT("noise_type"), ParamNoiseType);
	Params.Add(TEXT("softness"), ParamEdgeSoftness);
	Params.Add(TEXT("edge_width"), ParamEdgeWidth);
	Params.Add(TEXT("edge_thickness"), ParamEdgeSoftness); // For hexagon
	Params.Add(TEXT("width"), ParamBrickWidth);
	Params.Add(TEXT("height"), ParamBrickHeight);
	Params.Add(TEXT("mortar"), ParamMortar);
	Params.Add(TEXT("dot_size"), ParamDotSize);
	Params.Add(TEXT("spacing"), ParamSpacing);
	Params.Add(TEXT("seamless_noise_type"), ParamSeamlessNoiseType);
	Params.Add(TEXT("thickness"), ParamThickness);
	Params.Add(TEXT("angle"), ParamAngle);
	Params.Add(TEXT("frequency"), ParamFrequency);
	Params.Add(TEXT("amplitude"), ParamAmplitude);
	Params.Add(TEXT("wave_type"), ParamWaveType);
	Params.Add(TEXT("length"), ParamLength);
	Params.Add(TEXT("density"), ParamDensity);
	Params.Add(TEXT("size"), ParamSize);
	Params.Add(TEXT("width"), ParamWidth);
	Params.Add(TEXT("contrast"), ParamContrast);
	Params.Add(TEXT("detail"), ParamDetail);
	Params.Add(TEXT("time"), ParamTime);
	
	UTexture2D* ExportTexture = FAlphaProceduralGenerator::Generate(CurrentType, ExportSize, Params);
	if (!ExportTexture) return;
	
	
	FString SanName = ObjectTools::SanitizeObjectName(ExportName);
	FString PkgPath = ExportPath / SanName;
	PkgPath = FPackageName::ObjectPathToPackageName(PkgPath);
	
	if (FPackageName::DoesPackageExist(PkgPath))
	{
		int32 Cnt = 1;
		FString Unique;
		do { Unique = FString::Printf(TEXT("%s_%d"), *PkgPath, Cnt++); }
		while (FPackageName::DoesPackageExist(Unique));
		PkgPath = Unique;
		SanName = FPackageName::GetShortName(PkgPath);
	}
	
	UPackage* Pkg = CreatePackage(*PkgPath);
	if (!Pkg) return;
	Pkg->FullyLoad();
	
	int32 W = ExportTexture->GetSizeX();
	int32 H = ExportTexture->GetSizeY();
	
	TArray<FColor> Pixels;
	Pixels.SetNum(W * H);
	
	FTextureResource* Res = ExportTexture->GetResource();
	if (Res && Res->GetTexture2DRHI())
	{
		ALPHAGEN_TEXTURE2D_RHI_TYPE RHI = ALPHAGEN_GET_TEXTURE2D_RHI(Res);
		ENQUEUE_RENDER_COMMAND(ReadTex)([RHI, &Pixels, W, H](FRHICommandListImmediate& Cmd)
		{
			FReadSurfaceDataFlags Flags(RCM_UNorm);
			Flags.SetLinearToGamma(false);
			Cmd.ReadSurfaceData(RHI, FIntRect(0, 0, W, H), Pixels, Flags);
		});
		FlushRenderingCommands();
	}
	
	UTexture2D* NewTex = NewObject<UTexture2D>(Pkg, *SanName, RF_Public | RF_Standalone);
	NewTex->Source.Init(W, H, 1, 1, TSF_BGRA8, (uint8*)Pixels.GetData());
	NewTex->SetPlatformData(new FTexturePlatformData());
	NewTex->GetPlatformData()->SizeX = W;
	NewTex->GetPlatformData()->SizeY = H;
	NewTex->GetPlatformData()->PixelFormat = PF_B8G8R8A8;
	
	FTexture2DMipMap* Mip = new FTexture2DMipMap();
	Mip->SizeX = W; Mip->SizeY = H;
	Mip->BulkData.Lock(LOCK_READ_WRITE);
	void* Data = Mip->BulkData.Realloc(W * H * sizeof(FColor));
	FMemory::Memcpy(Data, Pixels.GetData(), W * H * sizeof(FColor));
	Mip->BulkData.Unlock();
	NewTex->GetPlatformData()->Mips.Add(Mip);
	
	NewTex->CompressionSettings = TC_Grayscale;
	NewTex->SRGB = false;
	NewTex->MipGenSettings = TMGS_NoMipmaps;
	NewTex->Filter = TF_Bilinear;
	NewTex->UpdateResource();
	NewTex->MarkPackageDirty();
	
	FAssetRegistryModule::AssetCreated(NewTex);
	
	FString FileName = FPackageName::LongPackageNameToFilename(PkgPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.Error = GError;
	
	FSavePackageResultStruct Result = UPackage::Save(Pkg, NewTex, *FileName, Args);
	if (Result.Result == ESavePackageResult::Success)
	{
		TArray<FAssetData> Assets;
		Assets.Add(FAssetData(NewTex));
		FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		CB.Get().SyncBrowserToAssets(Assets);
	}
}

void SAlphaGenWidget::OnSaveToDisk()
{
	if (!PreviewTexture) return;
	
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop) return;
	
	TArray<FString> Files;
	if (Desktop->SaveFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		TEXT("Save Alpha"), FPaths::ProjectSavedDir(), ExportName + TEXT(".png"),
		TEXT("PNG (*.png)|*.png"), EFileDialogFlags::None, Files))
	{
		if (Files.Num() > 0)
		{
			int32 W = PreviewTexture->GetSizeX();
			int32 H = PreviewTexture->GetSizeY();
			
			TArray<FColor> Pixels;
			Pixels.SetNum(W * H);
			
			FTextureResource* Res = PreviewTexture->GetResource();
			if (Res && Res->GetTexture2DRHI())
			{
				ALPHAGEN_TEXTURE2D_RHI_TYPE RHI = ALPHAGEN_GET_TEXTURE2D_RHI(Res);
				ENQUEUE_RENDER_COMMAND(ReadTex)([RHI, &Pixels, W, H](FRHICommandListImmediate& Cmd)
				{
					FReadSurfaceDataFlags Flags(RCM_UNorm);
					Cmd.ReadSurfaceData(RHI, FIntRect(0, 0, W, H), Pixels, Flags);
				});
				FlushRenderingCommands();
			}
			
			TArray<uint8> PNG;
			FImageUtils::ThumbnailCompressImageArray(W, H, Pixels, PNG);
			FFileHelper::SaveArrayToFile(PNG, *Files[0]);
		}
	}
}

#undef LOCTEXT_NAMESPACE
