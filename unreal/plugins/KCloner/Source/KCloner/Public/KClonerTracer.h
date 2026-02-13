// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KClonerTracer.generated.h"

class AKClonerActor;
class USplineComponent;
class USplineMeshComponent;

/**
 * K-Cloner Tracer: Generates splines from the motion history of clones.
 * Creates motion trails, particle ribbons, and time-lapse effects.
 */
UCLASS(Blueprintable, BlueprintType, Category = "K-Studio")
class KCLONER_API AKClonerTracer : public AActor
{
	GENERATED_BODY()

public:
	AKClonerTracer();

	virtual void Tick(float DeltaTime) override;

	// --- TRACKING SOURCE ---

	/** K-Cloner actor to track */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracer|Source")
	TSoftObjectPtr<AKClonerActor> TrackedCloner;

	/** Specific clone indices to track (empty = track all) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracer|Source")
	TArray<int32> TrackedIndices;

	/** Maximum number of clones to track (0 = all) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracer|Source", meta = (ClampMin = "0"))
	int32 MaxTrackedClones = 10;

	// --- TRAIL SETTINGS ---

	/** Maximum number of spline points per trail */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracer|Trail", meta = (ClampMin = "2", ClampMax = "1000"))
	int32 TrailLength = 100;

	/** Seconds between position samples */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracer|Trail", meta = (ClampMin = "0.01"))
	float SampleRate = 0.033f; // ~30 FPS

	/** Minimum distance between samples (prevents clutter when stationary) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracer|Trail", meta = (ClampMin = "0.0"))
	float MinSampleDistance = 1.0f;

	// --- VISUALIZATION ---

	/** Optional static mesh to use for spline mesh rendering */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracer|Visualization")
	UStaticMesh* SplineMesh;

	/** Material for spline mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracer|Visualization")
	UMaterialInterface* SplineMaterial;

	/** Width of the trail at start (oldest point) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracer|Visualization", meta = (ClampMin = "0.0"))
	float TrailWidthStart = 5.0f;

	/** Width of the trail at end (newest point) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracer|Visualization", meta = (ClampMin = "0.0"))
	float TrailWidthEnd = 20.0f;

	/** Enable debug drawing of splines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracer|Visualization")
	bool bDebugDraw = true;

	/** Debug line color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracer|Visualization", meta = (EditCondition = "bDebugDraw"))
	FLinearColor DebugColor = FLinearColor::Green;

	// --- CONTROL ---

	/** Start/stop recording */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracer|Control")
	bool bRecording = true;

	/** Clear all trails */
	UFUNCTION(BlueprintCallable, Category = "Tracer")
	void ClearTrails();

	/** Get spline component for a tracked clone */
	UFUNCTION(BlueprintCallable, Category = "Tracer")
	USplineComponent* GetTrailSpline(int32 CloneIndex) const;

protected:
	virtual void BeginPlay() override;

private:
	/** Root component */
	UPROPERTY()
	USceneComponent* Root;

	/** Time since last sample */
	float TimeSinceLastSample = 0.0f;

	/** Trail data per tracked clone */
	struct FTrailData
	{
		TArray<FVector> Points;
		TObjectPtr<USplineComponent> Spline = nullptr;
		FVector LastSampledPosition = FVector::ZeroVector;
	};
	
	TMap<int32, FTrailData> TrailMap;

	/** Sample current positions and add to trails */
	void SamplePositions();

	/** Update spline components from trail data */
	void UpdateSplines();

	/** Create spline component for a trail */
	USplineComponent* CreateSplineForTrail(int32 CloneIndex);
};
