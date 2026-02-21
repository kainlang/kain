// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/HitResult.h"
#include "NotifyData.h"
#include "DamageShape.h"

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "StructUtils/InstancedStruct.h"
#else 
#include "InstancedStruct.h"
#endif

#include "DamageNotifyData.generated.h" 

UCLASS()
class COMBOGRAPHASC_API UDamageNotifyData : public UAnimNotifyData
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName() const override;
	virtual void DebugNotifyData(const AActor* DebugActor, EAnimNotifyState NotifyState) const override; 
	TArray<FHitResult> TraceHit(const AActor* Source, const TArray<ECollisionChannel>& CollisionChannels) const;
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Notify")
	TArray<TInstancedStruct<FDamageShape>> DamageShapes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Notify")
	FGameplayTagContainer DamageTags;
};
