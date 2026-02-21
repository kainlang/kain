/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "VoiceSelectionSettingsDetails.h"

#include "ActorInfo.h"
#include "DetailWidgetRow.h"
#include "DialogAssetEditorSettings.h"
#include "ElevenLabsTTSCacheSubsystem.h"
#include "HttpModule.h"
#include "IDetailChildrenBuilder.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

#define MINIMP3_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 4456)
#pragma warning(disable: 4706)
#include "minimp3.h"
#include "minimp3_ex.h"
#pragma warning(pop)

TSharedRef<IPropertyTypeCustomization> FVoiceSelectionSettingsDetails::MakeInstance()
{
	return MakeShareable(new FVoiceSelectionSettingsDetails);
}

FVoiceSelectionSettingsDetails::FVoiceSelectionSettingsDetails()
{
	
}

void FVoiceSelectionSettingsDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
                                                     FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow.NameContent()[StructPropertyHandle->CreatePropertyNameWidget()]
		.ValueContent()[StructPropertyHandle->CreatePropertyValueWidget()];
}

void FVoiceSelectionSettingsDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> VoiceIdHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FElevenLabsParameters, SelectedVoice));
	TSharedPtr<IPropertyHandle> SpeedHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FElevenLabsParameters, Speed));
	TSharedPtr<IPropertyHandle> StabilityHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FElevenLabsParameters, Stability));
	TSharedPtr<IPropertyHandle> SimilarityHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FElevenLabsParameters, Similarity));
	TSharedPtr<IPropertyHandle> StyleExaggerationHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FElevenLabsParameters, StyleExaggeration));
	TSharedPtr<IPropertyHandle> SpeakerBoostHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FElevenLabsParameters, SpeakerBoost));

	UElevenLabsTTSCacheSubsystem * Cache = GEditor->GetEditorSubsystem<UElevenLabsTTSCacheSubsystem>();
	
	StructBuilder.AddCustomRow(FText::FromString("VoiceID"))
	.NameContent()[VoiceIdHandle->CreatePropertyNameWidget()]
	.ValueContent()
	[
		SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&Cache->GetVoiceOptions())
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem) {
			return SNew(STextBlock).Text(FText::FromString(*InItem));
		})
		.OnSelectionChanged_Lambda([VoiceIdHandle](TSharedPtr<FString> NewSelection, ESelectInfo::Type) {
			VoiceIdHandle->SetValue(*NewSelection);
		})
		.Content()
		[
			SNew(STextBlock).Text_Lambda([VoiceIdHandle]() {
				FString CurrentValue;
				VoiceIdHandle->GetValue(CurrentValue);
				return FText::FromString(CurrentValue);
			})
		]
	];

	
	StructBuilder.AddProperty(SpeedHandle.ToSharedRef());
	StructBuilder.AddProperty(StabilityHandle.ToSharedRef());
	StructBuilder.AddProperty(SimilarityHandle.ToSharedRef());
	StructBuilder.AddProperty(StyleExaggerationHandle.ToSharedRef());
	StructBuilder.AddProperty(SpeakerBoostHandle.ToSharedRef());

	StructBuilder.AddCustomRow(FText::FromString("Reset Button"))
	.ValueContent()
	.MaxDesiredWidth(250)
	[
		SNew(SButton)
		.Text(FText::FromString("Reset values"))
		.OnClicked_Lambda([StructPropertyHandle]()
		{
			FElevenLabsParameters* Settings = nullptr;
			void* RawData = nullptr;

			if (StructPropertyHandle->GetValueData(RawData) == FPropertyAccess::Success)
			{
				Settings = reinterpret_cast<FElevenLabsParameters*>(RawData);
				if (Settings)
				{
					Settings->ResetToDefaultValues();
				}
			}

			return FReply::Handled();
		})
	];

	StructBuilder.AddCustomRow(FText::FromString("Preview Button"))
	.ValueContent()
	.MaxDesiredWidth(250)
	[
		SNew(SButton)
		.Text(FText::FromString("Preview selected voice"))
		.OnClicked_Lambda([StructPropertyHandle]()
		{
			FElevenLabsParameters* Settings = nullptr;
			void* RawData = nullptr;

			if (StructPropertyHandle->GetValueData(RawData) == FPropertyAccess::Success)
			{
				Settings = reinterpret_cast<FElevenLabsParameters*>(RawData);
				if (Settings)
				{
					UElevenLabsTTSCacheSubsystem * Cache = GEditor->GetEditorSubsystem<UElevenLabsTTSCacheSubsystem>();

					
					if(Cache->IsPreviewCached(Settings->SelectedVoice))
					{
						USoundWave* SoundWave = Cache->GetVoicePreview(Settings->SelectedVoice);
						if(SoundWave)
							GEditor->PlayPreviewSound(SoundWave);
					}
					else
					{
						FString URL = Cache->GetVoicePreviewURL(Settings->SelectedVoice);
                        if(!URL.IsEmpty())
                        {
                        	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
                        	Request->SetURL(URL);
                        	Request->SetVerb("GET");
                        	//Request->SetHeader("Content-Type", "application/json");
							FString VoiceName = Settings->SelectedVoice;
                        	Request->OnProcessRequestComplete().BindLambda([VoiceName, Cache](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
                        	{
                        		if (bSuccess && Response->GetResponseCode() == 200)
                        		{
                        			const TArray<uint8>& AudioData = Response->GetContent();
                        			USoundWave* SoundWave = CreateSoundWaveFromResponse(AudioData);
                        			if(SoundWave)
                        			{
                        				if(!Cache->IsPreviewCached(VoiceName))
                        					Cache->CacheVoicePreview(VoiceName, SoundWave);
                        				GEditor->PlayPreviewSound(SoundWave);
                        			}
			                        else
			                        {
			                        	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Unable to preview the voice: An error occured while decoding the preview."));
			                        }
                        		}
                        		else
                        		{
                        			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Unable to preview the voice: An error occured while downloading the preview."));
									if(Response)
									{
										UE_LOG(LogTemp, Error, TEXT("Failed to get voices preview! HTTP Status: %d"), Response->GetResponseCode());
									}
			                        else
			                        {
				                        UE_LOG(LogTemp, Error, TEXT("Failed to get voices preview!"));
			                        }
                        		}
                        	});
    
                        	Request->ProcessRequest();
                        }
                        else
                        {
                        	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Unable to preview the voice: There is no Preview URL cached for this voice (maybe there was an error on editor startup and voices data have not been fetched correctly! Try to restart the editor.)."));
                        }
					}
				}
			}

			return FReply::Handled();
		})
	];
}

