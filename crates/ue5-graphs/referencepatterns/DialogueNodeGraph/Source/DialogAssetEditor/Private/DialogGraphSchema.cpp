/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "DialogGraphSchema.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "DialogAssetEditorSettings.h"
#include "DialogGraphNode.h"
#include "DialogStartGraphNode.h"
#include "DialogNodeInfo.h"
#include "ElevenLabsTTSCacheSubsystem.h"
#include "HttpModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Toolkits/ToolkitManager.h"
#include "Sound/SoundWave.h"
#include "UObject/SavePackage.h"

/*******************************************************************
 * Dialogue graph editor contextual menu.
 *******************************************************************/
void UDialogGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& contextMenuBuilder) const {
    // Get asset info from graph context menu builder object.
    UDialogAsset * asset = Cast<UDialogAsset>(contextMenuBuilder.CurrentGraph->GetTypedOuter(UDialogAsset::StaticClass()));
    
    // Create context menu action for each actor declared in the asset :
    if(asset != nullptr)
    {
        FString newDialogForPlayer = "New Dialog Node for Player (" + asset->PlayerActor.ActorIdentifier.ToString() + ")";
        
        TSharedPtr<FNewNodeAction> newPlayerDialogNodeAction(
            new FNewNodeAction(
                    UDialogGraphNode::StaticClass(),
                    FText::FromString(TEXT("Nodes")),
                    FText::FromString(newDialogForPlayer),
                    FText::FromString(TEXT("Makes a new dialog node for the specified actor")),
                    0,
                    0
            )
        );

        contextMenuBuilder.AddAction(newPlayerDialogNodeAction);
        
        int i = 1; 
        for(FActorInfo info : asset->DialogActors)
        {
            FString newDialogForActor = "New Dialog Node for " + info.ActorIdentifier.ToString();
        
            TSharedPtr<FNewNodeAction> newActorDialogNodeAction(
                new FNewNodeAction(
                        UDialogGraphNode::StaticClass(),
                        FText::FromString(TEXT("Nodes")),
                        FText::FromString(newDialogForActor),
                        FText::FromString(TEXT("Makes a new dialog node for the specified actor")),
                        0,
                        i++
                )
            );

            contextMenuBuilder.AddAction(newActorDialogNodeAction);
        }
    }

    TSharedPtr<FNewNodeAction> newDialogBranchingNodeAction(
        new FNewNodeAction(
            UDialogGraphNode::StaticClass(),
            FText::FromString(TEXT("Nodes")),
            FText::FromString(TEXT("New Dialog Branching Node")),
            FText::FromString(TEXT("Makes a new dialog branching node")),
            0,
            BRANCHING_DIALOG_NODE
        )
    );
    
    contextMenuBuilder.AddAction(newDialogBranchingNodeAction);

    if (const UDialogAssetEditorSettings* DialogueSettings = GetDefault<UDialogAssetEditorSettings>())
    {
        if(!DialogueSettings->ElevenLabsAPIKey.IsEmpty())
        {
            TSharedPtr<FGenerateVoiceSoundAction> GenerateVoiceSoundAction(
                new FGenerateVoiceSoundAction(
                FText::FromString(TEXT("Voices")),
                FText::FromString(TEXT("Generate voices for selected nodes")),
                FText::FromString(TEXT("Generate voices with ElevenLabs API for selected nodes")),
                0
                    )
                );

            contextMenuBuilder.AddAction(GenerateVoiceSoundAction);
        }
    }

    TSharedPtr<FAssociateFacialAnimMontageBasedOnVoiceSoundFileName> AssociateFacialAnimMontageBasedOnVoiceSoundFileName(
        new FAssociateFacialAnimMontageBasedOnVoiceSoundFileName(
            FText::FromString(TEXT("Animations")),
            FText::FromString(TEXT("Associate facial AnimMontages based on voice sound file name for selected nodes")),
            FText::FromString(TEXT("Associate facial AnimMontages based on voice sound file name for selected nodes")),
             0 
                )
            );
    
    contextMenuBuilder.AddAction(AssociateFacialAnimMontageBasedOnVoiceSoundFileName);
}

