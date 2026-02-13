// Copyright 2026 K-Studio. All Rights Reserved.

// clang-format off
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KClonerTypes.h"
#include "KClonerModifier.h"
#include "KClonerData.h"
#include "Components/SplineComponent.h"
#include "KClonerActor.generated.h"

// Forward declarations
class AKClonerEffector;
class UKClonerModifier;
class USkeletalMesh;
class UAnimSequence;
class UTexture2D;
class UMaterialInstanceDynamic;
class UMaterialInterface;

// BP event delegates


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnClonerRebuilt);


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClonerUpdated, float, DeltaTime);


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCloneInteracted, int32, CloneIndex, FVector, HitLocation);

// perf cache - avoid recomputing shit every frame
struct FKClonerInstanceCache
{
	float R = 1.0f;
	float G = 1.0f;
	float B = 1.0f;
	float Time = 0.0f;
	bool bVisible = true;
};

UCLASS(Blueprintable, BlueprintType, Category = "K-Studio")
class KCLONER_API AKClonerActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AKClonerActor();
	virtual void Tick(float DeltaTime) override;

  // play an anim on all skeletal clones - pretty cool for crowds
	UFUNCTION(BlueprintCallable, Category = "Cloner|Animation")
	void PlayClonerMontage(UAnimMontage* Montage, float InPlayRate = 1.0f, FName StartSectionName = NAME_None);


	UFUNCTION(BlueprintCallable, Category = "Cloner")
	void ApplyPreset(UKClonerData* Preset);

  // force rebuild - call this if you change distribution params at runtime
	UFUNCTION(BlueprintCallable, Category = "Cloner")
	void ForceRebuild();


	UFUNCTION(BlueprintPure, Category = "Cloner")
	int32 GetInstanceCount() const { return BaseTransforms.Num(); }

  // ======= BP QUERY FUNCTIONS =======


	UFUNCTION(BlueprintPure, Category = "Cloner|Query")
	FTransform GetCloneTransform(int32 Index) const;


	UFUNCTION(BlueprintPure, Category = "Cloner|Query")
	FVector GetCloneLocation(int32 Index) const;


	UFUNCTION(BlueprintPure, Category = "Cloner|Query")
	int32 GetNearestCloneIndex(FVector WorldLocation) const;

  // warning: allocates every call, dont spam this
	UFUNCTION(BlueprintCallable, Category = "Cloner|Query")
	TArray<FTransform> GetAllCloneTransforms() const;


	UFUNCTION(BlueprintPure, Category = "Cloner|Query")
	bool IsAnimating() const;


	UFUNCTION(BlueprintPure, Category = "Cloner|Query")
	float GetEffectorInfluenceAtLocation(const FVector& WorldLocation) const;

  // ======= MODIFIER MANAGEMENT =======


	UFUNCTION(BlueprintPure, Category = "Cloner|Modifiers")
	UKClonerModifier* GetModifierByIndex(int32 Index) const;


	UFUNCTION(BlueprintPure, Category = "Cloner|Modifiers")
	int32 GetModifierCount() const { return Modifiers.Num(); }

  // runtime modifier spawning - returns the new modifier so you can config it
	UFUNCTION(BlueprintCallable, Category = "Cloner|Modifiers", meta = (DeterminesOutputType = "ModifierClass"))
	UKClonerModifier* AddModifierOfClass(TSubclassOf<UKClonerModifier> ModifierClass);


	UFUNCTION(BlueprintCallable, Category = "Cloner|Modifiers")
	bool RemoveModifier(UKClonerModifier* Modifier);


	UFUNCTION(BlueprintCallable, Category = "Cloner|Modifiers")
	void ClearAllModifiers();


	UFUNCTION(BlueprintCallable, Category = "Cloner|Modifiers")
	void SetModifierEnabled(int32 Index, bool bEnabled);

  // ======= CLONE VIS CONTROL =======

  // temp visibility, resets when you rebuild
	UFUNCTION(BlueprintCallable, Category = "Cloner|Control")
	void SetCloneVisible(int32 Index, bool bVisible);


	UFUNCTION(BlueprintCallable, Category = "Cloner|Control")
	void HideAllClones();


	UFUNCTION(BlueprintCallable, Category = "Cloner|Control")
	void ShowAllClones();

  // ======= BP EVENTS =======


	UPROPERTY(BlueprintAssignable, Category = "Cloner|Events")
	FOnClonerRebuilt OnClonerRebuilt;


	UPROPERTY(BlueprintAssignable, Category = "Cloner|Events")
	FOnClonerUpdated OnClonerUpdated;

  // hook this up if you want click interaction
	UPROPERTY(BlueprintAssignable, Category = "Cloner|Events")
	FOnCloneInteracted OnCloneInteracted;

  // C++ only accessors for cached data
	const TArray<FTransform>& GetCachedTransforms() const { return CachedTransforms; }


	const TArray<FKClonerInstanceCache>& GetCachedInstanceData() const { return CachedInstanceData; }

  // editor preview / sequencer scrubbing
	UPROPERTY(Transient)
	bool bUseOverrideTime = false;

	UPROPERTY(Transient)
	float OverrideTime = 0.0f;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool ShouldTickIfViewportsOnly() const override;
