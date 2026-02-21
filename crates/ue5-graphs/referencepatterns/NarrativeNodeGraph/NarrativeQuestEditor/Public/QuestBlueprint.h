// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Blueprint.h"
#include "QuestBlueprint.generated.h"

class UQuestGraphNode_Root;
class UQuest;
class UQuestNode;

/**
 * A special type of blueprint that includes a node based quest editor.
 */
UCLASS(BlueprintType)
class NARRATIVEQUESTEDITOR_API UQuestBlueprint : public UBlueprint
{
	GENERATED_UCLASS_BODY()
	
public:

	/** Graph for quest asset */
	UPROPERTY()
	class UEdGraph* QuestGraph;

	UPROPERTY(BlueprintReadWrite, Category = "Quest")
	class UQuest* QuestTemplate;

	virtual UClass* GetBlueprintClass() const override;
	virtual bool SupportedByDefaultBlueprintFactory() const override
	{
		return false;
	}
	virtual bool CanAlwaysRecompileWhilePlayingInEditor() const override { return true; }

	virtual void PostLoad() override;
	virtual bool IsValidForBytecodeOnlyRecompile() const override { return false; }
	static bool ValidateGeneratedClass(const UClass* InClass);
	//Return true if there is already a state with the same name in the graph. Return a new name that is not identical by ref.
	bool DoesStateHaveDuplicate(const class UQuestState* State, FName& OutBetterName);

};
