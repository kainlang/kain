// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ISMPreviewModeViewportClient.h"

#include "CoreMinimal.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SEditorViewport.h"

#include "AdvancedPreviewScene.h"

class FScopedTransaction;

class USMBlueprint;
class ASMPreviewStateMachineActor;
class USMPreviewObject;
class FSMBlueprintEditor;
class SSMPreviewModeViewportView;

/**
 * Our own AdvancedPreviewScene implementation. Manages an OriginalWorld which gets cloned into the inherited PreviewWorld.
 * Handles world destruction and creation.
 * Requires the use of a PreviewObject to detect changes and spawn actors.
 *
 * Some of this logic should probably be in our FEditorViewportClient, but because this already manages the world & world context
 * it's simpler to include it here.
 */
class FSMAdvancedPreviewScene : public FAdvancedPreviewScene
{
public:
	explicit FSMAdvancedPreviewScene(const ConstructionValues& CVS, const TSharedPtr<FSMBlueprintEditor>& InEditor, float InFloorOffset = 0.0f);
	~FSMAdvancedPreviewScene();
	
	// FAdvancedPreviewScene
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual void AddComponent(UActorComponent* Component, const FTransform& LocalToWorld, bool bAttachToRoot = false) override;
	virtual void RemoveComponent(UActorComponent* Component) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// ~FAdvancedPreviewScene

	void SetSceneViewport(TSharedPtr<FSceneViewport> InSceneViewport, TSharedPtr<SOverlay> InViewportOverlay);
	
	void FlagTickable();
	
	/**
	 * Call when the user is going to use or stop using this window. Updates the
	 * world context to help prevent the level outliner from capturing it.
	 */
	void CheckRefreshLevelOutliner();
	
	/** Clone the preview world. */
	void CloneOriginalWorldToPreviewWorld();

	/** Destroy the simulated world and restore the preview world.  */
	void RestoreOriginalWorld();

	/** The original world which may be the current PreviewWorld. */
	UWorld* GetOriginalWorld() const { return OriginalWorld; }

	/** The current PreviewWorld. */
	UWorld* GetPreviewWorld() const { return PreviewWorld; }
	
	/** Spawn and set the actor. The NewActor is assumed to be a template. */
	void SetPreviewObject(USMPreviewObject* Object);

	/** Get the preview object. */
	USMPreviewObject* GetPreviewObject() const { return PreviewObject.Get(); }
	
	/** Return the actor for the current preview world. */
	AActor* GetContextActorForCurrentWorld() const;

	/** Return the transient package used for the preview world. */
	UPackage* GetPackageForPreviewWorld() const { return PreviewPackage; }

protected:
	/** Set a new preview world. */
	void SetPreviewWorld(UWorld* InPreviewWorld);

	/** Destroy everything in the current preview world. */
	void DestroyPreviewWorld();
	
	/** Load in environment components (floor, light, ect) */
	void LoadEnvironmentComponents();

	/** Set the OriginalWorld and rename the world based on the blueprint. */
	void SetupInitialPreviewWorld();

	/** The preview object has requested the world be refreshed. */
	void OnPreviewObjectWorldRefreshRequested(USMPreviewObject* InPreviewObject);

	/** When the active tab changes. This is where we bind mouse events. */
	void OnActiveTabChanged(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated);

	/** When the blueprint editor mode is changed. */
	void OnBlueprintModeSet(FName NewMode);

protected:
	/** Components field is private, need to track ours. */
	TArray<TObjectPtr<UActorComponent>> OurComponents;
	FPreviewScene::ConstructionValues CVSStored;
	TSharedPtr<FSceneViewport> SceneViewportPtr;
	/** Editor viewport overlay ptr. */
	TSharedPtr<SOverlay> OverlayPtr;
	/** For the overlay created locally and managed by the manually created game viewport client. */
	TSharedPtr<SOverlay> GameOverlay;

	TWeakPtr<FSMBlueprintEditor> BlueprintEditor;
	TWeakObjectPtr<USMPreviewObject> PreviewObject;
	TWeakObjectPtr<USMBlueprint> BlueprintPtr;
	
