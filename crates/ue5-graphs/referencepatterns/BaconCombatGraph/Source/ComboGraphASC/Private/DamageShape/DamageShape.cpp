// © 2025 mrbaconvn. All Rights Reserved.

#include "DamageShape/DamageShape.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

#define ADD_DEBUG_SHAPE(Toggle)\
FDamageShapeDebugProfile Debug;\
Debug.DrawDebugType = Toggle ? Debug.DrawDebugType : EDrawDebugTrace::None;

void FDamageShape::TraceShape(const FDamageShapeQuery& TraceQuery, TArray<FHitResult>& OutHits) const
{
}

void FDamageShape::GetSocketLocationAndRotation(const AActor* SourceActor, FName InSocketName, FVector& OutLocation, FRotator& OutRotation) const
{
	if (!IsValid(SourceActor))
	{
		return;
	}

	USkeletalMeshComponent* SkeletalMeshComponent = SourceActor->FindComponentByClass<USkeletalMeshComponent>();
	if (IsValid(SkeletalMeshComponent))
	{
		FTransform SocketTransform = SkeletalMeshComponent->GetSocketTransform(InSocketName);
		OutLocation = SocketTransform.GetLocation();
		OutRotation = SocketTransform.GetRotation().Rotator();
	}
	else
	{
		UStaticMeshComponent* StaticMeshComponent = SourceActor->FindComponentByClass<UStaticMeshComponent>();
		if (IsValid(StaticMeshComponent))
		{
			OutLocation = StaticMeshComponent->GetComponentLocation();
			OutRotation = StaticMeshComponent->GetComponentRotation();
		}
	}
}

void FDamageShape::GetShapeOriginLocationAndRotation(const AActor* SourceActor, FVector& OutLocation, FRotator& OutRotation) const
{
	if (!IsValid(SourceActor))
	{
		return;
	}

	FTransform RootTransform;

	if (SocketName != NAME_None)
	{
		FVector SocketLocation;
		FRotator SocketRotation;
		GetSocketLocationAndRotation(SourceActor, SocketName, SocketLocation, SocketRotation);

		RootTransform = FTransform(SocketRotation, SocketLocation);
	}
	else
	{
		RootTransform = SourceActor->GetActorTransform();
	}

	OutLocation = RootTransform.TransformPosition(OffsetLocation);
	OutRotation = (RootTransform.TransformVector(OffsetRotation.Vector())).Rotation();
}


void FDamageShape_Sphere::TraceShape(const FDamageShapeQuery& TraceQuery, TArray<FHitResult>& OutHits) const
{ 
	TArray<AActor*> ActorsToIgnore;

	FVector TracePosition;
	FRotator Rotation;
	GetShapeOriginLocationAndRotation(TraceQuery.SourceActor, TracePosition, Rotation);

	ADD_DEBUG_SHAPE(TraceQuery.bDrawDebug);
	UKismetSystemLibrary::SphereTraceMultiForObjects(
		TraceQuery.SourceActor,
		TracePosition,
		TracePosition,
		Radius,
		TraceQuery.ObjectTypes, false, ActorsToIgnore, Debug.DrawDebugType, OutHits, TraceQuery.bIgnoreSelf, Debug.TraceColor, Debug.TraceHitColor, Debug.DebugDuration);
}

void FDamageShape_Box::TraceShape(const FDamageShapeQuery& TraceQuery, TArray<FHitResult>& OutHits) const
{  
	TArray<AActor*> ActorsToIgnore;

	FVector TracePosition;
	FRotator Rotation;
	GetShapeOriginLocationAndRotation(TraceQuery.SourceActor, TracePosition, Rotation);

	ADD_DEBUG_SHAPE(TraceQuery.bDrawDebug);
	UKismetSystemLibrary::BoxTraceMultiForObjects(
		TraceQuery.SourceActor,
		TracePosition,
		TracePosition,
		BoxExtent,
		Rotation,
		TraceQuery.ObjectTypes, false, ActorsToIgnore, Debug.DrawDebugType, OutHits, TraceQuery.bIgnoreSelf, Debug.TraceColor, Debug.TraceHitColor, Debug.DebugDuration);
}

void FDamageShape_Capsule::TraceShape(const FDamageShapeQuery& TraceQuery, TArray<FHitResult>& OutHits) const
{  
	TArray<AActor*> ActorsToIgnore;

	FVector TracePosition;
	FRotator Rotation;
	GetShapeOriginLocationAndRotation(TraceQuery.SourceActor, TracePosition, Rotation);

	ADD_DEBUG_SHAPE(TraceQuery.bDrawDebug);
	UKismetSystemLibrary::SphereTraceMultiForObjects(
		TraceQuery.SourceActor,
		TracePosition + Rotation.Vector() * -SphereHalfHeight,
		TracePosition + Rotation.Vector() * SphereHalfHeight,
		Radius,
		TraceQuery.ObjectTypes, false, ActorsToIgnore, Debug.DrawDebugType, OutHits, TraceQuery.bIgnoreSelf, Debug.TraceColor, Debug.TraceHitColor, Debug.DebugDuration);
}

void FDamageShape_Ring::TraceShape(const FDamageShapeQuery& TraceQuery, TArray<FHitResult>& OutHits) const
{ 
	TArray<AActor*> ActorsToIgnore;

	FVector TracePosition;
	FRotator Rotation;
	GetShapeOriginLocationAndRotation(TraceQuery.SourceActor, TracePosition, Rotation);

	FQuat RotationQuat = FQuat(Rotation);
	const float AngleIncrement = 360.0f / RingSegments;

	float MidLocation = (InnerRingRadius + OuterRingRadius) / 2;
	float MidRadius = FMath::Abs(InnerRingRadius - OuterRingRadius) / 2;
	for (int32 i = 0; i < RingSegments; i++)
	{
		float StartAngle = FMath::DegreesToRadians(i * AngleIncrement);
		float EndAngle = FMath::DegreesToRadians((i + 1) * AngleIncrement);

		FVector Start = FVector(FMath::Cos(StartAngle) * MidLocation, FMath::Sin(StartAngle) * MidLocation, 0);
		FVector End = FVector(FMath::Cos(EndAngle) * MidLocation, FMath::Sin(EndAngle) * MidLocation, 0);

		Start = RotationQuat.RotateVector(Start);
		End = RotationQuat.RotateVector(End);

		ADD_DEBUG_SHAPE(TraceQuery.bDrawDebug);
		UKismetSystemLibrary::SphereTraceMultiForObjects(
			TraceQuery.SourceActor,
			TracePosition + Start,
			TracePosition + End,
			MidRadius,
			TraceQuery.ObjectTypes, false, ActorsToIgnore, Debug.DrawDebugType, OutHits, TraceQuery.bIgnoreSelf, Debug.TraceColor, Debug.TraceHitColor, Debug.DebugDuration);
	}
}