// Code from : https://github.com/frinky04/ElevenLabsTTS-UnrealEngine & updated to work well.
USoundWave* FVoiceSelectionSettingsDetails::CreateSoundWaveFromResponse(const TArray<uint8>& AudioData)
{
	mp3dec_t Mp3Decoder;
	mp3dec_init(&Mp3Decoder);

	mp3dec_file_info_t FileInfo;
	int LoadResult = mp3dec_load_buf(&Mp3Decoder, AudioData.GetData(), AudioData.Num(), &FileInfo, NULL, NULL);

	if (LoadResult != 0)
	{
		// Error loading MP3 data
		return nullptr;
	}

	int32 RawPCMDataSize = FileInfo.samples * sizeof(int16_t);
	uint8 * RawPCMData = static_cast<uint8*>(FMemory::Malloc(RawPCMDataSize));
	FMemory::Memcpy(RawPCMData, FileInfo.buffer, RawPCMDataSize);

	TArray<uint8> WavData;
	SerializeWaveFile(WavData, RawPCMData, RawPCMDataSize, FileInfo.channels, FileInfo.hz);

	FMemory::Free(RawPCMData);
	FWaveModInfo AudioInfo;
	FString ErrorReason;
	if(AudioInfo.ReadWaveInfo(WavData.GetData(), WavData.Num(), &ErrorReason))
	{
		USoundWave* SoundWave = NewObject<USoundWave>();
		if (SoundWave)
		{
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
			
			SoundWave->SoundGroup = SOUNDGROUP_Voice;
			SoundWave->bLooping = false;
		}

		// Free the decoded buffer and return the SoundWave object
		free(FileInfo.buffer);
		return SoundWave;
	}
	return nullptr;
}

/*
void FVoiceSelectionSettingsDetails::OnVoiceSelected(TSharedPtr<FString> NewValue, ESelectInfo::Type)
{
	if (EditedObject.IsValid())
	{
		UMyTTSSettings* Settings = Cast<UMyTTSSettings>(EditedObject.Get());
		if (Settings)
		{
			Settings->SelectedVoice = *NewValue;
		}
	}
}

TSharedPtr<FString> FVoiceSelectionSettingsDetails::GetSelectedVoice() const
{
	if (EditedObject.IsValid())
	{
		const UMyTTSSettings* Settings = Cast<UMyTTSSettings>(EditedObject.Get());
		if (Settings)
		{
			return MakeShared<FString>(Settings->SelectedVoice);
		} 
	}
	return VoiceOptions.Num() > 0 ? VoiceOptions[0] : nullptr;
}
*/
