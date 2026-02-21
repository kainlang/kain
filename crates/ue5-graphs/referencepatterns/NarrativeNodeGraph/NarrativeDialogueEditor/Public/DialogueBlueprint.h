// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Blueprint.h"
#include "DialogueBlueprint.generated.h"

class UDialogue;

/**
 * A special type of blueprint that includes a node based dialogue editor.
 */
UCLASS(BlueprintType)
class NARRATIVEDIALOGUEEDITOR_API UDialogueBlueprint : public UBlueprint
{
	GENERATED_BODY()
	
public:

	UDialogueBlueprint();

	/** Graph for dialogue asset */
	UPROPERTY()
	class UEdGraph* DialogueGraph;

	UPROPERTY(BlueprintReadWrite, Category = "Dialogue")
	UDialogue* DialogueTemplate;

	//If set, the factory will use this legacy asset as a template for the new asset
	UPROPERTY()
	TObjectPtr<class UDialogueAsset> LegacyAsset;

	virtual UClass* GetBlueprintClass() const override;
	virtual bool SupportedByDefaultBlueprintFactory() const override
	{
		return false;
	}
	virtual bool CanAlwaysRecompileWhilePlayingInEditor() const override { return true; }

	virtual void PostLoad() override;
	virtual bool IsValidForBytecodeOnlyRecompile() const override { return false; }
	static bool ValidateGeneratedClass(const UClass* InClass);
};
