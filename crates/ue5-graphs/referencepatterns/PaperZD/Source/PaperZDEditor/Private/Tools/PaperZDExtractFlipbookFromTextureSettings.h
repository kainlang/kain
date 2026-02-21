// Copyright 2017 ~ 2022 Critical Failure Studio Ltd. All rights reserved.

#pragma once
#include "SpriteEditorOnlyTypes.h"
#include "PaperZDExtractFlipbookFromTextureSettings.generated.h"

class UPaperZDAnimationSource_Flipbook;

USTRUCT()
struct FPaperZDExtractFlipbookColoringSettings
{
    GENERATED_BODY()

public:
    // The color of the sprite boundary outlines
    UPROPERTY(Category = Coloring, EditAnywhere, meta = (HideAlphaChannel))
    FLinearColor OutlineColor;
};

USTRUCT()
struct FPaperZDExtractFlipbookNamingSettings
{
    GENERATED_BODY()

public:
    // The name of the flipbook that will be created. {0} will get replaced by the generic flipbook name {1} will get replaced by the flipbook number.
    UPROPERTY(Category = Flipbook, EditAnywhere)
    FString FlipbookNamingTemplate;

    // The number to start naming sprites with
    UPROPERTY(Category = Flipbook, EditAnywhere)
    int32 FlipbookNamingStartIndex;

    // The name of the sprite that will be created. {0} will get replaced by the generic sprite name {1} will get replaced by the sprite number.
    UPROPERTY(Category = Sprite, EditAnywhere)
    FString SpriteNamingTemplate;

    // The number to start naming sprites with
    UPROPERTY(Category = Sprite, EditAnywhere)
    int32 SpriteNamingStartIndex;

    // The name of the anim sequence that will be created. {0} will get replaced by the generic anim sequence name {1} will get replaced by the sprite number.
    UPROPERTY(Category = Sprite, EditAnywhere)
    FString AnimSequenceNamingTemplate;
};

USTRUCT()
struct FPaperZDExtractFlipbookFlipbookSettings
{
    GENERATED_BODY()

public:
    // The flipbook speed
    UPROPERTY(Category = Flipbook, EditAnywhere)
    float FramesPerSecond;
};

USTRUCT()
struct FPaperZDExtractFlipbookSpriteSettings
{
    GENERATED_BODY()

public:
    // The pivot mode of the extracted sprites
    UPROPERTY(Category = Sprite, EditAnywhere)
    TEnumAsByte<ESpritePivotMode::Type> PivotMode;

    // The pivot point to use as a base for the custom pivot point
    UPROPERTY(Category = Sprite, EditAnywhere)
    TEnumAsByte<ESpritePivotMode::Type> CustomPivotModeReference;

    // The relative value to add on top of the base pivot point
    UPROPERTY(Category = Sprite, EditAnywhere)
    FVector2D CustomPivotPoint;
};

USTRUCT()
struct FPaperZDExtractFlipbookAnimationSettings
{
    GENERATED_BODY()

public:
    // The id of the animation sequence to use from the extracted flipbook (Must match with the Animation > Animation Sequence Settings > Index) (Leave to -1 to have separate animation sequences)
    UPROPERTY(Category = Animation, EditAnywhere)
    int32 AnimationSequenceID;

    // The direction index of the animation sequence clockwise (leave it to -1 to auto pick)
    // E.g. In an animation sequence of 4 directions: 0 = up, 1 = right, 2 = down, 3 = left
    UPROPERTY(Category = Animation, EditAnywhere)
    int32 AnimationSequenceDirectionIndex;
};

USTRUCT()
struct FPaperZDExtractFlipbookExtractionSettings
{
    GENERATED_BODY()

public:
    // The width of each sprite in grid mode
    UPROPERTY(Category = Grid, EditAnywhere, meta = (UIMin = 1, ClampMin = 1))
    int32 CellWidth;

    // The height of each sprite in grid mode
    UPROPERTY(Category = Grid, EditAnywhere, meta = (UIMin = 1, ClampMin = 1))
    int32 CellHeight;

    // Number of cells extracted horizontally. Can be used to limit the number of sprites extracted. Set to 0 to extract all sprites
    UPROPERTY(Category = Grid, EditAnywhere, meta = (UIMin = 0, ClampMin = 0))
    int32 NumCellsX;

    // Number of cells extracted vertically. Can be used to limit the number of sprites extracted. Set to 0 to extract all sprites
    UPROPERTY(Category = Grid, EditAnywhere, meta = (UIMin = 0, ClampMin = 0))
    int32 NumCellsY;

    // Margin from the left of the texture to the first sprite
    UPROPERTY(Category = Grid, EditAnywhere, meta = (UIMin = 0, ClampMin = 0))
    int32 MarginX;

    // Margin from the top of the texture to the first sprite
    UPROPERTY(Category = Grid, EditAnywhere, meta = (UIMin = 0, ClampMin = 0))
    int32 MarginY;

    // Horizontal spacing between sprites
    UPROPERTY(Category = Grid, EditAnywhere, meta = (UIMin = 0, ClampMin = 0))
    int32 SpacingX;

