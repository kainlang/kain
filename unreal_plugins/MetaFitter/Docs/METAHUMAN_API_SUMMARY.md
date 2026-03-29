# MetaHuman API Analysis Summary

Generated: 1771764352.6348615

## Summary Statistics

- **Total Classes**: 614
- **Total Blueprint Functions**: 94
- **Total Components**: 0
- **Total Subsystems**: 1
- **Total Enums**: 97
- **Total Structs**: 175
- **Total Interfaces**: 4
- **Total Modules**: 46
- **Clothing Apis Found**: 0
- **Mesh Apis Found**: 8
- **Material Apis Found**: 33
- **Physics Apis Found**: 0

## Key Modules

### MetaHumanBatchProcessor.build
**Dependencies**: Core, CoreUObject, 

### MetaHumanCaptureDataEditor
**Dependencies**: 

### MetaHumanCaptureProtocolStack
**Dependencies**: Core, MetaHumanCaptureUtils

### MetaHumanCaptureSource
**Dependencies**: Core, CoreUObject, ImgMedia, Engine, MetaHumanCaptureUtils, CaptureDataCore

### MetaHumanCaptureUtils
**Dependencies**: Core, CoreUObject, Engine, 

### MetaHumanConfig
**Dependencies**: Core, 

### MetaHumanConfigEditor
**Dependencies**: Core, 

### MetaHumanControlsConversionTest
**Dependencies**: 

### MetaHumanCore
**Dependencies**: Core, CoreUObject, Engine, RigLogicLib, RigLogicModule, CaptureDataCore, MetaHumanCoreTech, 

### MetaHumanCoreEditor
**Dependencies**: Core, 

### MetaHumanDepthGenerator
**Dependencies**: Core, CoreUObject, CaptureDataCore

### MetaHumanFaceAnimationSolver
**Dependencies**: Core, 

### MetaHumanFaceAnimationSolverEditor
**Dependencies**: Core, 

### MetaHumanFaceContourTracker
**Dependencies**: Core, CoreUObject, NNE, 

### MetaHumanFaceContourTrackerEditor
**Dependencies**: 

### MetaHumanFaceFittingSolver
**Dependencies**: Core, 

### MetaHumanFaceFittingSolverEditor
**Dependencies**: Core, 

### MetaHumanFootageIngest
**Dependencies**: Core, CoreUObject, 

### MetaHumanIdentity
**Dependencies**: CoreUObject, Engine, SlateCore, GeometryFramework, MetaHumanCore, MetaHumanCaptureData, MetaHumanPipelineCore, MetaHumanCoreTechLib

### MetaHumanImageViewerEditor
**Dependencies**: Core, EditorStyle, SlateCore, MediaAssets, ProceduralMeshComponent, MetaHumanImageViewer, MetaHumanCore, MetaHumanCoreTech

### MetaHumanPerformance
**Dependencies**: Core, Engine, MetaHumanCaptureData, MetaHumanPipelineCore, MetaHumanPipeline, MetaHumanCoreTech, MetaHumanCoreTechLib

### MetaHumanPipeline
**Dependencies**: Core, Eigen, MetaHumanCore, MetaHumanSpeech2Face, MetaHumanCoreTech, MetaHumanCoreTechLib, MetaHumanPipelineCore, MeshTrackerInterface, 

### MetaHumanPlatform
**Dependencies**: Core, 

### MetaHumanSequencer
**Dependencies**: Core, Engine, MovieScene, MediaCompositing, MovieSceneTools, MovieSceneTracks, Sequencer, CaptureDataCore

### MetaHumanSpeech2Face
**Dependencies**: Core, ControlRig

### MetaHumanToolkit
**Dependencies**: Core, CoreUObject, UnrealEd, Slate, SlateCore, AdvancedPreviewScene, MovieScene, Sequencer, ImgMedia, MovieSceneTracks, MetaHumanCaptureData, MetaHumanImageViewer, MetaHumanImageViewerEditor, MetaHumanSequencer, 

### MetaHumanCalibrationCore
**Dependencies**: Core, CoreUObject, Engine, CaptureDataCore

