// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NotifyData.h"
#include "Components/ActorComponent.h"
#include "AnimationDrivenComponent.generated.h"

class UAnimSequenceBase; 

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAnimDrivenNotifyReceived, const UAnimSequenceBase*, SourceAnim, const UAnimNotifyData*, NotifyData, EAnimNotifyState, NotifyState);

UCLASS(ClassGroup=(AnimationDriven), meta=(BlueprintSpawnableComponent), Blueprintable)
class ANIMATIONDRIVENSYSTEM_API UAnimationDrivenComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAnimationDrivenComponent();

public:
	UPROPERTY(BlueprintAssignable)
	FOnAnimDrivenNotifyReceived OnNotifyReceived;
};
