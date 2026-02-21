/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */


#include "QuestNodeInfo.h"
#include "DialogueMasterTask.h"

#define LOCTEXT_NAMESPACE "DialogueMasterQuestNodes"

// Base implementation (it is overridden for both step and task nodes so it is unused).
FText UQuestNodeInfo::GetGraphNodeDescription() const
{
	FText FormatInput = FText::FromString("<RichText.Bold>ID = {NodeName}</>\n{NodeDescription}");
	FFormatNamedArguments Args;

	// Add Title :
	Args.Add("NodeName", FText::FromName(ID));

	// Construct description :
	Args.Add("NodeDescription", Description);
	
	return FText::Format(FormatInput, Args);
}

FString UQuestStepNodeInfo::GetActionsInfoString() const
{
	FString result = "";

	if(Actions.Num() > 0)
	{
		result += "\n\nActions :\n";

		for(UDialogueMasterAction *action : Actions)
		{
			if(IsValid(action))
			{
				FString moment = " (beginning and end of step)";
				if(action->TriggerMoment == Beginning)
					moment = " (beginning of step)";
				else if(action->TriggerMoment == End)
					moment = " (end of step)";
				
				result += " - " + action->GetDescription() + moment + "\n";
			}
			else
			{
				result += " - Invalid action!\n";
			}
		}
	}
	
	return result;
}

FText UQuestStepNodeInfo::GetShortDescription() const
{
	if(StepName.IsEmpty())
	{
		if(Description.IsEmpty())
		{
			if(StepType == NORMAL)
				return LOCTEXT("Objectives", "Objectives");
			else if(StepType == QUEST_SUCCEEDED)
				return LOCTEXT("QuestSucceeded", "Quest succeeded!");
			else if(StepType == QUEST_FAILED)
				return LOCTEXT("QuestFailed", "Quest failed...");
		}
		else
		{
			return Description;
		}
	}

	return StepName;
}

FText UQuestStepNodeInfo::GetGraphNodeDescription() const
{
	FText FormatInput = FText::FromString("<RichText.Bold>ID = {NodeID}</>\n{NodeShortName}{NodeDescription}{AdditionalStuff}");
	FFormatNamedArguments Args;

	// Add Title :
	Args.Add("NodeID", FText::FromName(ID));

	FText nodeShortName = FText::Format(FTextFormat::FromString("Short description : {0}\n"), GetShortDescription());
	Args.Add("NodeShortName", nodeShortName);
	
	// Construct description :
	FText detailedDescription = FText::Format(FTextFormat::FromString("Detailed description : {0}"), Description);
	Args.Add("NodeDescription", detailedDescription);

	FText actionList = FText::FromString(GetActionsInfoString());
	Args.Add("AdditionalStuff", actionList);
	
	return FText::Format(FormatInput, Args);
}


FText UQuestTaskListNodeInfo::GetGraphNodeDescription() const
{
	FText FormatInput = FText::FromString("<RichText.Bold>ID = {NodeName}</>\n{NodeDescription}");
	FFormatNamedArguments Args;
	
	// Add Title :
	Args.Add("NodeName", FText::FromString(ID.ToString()));

	// Construct description :
	FString result;

	if(Tasks.Num() > 0)
	{
		if(!Description.IsEmpty())
		{
			result = "Dev note : " + Description.ToString() + "\n";
		}
		result += "\nTasks :";

		if(Hidden)
			result += " (hidden task list)";
		
		result += "\n";
		
		for(UDialogueMasterTask * Task : Tasks)
		{
			if(IsValid(Task))
			{
				result += " - " + Task->GetInGameTaskDescription().ToString();
				if(Task->NumberOfCompletionRequired > 1)
				{
					result += " " + Task->GetProgressText().ToString();
				}
				if(Task->bHiddenTask)
				{
					result += " (hidden)";
				}
				result += "\n";
			}
			else
			{
				result += " - Please select a task type\n";
			}
		}
		Args.Add("NodeDescription", FText::FromString(result));
	}
	else
	{
		Args.Add("NodeDescription", FText::FromString("Empty task list !"));
	}
	
	return FText::Format(FormatInput, Args);
}

#undef LOCTEXT_NAMESPACE