### MetaHumanCalibrationGenerator
**Dependencies**: Core, CoreUObject, Engine, CaptureDataCore

### MetaHumanCalibrationLib
**Dependencies**: 

### MetaHumanCharacter
**Dependencies**: Core, CoreUObject, Engine, ImageCore, MetaHumanCharacterPalette, MetaHumanSDKRuntime, Projects, SlateCore

### MetaHumanCharacterEditor
**Dependencies**: MetaHumanCoreTechLib, RigLogicModule, MetaHumanSDKEditor, MetaHumanCoreTech

### MetaHumanCharacterMigrationEditor
**Dependencies**: 

### MetaHumanCharacterPalette
**Dependencies**: Core, CoreUObject, Engine, DeveloperSettings, MetaHumanSDKRuntime, 

### MetaHumanCharacterPaletteEditor
**Dependencies**: 

### MetaHumanDefaultEditorPipeline
**Dependencies**: Core, CoreUObject, Engine, HairStrandsCore, ChaosClothAssetEngine, ChaosOutfitAssetEngine, MetaHumanCharacter, MetaHumanCharacterEditor, MetaHumanCharacterPalette, MetaHumanCharacterPaletteEditor, MetaHumanDefaultPipeline, MetaHumanSDKRuntime, MetaHumanCoreTech, TextureGraph, 

### MetaHumanDefaultPipeline
**Dependencies**: Core, CoreUObject, Engine, HairStrandsCore, ChaosClothAssetEngine, ChaosOutfitAssetEngine, MetaHumanCharacter, MetaHumanCharacterPalette, 

### MetaHumanCaptureData
**Dependencies**: Core, 

### MetaHumanCoreTech
**Dependencies**: 

### MetaHumanCoreTechLib
**Dependencies**: Eigen, RigLogicLib, RigLogicModule, MetaHumanSDKRuntime, 

### MetaHumanImageViewer
**Dependencies**: 

### MetaHumanPipelineCore
**Dependencies**: Eigen, 

### LiveLinkFaceSourceEditor
**Dependencies**: 

### MetaHumanLiveLinkSource
**Dependencies**: LiveLinkInterface, LiveLink, Engine, 

### MetaHumanLiveLinkSourceEditor
**Dependencies**: LiveLinkInterface, UnrealEd, 

### MetaHumanLocalLiveLinkSource
**Dependencies**: MetaHumanLiveLinkSource, 

### MetaHumanLocalLiveLinkSourceEditor
**Dependencies**: 


## Important Classes

### IPredictiveSolverInterface
- **Base Class**: IModularFeature
- **File**: MetaHumanAnimator\Source\MeshTrackerInterface\Public\MetaHumanFaceTrackerInterface.h

### IDepthProcessingMetadataProvider
- **Base Class**: IModularFeature
- **File**: MetaHumanAnimator\Source\MeshTrackerInterface\Public\MetaHumanFaceTrackerInterface.h

### IFaceTrackerNodeImplFactory
- **Base Class**: IModularFeature
- **File**: MetaHumanAnimator\Source\MeshTrackerInterface\Public\MetaHumanFaceTrackerInterface.h

### FMetaHumanBatchProcessorModule
- **Base Class**: IModuleInterface
- **File**: MetaHumanAnimator\Source\MetaHumanBatchProcessor\Private\MetaHumanBatchProcessorModule.h

### SMetaHumanBatchExportPathDialog
- **Base Class**: SWindow
- **File**: MetaHumanAnimator\Source\MetaHumanBatchProcessor\Private\SMetaHumanBatchExportPathDialog.h

### UMetaHumanBatchOperation
- **Base Class**: UObject
- **File**: MetaHumanAnimator\Source\MetaHumanBatchProcessor\Public\MetaHumanBatchOperation.h

### UMetaHumanSpeechToPerformance
- **Base Class**: UObject
- **File**: MetaHumanAnimator\Source\MetaHumanBatchProcessor\Public\MetaHumanSpeechProcessingSettings.h

