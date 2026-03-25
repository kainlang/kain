/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "ElevenLabsTTSCacheSubsystem.h"

#include "DialogAssetEditorSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

void UElevenLabsTTSCacheSubsystem::LoadVoicesFromAPI()
{
#if WITH_EDITOR
	if (const UDialogAssetEditorSettings* DialogueSettings = GetDefault<UDialogAssetEditorSettings>())
	{
		if(DialogueSettings->ElevenLabsAPIKey.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("Eleven Labs API Key is not set!"));
			return;
		}
	
		FString ApiKey = DialogueSettings->ElevenLabsAPIKey;
		FString Url = "https://api.elevenlabs.io/v2/voices?page_size=100";//"https://api.elevenlabs.io/v1/voices";

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(Url);
		Request->SetVerb("GET");
		Request->SetHeader("Content-Type", "application/json");
		Request->SetHeader("xi-api-key", ApiKey);
    
		Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			if (bSuccess && Response->GetResponseCode() == 200)
			{
				FString ResponseBody = Response->GetContentAsString();
				UE_LOG(LogTemp, Log, TEXT("API Response: %s"), *ResponseBody);

				TSharedPtr<FJsonObject> JsonObject;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

				if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
				{
					const TArray<TSharedPtr<FJsonValue>>* VoicesArray;
					if (JsonObject->TryGetArrayField(TEXT("voices"), VoicesArray))
					{
						for (const TSharedPtr<FJsonValue>& Voice : *VoicesArray)
						{
							FString VoiceId = Voice->AsObject()->GetStringField(TEXT("voice_id"));
							FString Name = Voice->AsObject()->GetStringField(TEXT("name"));
							FString PreviewURL = Voice->AsObject()->GetStringField(TEXT("preview_url"));
							UE_LOG(LogTemp, Log, TEXT("Voice: %s (ID: %s)"), *Name, *VoiceId);
							if(!VoiceNameToVoiceIDMap.Contains(Name))
							{
								VoiceNameToVoiceIDMap.Add(Name, FVoiceData(Name, VoiceId, PreviewURL));
								VoiceOptions.Add(MakeShared<FString>(Name));
							}
						}

						VoiceOptions.Sort([](const TSharedPtr<FString>& A, const TSharedPtr<FString>& B)
						{
							return A->Compare(*B) < 0; 
						});
					}
				}
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("An error occured while loading ElevenLabs voices' list."));
				if(Response)
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to get voices! HTTP Status: %d"), Response->GetResponseCode());
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to get voices!"));
				}
			}
		});

		Request->ProcessRequest();
	}
#endif
}

FString UElevenLabsTTSCacheSubsystem::GetVoiceID(FString VoiceName) const
{
	if(VoiceNameToVoiceIDMap.Contains(VoiceName))
		return VoiceNameToVoiceIDMap[VoiceName].VoiceID;

	return "";
}

FString UElevenLabsTTSCacheSubsystem::GetVoicePreviewURL(FString VoiceName) const
{
	if(VoiceNameToVoiceIDMap.Contains(VoiceName))
		return VoiceNameToVoiceIDMap[VoiceName].PreviewURL;

	return "";
}

const TArray<TSharedPtr<FString>>& UElevenLabsTTSCacheSubsystem::GetVoiceOptions() const
{
	return VoiceOptions;
}

bool UElevenLabsTTSCacheSubsystem::IsPreviewCached(FString VoiceName) const
{
	return VoicePreviewCache.Contains(VoiceName);
}

void UElevenLabsTTSCacheSubsystem::CacheVoicePreview(FString VoiceName, USoundWave* Preview)
{
	if(IsPreviewCached(VoiceName)) VoicePreviewCache[VoiceName]->MarkAsGarbage();
	VoicePreviewCache.Add(VoiceName, Preview);
}

USoundWave* UElevenLabsTTSCacheSubsystem::GetVoicePreview(FString VoiceName)
{
	if(IsPreviewCached(VoiceName))
	{
		return VoicePreviewCache[VoiceName];
	}

	return nullptr;
}

void UElevenLabsTTSCacheSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadVoicesFromAPI();
}
