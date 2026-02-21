// © 2025 mrbaconvn. All Rights Reserved.

#include "Notify/AnimationDrivenNotifyState.h" 
#include "Components/SkeletalMeshComponent.h" 
#include "Notify/AnimationDrivenNotify.h"
#include "NotifyData.h" 

FString UAnimationDrivenNotifyState::GetNotifyName_Implementation() const
{
	if(!IsValid(NotifyData))
	{
		return Super::GetNotifyName_Implementation();
	}
	
	return NotifyData->GetNotifyName();
}

void UAnimationDrivenNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	
	if(!IsValid(MeshComp) || !IsValid(NotifyData))
	{
		return;
	}  
 
	UAnimationDrivenNotify::SendNotifyData(MeshComp->GetOwner(), Animation, NotifyData, EAnimNotifyState::ENSD_START);
}

#ifdef UE_EDITOR
void UAnimationDrivenNotifyState::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	
	if(!IsValid(MeshComp) || !IsValid(NotifyData))
	{
		return;
	}  
 
	UAnimationDrivenNotify::SendNotifyData(MeshComp->GetOwner(), Animation, NotifyData, EAnimNotifyState::ENSD_TICK);
}
#endif

void UAnimationDrivenNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	
	if(!IsValid(MeshComp) || !IsValid(NotifyData))
	{
		return;
	}  
 
	UAnimationDrivenNotify::SendNotifyData(MeshComp->GetOwner(), Animation, NotifyData, EAnimNotifyState::ENSD_END);
}

