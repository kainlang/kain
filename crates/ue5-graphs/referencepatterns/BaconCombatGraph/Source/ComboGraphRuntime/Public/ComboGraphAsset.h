// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" 
#include "ComboGraphAsset.generated.h"

class UComboGraphData;

DECLARE_MULTICAST_DELEGATE_OneParam(FComboGraphAssetChangedDelegate, UComboGraphAsset*);

UCLASS(BlueprintType)
class COMBOGRAPHRUNTIME_API UComboGraphAsset : public UObject
{
	GENERATED_BODY()
	
public:  
	FComboGraphAssetChangedDelegate PreSaveDelegate;
	FComboGraphAssetChangedDelegate PostSaveDelegate;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo Graph")
	FText ComboName = {};
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo Graph")
	bool bResetWhenGraphBlocked = true;

	UPROPERTY()
	UComboGraphData* GraphData = nullptr;
};