/*******************************************************************
 * Dialogue graph editor connections rules.
 *******************************************************************/
const FPinConnectionResponse UDialogGraphSchema::CanCreateConnection(const UEdGraphPin* a, const UEdGraphPin* b) const {
    if (a == nullptr || b == nullptr) {
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Need 2 pins"));
    }

    if (a->Direction == b->Direction) {
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Inputs can only connect to outputs"));
    }

    //return FPinConnectionResponse(CONNECT_RESPONSE_MAKE/*CONNECT_RESPONSE_BREAK_OTHERS_AB*/, TEXT(""));
    return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, TEXT(""));
}

/*******************************************************************
 * Dialogue graph editor default nodes creation.
 *******************************************************************/
void UDialogGraphSchema::CreateDefaultNodesForGraph(UEdGraph& graph) const {
    UDialogStartGraphNode* startNode = NewObject<UDialogStartGraphNode>(&graph);
    startNode->CreateNewGuid();
    startNode->NodePosX = 0;
    startNode->NodePosY = 0;

    startNode->CreateDialogPin(EEdGraphPinDirection::EGPD_Output, FName(TEXT("Start")));
    
    graph.AddNode(startNode, true, true);
    graph.Modify();
}

/*******************************************************************
 * Create dialogue node.
 *******************************************************************/

UEdGraphNode* FNewNodeAction::PerformAction(UEdGraph* parentGraph, UEdGraphPin* fromPin, const FVector2D location, bool bSelectNewNode) {
    UDialogGraphNodeBase* result = NewObject<UDialogGraphNodeBase>(parentGraph, _classTemplate);
    result->CreateNewGuid();
    result->NodePosX = location.X;
    result->NodePosY = location.Y;
    result->InitNodeInfo(result);

    ELineDurationType durationType = ELineDurationType::DEFAULT;
    if (const UDialogAssetEditorSettings* DialogueSettings = GetDefault<UDialogAssetEditorSettings>())
    {
        durationType = DialogueSettings->DefaultDurationType;
    }
    
    // If it is a dialog node, initialize the actor idx :
    UDialogGraphNode * node = Cast<UDialogGraphNode>(result);
    if(node != nullptr)
    {
        UDialogNodeInfo * info = Cast<UDialogNodeInfo>(node->GetNodeInfo());
        if(_actorIdx >= 0)
        {
            if(info != nullptr)
            {
                info->ActorIdx = _actorIdx;
            }
        }
        else if(_actorIdx == BRANCHING_DIALOG_NODE)
        {
            info->isBranchingNode = true;
        }

        info->Replique.DurationType = durationType;
    }

    // Also initialize the UserID :
    node->UpdateIDToBeUnique();
    
    UEdGraphPin* inputPin = result->CreateDefaultInputPin();
    result->CreateDefaultOutputPins();

    if (fromPin != nullptr) {
        result->GetSchema()->TryCreateConnection(fromPin, inputPin);
    }

    parentGraph->Modify();
    parentGraph->AddNode(result, true, true);
    
    return result;
}

/*******************************************************************
 * Voice generation with ElevenLabs.
 *******************************************************************/

