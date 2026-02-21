// Copyright Narrative Tools 2022. 

#include "NarrativeDataTask.h"

#define LOCTEXT_NAMESPACE "NarrativeTask"

UNarrativeDataTask::UNarrativeDataTask(const FObjectInitializer& ObjectInitializer)
{
	//We automatically fill all of the fields to make peoples lives easier 
	TaskName = FName::NameToDisplayString(GetName(), false);

	int32 SpaceIndex;
	bool bNameHasSpace = TaskName.FindLastChar(' ', SpaceIndex);

	DefaultArgument = TaskName.RightChop(SpaceIndex + 1);

	if (bNameHasSpace)
	{
		ArgumentName = DefaultArgument + " Name";
	}

	TaskCategory = DefaultArgument + 's';

	TaskDescription = FText::Format(LOCTEXT("DefaultDescriptionText", "{0}. Argument is the {1}"), FText::FromString(TaskName), FText::FromString(ArgumentName));

	/*If action name is TalkToCharacter, this will make default description Talk To{Character}.,
	//Which narrative automatically uses to format the description 
	
	I've commented this out as this feature actually gets kind of annoying - generally the autofilled descriptions aren't correct and need to be changed anyway **/
	//AutofillDescription = TaskName.LeftChop(DefaultArgument.Len()) + "%argument%";
}

FString UNarrativeDataTask::MakeTaskString(const FString& Argument) const
{
	FString Str = (TaskName + '_' + Argument).ToLower();
	Str.RemoveSpacesInline();
	return Str;
}

FText UNarrativeDataTask::GetReferenceDisplayText()
{
	return FText::FromString(ArgumentName);
}

#undef LOCTEXT_NAMESPACE