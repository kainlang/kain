// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "KClonerTypes.h"
#include "KClonerEffectorComponent.generated.h"

// Effector component - stick this on any actor to make it influence nearby cloners
// players, enemies, projectiles, whatever - it all works!
// multiple effectors can affect the same cloner at once
UCLASS(ClassGroup = (KStudio), meta = (BlueprintSpawnableComponent, DisplayName = "KClonerEffector"))
class KCLONER_API UKClonerEffectorComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UKClonerEffectorComponent();

  // ======= SHAPE =======


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effector")
	EKClonerEffectorShape Shape = EKClonerEffectorShape::Sphere;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effector", meta = (ClampMin = "0.0", EditCondition = "Shape == EKClonerEffectorShape::Sphere || Shape == EKClonerEffectorShape::Cylinder || Shape == EKClonerEffectorShape::Torus"))
	float Radius = 500.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effector", meta = (EditCondition = "Shape == EKClonerEffectorShape::Box"))
	FVector Extent = FVector(250.0f);

  // donut hole
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effector", meta = (ClampMin = "0.0", EditCondition = "Shape == EKClonerEffectorShape::Torus"))
	float InnerRadius = 100.0f;

  // ======= INFLUENCE =======

  // 0=hard edge, 1=smooth gradient
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effector", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Falloff = 0.5f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effector", meta = (ClampMin = "0.0"))
	float Strength = 1.0f;

  // flip it - affect outside instead of inside
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effector")
	bool bInvert = false;

  // higher = processed first when overlapping
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effector")
	int32 Priority = 0;




	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effector")
	bool bEnabled = true;

  // ======= DEBUG VIS =======


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	bool bVisualize = true;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	FLinearColor VisualizerColor = FLinearColor(1.0f, 0.4f, 0.0f, 1.0f);


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization", meta = (ClampMin = "0.1"))
	float VisualizerThickness = 2.0f;



	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

  // returns 0 if outside, 1 if fully inside, gradient in between
	UFUNCTION(BlueprintCallable, Category = "Effector")
	float GetInfluenceAtLocation(const FVector& WorldLocation) const;


	UFUNCTION(BlueprintPure, Category = "Effector")
	FVector GetEffectorLocation() const { return GetComponentLocation(); }


	UFUNCTION(BlueprintPure, Category = "Effector")
	FTransform GetEffectorTransform() const { return GetComponentTransform(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	FVector LastLocation;
};

// helper to find effectors in the world
struct KCLONER_API FKClonerEffectorFinder
{

	static TArray<UKClonerEffectorComponent*> FindEffectorsNear(const UWorld* World, const FVector& Location, float SearchRadius);


	static TArray<UKClonerEffectorComponent*> FindEffectorsInBounds(const UWorld* World, const FBox& Bounds);

  // blend all effectors together
	static float GetCombinedInfluence(const TArray<UKClonerEffectorComponent*>& Effectors, const FVector& Location);
};
