// Copyright Narrative Tools 2022. 


#include "DialogueAsset.h"
#include "Dialogue.h"

UDialogueAsset::UDialogueAsset()
{
	Dialogue = CreateDefaultSubobject<UDialogue>(TEXT("Dialogue"));

	//Any dialogues created prior to the speakers update need a speaker added 
	//if (Dialogue->Speakers.Num() == 0)
	//{
	//	FSpeakerInfo DefaultSpeaker;
	//	DefaultSpeaker.SpeakerID = GetFName();
	//	Dialogue->Speakers.Add(DefaultSpeaker);
	//}
}
