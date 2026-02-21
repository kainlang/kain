// © 2025 mrbaconvn. All Rights Reserved.


#include "ComboGraphHelper.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"

const TArray<FName> UComboGraphHelper::DIRECTION_NAMES = {
	FName(TEXT("F")),
	FName(TEXT("R")),
	FName(TEXT("B")),
	FName(TEXT("L"))
};

FGameplayTag UComboGraphHelper::GetTagInDirection(const TArray<FGameplayTag>& Actions, const FVector2D& Direction)
{
	return GetObjectInDirection<FGameplayTag>(Actions, Direction);
}

void UComboGraphHelper::GetGenericTagDirection(const FGameplayTag& Tag, FVector2D& Direction)
{
	FName DirectionPrefix = FName(*Tag.ToString().Right(1));
	Direction = GetDirectionFromObject<FName>(DIRECTION_NAMES, DirectionPrefix);
}

FVector UComboGraphHelper::GetPawnInputDirection(const APawn* Pawn)
{
	if(!IsValid(Pawn))
	{
		return FVector::ZeroVector;
	}
 
	UCharacterMovementComponent* MovementComponent = Pawn->GetComponentByClass<UCharacterMovementComponent>();
	if(!IsValid(MovementComponent))
	{
		return FVector::ZeroVector;
	}
	
	return Pawn->GetTransform().InverseTransformVector(Pawn->GetLastMovementInputVector());
}

FName UComboGraphHelper::GetDirectionName(const FVector2D& Direction)
{ 
	return GetObjectInDirection<FName>(DIRECTION_NAMES, Direction);
}

FName UComboGraphHelper::GetRandomDirection()
{
	return DIRECTION_NAMES[FMath::RandRange(0, DIRECTION_NAMES.Num() - 1)];
}
