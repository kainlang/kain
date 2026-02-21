// © 2025 mrbaconvn. All Rights Reserved.

#include "Notify/AnimationDrivenNotify.h" 
#include "AnimationDrivenComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "NotifyData.h"

FString UAnimationDrivenNotify::GetNotifyName_Implementation() const
{
	return Super::GetNotifyName_Implementation();
}

void UAnimationDrivenNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
		
	if(!IsValid(MeshComp) || !IsValid(NotifyData))
	{
		return;
	}  
 
	SendNotifyData(MeshComp->GetOwner(), Animation, NotifyData, EAnimNotifyState::ENSD_NONE);
}

bool UAnimationDrivenNotify::SendNotifyData(const AActor* Target, const UAnimSequenceBase* SourceAnim,
	const UAnimNotifyData* NotifyData, EAnimNotifyState NotifyState)
{
	check(NotifyData);

	if(!IsValid(Target))
	{
		return false;
	}
	
	bool bIsPreviewWorld = false;
	
#if UE_EDITOR 
	bIsPreviewWorld = IsValid(Target->GetWorld()) && Target->GetWorld()->IsPreviewWorld();
	if ((CVarDebugAnimDrivenNotify.GetValueOnGameThread() == 1 && bIsPreviewWorld) || CVarDebugAnimDrivenNotify.GetValueOnGameThread() == 2)
	{
		if(Target->GetLocalRole() == ROLE_AutonomousProxy || bIsPreviewWorld)
		{
			NotifyData->DebugNotifyData(Target, NotifyState);
		}
	}
#endif

	if(NotifyState == EAnimNotifyState::ENSD_TICK)
	{
		// We don't send tick notify to the component, since you can tick yourself with start and end notify
		return false;
	}
	
	UAnimationDrivenComponent* AnimDrivenComponent = Cast<UAnimationDrivenComponent>(Target->GetComponentByClass(UAnimationDrivenComponent::StaticClass()));
	if (!IsValid(AnimDrivenComponent))
	{
		if(!bIsPreviewWorld)
		{
			// If it's not preview world, then we need to log it since the actor doesn't have the AnimationDrivenComponent and it's in gameplay
			UE_LOG(LogTemp, Error, TEXT("UAnimationDrivenNotify::Notify: AnimDrivenComponent is not valid in gameplay"));
		}
		return false;
	} 

	AnimDrivenComponent->OnNotifyReceived.Broadcast(SourceAnim, NotifyData, NotifyState);
	return true;
}  