UEdGraphNode* FGenerateVoiceSoundAction::PerformAction(UEdGraph* parentGraph, UEdGraphPin* fromPin,
    const FVector2D location, bool bSelectNewNode)
{
    TArray<GenerateVoiceSoundJob*> jobs;
    
    TSharedPtr<IToolkit> Toolkit = FToolkitManager::Get().FindEditorForAsset(parentGraph->GetOuter());
    if (Toolkit.IsValid())
    {
        TSharedPtr<DialogAssetEditorApp> DialogEditorApp = StaticCastSharedPtr<DialogAssetEditorApp>(Toolkit);
        if(DialogEditorApp.IsValid())
        {
            SGraphEditor* GraphEditor = DialogEditorApp->GetWorkingGraphUi();
            UDialogAsset* DialogAsset = DialogEditorApp->GetWorkingAsset();
            
            if (GraphEditor != nullptr && DialogAsset != nullptr)
            {
                FString DialogName = DialogAsset->GetName();
                FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
                if (SelectedNodes.Num() > 0)
                {                    
                    for (UObject* NodeObject : SelectedNodes)
                    {
                        if (UEdGraphNode* Node = Cast<UEdGraphNode>(NodeObject))
                        {
                            if(Node->IsA<UDialogGraphNode>())
                            {
                                UDialogGraphNode * DialogNode = Cast<UDialogGraphNode>(Node);
                                UDialogNodeInfo * currentNodeInfo = Cast<UDialogNodeInfo>(DialogNode->GetNodeInfo());

                                if(currentNodeInfo->isBranchingNode) continue;

                                FActorInfo * actorInfo = DialogNode->getActorInfo();
                                if(actorInfo != nullptr)
                                {
                                    GenerateVoiceSoundJob * job = new GenerateVoiceSoundJob();
                                    job->actorInfo = actorInfo;
                                    job->info = currentNodeInfo;
                                    jobs.Add(job);
                                }
                            }
                        }
                    }

                    ProcessJobs(jobs, GraphEditor, DialogName);
                    DialogAsset->MarkPackageDirty();
                }
            }
        }
    }
    
    return nullptr;
}

FString FGenerateVoiceSoundAction::SanitazeFileName(const FString& String)
{
    FString result = "";
    for(wchar_t c : String)
    {
        if(IsAlphanumeric(c) || c == '_')
        {
            result += c;
        }
    }
    return result;
}

bool FGenerateVoiceSoundAction::IsAlphanumeric(TCHAR Char)
{
    return (Char >= 'A' && Char <= 'Z') ||
           (Char >= 'a' && Char <= 'z') ||
           (Char >= '0' && Char <= '9');
}