### UMetaHumanExportAnimSequenceSettings
- **Base Class**: UObject
- **File**: MetaHumanAnimator\Source\MetaHumanBatchProcessor\Public\MetaHumanSpeechProcessingSettings.h

### UMetaHumanSpeechToAnimSequenceProcessingSettings
- **Base Class**: UObject
- **File**: MetaHumanAnimator\Source\MetaHumanBatchProcessor\Public\MetaHumanSpeechProcessingSettings.h

### UMetaHumanExportLevelSequenceSettings
- **Base Class**: UObject
- **File**: MetaHumanAnimator\Source\MetaHumanBatchProcessor\Public\MetaHumanSpeechProcessingSettings.h

### UMetaHumanSpeechToLevelSequenceSettings
- **Base Class**: UObject
- **File**: MetaHumanAnimator\Source\MetaHumanBatchProcessor\Public\MetaHumanSpeechProcessingSettings.h

### SMetaHumanSpeechToAnimProcessingSettings
- **Base Class**: SCompoundWidget
- **File**: MetaHumanAnimator\Source\MetaHumanBatchProcessor\Public\SMetaHumanSpeechProcessingSettings.h

### FMetaHumanCaptureDataEditorModule
- **Base Class**: IModuleInterface
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureDataEditor\Private\MetaHumanCaptureDataEditorModule.h

### SMetaHumanCameraCombo
- **Base Class**: SCompoundWidget
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureDataEditor\Public\SMetaHumanCameraCombo.h

### FMetaHumanCaptureProtocolStackModule
- **Base Class**: IModuleInterface
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureProtocolStack\Private\MetaHumanCaptureProtocolStack.h

### FDataProvider
- **Base Class**: ITcpSocketReader
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureProtocolStack\Private\Tests\Utility.h

### FDataSender
- **Base Class**: ITcpSocketWriter
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureProtocolStack\Private\Tests\Utility.h

### FFailedDataSender
- **Base Class**: FDataSender
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureProtocolStack\Private\Tests\Utility.h

### FCommunicationRunnable
- **Base Class**: FRunnable
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureProtocolStack\Public\Control\Communication\ControlCommunication.h

### FExportWorker
- **Base Class**: FRunnable
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureProtocolStack\Public\ExportClient\ExportWorker.h

### TQueueRunner
- **Base Class**: FRunnable
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureProtocolStack\Public\Utility\QueueRunner.h

### UMetaHumanCaptureSourceFactoryNew
- **Base Class**: UFactory
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Private\MetaHumanCaptureSourceFactoryNew.h

### UMetaHumanCaptureSourceSyncFactoryNew
- **Base Class**: UFactory
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Private\MetaHumanCaptureSourceFactoryNew.h

### UAssetDefinition_MetaHumanCaptureSource
- **Base Class**: UAssetDefinitionDefault
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Private\AssetDefinitions\AssetDefinition_MetaHumanCaptureSource.h

### FCubicCameraSystemIngest
- **Base Class**: FFileFootageIngest
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Private\FootageIngest\CubicCameraSystemIngest.h

### FFileFootageIngest
- **Base Class**: FFootageIngest
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Private\FootageIngest\FileFootageIngest.h

### FFootageIngest
- **Base Class**: IFootageIngestAPI
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Private\FootageIngest\FootageIngest.h

### FHMCArchiveIngest
- **Base Class**: FStereoReconstructionSystemIngest
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Private\FootageIngest\HMCArchiveIngest.h

### IFootageIngestAPI
- **Base Class**: FCommandHandler
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Private\FootageIngest\IFootageIngestAPI.h

### FLiveLinkFaceIngestBase
- **Base Class**: FFootageIngest
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Private\FootageIngest\LiveLinkFaceFootageIngest.h

### FStereoReconstructionSystemIngest
- **Base Class**: FCubicCameraSystemIngest
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Private\FootageIngest\StereoReconstructionSystemIngest.h

### FFileStream
- **Base Class**: FBaseStream
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Private\FootageIngest\Utils\LiveLinkFaceConnectionExportStreams.h

