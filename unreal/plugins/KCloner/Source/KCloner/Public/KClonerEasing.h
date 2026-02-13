// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KClonerEasing.generated.h"

/**
 * Easing functions for K-Cloner modifiers.
 * Public domain math from https://easings.net/
 */
UENUM(BlueprintType)
enum class EKClonerEasing : uint8
{
	Linear UMETA(DisplayName = "Linear"),
	
	// Sine
	InSine UMETA(DisplayName = "In Sine"),
	OutSine UMETA(DisplayName = "Out Sine"),
	InOutSine UMETA(DisplayName = "In/Out Sine"),
	
	// Quadratic
	InQuad UMETA(DisplayName = "In Quad"),
	OutQuad UMETA(DisplayName = "Out Quad"),
	InOutQuad UMETA(DisplayName = "In/Out Quad"),
	
	// Cubic
	InCubic UMETA(DisplayName = "In Cubic"),
	OutCubic UMETA(DisplayName = "Out Cubic"),
	InOutCubic UMETA(DisplayName = "In/Out Cubic"),
	
	// Quartic
	InQuart UMETA(DisplayName = "In Quart"),
	OutQuart UMETA(DisplayName = "Out Quart"),
	InOutQuart UMETA(DisplayName = "In/Out Quart"),
	
	// Quintic
	InQuint UMETA(DisplayName = "In Quint"),
	OutQuint UMETA(DisplayName = "Out Quint"),
	InOutQuint UMETA(DisplayName = "In/Out Quint"),
	
	// Exponential
	InExpo UMETA(DisplayName = "In Expo"),
	OutExpo UMETA(DisplayName = "Out Expo"),
	InOutExpo UMETA(DisplayName = "In/Out Expo"),
	
	// Circular
	InCirc UMETA(DisplayName = "In Circ"),
	OutCirc UMETA(DisplayName = "Out Circ"),
	InOutCirc UMETA(DisplayName = "In/Out Circ"),
	
	// Back (overshoot)
	InBack UMETA(DisplayName = "In Back"),
	OutBack UMETA(DisplayName = "Out Back"),
	InOutBack UMETA(DisplayName = "In/Out Back"),
	
	// Elastic (spring)
	InElastic UMETA(DisplayName = "In Elastic"),
	OutElastic UMETA(DisplayName = "Out Elastic"),
	InOutElastic UMETA(DisplayName = "In/Out Elastic"),
	
	// Bounce
	InBounce UMETA(DisplayName = "In Bounce"),
	OutBounce UMETA(DisplayName = "Out Bounce"),
	InOutBounce UMETA(DisplayName = "In/Out Bounce"),
	
	// Special
	Random UMETA(DisplayName = "Random")
};

/**
 * Static easing utility functions.
 * All functions take t in [0,1] and return value in [0,1] (approximately, some overshoot).
 */
struct KCLONER_API FKClonerEasing
{
	static constexpr float KE_PI = 3.14159265358979323846f;
	static constexpr float C1 = 1.70158f;
	static constexpr float C2 = C1 * 1.525f;
	static constexpr float C3 = C1 + 1.0f;
	static constexpr float C4 = (2.0f * KE_PI) / 3.0f;
	static constexpr float C5 = (2.0f * KE_PI) / 4.5f;
	static constexpr float N1 = 7.5625f;
	static constexpr float D1 = 2.75f;