void FGenerateVoiceSoundAction::GenerateSpeech_ElevenLabs(FActorInfo* actorInfo, UDialogNodeInfo* info, TSharedPtr<FScopedSlowTask> SlowTask, TArray<uint8> *output, FEvent * WaitEvent)
{
    if(actorInfo == nullptr || info == nullptr)
    {
        WaitEvent->Trigger();
        return;
    }

    AsyncTask(ENamedThreads::GameThread, [SlowTask, info]()
    {
        if(SlowTask.IsValid())
        {
            SlowTask->EnterProgressFrame(1.f, FText::FromString("Processing dialogue node: " + info->Replique.SpokenText.ToString()));
        }
    });
    
    if (const UDialogAssetEditorSettings* DialogueSettings = GetDefault<UDialogAssetEditorSettings>())
    {
        if(DialogueSettings->ElevenLabsAPIKey.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("Eleven Labs API Key is not set!"));
            WaitEvent->Trigger();
            return;
        }

        UElevenLabsTTSCacheSubsystem * Cache = GEditor->GetEditorSubsystem<UElevenLabsTTSCacheSubsystem>();
        if(Cache)
        {
            FString APIKey = DialogueSettings->ElevenLabsAPIKey;
            FString VoiceID = Cache->GetVoiceID(actorInfo->ElevenLabsParameters.SelectedVoice);

            if(VoiceID.IsEmpty())
            {
                UE_LOG(LogTemp, Error, TEXT("Eleven Labs voice not found!"));
                WaitEvent->Trigger();
                return;
            }

            int SampleRate = GetSampleRate(DialogueSettings->ElevenLabsOutputFormat);
            
            FString URL = "https://api.elevenlabs.io/v1/text-to-speech/" + VoiceID + "?output_format=pcm_" + FString::FromInt(SampleRate);

            // Get voice parameters : if OverrideVoiceParameters is true, we get the parameter from the overriden value in the
            // dialogue node. If OverrideVoiceParameters is false, we get the parameter from the actor configuration.
            double Stability = info->OverrideVoiceParameters ? info->VoiceParametersOverride.Stability : actorInfo->ElevenLabsParameters.Stability;
            double Similarity = info->OverrideVoiceParameters ? info->VoiceParametersOverride.Similarity : actorInfo->ElevenLabsParameters.Similarity;
            double Speed = info->OverrideVoiceParameters ? info->VoiceParametersOverride.Speed : actorInfo->ElevenLabsParameters.Speed;
            double StyleExaggeration = info->OverrideVoiceParameters ? info->VoiceParametersOverride.StyleExaggeration : actorInfo->ElevenLabsParameters.StyleExaggeration;
            bool SpeakerBoost = info->OverrideVoiceParameters ? info->VoiceParametersOverride.SpeakerBoost : actorInfo->ElevenLabsParameters.SpeakerBoost;
            
            FString Payload = FString::Printf(TEXT("{\"text\": \"%s\", \"model_id\": \"%s\", \"voice_settings\": {\"stability\": %f, \"similarity_boost\": %f, \"speed\": %f, \"style\": %f, \"use_speaker_boost\": %s}, \"output_format\": \"pcm_%d\"}"),
                *info->Replique.SpokenText.ToString(),
                *actorInfo->ElevenLabsParameters.Model,
                Stability / 100.0,
                Similarity / 100.0,
                Speed,
                StyleExaggeration / 100.0,
                SpeakerBoost ? TEXT("true") : TEXT("false"),
                SampleRate);

            TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
            Request->SetURL(URL);
            Request->SetVerb("POST");
            Request->SetHeader("Content-Type", "application/json");
            Request->SetHeader("xi-api-key", APIKey);
            Request->SetContentAsString(Payload);        
            
            Request->OnProcessRequestComplete().BindLambda([output, WaitEvent, SampleRate](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
            {
                if (bWasSuccessful && Response.IsValid())
                {
                    if(Response->GetResponseCode() == 200)
                    {
                        // Get the data from the response.
                        TArray<uint8> AudioData = Response->GetContent();
                        TArray<uint8> WavData;
                        SerializeWaveFile(WavData, AudioData.GetData(), AudioData.Num(), 1, SampleRate);
                        output->Append(WavData);
                    }
                }
                WaitEvent->Trigger();
            });
            
            Request->ProcessRequest();
        }
    }
}

void FGenerateVoiceSoundAction::SaveAsAsset(USoundWave* SoundWave, FString fileName, FString DialogName)
{
    // Create a package for the SoundWave asset
    FString BasePackageName = "/Game/Voices/" + DialogName + "/" + fileName;

    // Code from https://dev.epicgames.com/community/learning/knowledge-base/wzdm/unreal-engine-how-to-create-new-assets-in-c
    // Load necessary modules
    FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
    FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
 
    // Generate a unique asset name
    FString Name, PackageName;
    AssetToolsModule.Get().CreateUniqueAssetName(*BasePackageName, TEXT(""), PackageName, Name);
    const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
 
    // Create object and package
    UPackage* package = CreatePackage(*PackageName);

    // Add the RF_Standalone flag
    SoundWave->SetFlags(RF_Public | RF_Standalone);
                    
    // Set the SoundWave as an asset within the package
    SoundWave->Rename(*fileName, package);
    
    FSavePackageArgs saveArgs;
    saveArgs.TopLevelFlags = EObjectFlags::RF_Public | EObjectFlags::RF_Standalone;
    
    UPackage::Save(package, SoundWave, *FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension()), saveArgs);
 
    // Inform asset registry
    AssetRegistry.AssetCreated(SoundWave);
 
    // Tell content browser to show the newly created asset
    TArray<UObject*> Objects;
    Objects.Add(SoundWave);
    ContentBrowserModule.Get().SyncBrowserToAssets(Objects);
}

