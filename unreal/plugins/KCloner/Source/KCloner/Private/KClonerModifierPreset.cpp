// Copyright 2026 K-Studio. All Rights Reserved.

// KClonerModifierPreset.cpp
// Implementation of the modifier preset data asset

#include "KClonerModifierPreset.h"
#include "KClonerExpressionEvaluator.h"

UKClonerModifierPreset::UKClonerModifierPreset()
{
	// Add one default variable
	FKClonerPresetVariable DefaultVar;
	DefaultVar.Name = TEXT("Amplitude");
	DefaultVar.DefaultValue = 50.0f;
	DefaultVar.MinValue = 0.0f;
	DefaultVar.MaxValue = 200.0f;
	DefaultVar.Tooltip = TEXT("Strength of the effect");
	Variables.Add(DefaultVar);
}

#if WITH_EDITOR
void UKClonerModifierPreset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// check if the math expressions work
	// if they don't, scream in the log
	FString Error;
	if (!ValidateExpressions(Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("K-Cloner Preset '%s': %s"), *DisplayName, *Error);
	}
}
#endif

bool UKClonerModifierPreset::ValidateExpressions(FString& OutError) const
{
	FString Error;

	// Validate Position Expression
	if (!PositionExpression.IsEmpty())
	{
		if (!FKClonerExpressionEvaluator::Validate(PositionExpression, Error))
		{
			OutError = FString::Printf(TEXT("Position Expression: %s"), *Error);
			return false;
		}
	}

	// Validate Rotation Expression
	if (!RotationExpression.IsEmpty())
	{
		if (!FKClonerExpressionEvaluator::Validate(RotationExpression, Error))
		{
			OutError = FString::Printf(TEXT("Rotation Expression: %s"), *Error);
			return false;
		}
	}

	// Validate Scale Expression
	if (!ScaleExpression.IsEmpty())
	{
		if (!FKClonerExpressionEvaluator::Validate(ScaleExpression, Error))
		{
			OutError = FString::Printf(TEXT("Scale Expression: %s"), *Error);
			return false;
		}
	}

	return true;
}
