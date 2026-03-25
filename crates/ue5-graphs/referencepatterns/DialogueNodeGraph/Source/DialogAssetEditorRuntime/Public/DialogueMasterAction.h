/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DialogueMasterAction.generated.h"

class UDialogueMasterComponent;
class ADialogPlayer;
class UDataLayerSubsystem;

UENUM(BlueprintType)
enum EActionTriggerMoment
{
	Beginning       UMETA(DisplayName = "At the beginning of the dialogue line / quest step"),
	End				UMETA(DisplayName = "At the end of the dialogue line / quest step"),
	Both            UMETA(DisplayName = "At the beginning AND the end of the dialogue line / quest step")
};


/**
 * 
 */
UCLASS(DefaultToInstanced, EditInlineNew, BlueprintType, Blueprintable)
class DIALOGASSETEDITORRUNTIME_API UDialogueMasterAction : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Dialogue Master - Actions")
	void Initialize(AActor * NPC, AActor * Player, UWorld * world, ADialogPlayer * DialoguePlayerRef);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Dialogue Master - Actions")
	void Trigger(UDialogueMasterComponent * DialogueMasterComponent, EActionTriggerMoment CurrentMoment);

	virtual UWorld* GetWorld() const override
	{
		if (HasAllFlags(RF_ClassDefaultObject))
		{
			return nullptr;
		}

		if(_World != nullptr) return _World;
		
		UObject* Outer = GetOuter();

		while (Outer)
		{
			UWorld* World = Outer->GetWorld();
			if (World)
			{
				return World;
			}

			Outer = Outer->GetOuter();
		}

		return nullptr;
	}

	/**
	 * The moment of the dialogue line when the action have to trigger.
	 * You have 3 options :
	 *  - At the beginning of the dialogue line.
	 *  - At the end of the dialogue line.
	 *  - At the beginning AND the end of the dialogue line.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initialization")
	TEnumAsByte<EActionTriggerMoment> TriggerMoment = End;
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Dialogue Master - Actions")
	FString GetDescription();

	UFUNCTION(BlueprintCallable, Category="Dialogue Master - World Partition Manager")
	UDataLayerSubsystem* GetDataLayerSubsystem();

protected:
	UWorld * _World = nullptr;
	
	UPROPERTY(BlueprintReadWrite, Category="Runtime (do not edit)")
	AActor * _NPC;

	UPROPERTY(BlueprintReadWrite, Category="Runtime (do not edit)")
	AActor * _Player;

	UPROPERTY(BlueprintReadOnly, Category="Runtime (do not edit)")
	ADialogPlayer * _DialoguePlayer;
};