void FGenerateVoiceSoundAction::ProcessJobs(TArray<GenerateVoiceSoundJob*> jobs, SGraphEditor * GraphEditor, FString DialogName)
{
    TSharedPtr<FScopedSlowTask> SlowTaskPtr = MakeShared<FScopedSlowTask>(jobs.Num(), FText::FromString("Generating voices sounds..."));
    SlowTaskPtr->MakeDialog(true);
    TArray<FString> * ErrorsArray = new TArray<FString>();
    
    Async(EAsyncExecution::Thread, [ErrorsArray, SlowTaskPtr, jobs, GraphEditor, DialogName]
    {
        for(GenerateVoiceSoundJob * job : jobs)
        {
            FEvent* WaitEvent = FPlatformProcess::GetSynchEventFromPool(true);
            TArray<uint8> WavData;
            GenerateSpeech_ElevenLabs(job->actorInfo, job->info, SlowTaskPtr, &WavData, WaitEvent);

            while(!WaitEvent->Wait(FTimespan::FromSeconds(0.1f)))
            {
                FPlatformProcess::Sleep(0.01f);
            }

            FPlatformProcess::ReturnSynchEventToPool(WaitEvent);

            if(WavData.Num() > 0)
            {
                AsyncTask(ENamedThreads::GameThread, [WavData, job, DialogName]
                {
                    FWaveModInfo AudioInfo;
                    FString ErrorReason;
                    if(AudioInfo.ReadWaveInfo(WavData.GetData(), WavData.Num(), &ErrorReason))
                    {
                        // Create a new USoundWave object
                        USoundWave* SoundWave = NewObject<USoundWave>();
                            
                        FSharedBuffer SharedBuffer = FSharedBuffer::Clone(WavData.GetData(), WavData.Num());
                        SoundWave->RawData.UpdatePayload(SharedBuffer);
                            
                        int32 DataPerSecond = *AudioInfo.pChannels * (*AudioInfo.pBitsPerSample / 8.0f) * *AudioInfo.pSamplesPerSec;
                        if(DataPerSecond > 0)
                        {
                            SoundWave->Duration = static_cast<float>(AudioInfo.SampleDataSize) / static_cast<float>(DataPerSecond);
                        }

                        SoundWave->SetImportedSampleRate(*AudioInfo.pSamplesPerSec);
                        SoundWave->SetSampleRate(*AudioInfo.pSamplesPerSec);
                        SoundWave->NumChannels = *AudioInfo.pChannels;
                        SoundWave->TotalSamples = *AudioInfo.pSamplesPerSec * SoundWave->Duration;
                        SoundWave->SetTimecodeInfo(FSoundWaveTimecodeInfo{});
                        SoundWave->InvalidateCompressedData(true, true);
                        SoundWave->LoadZerothChunk();
                            
                        SoundWave->PostImport();
                        SoundWave->SetRedrawThumbnail(true);

                        FGuid guid = FGuid::NewGuid();
                        FString fileName = job->actorInfo->ActorIdentifier.ToString() + "_" + job->info->Replique.SpokenText.ToString().Left(42) + "_" + guid.ToString().Left(8);
                        fileName.ReplaceInline(TEXT(" "), TEXT("_"));
                        fileName = SanitazeFileName(fileName);
                        SaveAsAsset(SoundWave, fileName, DialogName);

                        job->info->Replique.VoiceSound = SoundWave;
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("%s"), *ErrorReason);
                    }
                });
            }
            else
            {
                ErrorsArray->Add(job->info->Replique.SpokenText.ToString());
                UE_LOG(LogTemp, Error, TEXT("An error occured while calling ElevenLabs API!"));
            }
        }

        // Update the graph :
        AsyncTask(ENamedThreads::GameThread, [ErrorsArray, jobs, GraphEditor, SlowTaskPtr]
        {
            TSharedPtr<FScopedSlowTask> ScopedSlowTaskInGameThread = SlowTaskPtr;
            GraphEditor->GetCurrentGraph()->NotifyGraphChanged();
            ScopedSlowTaskInGameThread.Reset();

            if(ErrorsArray->Num() > 0)
            {
                FString ErrorMessage = "An error occured while generating:\n";

                for(FString Error : *ErrorsArray)
                {
                    ErrorMessage += " - " + Error + "\n";
                }

                FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(ErrorMessage));
            }
            
            // Free memory:
            for(GenerateVoiceSoundJob * job : jobs)
                delete job;
            
            delete ErrorsArray;
        });
    });
}

