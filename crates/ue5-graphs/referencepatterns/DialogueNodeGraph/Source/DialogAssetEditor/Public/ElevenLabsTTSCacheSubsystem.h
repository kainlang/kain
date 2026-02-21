/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "ElevenLabsTTSCacheSubsystem.generated.h"

USTRUCT()
struct FVoiceData
{
	GENERATED_USTRUCT_BODY()
	
	FString Name;
	FString VoiceID;
	FString PreviewURL;

	FVoiceData()
	{
		Name = "";
		VoiceID = "";
		PreviewURL = "";
	}
	
	FVoiceData(FString Name, FString VoiceID, FString PreviewURL)
	{
		this->Name = Name;
		this->VoiceID = VoiceID;
		this->PreviewURL = PreviewURL;
	}
};


UCLASS()
class UElevenLabsTTSCacheSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

	
	TArray<TSharedPtr<FString>> VoiceOptions;
	
	UPROPERTY()
	TMap<FString, FVoiceData> VoiceNameToVoiceIDMap;

	UPROPERTY()
	TMap<FString, USoundWave*> VoicePreviewCache;

	void LoadVoicesFromAPI();
public:
	FString GetVoiceID(FString VoiceName) const;
	FString GetVoicePreviewURL(FString VoiceName) const;
	const TArray<TSharedPtr<FString>> & GetVoiceOptions() const;
	bool IsPreviewCached(FString VoiceName) const;
	void CacheVoicePreview(FString VoiceName, USoundWave * Preview);
	USoundWave* GetVoicePreview(FString VoiceName);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
};
#endif