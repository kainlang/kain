/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "DialogGraphSchema.generated.h"

#define UNDEFINED_ACTOR -1
#define BRANCHING_DIALOG_NODE -2

struct GenerateVoiceSoundJob;
struct FActorInfo;
class UDialogNodeInfo;

UCLASS()
class UDialogGraphSchema : public UEdGraphSchema {
    GENERATED_BODY()

public:
    virtual void GetGraphContextActions(FGraphContextMenuBuilder& contextMenuBuilder) const override;
    virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* a, const UEdGraphPin* b) const override;
	virtual void CreateDefaultNodesForGraph(UEdGraph& graph) const override;
};


/*******************************************************************
 * Create dialogue node.
 *******************************************************************/
USTRUCT()
struct FNewNodeAction : public FEdGraphSchemaAction {
    GENERATED_BODY()

    int _actorIdx;
public:
    FNewNodeAction() {}
    FNewNodeAction(UClass* classTemplate, FText inNodeCategory, FText inMenuDesc, FText inToolTip, const int32 inGrouping, int actorIdx = UNDEFINED_ACTOR)
        : FEdGraphSchemaAction(inNodeCategory, inMenuDesc, inToolTip, inGrouping), _classTemplate(classTemplate)
    {
        this->_actorIdx = actorIdx;
    }

    virtual UEdGraphNode* PerformAction(UEdGraph* parentGraph, UEdGraphPin* fromPin, const FVector2D location, bool bSelectNewNode = true);

protected:
    UClass* _classTemplate = nullptr;
};

/*******************************************************************
 * Voice generation with ElevenLabs.
 *******************************************************************/
USTRUCT()
struct FGenerateVoiceSoundAction : public FEdGraphSchemaAction {
    GENERATED_BODY()

public:
    FGenerateVoiceSoundAction() {}
    FGenerateVoiceSoundAction(FText inNodeCategory, FText inMenuDesc, FText inToolTip, const int32 inGrouping)
        : FEdGraphSchemaAction(inNodeCategory, inMenuDesc, inToolTip, inGrouping)
    {
        
    }

    virtual UEdGraphNode* PerformAction(UEdGraph* parentGraph, UEdGraphPin* fromPin, const FVector2D location, bool bSelectNewNode = true);
    static FString SanitazeFileName(const FString& String);

    static bool IsAlphanumeric(TCHAR Char);
    static void GenerateSpeech_ElevenLabs(FActorInfo * actorInfo, UDialogNodeInfo * info, TSharedPtr<FScopedSlowTask> SlowTask, TArray<uint8> *output, FEvent* WaitEvent);
    static void SaveAsAsset(USoundWave * SoundWave, FString fileName, FString DialogName);
    static void ProcessJobs(TArray<GenerateVoiceSoundJob*> jobs, SGraphEditor * GraphEditor, FString DialogName);
};

struct GenerateVoiceSoundJob
{
    FActorInfo * actorInfo;
    UDialogNodeInfo * info;
};

/*******************************************************************
 * Associate facial AnimMontage based on Voice SoundWave filename.
 *******************************************************************/
USTRUCT()
struct FAssociateFacialAnimMontageBasedOnVoiceSoundFileName : public FEdGraphSchemaAction
{
    GENERATED_BODY()

public:
    FAssociateFacialAnimMontageBasedOnVoiceSoundFileName() {}
    FAssociateFacialAnimMontageBasedOnVoiceSoundFileName(FText inNodeCategory, FText inMenuDesc, FText inToolTip, const int32 inGrouping)
        : FEdGraphSchemaAction(inNodeCategory, inMenuDesc, inToolTip, inGrouping)
    {
        
    }

    virtual UEdGraphNode* PerformAction(UEdGraph* parentGraph, UEdGraphPin* fromPin, const FVector2D location, bool bSelectNewNode = true);
    static void GetAllAnimMontages(TArray<FAssetData> &output);
    static UAnimMontage* GetAnimMontageWithNameContaining(TArray<FAssetData> &assets, FString name);
};