### FDataStream
- **Base Class**: FBaseStream
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Private\FootageIngest\Utils\LiveLinkFaceConnectionExportStreams.h

### FIngestTask
- **Base Class**: FNonAbandonableTask
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Private\FootageIngest\Utils\LiveLinkFaceTakeDataConverter.h

### FDepthWriterTask
- **Base Class**: IImageWriteTaskBase
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Private\FootageIngest\Utils\MetaHumanDepthConverter.h

### FIpAddressDetailsCustomization
- **Base Class**: IPropertyTypeCustomization
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureSource\Public\IpAddressDetailsCustomization.h

### MetaHumanCaptureUtilsModule
- **Base Class**: IModuleInterface
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureUtils\Private\MetaHumanCaptureUtilsModule.h

### FCaptureEventSource
- **Base Class**: detail
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureUtils\Public\Async\EventSourceUtils.h

### FCaptureEventSourceWithLimiter
- **Base Class**: detail
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureUtils\Public\Async\EventSourceUtils.h

### FAsyncTaskInternal
- **Base Class**: FNonAbandonableTask
- **File**: MetaHumanAnimator\Source\MetaHumanCaptureUtils\Public\Async\Task.h

### FMetaHumanConfigStyle
- **Base Class**: FSlateStyleSet
- **File**: MetaHumanAnimator\Source\MetaHumanConfig\Private\MetaHumanConfigStyle.h

### UMetaHumanConfig
- **Base Class**: UObject
- **File**: MetaHumanAnimator\Source\MetaHumanConfig\Public\MetaHumanConfig.h

### UMetaHumanConfigFactory
- **Base Class**: UFactory
- **File**: MetaHumanAnimator\Source\MetaHumanConfigEditor\Private\MetaHumanConfigFactory.h

### UAssetDefinition_MetaHumanConfig
- **Base Class**: UAssetDefinitionDefault
- **File**: MetaHumanAnimator\Source\MetaHumanConfigEditor\Private\AssetDefinitions\AssetDefinition_MetaHumanConfig.h

### FMetaHumanConfigCustomization
- **Base Class**: IDetailCustomization
- **File**: MetaHumanAnimator\Source\MetaHumanConfigEditor\Private\Customizations\MetaHumanConfigCustomizations.h

### SMetaHumanConfigCombo
- **Base Class**: SCompoundWidget
- **File**: MetaHumanAnimator\Source\MetaHumanConfigEditor\Public\SMetaHumanConfigCombo.h

### FMetaHumanControlsConversionTestModule
- **Base Class**: IModuleInterface
- **File**: MetaHumanAnimator\Source\MetaHumanControlsConversionTest\Private\MetaHumanControlsConversionTestModule.h

### UMetaHumanContourData
- **Base Class**: UObject
- **File**: MetaHumanAnimator\Source\MetaHumanCore\Public\MetaHumanContourData.h

### FMetaHumanCoreStyle
- **Base Class**: FSlateStyleSet
- **File**: MetaHumanAnimator\Source\MetaHumanCore\Public\MetaHumanCoreStyle.h

### UMetaHumanProcessingAsset
- **Base Class**: UObject
- **File**: MetaHumanAnimator\Source\MetaHumanCore\Public\MetaHumanProcessingAsset.h


## Blueprint-Callable Functions

### Process
**File**: MetaHumanAnimator\Source\MetaHumanDepthGenerator\Private\MetaHumanDepthGenerator.h

```cpp
;
```

### IsControlRigVisible
**File**: MetaHumanAnimator\Source\MetaHumanPerformance\Private\MetaHumanPerformanceViewportSettings.h

```cpp

```

### ToggleControlRigVisibility
**File**: MetaHumanAnimator\Source\MetaHumanPerformance\Private\MetaHumanPerformanceViewportSettings.h

```cpp
UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	bool IsControlRigVisible(EABImageViewMode InView) const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings"
```

### ComparePerformanceContourData
**File**: MetaHumanAnimator\Source\MetaHumanPerformance\Private\Tests\ContourDataComparisonHelper.h

```cpp

```

