/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "DialogueMasterAction.h"
#include "QuestNodeInfoBase.h"
#include "StepTypeEnum.h"
#include "QuestNodeInfo.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class DIALOGASSETEDITORRUNTIME_API UQuestNodeInfo : public UQuestNodeInfoBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Info")
	FName ID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Info", meta=(MultiLine = true))
	FText Description;

	UPROPERTY()
	TArray<FText> NodeOutputs;
	
	virtual FText GetGraphNodeDescription() const;

	virtual FName GetID() const override { return ID; }
};

UCLASS(BlueprintType)
class DIALOGASSETEDITORRUNTIME_API UQuestStepNodeInfo : public UQuestNodeInfo
{
	GENERATED_BODY()

	UQuestStepNodeInfo()
	{
		this->StepType = NORMAL;
	}
	
	UQuestStepNodeInfo(TEnumAsByte<EStepType> StepType)
	{
		this->StepType = StepType;
	}

	FString GetActionsInfoString() const;
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Info")
	TEnumAsByte<EStepType> StepType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Info")
	FText StepName;
	
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadOnly, Category="Info")
	TArray<UDialogueMasterAction*> Actions;

	UFUNCTION(BlueprintCallable, Category="Quests")
	FText GetShortDescription() const;

	virtual FText GetGraphNodeDescription() const override;
};

UCLASS(BlueprintType)
class DIALOGASSETEDITORRUNTIME_API UQuestTaskListNodeInfo : public UQuestNodeInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Tasks")
	TArray<class UDialogueMasterTask*> Tasks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Info")
	bool Hidden;
	
	virtual FText GetGraphNodeDescription() const override;
};