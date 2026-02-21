// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" 
#include "NotifyData.h"
#include "Animation/AnimNotifies/AnimNotifyState.h" 
#include "AnimationDrivenNotifyState.generated.h"

class UAnimationDrivenComponent;
enum class EAnimNotifyState : uint8;
class UAnimNotifyData;

UCLASS()
class ANIMATIONDRIVENSYSTEM_API UAnimationDrivenNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()
	
public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	const UAnimNotifyData* GetNotifyData() const { return NotifyData; }

#ifdef UE_EDITOR
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
#endif
	
protected:
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "Notify")
	UAnimNotifyData* NotifyData; 
};