/*******************************************************************
 * Associate facial AnimMontage based on Voice SoundWave filename.
 *******************************************************************/
UEdGraphNode* FAssociateFacialAnimMontageBasedOnVoiceSoundFileName::PerformAction(UEdGraph* parentGraph,
    UEdGraphPin* fromPin, const FVector2D location, bool bSelectNewNode)
{
    TSharedPtr<IToolkit> Toolkit = FToolkitManager::Get().FindEditorForAsset(parentGraph->GetOuter());
    if (Toolkit.IsValid())
    {
        TSharedPtr<DialogAssetEditorApp> DialogEditorApp = StaticCastSharedPtr<DialogAssetEditorApp>(Toolkit);
        if(DialogEditorApp.IsValid())
        {
            SGraphEditor* GraphEditor = DialogEditorApp->GetWorkingGraphUi();
            UDialogAsset* DialogAsset = DialogEditorApp->GetWorkingAsset();
            if (GraphEditor != nullptr && DialogAsset != nullptr)
            {
                FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
                if (SelectedNodes.Num() > 0)
                {
                    TArray<FAssetData> AnimMontages;
                    GetAllAnimMontages(AnimMontages);

                    for (UObject* NodeObject : SelectedNodes)
                    {
                        if (UEdGraphNode* Node = Cast<UEdGraphNode>(NodeObject))
                        {
                            if(Node->IsA<UDialogGraphNode>())
                            {
                                UDialogGraphNode * DialogNode = Cast<UDialogGraphNode>(Node);
                                UDialogNodeInfo * currentNodeInfo = Cast<UDialogNodeInfo>(DialogNode->GetNodeInfo());

                                if(currentNodeInfo->isBranchingNode) continue;

                                USoundWave * VoiceSound = currentNodeInfo->Replique.VoiceSound;

                                if(VoiceSound == nullptr) continue;

                                FString soundName = VoiceSound->GetName();

                                UAnimMontage * Montage = GetAnimMontageWithNameContaining(AnimMontages, soundName);
                                if(Montage)
                                {
                                    currentNodeInfo->Replique.FaceLipSyncAnimMontage = Montage;
                                }
                            }
                        }
                    }

                    GraphEditor->GetCurrentGraph()->NotifyGraphChanged();
                    DialogAsset->MarkPackageDirty();
                }
            }
        }
    }
    
    return nullptr;
}


void FAssociateFacialAnimMontageBasedOnVoiceSoundFileName::GetAllAnimMontages(TArray<FAssetData> &output)
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    
    AssetRegistryModule.Get().SearchAllAssets(true);

    // Filter on AnimMontage class
    FARFilter Filter;
    Filter.ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    
    AssetRegistryModule.Get().GetAssets(Filter, output);
}

UAnimMontage* FAssociateFacialAnimMontageBasedOnVoiceSoundFileName::GetAnimMontageWithNameContaining(
    TArray<FAssetData>& assets, FString name)
{
    for (const FAssetData& AssetData : assets)
    {
        if (AssetData.AssetName.ToString().Contains(name))
        {
            return Cast<UAnimMontage>(AssetData.GetAsset());
        }
    }

    return nullptr;
}
