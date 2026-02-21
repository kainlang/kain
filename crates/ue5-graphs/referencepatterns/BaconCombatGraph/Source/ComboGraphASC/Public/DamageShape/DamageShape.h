// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DamageShape.generated.h"

inline static TAutoConsoleVariable<bool> CVarShowDebugDamageShape(TEXT("debug.DamageShape"), false, TEXT("Debug Damage Shape"), ECVF_Default);
 
struct FDamageShapeDebugProfile
{ 
	EDrawDebugTrace::Type DrawDebugType = EDrawDebugTrace::Type::ForOneFrame;
	float DebugDuration = 0.1f;
	FLinearColor TraceHitColor = FLinearColor::Green;
	FLinearColor TraceColor = FLinearColor::Red;
};

USTRUCT()
struct COMBOGRAPHASC_API FDamageShapeQuery
{
	GENERATED_BODY()
	
	UPROPERTY()
	const AActor* SourceActor = nullptr;
	
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = {};

	uint8 bIgnoreSelf : 1 = true;
	uint8 bDrawDebug : 1 = false;
};

USTRUCT(BlueprintType)
struct COMBOGRAPHASC_API FDamageShape
{
	GENERATED_BODY()

public:
	virtual void TraceShape(const FDamageShapeQuery& TraceQuery, TArray<FHitResult>& OutHits) const;
	virtual ~FDamageShape() = default;

protected:
	void GetShapeOriginLocationAndRotation(const AActor* SourceActor, FVector& OutLocation, FRotator& OutRotation) const;
	void GetSocketLocationAndRotation(const AActor* SourceActor, FName SocketName, FVector& OutLocation, FRotator& OutRotation) const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shape")
	FName SocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shape")
	FVector OffsetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shape")
	FRotator OffsetRotation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct COMBOGRAPHASC_API FDamageShape_Sphere : public FDamageShape
{
	GENERATED_BODY()

public:
	virtual void TraceShape(const FDamageShapeQuery& TraceQuery, TArray<FHitResult>& OutHits) const override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sphere")
	float Radius = 10.f;
};

USTRUCT(BlueprintType)
struct COMBOGRAPHASC_API FDamageShape_Box : public FDamageShape
{
	GENERATED_BODY()

public:
	virtual void TraceShape(const FDamageShapeQuery& TraceQuery, TArray<FHitResult>& OutHits) const override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Box")
	FVector BoxExtent = FVector(10.f, 10.f, 10.f);
};

USTRUCT(BlueprintType)
struct COMBOGRAPHASC_API FDamageShape_Capsule : public FDamageShape
{
	GENERATED_BODY()

public:
	virtual void TraceShape(const FDamageShapeQuery& TraceQuery, TArray<FHitResult>& OutHits) const override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Capsule")
	float Radius = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Capsule")
	float SphereHalfHeight = 20.f; 
};

USTRUCT(BlueprintType)
struct COMBOGRAPHASC_API FDamageShape_Ring : public FDamageShape
{
	GENERATED_BODY()

public:
	virtual void TraceShape(const FDamageShapeQuery& TraceQuery, TArray<FHitResult>& OutHits) const override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ring")
	int RingSegments = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ring")
	float InnerRingRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ring")
	float OuterRingRadius = 150.f;
};
