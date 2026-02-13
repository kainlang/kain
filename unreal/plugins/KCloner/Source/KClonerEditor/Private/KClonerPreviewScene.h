// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AdvancedPreviewScene.h"

class FKClonerPreviewScene : public FAdvancedPreviewScene
{
public:
	FKClonerPreviewScene(ConstructionValues CVS);
	~FKClonerPreviewScene();

	virtual void Tick(float InDeltaTime) override;

	void SetClonerData(class UKClonerData* InData);
	class AKClonerActor* GetClonerActor() const { return ClonerActor; }

	// Timeline support
	float GetCurrentTime() const { return CurrentTime; }
	void SetCurrentTime(float InTime);
	bool IsPlaying() const { return bIsPlaying; }
	void SetPlaying(bool bPlay) { bIsPlaying = bPlay; }

private:
	class AKClonerActor* ClonerActor;
	
	float CurrentTime = 0.0f;
	bool bIsPlaying = false;
};
