// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "StructUtils/InstancedStruct.h"
#else 
#include "InstancedStruct.h"
#endif
#include "Action/ComboActionPass.h" 
#include "ComboActionPass_ComboAbility.generated.h"

struct FCombatAbilityDataPayload; 
class UGameplayAbility;

UCLASS(DisplayName = "GAS activate combat ability", meta = (ToolTip = "Activate combat GAS ability"))
class COMBOGRAPHASC_API UComboActionPass_ComboAbility : public UComboActionPass
{
	GENERATED_BODY()
	
public:
	virtual TArray<FSoftObjectPath> GetPreviewAssets() const override;
	virtual FString GetPassInformation_Implementation() const override;
	virtual bool ExecutePass_Implementation(UComboGraphInstance* GraphInstance) const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	TInstancedStruct<FCombatAbilityDataPayload> AbilityPayload;
};