#endif

public:
	// --- PRESET (FIRST!) ---

	// --- MODE ---

  // ANIM TWEAK MODE - modifies existing animations with modifiers instead of generating new clones
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner", meta = (DisplayPriority = -110))
	bool bAnimTweakMode = false;

	// --- PRESET (FIRST!) ---


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner", meta = (DisplayName = "Cloner Preset", DisplayPriority = -100, EditCondition = "!bAnimTweakMode"))
	UKClonerData* ClonerPreset;

	// --- SOURCE MESH ---

  // set one or the other, not both
  // static mesh uses HISM (instanced), skeletal spawns actual components
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner", meta = (DisplayName = "Source Mesh", DisplayPriority = -99, EditCondition = "!bAnimTweakMode"))
	UStaticMesh* SourceMesh;

  // overrides static mesh if set
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner", meta = (DisplayPriority = -98))
	USkeletalMesh* SourceSkeletalMesh;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner", meta = (DisplayPriority = -97))
	UAnimSequence* SourceAnimSequence;

  // PhysicsIK=accurate+expensive, VAT=fast+baked, Auto=distance LOD
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner|Skeletal", meta = (DisplayPriority = -96))
	EKClonerSkeletalMode SkeletalMode = EKClonerSkeletalMode::PhysicsIK;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner|Skeletal", meta = (DisplayPriority = -95, ClampMin = "100.0", EditCondition = "SkeletalMode == EKClonerSkeletalMode::Auto"))
	float SkeletalModeDistanceThreshold = 2000.0f;

  // bake via Bake->VAT first, then plug the textures in here
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner|Skeletal", meta = (DisplayPriority = -94, EditCondition = "SkeletalMode != EKClonerSkeletalMode::PhysicsIK"))
	class UTexture2D* VATPositionTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner|Skeletal", meta = (DisplayPriority = -93, EditCondition = "SkeletalMode != EKClonerSkeletalMode::PhysicsIK"))
	class UTexture2D* VATRotationTexture;

	// --- DISTRIBUTION ---

  // stack multiple layers for combinatorial layouts (grid on spline, radial on mesh, etc)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = " K-Cloner|Distribution", meta = (DisplayPriority = -90, EditCondition = "!bAnimTweakMode"))
	TArray<FKClonerDistributionLayer> DistributionLayers;

	// --- MODIFIERS ---

  // THE GOOD STUFF - stack modifiers to animate your clones
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = " K-Cloner|Modifiers", meta = (DisplayPriority = -80))
	TArray<UKClonerModifier*> Modifiers;

	// --- EFFECTOR ---

  // drag an effector actor here for spatial falloff zones
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner|Effector", meta = (DisplayPriority = -75))
	TSoftObjectPtr<AActor> Effector;

  // or just auto-find all KClonerEffectorComponents in the level
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner|Effector", meta = (DisplayPriority = -74))
	bool bAutoDiscoverEffectors = false;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner|Effector", meta = (ClampMin = "0.0", DisplayPriority = -73, EditCondition = "bAutoDiscoverEffectors"))
	float EffectorSearchRadius = 2000.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner|Effector", meta = (DisplayPriority = -72, EditCondition = "!bAutoDiscoverEffectors"))
	EKClonerEffectorShape EffectorShape = EKClonerEffectorShape::Sphere;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = " K-Cloner|Effector", Setter = "SetEffectorRadius", meta = (ClampMin = "0.0", DisplayPriority = -71, EditCondition = "!bAutoDiscoverEffectors && (EffectorShape == EKClonerEffectorShape::Sphere || EffectorShape == EKClonerEffectorShape::Cylinder || EffectorShape == EKClonerEffectorShape::Torus)"))
	float EffectorRadius = 500.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = " K-Cloner|Effector", meta = (DisplayPriority = -70, EditCondition = "!bAutoDiscoverEffectors && EffectorShape == EKClonerEffectorShape::Box"))
	FVector EffectorExtent = FVector(250.0f);

  // donut hole size
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = " K-Cloner|Effector", meta = (ClampMin = "0.0", DisplayPriority = -69, EditCondition = "!bAutoDiscoverEffectors && EffectorShape == EKClonerEffectorShape::Torus"))
	float EffectorInnerRadius = 100.0f;

  // 0=hard edge, 1=full gradient
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = " K-Cloner|Effector", Setter = "SetEffectorFalloff", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = -68, EditCondition = "!bAutoDiscoverEffectors"))
	float EffectorFalloff = 0.5f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner|Effector", meta = (DisplayPriority = -67, EditCondition = "!bAutoDiscoverEffectors"))
	bool bInvertEffector = false;

	// --- ANIM TWEAK ---


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner", meta = (EditCondition = "bAnimTweakMode", ContentDir))
	FDirectoryPath OutputFolder; // Defaults set in constructor


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner", meta = (EditCondition = "bAnimTweakMode"))
	FString OutputName = TEXT("M_TweakedAnim");

	// --- ANIMATION ---

  // keyframe this for slow-mo effects
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = " K-Cloner|Animation", Setter = "SetTimeScale", meta = (ClampMin = "0.0", ClampMax = "10.0", DisplayPriority = -70))
	float TimeScale = 1.0f;

  // hidden from details but keyframeable in sequencer
  // auto-syncs with DistributionLayers[0]

	UPROPERTY(BlueprintReadWrite, Interp, Setter = "SetGridCount", BlueprintGetter = "GetGridCount", Category = "K-Cloner|Sequencer", meta = (DisplayName = "Grid Count (Sequencer Proxy)"))
	FVector GridCount = FVector(3.0f, 3.0f, 1.0f);


	UPROPERTY(BlueprintReadWrite, Interp, Setter = "SetGridSpacing", BlueprintGetter = "GetGridSpacing", Category = "K-Cloner|Sequencer", meta = (DisplayName = "Grid Spacing (Sequencer Proxy)"))
	FVector GridSpacing = FVector(150.0f);

  // ======= SEQUENCER SETTERS =======
	
	UFUNCTION(BlueprintCallable, Category = "Cloner|Sequencer")
	void SetTimeScale(float InTimeScale);
	
	UFUNCTION(BlueprintCallable, Category = "Cloner|Sequencer")
	void SetEffectorRadius(float InRadius);

	UFUNCTION(BlueprintCallable, Category = "Cloner|Sequencer")
	void SetEffectorFalloff(float InFalloff);


	UFUNCTION(BlueprintCallable, Category = "Cloner|Sequencer")
	void SetGridCount(FVector InCount);

	UFUNCTION(BlueprintPure, Category = "Cloner|Sequencer")
	FVector GetGridCount() const;


	UFUNCTION(BlueprintCallable, Category = "Cloner|Sequencer")
	void SetGridSpacing(FVector InSpacing);

	UFUNCTION(BlueprintPure, Category = "Cloner|Sequencer")
	FVector GetGridSpacing() const;

  // call this from BP if you change modifier props at runtime
	UFUNCTION(BlueprintCallable, Category = "Cloner|Sequencer")
	void MarkModifiersDirty() { bModifiersDirty = true; }

	// --- PERFORMANCE ---

  // perf: skip updates if nothing is moving
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner|Performance", meta = (DisplayPriority = -60))
	bool bOptimizeStaticInstances = true;

  // 0=every frame, 1=every other, 2=every third, etc
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner|Performance", meta = (ClampMin = "0", ClampMax = "10", DisplayPriority = -59))
	int32 UpdateSkipFrames = 0;

  // optional: spawn niagara at each clone for bonus VFX
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner|VFX", meta = (DisplayName = "Niagara System", DisplayPriority = -50))
	TObjectPtr<class UNiagaraSystem> NiagaraEmitter;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner|VFX", meta = (ClampMin = "0.1", ClampMax = "10.0", EditCondition = "NiagaraEmitter != nullptr", EditConditionHides))
	float VFXScale = 1.0f;


	UPROPERTY(Transient)
	TObjectPtr<class UNiagaraComponent> VFXComponent;


	UFUNCTION(BlueprintCallable, Category = "Cloner|VFX")
	void UpdateVFXComponent();

  // ======= COMPONENTS (dont mess with these) =======
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = " K-Cloner|Components", meta = (DisplayPriority = 100))
	class UHierarchicalInstancedStaticMeshComponent* InstancedMesh;

  // VAT mode uses this second HISM for baked animations
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = " K-Cloner|Components", meta = (DisplayPriority = 100))
	class UHierarchicalInstancedStaticMeshComponent* VATInstancedMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = " K-Cloner|Components", meta = (DisplayPriority = 101))
	class USkeletalMeshComponent* TemplateSkeletalComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = " K-Cloner|Components", meta = (DisplayPriority = 102))
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = " K-Cloner|Components", meta = (DisplayPriority = 103))
	USplineComponent* SplineComponent;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " K-Cloner|Skeletal", meta = (DisplayPriority = -92, EditCondition = "SkeletalMode != EKClonerSkeletalMode::PhysicsIK"))
	class UMaterialInterface* VATBaseMaterial;


	UPROPERTY(Transient)
	class UMaterialInstanceDynamic* VATMaterialInstance;

