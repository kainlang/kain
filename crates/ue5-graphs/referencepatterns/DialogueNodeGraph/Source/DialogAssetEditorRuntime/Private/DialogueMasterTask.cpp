/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */


#include "DialogueMasterTask.h"
#include "DialogueMasterComponent.h"
#include "QuestAsset.h"

#define LOCTEXT_NAMESPACE "DialogueMasterTask"

UDialogueMasterTask::UDialogueMasterTask()
{
	CurrentNumberOfCompletionDone = 0;
}

bool UDialogueMasterTask::IsCompleted()
{
	return CurrentNumberOfCompletionDone >= NumberOfCompletionRequired || bOptionalTask;
}

void UDialogueMasterTask::PreInitializeTask()
{
	CurrentNumberOfCompletionDone = 0;
}

void UDialogueMasterTask::RestoreTask(UQuestAsset* ParQuest, UDialogueMasterComponent* OwningComp,
	APlayerController* OwningPC)
{
	this->ParentQuest = ParQuest;
	this->OwningComponent = OwningComp;
	this->OwningPlayerController = OwningPC;
	this->_World = OwningComponent->GetWorld();

	BP_BeginTask();
}

void UDialogueMasterTask::BeginTask(UQuestAsset* ParQuest, UDialogueMasterComponent* OwningComp, APlayerController* OwningPC)
{
	this->ParentQuest = ParQuest;
	this->OwningComponent = OwningComp;
	this->OwningPlayerController = OwningPC;
	this->_World = OwningComponent->GetWorld();

	CurrentNumberOfCompletionDone = 0;

	BP_BeginTask();
}

void UDialogueMasterTask::EndTask()
{
	CurrentNumberOfCompletionDone = 0;
	
	BP_EndTask();
}

void UDialogueMasterTask::UpdateNbCompletion(int newValue)
{
	if(newValue != CurrentNumberOfCompletionDone)
	{
		int PreviousValue = CurrentNumberOfCompletionDone;

		CurrentNumberOfCompletionDone = FMath::Clamp(newValue, 0, NumberOfCompletionRequired);
		ParentQuest->OnTaskProgressUpdated(this, PreviousValue, CurrentNumberOfCompletionDone);
	
		if(CurrentNumberOfCompletionDone >= NumberOfCompletionRequired)
		{
			BP_OnTaskCompleted();
			ParentQuest->OnTaskCompleted(this);
		}
	}
}

void UDialogueMasterTask::AddTaskCompletion(int NbOfCompletion)
{
	UpdateNbCompletion(CurrentNumberOfCompletionDone + NbOfCompletion);
}

void UDialogueMasterTask::SetTaskCompletion(int NbOfCompletion)
{
	UpdateNbCompletion(NbOfCompletion);
}

void UDialogueMasterTask::CompleteTask()
{
	UpdateNbCompletion(NumberOfCompletionRequired);
}

void UDialogueMasterTask::ForceCompletionUpdate()
{
	ParentQuest->OnTaskProgressUpdated(this, CurrentNumberOfCompletionDone, CurrentNumberOfCompletionDone);
}

FText UDialogueMasterTask::GetDescription_Implementation()
{
	return FText::FromString("Not implemented description getter !");
}

FText UDialogueMasterTask::GetProgressText_Implementation()
{
	return FText::Format(FTextFormat::FromString("({0}/{1})"), CurrentNumberOfCompletionDone, NumberOfCompletionRequired);
}

FText UDialogueMasterTask::GetInGameTaskDescription_Implementation()
{
	FText FormatInput = FText::FromString("{Optional}{Description}");
	FFormatNamedArguments Args;

	FText optionalIndicator = FText::FromString("");
	if(bOptionalTask)
		optionalIndicator = FText::Format(FTextFormat::FromString("({0}) "), LOCTEXT("Optional", "Optional"));
	// Add Title :
	Args.Add("Optional", optionalIndicator);
	
	if(!DescriptionOverride.IsEmpty())
		Args.Add("Description", DescriptionOverride);
	else
		Args.Add("Description", GetDescription());

	return FText::Format(FormatInput, Args);
}

#undef LOCTEXT_NAMESPACE