	static FORCEINLINE float Evaluate(EKClonerEasing Type, float T)
	{
		T = FMath::Clamp(T, 0.0f, 1.0f);
		
		switch (Type)
		{
		case EKClonerEasing::Linear: return T;
		
		// Sine
		case EKClonerEasing::InSine: return 1.0f - FMath::Cos(T * KE_PI * 0.5f);
		case EKClonerEasing::OutSine: return FMath::Sin(T * KE_PI * 0.5f);
		case EKClonerEasing::InOutSine: return -(FMath::Cos(KE_PI * T) - 1.0f) * 0.5f;
		
		// Quad
		case EKClonerEasing::InQuad: return T * T;
		case EKClonerEasing::OutQuad: return 1.0f - (1.0f - T) * (1.0f - T);
		case EKClonerEasing::InOutQuad: return T < 0.5f ? 2.0f * T * T : 1.0f - FMath::Pow(-2.0f * T + 2.0f, 2.0f) * 0.5f;
		
		// Cubic
		case EKClonerEasing::InCubic: return T * T * T;
		case EKClonerEasing::OutCubic: return 1.0f - FMath::Pow(1.0f - T, 3.0f);
		case EKClonerEasing::InOutCubic: return T < 0.5f ? 4.0f * T * T * T : 1.0f - FMath::Pow(-2.0f * T + 2.0f, 3.0f) * 0.5f;
		
		// Quart
		case EKClonerEasing::InQuart: return T * T * T * T;
		case EKClonerEasing::OutQuart: return 1.0f - FMath::Pow(1.0f - T, 4.0f);
		case EKClonerEasing::InOutQuart: return T < 0.5f ? 8.0f * T * T * T * T : 1.0f - FMath::Pow(-2.0f * T + 2.0f, 4.0f) * 0.5f;
		
		// Quint
		case EKClonerEasing::InQuint: return T * T * T * T * T;
		case EKClonerEasing::OutQuint: return 1.0f - FMath::Pow(1.0f - T, 5.0f);
		case EKClonerEasing::InOutQuint: return T < 0.5f ? 16.0f * T * T * T * T * T : 1.0f - FMath::Pow(-2.0f * T + 2.0f, 5.0f) * 0.5f;
		
		// Expo
		case EKClonerEasing::InExpo: return T <= 0.0f ? 0.0f : FMath::Pow(2.0f, 10.0f * T - 10.0f);
		case EKClonerEasing::OutExpo: return T >= 1.0f ? 1.0f : 1.0f - FMath::Pow(2.0f, -10.0f * T);
		case EKClonerEasing::InOutExpo: 
			return T <= 0.0f ? 0.0f : T >= 1.0f ? 1.0f : T < 0.5f ? FMath::Pow(2.0f, 20.0f * T - 10.0f) * 0.5f : (2.0f - FMath::Pow(2.0f, -20.0f * T + 10.0f)) * 0.5f;
		
		// Circ
		case EKClonerEasing::InCirc: return 1.0f - FMath::Sqrt(1.0f - T * T);
		case EKClonerEasing::OutCirc: return FMath::Sqrt(1.0f - FMath::Pow(T - 1.0f, 2.0f));
		case EKClonerEasing::InOutCirc: return T < 0.5f ? (1.0f - FMath::Sqrt(1.0f - FMath::Pow(2.0f * T, 2.0f))) * 0.5f : (FMath::Sqrt(1.0f - FMath::Pow(-2.0f * T + 2.0f, 2.0f)) + 1.0f) * 0.5f;
		
		// Back
		case EKClonerEasing::InBack: return C3 * T * T * T - C1 * T * T;
		case EKClonerEasing::OutBack: return 1.0f + C3 * FMath::Pow(T - 1.0f, 3.0f) + C1 * FMath::Pow(T - 1.0f, 2.0f);
		case EKClonerEasing::InOutBack: return T < 0.5f ? (FMath::Pow(2.0f * T, 2.0f) * ((C2 + 1.0f) * 2.0f * T - C2)) * 0.5f : (FMath::Pow(2.0f * T - 2.0f, 2.0f) * ((C2 + 1.0f) * (T * 2.0f - 2.0f) + C2) + 2.0f) * 0.5f;
		
		// Elastic
		case EKClonerEasing::InElastic: return T <= 0.0f ? 0.0f : T >= 1.0f ? 1.0f : -FMath::Pow(2.0f, 10.0f * T - 10.0f) * FMath::Sin((T * 10.0f - 10.75f) * C4);
		case EKClonerEasing::OutElastic: return T <= 0.0f ? 0.0f : T >= 1.0f ? 1.0f : FMath::Pow(2.0f, -10.0f * T) * FMath::Sin((T * 10.0f - 0.75f) * C4) + 1.0f;
		case EKClonerEasing::InOutElastic: 
			return T <= 0.0f ? 0.0f : T >= 1.0f ? 1.0f : T < 0.5f ? -(FMath::Pow(2.0f, 20.0f * T - 10.0f) * FMath::Sin((20.0f * T - 11.125f) * C5)) * 0.5f : (FMath::Pow(2.0f, -20.0f * T + 10.0f) * FMath::Sin((20.0f * T - 11.125f) * C5)) * 0.5f + 1.0f;
		
		// Bounce
		case EKClonerEasing::OutBounce: return BounceOut(T);
		case EKClonerEasing::InBounce: return 1.0f - BounceOut(1.0f - T);
		case EKClonerEasing::InOutBounce: return T < 0.5f ? (1.0f - BounceOut(1.0f - 2.0f * T)) * 0.5f : (1.0f + BounceOut(2.0f * T - 1.0f)) * 0.5f;
		
		// Random
		case EKClonerEasing::Random: return FMath::FRand();
		
		default: return T;
		}
	}

private:
	static FORCEINLINE float BounceOut(float T)
	{
		if (T < 1.0f / D1)
		{
			return N1 * T * T;
		}
		else if (T < 2.0f / D1)
		{
			T -= 1.5f / D1;
			return N1 * T * T + 0.75f;
		}
		else if (T < 2.5f / D1)
		{
			T -= 2.25f / D1;
			return N1 * T * T + 0.9375f;
		}
		else
		{
			T -= 2.625f / D1;
			return N1 * T * T + 0.984375f;
		}
	}
};