### Init
**File**: MetaHumanCalibrationProcessing\Source\MetaHumanCalibrationGenerator\Private\MetaHumanCalibrationGenerator.h

```cpp

```

### ConfigureCameras
**File**: MetaHumanCalibrationProcessing\Source\MetaHumanCalibrationGenerator\Private\MetaHumanCalibrationGenerator.h

```cpp
UFUNCTION(BlueprintCallable, Category = "MetaHuman | Calibration Generator")
	bool Init(const UMetaHumanCalibrationGeneratorConfig* InConfig);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman | Ca
```

### Process
**File**: MetaHumanCalibrationProcessing\Source\MetaHumanCalibrationGenerator\Private\MetaHumanCalibrationGenerator.h

```cpp
UFUNCTION(BlueprintCallable, Category = "MetaHuman | Calibration Generator")
	bool ConfigureCameras(const UFootageCaptureData* InCaptureData);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman | Ca
```

### GetLastRMSError
**File**: MetaHumanCalibrationProcessing\Source\MetaHumanCalibrationGenerator\Private\MetaHumanCalibrationGenerator.h

```cpp
UFUNCTION(BlueprintCallable, Category = "MetaHuman | Calibration Generator")
	bool Process(UFootageCaptureData* InCaptureData, const UMetaHumanCalibrationGeneratorOptions* InOptions);

	UFUNCTION(Blue
```

### SetSelectedFrames
**File**: MetaHumanCalibrationProcessing\Source\MetaHumanCalibrationGenerator\Private\MetaHumanCalibrationGeneratorOptions.h

```cpp
;
```

### SetRotation
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Private\MetaHumanCharacterEnvironmentLightRig.h

```cpp

```

### SetBackgroundColor
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Private\MetaHumanCharacterEnvironmentLightRig.h

```cpp
UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Lighting")
	void SetRotation(float InRotation);
};

UINTERFACE(Blueprintable)
class UMetaHumanCharacterEnvironmentBackground : public UIn
```

### IsObjectAddedForEditing
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Initialization")
	[[nodiscard]] bool TryAddObjectToEdit(UMetaHumanCharacter* InCharacter);

	/** Returns true if the object is registered for editing */
	UFUNC
```

### RemoveObjectToEdit
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Initialization")
	bool IsObjectAddedForEditing(const UMetaHumanCharacter* InCharacter) const;

	/**
	 * Tells the subsystem that a character is 
```

