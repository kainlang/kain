// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KClonerTypes.generated.h"

// how clones get positioned
UENUM(BlueprintType)
enum class EKClonerMode : uint8
{
	Grid UMETA(DisplayName = "Grid"),
	Radial UMETA(DisplayName = "Radial"),
	Linear UMETA(DisplayName = "Linear"),
	Spline UMETA(DisplayName = "Spline"),
	Single UMETA(DisplayName = "Single"),
	Honeycomb UMETA(DisplayName = "Honeycomb"),
	Scatter UMETA(DisplayName = "Scatter"),
	Mesh UMETA(DisplayName = "Mesh Surface")
};

// effector falloff shapes
UENUM(BlueprintType)
enum class EKClonerEffectorShape : uint8
{
	Sphere UMETA(DisplayName = "Sphere"),
	Box UMETA(DisplayName = "Box"),
	Plane UMETA(DisplayName = "Plane"),
	Cylinder UMETA(DisplayName = "Cylinder"),
	Torus UMETA(DisplayName = "Torus"),
	Unbound UMETA(DisplayName = "Unbound (Global)")
};

UENUM(BlueprintType)
enum class EKClonerMeshSampleMode : uint8
{
	Vertex UMETA(DisplayName = "Vertex"),
	Surface UMETA(DisplayName = "Surface (Random)"),
	Volume UMETA(DisplayName = "Volume (Random)")
};

// skeletal mesh rendering - pick your poison:
// quality vs performance
UENUM(BlueprintType)
enum class EKClonerSkeletalMode : uint8
{
  // full skeletal anim with physics. looks great, EXPENSIVE
	PhysicsIK UMETA(DisplayName = "Physics/IK (Live)"),
	
  // baked vertex animation textures. can do 1000s of instances
	VATBaked UMETA(DisplayName = "VAT/Baked"),
	
  // LOD: near=full quality, far=baked
	Auto UMETA(DisplayName = "Auto (Distance-Based)")
};

USTRUCT(BlueprintType)
struct FKClonerDistributionLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	EKClonerMode Mode = EKClonerMode::Grid;

  // GRID params
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Grid", ClampMin = "1"))
	FIntVector GridCount = FIntVector(3, 3, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Grid"))
	FVector GridSpacing = FVector(100.0f, 100.0f, 100.0f);

  // RADIAL params
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Radial", ClampMin = "1"))
	int32 RadialCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Radial"))
	float RadialRadius = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Radial"))
	bool bRadialAlign = true;

  // LINEAR params
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Linear", ClampMin = "1"))
	int32 LinearCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Linear"))
	FVector LinearOffset = FVector(0.0f, 0.0f, 100.0f);

  // SPLINE params (uses the cloner's builtin spline)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Spline", ClampMin = "1"))
	int32 SplineCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Spline"))
	bool bSplineAlign = true;

  // HONEYCOMB - offset hex grid pattern
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Honeycomb", ClampMin = "1"))
	FIntVector HoneycombCount = FIntVector(5, 5, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Honeycomb"))
	float HoneycombSize = 100.0f;

  // SCATTER - random in bounds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Scatter", ClampMin = "1"))
	int32 ScatterCount = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Scatter"))
	FVector ScatterBounds = FVector(500.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Scatter"))
	int32 ScatterSeed = 0;

  // MESH SURFACE - sample points on a mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Mesh", ClampMin = "1"))
	int32 MeshCount = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Mesh"))
	TObjectPtr<UStaticMesh> MeshAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Mesh"))
	EKClonerMeshSampleMode MeshSampleMode = EKClonerMeshSampleMode::Surface;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (EditCondition = "Mode == EKClonerMode::Mesh"))
	int32 MeshSeed = 0;
};
