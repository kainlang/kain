// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

class USoundBase;
struct FAssetData;

namespace LD::SoundUtils
{
	/** If an asset is playing sound. */
	bool IsSoundPlaying(const FAssetData& InAssetData);

	/** Plays the specified sound wave. */
	void PlaySound(USoundBase* Sound);

	/** Stops any currently playing sounds. */
	void StopSound();
}
