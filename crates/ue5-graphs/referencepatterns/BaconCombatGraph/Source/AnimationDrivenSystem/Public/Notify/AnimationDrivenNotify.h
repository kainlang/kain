// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" 
#include "Animation/AnimNotifies/AnimNotify.h" 
#include "NotifyData.h"
#include "HAL/IConsoleManager.h"
#include "AnimationDrivenNotify.generated.h"

class UAnimationDrivenComponent;
class UAnimSequenceBase;

inline static TAutoConsoleVariable<int> CVarDebugAnimDrivenNotify(TEXT("debug.AnimDrivenNotify"), 1,TEXT("1 - Editor Only | 2 - Game & Editor | 0 - Disable debug"), ECVF_Default);

UCLASS()
class ANIMATIONDRIVENSYSTEM_API UAnimationDrivenNotify : public UAnimNotify
{
	GENERATED_BODY()
	
public: 
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	const UAnimNotifyData* GetNotifyData() const { return NotifyData; }

	static bool SendNotifyData(const AActor* Target, const UAnimSequenceBase* SourceAnim, const UAnimNotifyData* Data, EAnimNotifyState NotifyState); 
	 
protected:
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "Notify")
	UAnimNotifyData* NotifyData;
};