    // Vertical spacing between sprites
    UPROPERTY(Category = Grid, EditAnywhere, meta = (UIMin = 0, ClampMin = 0))
    int32 SpacingY;
};

USTRUCT()
struct FPaperZDExtractFlipbookSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Category = Settings, EditAnywhere)
    FPaperZDExtractFlipbookColoringSettings Coloring;

    UPROPERTY(Category = Settings, EditAnywhere)
    FPaperZDExtractFlipbookNamingSettings Naming;

    UPROPERTY(Category = Settings, EditAnywhere)
    FPaperZDExtractFlipbookFlipbookSettings Flipbook;

    UPROPERTY(Category = Settings, EditAnywhere)
    FPaperZDExtractFlipbookSpriteSettings Sprite;

    UPROPERTY(Category = Settings, EditAnywhere)
    FPaperZDExtractFlipbookAnimationSettings Animation;

    UPROPERTY(Category = Settings, EditAnywhere)
    FPaperZDExtractFlipbookExtractionSettings Extraction;
};

USTRUCT()
struct FPaperZDExtractFlipbooksColoringSettings
{
    GENERATED_BODY()

public:
    // Apply a tint to the texture in the viewport to improve outline visibility in this editor
    UPROPERTY(Category = Coloring, EditAnywhere)
    FLinearColor ViewportTextureTint;

    // The viewport background color
    UPROPERTY(Category = Coloring, EditAnywhere, meta = (HideAlphaChannel))
    FLinearColor BackgroundColor;
};

USTRUCT()
struct FPaperZDExtractFlipbooksNamingSettings
{
    GENERATED_BODY()

public:
    // The name of all the flipbook that will be created.
    UPROPERTY(Category = Naming, EditAnywhere)
    FString FlipbookNamingTemplate;

    // The name of all the sprite that will be created.
    UPROPERTY(Category = Naming, EditAnywhere)
    FString SpriteNamingTemplate;

    // The name of all the animation sequences that will be created.
    UPROPERTY(Category = Naming, EditAnywhere)
    FString AnimationSequenceNamingTemplate;

    // The name of the animation source that will be created.
    UPROPERTY(Category = Naming, EditAnywhere)
    FString AnimationSourceNamingTemplate;

    // The name of the animation blueprint that will be created.
    UPROPERTY(Category = Naming, EditAnywhere)
    FString AnimationBlueprintNamingTemplate;
};

USTRUCT()
struct FPaperZDExtractFlipbooksAnimationSequenceSettings
{
    GENERATED_BODY()

public:
    // The category set for the animation sequence
    UPROPERTY(Category = Animation, EditAnywhere)
    FString Category;

    // The naming template used for the animation sequence where {0} will get replaced by the generic anim sequence name.
    UPROPERTY(Category = Animation, EditAnywhere)
    FString AnimationSequenceNamingTemplate;
};

USTRUCT()
struct FPaperZDExtractFlipbooksAnimationSettings
{
    GENERATED_BODY()

public:
    // The animation source to use for the created animation blueprint and sequences created
    UPROPERTY(Category = Animation, EditAnywhere)
    UPaperZDAnimationSource_Flipbook* AnimationSource;

    // If a animation blueprint should be created using the given (or newly created) animation source
    UPROPERTY(Category = Animation, EditAnywhere)
    bool bCreateAnimationBlueprint;

    // If a animation source should be created
    UPROPERTY(Category = Animation, EditAnywhere)
    bool bCreateAnimationSource;

    // If a animation sequence should be created with the extracted flipbooks
    UPROPERTY(Category = Animation, EditAnywhere)
    bool bCreateAnimationSequence;

    // The configuration for the animation sequences created
    UPROPERTY(Category = Animation, EditAnywhere)
    TArray<FPaperZDExtractFlipbooksAnimationSequenceSettings> AnimationSequenceSettings;
};

UCLASS()
class UPaperZDExtractFlipbookFromTextureDialogSettings : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(Category = Settings, EditAnywhere)
    FPaperZDExtractFlipbooksColoringSettings Coloring;

    UPROPERTY(Category = Settings, EditAnywhere)
    FPaperZDExtractFlipbooksNamingSettings Naming;

    UPROPERTY(Category = Settings, EditAnywhere)
    FPaperZDExtractFlipbooksAnimationSettings Animation;

    UPROPERTY(Category = Settings, EditAnywhere)
    TArray<FPaperZDExtractFlipbookSettings> Flipbooks;
};

USTRUCT()
struct FPaperZDExtractFlipbookPreset
{
    GENERATED_BODY()

public:
    UPROPERTY(Category = Preset, EditAnywhere)
    FString PresetName;

    UPROPERTY(Category = Preset, EditAnywhere)
    FPaperZDExtractFlipbooksAnimationSettings AnimationSettings;

    UPROPERTY(Category = Preset, EditAnywhere)
    TArray<FPaperZDExtractFlipbookSettings> FlipbookSettings;
};