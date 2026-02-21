// © 2025 mrbaconvn. All Rights Reserved.

#pragma once
 
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NotifyData.generated.h"

UENUM(Blueprintable)
enum class EAnimNotifyState : uint8
{
	ENSD_NONE,
	ENSD_START,
	ENSD_END, 
	ENSD_TICK,
	ENSD_MAX UMETA(Hidden)
};

UCLASS(Blueprintable, DefaultToInstanced, EditInlineNew)
class ANIMATIONDRIVENSYSTEM_API UAnimNotifyData : public UObject
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName() const { return GetName(); }
	virtual void DebugNotifyData(const AActor* DebugActor, EAnimNotifyState NotifyState) const;
};