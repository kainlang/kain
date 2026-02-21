// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "QuestEditorSettings.generated.h"

/**
 * 
 */
UCLASS(config=Engine, defaultconfig)
class NARRATIVEQUESTEDITOR_API UQuestEditorSettings : public UObject
{
	GENERATED_BODY()

public:

	UQuestEditorSettings();
	
	//All the folders the quest editor searches in to find your quest tasks. 
	UPROPERTY(EditAnywhere, config, Category = "Quests", meta = (RelativeToGameContentDir))
	TArray<FString> QuestTaskSearchPaths;

	UPROPERTY(EditAnywhere, config, Category = "Graph Style")
	FLinearColor FailedNodeColor;

	UPROPERTY(EditAnywhere, config, Category = "Graph Style")
	FLinearColor SuccessNodeColor;

	UPROPERTY(EditAnywhere, config, Category = "Graph Style")
	FLinearColor StateNodeColor;

	UPROPERTY(EditAnywhere, config, Category = "Graph Style")
	FLinearColor RootNodeColor;

	UPROPERTY(EditAnywhere, config, Category = "Graph Style")
	FLinearColor TaskNodeColor;

	UPROPERTY(EditAnywhere, config, Category = "Graph Style")
	FLinearColor PersistentTasksNodeColor;

	UPROPERTY(EditAnywhere, config, Category = "Quests", noclear, meta = (MetaClass = "/Script/Narrative.Quest"))
	FSoftClassPath DefaultQuestClass;

	UPROPERTY(EditAnywhere, config, Category = "Quests", noclear, meta = (MetaClass = "/Script/Narrative.QuestBranch"))
	FSoftClassPath DefaultQuestBranch;

	UPROPERTY(EditAnywhere, config, Category = "Quests", noclear, meta = (MetaClass = "/Script/Narrative.QuestState"))
	FSoftClassPath DefaultQuestState;

	UPROPERTY(EditAnywhere, config, Category = "Graph Defaults", noclear, meta = (MetaClass = "/Script/NarrativeQuestEditor.QuestNodeUserWidget"))
	TSoftClassPtr<class UQuestNodeUserWidget> DefaultQuestWidgetClass;
};
