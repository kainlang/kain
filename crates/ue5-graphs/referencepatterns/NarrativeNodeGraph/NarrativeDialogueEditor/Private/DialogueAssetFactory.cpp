// Copyright Narrative Tools 2022. 


#include "DialogueAssetFactory.h"
#include "DialogueBlueprint.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "Dialogue.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintEditorSettings.h"
#include "DialogueGraph.h"
#include "DialogueGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "DialogueEditorSettings.h"

#define LOCTEXT_NAMESPACE "DialogueAssetFactory"

UDialogueAssetFactory::UDialogueAssetFactory()
{
	SupportedClass = UDialogueBlueprint::StaticClass();
	ParentClass = UDialogue::StaticClass();

	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UDialogueAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}


UObject* UDialogueAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	check(Class->IsChildOf(UDialogueBlueprint::StaticClass()));

	UDialogueBlueprint* DialogueBP = nullptr;

	FSoftClassPath DialogueClassPath = GetDefault<UDialogueEditorSettings>()->DefaultDialogueClass;
	UClass* DialogueClass = (DialogueClassPath.IsValid() ? LoadObject<UClass>(NULL, *DialogueClassPath.ToString()) : UDialogue::StaticClass());

	if (DialogueClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to load Dialogue Class '%s'. Falling back to generic UDialogue."), *DialogueClassPath.ToString());
		DialogueClass = UDialogue::StaticClass();
	}

	DialogueBP = CastChecked<UDialogueBlueprint>(FKismetEditorUtilities::CreateBlueprint(DialogueClass, InParent, Name, BPTYPE_Normal, UDialogueBlueprint::StaticClass(), UDialogueBlueprintGeneratedClass::StaticClass(), CallingContext));

	//Add a default speaker to the dialogue blueprint as it doesn't come with one
	if (DialogueBP)
	{
		if (UDialogue* DialogueCDO = Cast<UDialogue>(DialogueBP->GeneratedClass->GetDefaultObject()))
		{
			////Generally we would never want the parent classes speakers to inherit down into child classes
			//DialogueCDO->Speakers.Empty();

			//FString NameString = DialogueBP->GetFName().ToString();

			////Add the Default Speaker to the dialogue 
			//FSpeakerInfo DefaultSpeaker;
			//int32 UnderscoreIndex = -1;

			//if (NameString.FindChar(TCHAR('_'), UnderscoreIndex))
			//{
			//	//remove D_SpeakerName prefix 
			//	DefaultSpeaker.SpeakerID = FName(NameString.RightChop(UnderscoreIndex + 1));
			//}
			//else
			//{
			//	DefaultSpeaker.SpeakerID = FName(NameString);
			//}

			//DialogueCDO->Speakers.Add(DefaultSpeaker);
		}
	}

	return DialogueBP;
}

bool UDialogueAssetFactory::CanCreateNew() const
{
	return true;
}

FString UDialogueAssetFactory::GetDefaultNewAssetName() const
{
	return FString(TEXT("NewDialogue"));
}

FText UDialogueAssetFactory::GetDisplayName() const
{
	return LOCTEXT("DialogueText", "Dialogue");
}

#undef LOCTEXT_NAMESPACE