// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerEffectorComponent.h"
#include "KClonerEffectorSubsystem.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

UKClonerEffectorComponent::UKClonerEffectorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = true;
	bAutoActivate = true;
}

void UKClonerEffectorComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (UWorld* World = GetWorld())
	{
		if (UKClonerEffectorSubsystem* Subsystem = World->GetSubsystem<UKClonerEffectorSubsystem>())
		{
			Subsystem->RegisterEffector(this);
		}
	}
	
	LastLocation = GetComponentLocation();
}

void UKClonerEffectorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UKClonerEffectorSubsystem* Subsystem = World->GetSubsystem<UKClonerEffectorSubsystem>())
		{
			Subsystem->UnregisterEffector(this);
		}
	}
	
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UKClonerEffectorComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UKClonerEffectorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Check if we moved significantly
	// if we did, we gotta tell the subsystem to rebuild the KD tree
	if (bEnabled)
	{
		FVector CurrentLoc = GetComponentLocation();
		if (!CurrentLoc.Equals(LastLocation, 1.0f)) // 1 unit tolerance
		{
			LastLocation = CurrentLoc;
			if (UWorld* World = GetWorld())
			{
				if (UKClonerEffectorSubsystem* Subsystem = World->GetSubsystem<UKClonerEffectorSubsystem>())
				{
					Subsystem->MarkDirty();
				}
			}
		}
	}

#if WITH_EDITOR
	// draw some shapes in the editor so we know where the effector is 
	// don't draw in commandlets or cookers obviously
	if (bEnabled && bVisualize && !IsRunningCommandlet())
	{
		FTransform WorldTM = GetComponentTransform();
		FVector Center = WorldTM.GetLocation();
		FColor DrawColor = VisualizerColor.ToFColor(true);
		
		switch (Shape)
		{
		case EKClonerEffectorShape::Sphere:
			DrawDebugSphere(GetWorld(), Center, Radius, 32, DrawColor, false, -1.0f, 0, VisualizerThickness);
			break;
			
		case EKClonerEffectorShape::Box:
			DrawDebugBox(GetWorld(), Center, Extent, WorldTM.GetRotation(), DrawColor, false, -1.0f, 0, VisualizerThickness);
			break;
			
		case EKClonerEffectorShape::Plane:
		case EKClonerEffectorShape::Cylinder:
		{
			FVector Up = WorldTM.GetUnitAxis(EAxis::Z);
			FVector Right = WorldTM.GetUnitAxis(EAxis::Y);
			FVector Forward = WorldTM.GetUnitAxis(EAxis::X);
			DrawDebugCircle(GetWorld(), Center, Radius, 32, DrawColor, false, -1.0f, 0, VisualizerThickness, Right, Forward, false);
			
			if (Shape == EKClonerEffectorShape::Plane)
			{
				float PlaneSize = Radius * 2.0f;
				// Plane visualization: Draw a transparent-ish quad
				FColor PlaneColor = DrawColor;
				PlaneColor.A = 50;
				DrawDebugSolidPlane(GetWorld(), FPlane(Center, Up), Center, FVector2D(PlaneSize), PlaneColor, false, -1.0f, 0);
				// Plane normal arrow
				DrawDebugDirectionalArrow(GetWorld(), Center, Center + Up * Radius * 0.5f, 10.0f, DrawColor, false, -1.0f, 0, VisualizerThickness);
			}
			else if (Shape == EKClonerEffectorShape::Cylinder)
			{
				// Draw infinite line visual
				DrawDebugLine(GetWorld(), Center - Up * 10000.0f, Center + Up * 10000.0f, DrawColor, false, -1.0f, 0, VisualizerThickness * 0.5f);
			}
			break;
		}
			
		case EKClonerEffectorShape::Torus:
		{
			FVector Up = WorldTM.GetUnitAxis(EAxis::Z);
			FVector Right = WorldTM.GetUnitAxis(EAxis::Y);
			FVector Forward = WorldTM.GetUnitAxis(EAxis::X);
			
			// Outer major ring
			DrawDebugCircle(GetWorld(), Center, Radius, 32, DrawColor, false, -1.0f, 0, VisualizerThickness, Right, Forward, false);
			
			// Tube visualization
			float TubeRadius = InnerRadius > 0.0f ? InnerRadius : Radius * 0.25f;
			
			// Draw representative tube circles
			int32 Segments = 8;
			for (int32 i = 0; i < Segments; i++)
			{
				float Angle = (float)i / (float)Segments * 2.0f * UE_PI;
				FVector OffsetDir = (Right * FMath::Cos(Angle) + Forward * FMath::Sin(Angle));
				FVector RingCenter = Center + OffsetDir * Radius;
				
				// Tangent direction for the tube ring
				FVector Tangent = FVector::CrossProduct(Up, OffsetDir);
				DrawDebugCircle(GetWorld(), RingCenter, TubeRadius, 16, DrawColor, false, -1.0f, 0, VisualizerThickness, Tangent, Up, false);
			}
			break;
		}
			
		case EKClonerEffectorShape::Unbound:
			DrawDebugBox(GetWorld(), Center, FVector(50.0f), WorldTM.GetRotation(), DrawColor, false, -1.0f, 0, VisualizerThickness);
			break;
		}
	}
#endif
}

