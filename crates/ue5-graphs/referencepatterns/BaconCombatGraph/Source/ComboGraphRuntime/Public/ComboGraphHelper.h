// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ComboGraphHelper.generated.h"

/**
 * This helper contains function in building a combat combo graph
 */
UCLASS()
class COMBOGRAPHRUNTIME_API UComboGraphHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Brawl|Helper")
	static FGameplayTag GetTagInDirection(const TArray<FGameplayTag>& Actions, const FVector2D& Direction);

	/**
	 * Get the direction from generic direction tag
	 * the tag must end with .F, .B, .L, .R
	 */
	UFUNCTION(BlueprintPure, Category = "Brawl|Helper")
	static void GetGenericTagDirection(const FGameplayTag& Tag, FVector2D& Direction);

	UFUNCTION(BlueprintPure, Category = "Brawl|Helper")
	static FVector GetPawnInputDirection(const APawn* Pawn );

	UFUNCTION(BlueprintPure, Category = "Brawl|Helper")
	static FName GetDirectionName(const FVector2D& Direction);

	UFUNCTION(BlueprintPure, Category = "Brawl|Helper")
	static FName GetRandomDirection();
	
	template <typename T> 
	UFUNCTION(BlueprintPure, Category = "Brawl|Helper")
	static T GetObjectInDirection(const TArray<T>& Objects, const FVector2D& Direction)
	{
		if (Objects.Num() == 0)
		{
			if constexpr(TIsPointer<T>::Value)
			{
				return nullptr;
			}
			else
			{
				return T();
			} 
		}

		int NumObjects = Objects.Num();
		float AngleStep = 2.0f * PI / NumObjects;
		FVector2D NormalizedDirection = Direction.GetSafeNormal();
		float MaxDot = -1.0f;
		int ClosestIndex = 0;

		for (int i = 0; i < NumObjects; ++i)
		{
			float Angle = i * AngleStep;
			FVector2D ObjectDirection(FMath::Cos(Angle), FMath::Sin(Angle));

			float DotProduct = FVector2D::DotProduct(NormalizedDirection, ObjectDirection);
			if (DotProduct > MaxDot)
			{
				MaxDot = DotProduct;
				ClosestIndex = i;
			}
		}

		return Objects[ClosestIndex];
	}

	template <typename T>
	static FVector2D GetDirectionFromObject(const TArray<T>& Objects, const T& TargetObject)
	{
		if (Objects.Num() == 0)
		{
			return FVector2D::ZeroVector;
		}

		int NumObjects = Objects.Num();
		float AngleStep = 2.0f * PI / NumObjects;
		int TargetIndex = Objects.IndexOfByKey(TargetObject);

		if (TargetIndex == INDEX_NONE)
		{
			return FVector2D::ZeroVector;
		}

		float Angle = TargetIndex * AngleStep;
		return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
	}

public:
	static const TArray<FName> DIRECTION_NAMES;
};
