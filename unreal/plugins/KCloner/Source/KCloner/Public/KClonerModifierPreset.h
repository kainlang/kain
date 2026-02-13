// Copyright 2026 K-Studio. All Rights Reserved.

// KClonerModifierPreset.h
// DataAsset defining a custom modifier using math expressions
// Enables DLC modifier packs and studio-created custom modifiers

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Engine/DataAsset.h"
#include "KClonerModifierPreset.generated.h"

/**
 * Defines a single variable/slider for a modifier preset
 */
USTRUCT(BlueprintType)
struct KCLONER_API FKClonerPresetVariable
{
	GENERATED_BODY()

	/** Variable name shown in UI (also used as v0, v1, etc. in expressions) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable")
	FString Name = TEXT("Parameter");

	/** Default value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable")
	float DefaultValue = 1.0f;

	/** Minimum slider value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable")
	float MinValue = 0.0f;

	/** Maximum slider value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable")
	float MaxValue = 100.0f;

	/** Tooltip shown when hovering */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable")
	FString Tooltip = TEXT("");
};

/**
 * A modifier preset that defines motion behavior using math expressions.
 * 
 * Available Variables in Expressions:
 * - t: Current time in seconds
 * - i: Instance index (0, 1, 2, ...)
 * - n: Total instance count
 * - x, y, z: Current position
 * - rx, ry, rz: Current rotation in degrees (pitch, yaw, roll)
 * - sx, sy, sz: Current scale
 * - v0, v1, v2...: User-defined slider values
 * 
 * Available Functions:
 * - sin, cos, tan, asin, acos, atan, atan2
 * - abs, floor, ceil, round, clamp, min, max
 * - sqrt, pow, exp, log, log10
 * - fmod, frac
 * - lerp(a, b, t), smoothstep(edge0, edge1, x)
 * - noise(x) - Perlin-like noise
 * 
 * Example Expressions:
 * - Position XYZ: "x += sin(t + i * 0.1) * v0; y += cos(t + i * 0.1) * v0;"
 * - Rotation XYZ: "ry += t * v0 + i * v1;"
 * - Scale XYZ: "sx *= 1 + sin(t * 2 + i * 0.5) * 0.2; sy := sx; sz := sx;"
 */
UCLASS(BlueprintType)
class KCLONER_API UKClonerModifierPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UKClonerModifierPreset();

	// ========== METADATA ==========

	/** Display name shown in modifier dropdown */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset|Metadata")
	FString DisplayName = TEXT("Custom Modifier");

	/** Category for grouping in dropdown (e.g., "Motion", "Sci-Fi", "Nature") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset|Metadata")
	FString Category = TEXT("Custom");

	/** Description shown in tooltip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset|Metadata", meta = (MultiLine = true))
	FString Description = TEXT("");

	/** Optional icon for this preset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset|Metadata")
	TObjectPtr<UTexture2D> Icon;

	// ========== EXPRESSIONS ==========

	/** 
	 * Position expression. Use += to add, := to set.
	 * Example: "x += sin(t + i * 0.1) * v0; z += cos(t + i * 0.15) * v0;"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset|Expressions", meta = (MultiLine = true))
	FString PositionExpression = TEXT("");

	/** 
	 * Rotation expression (degrees). Use += to add, := to set.
	 * Example: "ry += t * v0 + i * v1;"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset|Expressions", meta = (MultiLine = true))
	FString RotationExpression = TEXT("");

	/** 
	 * Scale expression. Use *= to multiply, := to set.
	 * Example: "sx *= 1 + sin(t + i * 0.2) * 0.3; sy := sx; sz := sx;"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset|Expressions", meta = (MultiLine = true))
	FString ScaleExpression = TEXT("");

	// ========== VARIABLES ==========

	/** User-defined variables exposed as sliders (accessed as v0, v1, v2... in expressions) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset|Variables")
	TArray<FKClonerPresetVariable> Variables;

	// ========== SETTINGS ==========

	/** Time offset per instance (creates cascading/delay effects) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset|Settings")
	float Step = 0.1f;

	/** Speed multiplier for time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset|Settings")
	float SpeedMultiplier = 1.0f;

	// ========== VALIDATION ==========

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Returns true if all expressions are valid */
	UFUNCTION(BlueprintCallable, Category = "Preset")
	bool ValidateExpressions(FString& OutError) const;
};