float UKClonerEffectorComponent::GetInfluenceAtLocation(const FVector& WorldLocation) const
{
	if (!bEnabled || Strength <= 0.0f)
	{
		return 0.0f;
	}

	FVector LocalPos = GetComponentTransform().InverseTransformPosition(WorldLocation);
	float NormalizedDistance = 0.0f;

	switch (Shape)
	{
	case EKClonerEffectorShape::Sphere:
	{
		if (Radius <= 0.0f) return bInvert ? 0.0f : Strength;
		float Distance = LocalPos.Size();
		NormalizedDistance = Distance / Radius;
		break;
	}

	case EKClonerEffectorShape::Box:
	{
		FVector AbsLocal = LocalPos.GetAbs();
		if (Extent.GetMin() <= 0.0f) return bInvert ? 0.0f : Strength;
		FVector Ratio = AbsLocal / Extent;
		NormalizedDistance = Ratio.GetMax();
		break;
	}

	case EKClonerEffectorShape::Plane:
	{
		if (Radius <= 0.0f) return bInvert ? 0.0f : Strength;
		float Distance = FMath::Abs(LocalPos.Z);
		NormalizedDistance = Distance / Radius;
		break;
	}

	case EKClonerEffectorShape::Cylinder:
	{
		if (Radius <= 0.0f) return bInvert ? 0.0f : Strength;
		float Distance2D = FVector2D(LocalPos.X, LocalPos.Y).Size();
		NormalizedDistance = Distance2D / Radius;
		break;
	}

	case EKClonerEffectorShape::Torus:
	{
		if (Radius <= 0.0f) return bInvert ? 0.0f : Strength;
		float Distance2D = FVector2D(LocalPos.X, LocalPos.Y).Size();
		float TorusCenter = Radius;
		FVector TorusSample(Distance2D - TorusCenter, 0.0f, LocalPos.Z);
		float DistanceToTube = TorusSample.Size();
		float TubeRadius = InnerRadius > 0.0f ? InnerRadius : Radius * 0.25f;
		NormalizedDistance = DistanceToTube / TubeRadius;
		break;
	}

	case EKClonerEffectorShape::Unbound:
	default:
		return bInvert ? 0.0f : Strength;
	}

	// Apply inversion
	if (bInvert)
	{
		NormalizedDistance = 1.0f - NormalizedDistance;
	}

	// Outside range check
	if (NormalizedDistance >= 1.0f)
	{
		return bInvert ? Strength : 0.0f;
	}

	// Falloff calculation
	if (Falloff <= 0.0f)
	{
		return Strength; // Hard edge
	}

	float InnerThreshold = 1.0f - Falloff;
	if (NormalizedDistance <= InnerThreshold)
	{
		return Strength;
	}

	// Smooth falloff
	float T = (NormalizedDistance - InnerThreshold) / Falloff;
	return FMath::SmoothStep(Strength, 0.0f, T);
}

// ============================================================================
// EFFECTOR FINDER
// ============================================================================

TArray<UKClonerEffectorComponent*> FKClonerEffectorFinder::FindEffectorsNear(const UWorld* World, const FVector& Location, float SearchRadius)
{
	if (!World) return {};

	if (UKClonerEffectorSubsystem* Subsystem = World->GetSubsystem<UKClonerEffectorSubsystem>())
	{
		// KDTree stores points (centers). Effectors have volume (Radius).
		// We query with an expanded radius to catch effectors whose center is far but radius overlaps.
		// search with a massive radius to be safe. 10k units is plenty lol.
		const float MaxEffectorSize = 10000.0f; 
		float QueryRadius = SearchRadius + MaxEffectorSize;
		
		TArray<UKClonerEffectorComponent*> Candidates = Subsystem->FindEffectorsNear(Location, QueryRadius);
		
		TArray<UKClonerEffectorComponent*> Result;
		Result.Reserve(Candidates.Num());

		// Precise check
		for (UKClonerEffectorComponent* Comp : Candidates)
		{
			if (Comp && Comp->bEnabled)
			{
				float Distance = FVector::Dist(Comp->GetEffectorLocation(), Location);
				float MaxRange = Comp->Radius + SearchRadius;
				if (Distance <= MaxRange)
				{
					Result.Add(Comp);
				}
			}
		}
		
		return Result;
	}

	return {};
}

TArray<UKClonerEffectorComponent*> FKClonerEffectorFinder::FindEffectorsInBounds(const UWorld* World, const FBox& Bounds)
{
	TArray<UKClonerEffectorComponent*> Result;
	
	if (!World) return Result;

	FVector Center = Bounds.GetCenter();
	float SearchRadius = Bounds.GetExtent().Size();

	return FindEffectorsNear(World, Center, SearchRadius);
}

float FKClonerEffectorFinder::GetCombinedInfluence(const TArray<UKClonerEffectorComponent*>& Effectors, const FVector& Location)
{
	if (Effectors.Num() == 0)
	{
		return 1.0f; // No effectors = full influence everywhere (default behavior)
	}

	// Combine using max (could also use additive or multiplicative)
	float MaxInfluence = 0.0f;
	for (const UKClonerEffectorComponent* Effector : Effectors)
	{
		if (Effector)
		{
			float Influence = Effector->GetInfluenceAtLocation(Location);
			MaxInfluence = FMath::Max(MaxInfluence, Influence);
		}
	}

	return MaxInfluence;
}