	FDelegateHandle OnPreviewWorldRefreshHandle;
	FDelegateHandle OnBlueprintActiveTabForegroundedHandle;
	FDelegateHandle OnBlueprintActiveTabChangedHandle;
	FDelegateHandle OnBlueprintModeChangedHandle;
	/** The last time we were flagged for ticking */
	double LastTickTime;

	UPackage* PreviewPackage;
	TObjectPtr<UWorld> OriginalWorld;
	UGameViewportClient* GameViewportClient;
	FString OriginalWorldName;
	FWorldContext* WorldContext;
	float FloorOffset;
	
	TWeakPtr<SDockTab> ParentTabPtr;
	FVector2D MouseScreenSpace;
	
	/** True only while the bp editor is in preview mode. */
	bool bIsBPEditorInPreviewMode;

};


/** Viewport Client for the preview viewport */
class FSMPreviewModeViewportClient : public FEditorViewportClient, public ISMPreviewModeViewportClient, public TSharedFromThis<FSMPreviewModeViewportClient>
{
public:
	FSMPreviewModeViewportClient(FSMAdvancedPreviewScene& InPreviewScene, const TSharedRef<SSMPreviewModeViewportView>& InPreviewViewport);
	virtual ~FSMPreviewModeViewportClient() override;
	
	// FEditorViewportClient
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(FViewport* InViewport, FCanvas* Canvas) override;
	virtual bool ProcessScreenShots(FViewport* InViewport) override;
	virtual bool CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const override { return true; }
	virtual bool CanCycleWidgetMode() const override { return true; }
	virtual UE::Widget::EWidgetMode GetWidgetMode() const override;
	virtual FVector GetWidgetLocation() const override;
	virtual bool IsLevelEditorClient() const override { return false; }
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	virtual bool InputWidgetDelta(FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale) override;
	virtual void TrackingStarted(const FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge) override;
	virtual void TrackingStopped() override;
	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	// ~FEditorViewportClient

	void SetSceneViewport(TSharedPtr<FSceneViewport> InViewport);
	void SelectActor(AActor* NewActor);
	void ResetSelection();
	void ResetCamera();

	/**
	 * Returns true if the grid is currently visible in the viewport
	 */
	bool GetShowGrid() const;

	/**
	 * Will toggle the grid's visibility in the viewport
	 */
	void ToggleShowGrid();
	
	// ISMPreviewModeViewportClient
	virtual AActor* GetSelectedActor() const override { return SelectedActor.Get(); }
	virtual bool IsActorSelected() const override { return SelectedActor != nullptr; }
	virtual void CaptureThumbnail(UObject* InOwner, FOnThumbnailCaptured InOnThumbnailCaptured, FIntPoint InCaptureSize = FIntPoint(40, 40)) override;
	virtual void OnEditorTick(float DeltaTime) override;
	// ~ISMPreviewModeViewportClient

	FSMAdvancedPreviewScene* GetOurPreviewScene() const { return static_cast<FSMAdvancedPreviewScene*>(PreviewScene); }

protected:
	/** Initiates a transaction. */
	void BeginTransaction(const FText& Description);

	/** Ends the current transaction, if one exists. */
	void EndTransaction();

	/** Draws text indicating we are simulating. */
	void DrawSimulating(FViewport* InViewport, FCanvas* Canvas);

protected:
	void OnSimulationStarted(USMPreviewObject* PreviewObject);
	void OnSimulationEnded(USMPreviewObject* PreviewObject);

	FDelegateHandle OnSimStartHandle;
	FDelegateHandle OnSimEndHandle;

private:
	TWeakPtr<FSceneViewport> SceneViewportPtr;
	TWeakPtr<SSMPreviewModeViewportView> ViewportViewPtr;
	TWeakObjectPtr<AActor> SelectedActor;
	TWeakObjectPtr<UObject> ThumbnailOwner;
	FOnThumbnailCaptured OnThumbnailCaptured;
	FIntPoint ThumbnailCaptureSize;
	
	/** The current transaction for undo/redo */
	FScopedTransaction* ScopedTransaction;
	
	bool bDraggingActor;
	bool bCaptureThumbnail;
};