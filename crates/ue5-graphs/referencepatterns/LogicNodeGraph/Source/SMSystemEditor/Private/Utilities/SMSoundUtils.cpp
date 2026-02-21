// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMSoundUtils.h"

#include "Editor.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

bool LD::SoundUtils::IsSoundPlaying(const FAssetData& InAssetData)
{
	const UAudioComponent* PreviewComp = GEditor->GetPreviewAudioComponent();
	if (PreviewComp && PreviewComp->Sound && PreviewComp->IsPlaying())
	{
		if (PreviewComp->Sound->GetFName() == InAssetData.AssetName)
		{
			if (PreviewComp->Sound->GetOutermost()->GetFName() == InAssetData.PackageName)
			{
				return true;
			}
		}
	}

	return false;
}

void LD::SoundUtils::PlaySound(USoundBase* Sound)
{
	if (Sound)
	{
		GEditor->PlayPreviewSound(Sound);
	}
	else
	{
		StopSound();
	}
}

void LD::SoundUtils::StopSound()
{
	GEditor->ResetPreviewAudioComponent();
}
