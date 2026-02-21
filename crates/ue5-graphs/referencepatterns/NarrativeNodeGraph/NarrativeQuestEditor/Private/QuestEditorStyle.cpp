// Copyright Narrative Tools 2022. 

#include "QuestEditorStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FQuestEditorStyle::StyleSet = nullptr;
TSharedPtr<ISlateStyle> FQuestEditorStyle::Get() { return StyleSet; }

//Helper functions from UE4 forums to easily create box and image brushes
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )

FString FQuestEditorStyle::RootToContentDir(const ANSICHAR* RelativePath, const TCHAR* Extension)
{
	//Find quest plugin content directory
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("Narrative"))->GetContentDir();
	return (ContentDir / RelativePath) + Extension;
}
FName FQuestEditorStyle::GetStyleSetName()
{
	static FName QuestEditorStyleName(TEXT("QuestEditorStyle"));
	return QuestEditorStyleName;
}

void FQuestEditorStyle::Initialize()
{
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	//Thumbnails and icons
	StyleSet->Set(FName(TEXT("ClassThumbnail.QuestBlueprint")), new IMAGE_BRUSH("Quest", FVector2D(64, 64)));
	StyleSet->Set(FName(TEXT("ClassIcon.QuestBlueprint")), new IMAGE_BRUSH("Quest", FVector2D(16, 16)));

	StyleSet->Set(FName(TEXT("ClassThumbnail.Quest")), new IMAGE_BRUSH("Quest", FVector2D(64, 64)));
	StyleSet->Set(FName(TEXT("ClassIcon.Quest")), new IMAGE_BRUSH("Quest", FVector2D(16, 16)));

	StyleSet->Set(FName(TEXT("ClassThumbnail.NarrativeDataTask")), new IMAGE_BRUSH("DataTask", FVector2D(64, 64)));
	StyleSet->Set(FName(TEXT("ClassIcon.NarrativeDataTask")), new IMAGE_BRUSH("DataTask", FVector2D(16, 16)));
	StyleSet->Set(FName(TEXT("ClassThumbnail.NarrativeTask")), new IMAGE_BRUSH("Task", FVector2D(64, 64)));
	StyleSet->Set(FName(TEXT("ClassIcon.NarrativeTask")), new IMAGE_BRUSH("Task", FVector2D(16, 16)));

	StyleSet->Set(FName(TEXT("ClassThumbnail.NarrativeEvent")), new IMAGE_BRUSH("NarrativeEvent", FVector2D(64, 64)));
	StyleSet->Set(FName(TEXT("ClassIcon.NarrativeEvent")), new IMAGE_BRUSH("NarrativeEvent", FVector2D(16, 16)));
	StyleSet->Set(FName(TEXT("ClassThumbnail.NarrativeCondition")), new IMAGE_BRUSH("NarrativeCondition", FVector2D(64, 64)));
	StyleSet->Set(FName(TEXT("ClassIcon.NarrativeCondition")), new IMAGE_BRUSH("NarrativeCondition", FVector2D(16, 16)));

	StyleSet->Set(FName(TEXT("ClassThumbnail.NarrativeTaskBlueprint")), new IMAGE_BRUSH("Task", FVector2D(64, 64)));
	StyleSet->Set(FName(TEXT("ClassIcon.NarrativeTaskBlueprint")), new IMAGE_BRUSH("Task", FVector2D(16, 16)));

	StyleSet->Set(FName(TEXT("ClassThumbnail.NarrativeComponent")), new IMAGE_BRUSH("NarrativeLogo", FVector2D(64, 64)));
	StyleSet->Set(FName(TEXT("ClassIcon.NarrativeComponent")), new IMAGE_BRUSH("NarrativeLogo", FVector2D(16, 16)));
	StyleSet->Set(FName(TEXT("ClassThumbnail.NarrativePartyComponent")), new IMAGE_BRUSH("NarrativePartyLogo", FVector2D(64, 64)));
	StyleSet->Set(FName(TEXT("ClassIcon.NarrativePartyComponent")), new IMAGE_BRUSH("NarrativePartyLogo", FVector2D(16, 16)));

	//Toolbar buttons
	StyleSet->Set(FName(TEXT("QuestEditor.Common.ShowQuestDetails")), new IMAGE_BRUSH("QuestEditor/Icons/ShowQuestDetails_40x", FVector2D(40.f, 40.f)));
	StyleSet->Set(FName(TEXT("QuestEditor.Common.ShowQuestDetails.Small")), new IMAGE_BRUSH("QuestEditor/Icons/ShowQuestDetails_40x", FVector2D(20.f, 20.f)));
	StyleSet->Set(FName(TEXT("QuestEditor.Common.ViewTutorial")), new IMAGE_BRUSH("QuestEditor/Icons/ViewTutorial_40x", FVector2D(40.f, 40.f)));
	StyleSet->Set(FName(TEXT("QuestEditor.Common.ViewTutorial.Small")), new IMAGE_BRUSH("QuestEditor/Icons/ViewTutorial_40x", FVector2D(20.f, 20.f)));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
};

#undef BOX_BRUSH
#undef IMAGE_BRUSH

void FQuestEditorStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}

