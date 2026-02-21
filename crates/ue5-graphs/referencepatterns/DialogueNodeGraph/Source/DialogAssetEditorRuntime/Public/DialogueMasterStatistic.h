/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "Engine/DataAsset.h"
#include "DialogueMasterStatistic.generated.h"

UCLASS(BlueprintType, Blueprintable)
class DIALOGASSETEDITORRUNTIME_API UDialogueMasterStatistic : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UDialogueMasterStatistic();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stat info")
	FString StatName;

	UPROPERTY(EditAnywhere, Category="Stat info")
	FText Description;

	/** If counting player number of sword swing, the value of this property should be "Sword swing count" */
	UPROPERTY(EditAnywhere, Category="Stat info")
	FString CountedEntityName;

	/** A default value for the CountedEntityName */
	UPROPERTY(EditAnywhere, Category="Stat info")
	FString DefaultEntityName;

	static FString GenerateStatKey(const FString& StatName, const FString& EntityName);
	FString GenerateStatKey(const FString& EntityName) const;
	FText GetDisplayText() const;
};