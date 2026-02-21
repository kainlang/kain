// © 2025 mrbaconvn. All Rights Reserved.

#include "ComboManager/ComboManagerComponent.h"
#include "GraphInstance/ComboGraphInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "ComboGraphAsset.h"

UComboManagerComponent::UComboManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UComboManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// The component must tick after the animation component so notify events are processed
	AddTickPrerequisiteComponent( GetOwner()->FindComponentByClass<USkeletalMeshComponent>() );

	// Grant default combo graphs
	GrantDefaultComboGraphs();
}

bool UComboManagerComponent::GrantComboGraph(const FComboGraphGrantParams& GrantParams)
{
	UComboGraphAsset* ComboGraphAsset = GrantParams.ComboGraphAsset.LoadSynchronous();
	if(GetGraphIndex(ComboGraphAsset) >= 0)
	{
		// Already granted
		return false;
	}

	TSubclassOf<UComboGraphInstance> GraphInstanceClass = GrantParams.GraphInstance.LoadSynchronous();
	if(!IsValid(GraphInstanceClass))
	{
		UE_LOG(LogTemp, Error, TEXT("ComboManagerComponent::GrantComboGraph - Invalid graph instance class"));
		return false;
	}
	
	UComboGraphInstance* NewInstance = NewObject<UComboGraphInstance>(this, GraphInstanceClass);
	if(!IsValid(NewInstance))
	{
		return false;
	}
	
	bool bCanConstruct = NewInstance->ConstructInstance(ComboGraphAsset);
	if(!bCanConstruct)
	{
		return false;
	}
	
	ComboGraphInstances.Add(NewInstance);
	return true;
} 

bool UComboManagerComponent::RemoveComboGraph(TSoftObjectPtr<UComboGraphAsset> InComboGraphAsset)
{
	UComboGraphAsset* ComboGraphAsset = InComboGraphAsset.LoadSynchronous();
	int Index = GetGraphIndex(ComboGraphAsset);
	if(Index != INDEX_NONE)
	{
		ComboGraphInstances.RemoveAt(Index);
		return true;
	}
	
	return false;
}

int UComboManagerComponent::GetGraphIndex(UComboGraphAsset* InComboGraphAsset) const
{ 
	for (int i = 0; i < ComboGraphInstances.Num(); i++)
	{
		if (ComboGraphInstances[i]->GetComboGraphAsset() == InComboGraphAsset)
		{
			return i;
		}
	}
	
	return INDEX_NONE;
} 

void UComboManagerComponent::SetComboInstanceActive(const UComboGraphInstance* InComboGraphInstance)
{
	if(!IsValid(InComboGraphInstance))
	{
		UE_LOG (LogTemp, Warning, TEXT("ComboManagerComponent::SetComboInstanceActive - Invalid instance"));
		return;
	}
	
	int Index = ComboGraphInstances.IndexOfByKey(InComboGraphInstance);
	if(Index == INDEX_NONE || Index == ActiveGraphIndex)
	{
		UE_LOG (LogTemp, Warning, TEXT("ComboManagerComponent::SetComboInstanceActive - Instance does not exist in Manager or already active"));
		return;
	} 
	
	if(UComboGraphInstance* CurrentInstance = GetCurrentInstance())
	{
		// Clean up old instance
		CurrentInstance->OnInputWindowUpdate.RemoveDynamic(this, &UComboManagerComponent::OnCurrentInstanceInputWindowUpdate);
		CurrentInstance->OnInstanceReset.RemoveDynamic(this, &UComboManagerComponent::OnCurrentInstanceReset);
		CurrentInstance->SetInstanceActive(false);
	}

	ActiveGraphIndex = Index;
	
	if(UComboGraphInstance* CurrentInstance = GetCurrentInstance())
	{
		CurrentInstance->OnInputWindowUpdate.AddDynamic(this, &UComboManagerComponent::OnCurrentInstanceInputWindowUpdate);
		CurrentInstance->OnInstanceReset.AddDynamic(this, &UComboManagerComponent::OnCurrentInstanceReset);
		CurrentInstance->SetInstanceActive(true);
	}
}

UComboGraphInstance* UComboManagerComponent::GetComboGraphInstanceByAsset(
	const TSoftObjectPtr<UComboGraphAsset>& InComboGraphAsset) const
{
	UComboGraphAsset* ComboGraphAsset = InComboGraphAsset.LoadSynchronous();
	int Index = GetGraphIndex(ComboGraphAsset);
	if(Index != INDEX_NONE)
	{
		return ComboGraphInstances[Index];
	}
	
	return nullptr;
}

UComboGraphInstance* UComboManagerComponent::GetCurrentInstance(int OffsetIndex) const
{
	int Index = ActiveGraphIndex + OffsetIndex;

	if (Index < 0)
	{
		Index = ComboGraphInstances.Num() - 1;
	}
	else if(Index >= ComboGraphInstances.Num())
	{
		Index = 0;
	}
	
	if(ComboGraphInstances.IsValidIndex(Index))
	{
		return ComboGraphInstances[Index];
	}
	
	return nullptr;
}

bool UComboManagerComponent::TriggerInput(FGameplayTag InInputTag)
{
	if(UComboGraphInstance* CurrentInstance = GetCurrentInstance())
	{
		return CurrentInstance->TriggerInput(InInputTag);
	}
	
	return false;
}

bool UComboManagerComponent::GrantDefaultComboGraphs()
{
	bool bSuccess = true;
	for (const FComboGraphGrantParams& GrantParams : DefaultComboGraphs)
	{
		bSuccess &= GrantComboGraph(GrantParams);
	}

	if (ComboGraphInstances.Num() > 0)
	{
		SetComboInstanceActive(ComboGraphInstances[0]);
	}
	
	return bSuccess;
}

void UComboManagerComponent::OnCurrentInstanceReset(EComboResetReason Reason)
{
	OnInstanceReset.Broadcast(Reason);
}

void UComboManagerComponent::OnCurrentInstanceInputWindowUpdate(bool bIsInInputWindow)
{
	OnInputWindowUpdate.Broadcast(bIsInInputWindow);
}

void UComboManagerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UComboGraphInstance* CurrentInstance = GetCurrentInstance();
	if(CurrentInstance && CurrentInstance->bNeedProceed)
	{
		OnInstancePreProceed.Broadcast( CurrentInstance, true );  
		CurrentInstance->bNeedProceed = false;
		bool bSuccess = CurrentInstance->TryProceedGraph();
		OnInstanceProceed.Broadcast( CurrentInstance, bSuccess );
	}
} 