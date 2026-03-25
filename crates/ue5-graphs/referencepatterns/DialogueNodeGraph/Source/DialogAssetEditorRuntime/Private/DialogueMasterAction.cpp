/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */


#include "DialogueMasterAction.h"
#include "DialogPlayer.h"
#include "DialogueMasterComponent.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"

void UDialogueMasterAction::Trigger_Implementation(UDialogueMasterComponent * DialogueMasterComponent, EActionTriggerMoment CurrentMoment)
{
	
}

void UDialogueMasterAction::Initialize_Implementation(AActor* NPC, AActor* Player, UWorld * world, ADialogPlayer * DialoguePlayerRef)
{
	this->_NPC = NPC;
	this->_Player = Player;
	this->_World = world;
	this->_DialoguePlayer = DialoguePlayerRef;
}


FString UDialogueMasterAction::GetDescription_Implementation()
{
	return "Not implemented description getter !";
}

UDataLayerSubsystem* UDialogueMasterAction::GetDataLayerSubsystem()
{
	UDataLayerSubsystem* DataLayerSubsystem = _World->GetSubsystem<UDataLayerSubsystem>();
	return DataLayerSubsystem;
}