### CanBuildMetaHuman
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Actor")
	AActor* SpawnMetaHumanActor(UMetaHumanCharacter* InCharacter);

	/** 
	 * Gets the class of actor that will be spawned by CreateMetaHumanCharacterEdit
```

### BuildMetaHuman
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Assembly")
	bool CanBuildMetaHuman(const UMetaHumanCharacter* InCharacter, bool bInLogError = false);

	/**
	 * @brief Assemble a MetaHuman Character.
	 * 
	 *
```

### CommitHeadModelSettings
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Assembly")
	void BuildMetaHuman(UMetaHumanCharacter* InCharacter, const FMetaHumanCharacterEditorBuildParameters& InParams);

	/**
	 * Obtain a copy of the fac
```

### CommitSkinSettings
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Model")
	void CommitHeadModelSettings(UMetaHumanCharacter* InMetaHumanCharacter, const FMetaHumanCharacterHeadModelSettings& InHeadModelSettings);

	/**
	 * Ap
```

### RequestTextureSources
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Texture Synthesis")
	void CommitSkinSettings(UMetaHumanCharacter* InCharacter, const FMetaHumanCharacterSkinSettings& InSkinSettings);

	
	UE_DEPRECATED(5.7, "
```

### CompareFaceTextures
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Texture Synthesis")
	void RequestTextureSources(UMetaHumanCharacter* InCharacter, const FMetaHumanCharacterTextureRequestParams& InParams = FMetaHumanCharacter
```

### CommitEyesSettings
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Testing")
	static bool CompareFaceTextures(const UMetaHumanCharacter* InCharacter1, const UMetaHumanCharacter* InCharacter2, int32 InPixelTolerance);


	/** Ca
```

### CommitMakeupSettings
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Eyes")
	void CommitEyesSettings(UMetaHumanCharacter* InCharacter, const FMetaHumanCharacterEyesSettings& InEyesSettings) const;

private:

	/**
	 * Utility fun
```

### CommitFaceState
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Makeup")
	void CommitMakeupSettings(UMetaHumanCharacter* InCharacter, const FMetaHumanCharacterMakeupSettings& InMakeupSettings) const;

private:

	/**
	 * @br
```

### CompareFaceState
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Sculpting")
	void CommitFaceState(UMetaHumanCharacter* InCharacter);

	/**
	* Evaluate the face state for each of the supplied characters, and compare the vert
```

### GetFaceLandmarks
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Testing")
	bool CompareFaceState(const UMetaHumanCharacter* InCharacter1, const UMetaHumanCharacter* InCharacter2, float InTolerance) const;


	/** 
	 * Return
```

### TranslateFaceLandmarks
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Sculpting")
	void GetFaceLandmarks(const UMetaHumanCharacter* InCharacter, TArray<FVector>& OutFaceLandmarks) const;

	/**
	 * Translates the gi
```

### RequestAutoRigging
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Sculpting")
	void TranslateFaceLandmarks(const UMetaHumanCharacter* InCharacter, const TArray<int32>& InLandmarkIndices, const TArray<FVector>& InDeltas);

	/*
```

### FitStateToTargetVertices
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Auto-Rigging")
	void RequestAutoRigging(UMetaHumanCharacter* InCharacter, const FMetaHumanCharacterAutoRiggingRequestParams& InParams = FMetaHumanCharacterAuto
```

### ImportFromFaceDna
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Conforming")
	bool FitStateToTargetVertices(UMetaHumanCharacter* InCharacter, const FMetaHumanCharacterFitToVerticesParams& InParams);

	/**
	 * Fit the state 
```

### ImportFromTemplate
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Conforming")
	EImportErrorCode ImportFromFaceDna(UMetaHumanCharacter* InCharacter, const FString& InDNAFilePath, const FImportFromDNAParams& InImportParams);


```

### CompareBodyState
**File**: MetaHumanCharacter\Source\MetaHumanCharacterEditor\Public\MetaHumanCharacterEditorSubsystem.h

```cpp
UFUNCTION(BlueprintCallable, Category = "Conforming")
	EImportErrorCode ImportFromTemplate(UMetaHumanCharacter* InCharacter, UObject* InTemplateMesh, UObject* InTemplateLeftEyeMesh, UObject* InTemplat
```


## Components


## Subsystems

- **UMetaHumanCharacterEditorSubsystem** (extends UEditorSubsystem)

## Clothing-Related APIs


## Mesh Manipulation APIs

### mesh
**File**: MetaHumanCalibrationProcessing\Source\MetaHumanCalibrationLib\Private\nls\include\nls\geometry\Mesh.h

### mesh
**File**: MetaHumanCalibrationProcessing\Source\MetaHumanCalibrationLib\Private\nls\include\nls\geometry\Mesh.h

### push_back
**File**: MetaHumanCalibrationProcessing\Source\MetaHumanCalibrationLib\Private\nls\include\nls\geometry\MeshSmoothing.h

### selection
**File**: MetaHumanCalibrationProcessing\Source\MetaHumanCalibrationLib\Private\nls\include\nls\geometry\MeshTools.h

### mesh
**File**: MetaHumanCoreTechLib\Source\MetaHumanCoreTechLib\Private\nls\include\nls\geometry\Mesh.h

### mesh
**File**: MetaHumanCoreTechLib\Source\MetaHumanCoreTechLib\Private\nls\include\nls\geometry\Mesh.h

### push_back
**File**: MetaHumanCoreTechLib\Source\MetaHumanCoreTechLib\Private\nls\include\nls\geometry\MeshSmoothing.h

### selection
**File**: MetaHumanCoreTechLib\Source\MetaHumanCoreTechLib\Private\nls\include\nls\geometry\MeshTools.h

