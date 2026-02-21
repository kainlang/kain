// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSceneViewport;
class AActor;
class UTexture2D;

class SMPREVIEWEDITOR_API ISMPreviewModeViewportClient
{
public:
	DECLARE_DELEGATE_OneParam(FOnThumbnailCaptured, UTexture2D*);

public:
	virtual ~ISMPreviewModeViewportClient() = default;
	virtual void CaptureThumbnail(UObject* InOwner, FOnThumbnailCaptured InOnThumbnailCaptured, FIntPoint InCaptureSize = FIntPoint(40, 40)) = 0;
	virtual AActor* GetSelectedActor() const = 0;
	virtual bool IsActorSelected() const = 0;
	virtual void OnEditorTick(float DeltaTime) = 0;
};