private:
  // ======= INTERNAL CACHE (perf) =======
	

	TArray<FTransform> BaseTransforms;

  // pre-alloc working arrays - no per-frame allocs!
	TArray<FTransform> CachedTransforms;
	TArray<FKClonerInstanceCache> CachedInstanceData;


	bool bNeedsRebuild = true;
	bool bModifiersDirty = true;
	int32 FrameCounter = 0;
	float LastWorldTime = 0.0f;

  // skeletal mesh pool for animated clones
	UPROPERTY(Transient)
	TArray<USkeletalMeshComponent*> SkeletalMeshPool;
 

	TArray<class UKClonerEffectorComponent*> CachedEffectors;

  // ======= INTERNALS =======


	void RebuildInstances();
	

	void UpdateSkeletalMeshPool(int32 TargetCount);


	TArray<FTransform> GenerateLayerTransforms(const FKClonerDistributionLayer& Layer);


	TArray<FTransform> SampleMeshPoints(UStaticMesh* Mesh, int32 Count, EKClonerMeshSampleMode Mode, int32 Seed = 0);


	void UpdateModifierAnimation(float DeltaTime);


	float GetEffectorInfluence(const FVector& WorldPosition) const;


	void EnsureCacheCapacity(int32 Count);
};
// clang-format on
