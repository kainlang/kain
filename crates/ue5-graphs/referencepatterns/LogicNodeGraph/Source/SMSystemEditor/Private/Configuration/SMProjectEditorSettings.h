// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMNodeSettings.h"

#include "SMProjectEditorSettings.generated.h"

class USMTransitionInstance;
class USMConduitInstance;
class USMStateMachineInstance;
class USMStateInstance;
class USMInstance;

UENUM()
enum class ESMPinOverride : uint8
{
	/** Override is disabled for all assets. Restart required. */
	None,
	/** Override is only for Logic Driver assets. */
	LogicDriverOnly,
	/** Override is for all blueprint types. */
	AllBlueprints
};

UCLASS(config = Editor, defaultconfig)
class SMSYSTEMEDITOR_API USMProjectEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	USMProjectEditorSettings();

	UPROPERTY(config, VisibleAnywhere, Category = "Version Updates")
	FString InstalledVersion;
	
	/**
	 * Automatically update assets saved by older versions to the most current version. It is strongly recommended to leave this on.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Version Updates")
	bool bUpdateAssetsOnStartup;

	/**
	 * Display a progress bar when updating assets to a new version.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Version Updates", meta = (EditCondition = "bUpdateAssetsOnStartup"))
	bool bDisplayAssetUpdateProgress;

	/**
	 * Display a popup with a link to the patch notes when a new version is detected.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Version Updates")
	bool bDisplayUpdateNotification;

	/**
	 * Warn if approaching Blueprint memory limits on a compile.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Validation|Memory")
	bool bDisplayMemoryLimitsOnCompile;

	/**
	 * Display the used struct memory as an info message on compile. 
	 */
	UPROPERTY(config, EditAnywhere, Category = "Validation|Memory", meta = (EditCondition = "bDisplayMemoryLimitsOnCompile"))
	bool bAlwaysDisplayStructMemoryUsage;
	
	/**
	 * The percent of used struct memory that must be reached before a warning is triggered.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Validation|Memory", meta = (EditCondition = "bDisplayMemoryLimitsOnCompile", UIMin = 0.0, UIMax = 1.0))
	float StructMemoryLimitWarningThreshold;

	/**
	* Display a note in the compiler log when input events are used.
	*/
	UPROPERTY(config, EditAnywhere, Category = "Validation|Input")
	bool bDisplayInputEventNotes;
	
	/**
	 * Restrict invalid characters in state names and in node variable names. When false any character is allowed,
	 * but certain operations can cause Unreal to crash, such as copying and pasting states. Only set to false if you know what you are doing.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Validation")
	bool bRestrictInvalidCharacters;

	/**
	 * Children which reference a parent state machine graph risk being out of date if a package the parent references is modified.
	 * Compiling the package will signal that affected state machine children need to be compiled, however if you start a PIE session
	 * instead of pressing the compile button, the children may not be updated. In this case the state machine compiler will attempt to warn you.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Compile")
	bool bWarnIfChildrenAreOutOfDate;

	/**
	 * Calculate path guids during compile when possible reducing run-time initialization time. This requires the state machine to be partially
	 * instantiated during compile.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Compile")
	bool bCalculateGuidsOnCompile;

	/**
	* Perform special compile handling when linker load is detected to avoid possible crashes and improve sub-object packaging.
	* This should remain on.
	*
	* This setting exists primarily for troubleshooting and will likely be removed in a future update.
	*/
	UPROPERTY(config, EditAnywhere, Category = "Compile", AdvancedDisplay)
	bool bLinkerLoadHandling;
	
	/**
	 * Newly placed transitions will default to true if they do not have a node class assigned.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Transitions")
	bool bDefaultNewTransitionsToTrue;

	/**
	 * Newly placed conduits will default to true if they do not have a node class assigned.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Conduits")
	bool bDefaultNewConduitsToTrue;
	
	/**
	 * Newly placed conduits will automatically be configured as transitions.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Conduits")
	bool bConfigureNewConduitsAsTransitions;

	/**
	 * Default parent class to be assigned when creating new state machine blueprints (SMInstance).
	 */
	UPROPERTY(config, EditAnywhere, Category = "Blueprints")
	TSoftClassPtr<USMInstance> DefaultStateMachineBlueprintParentClass;

	/**
	 * The default prefix to use when creating a new state machine blueprint.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Blueprints")
	FString DefaultStateMachineBlueprintNamePrefix;

	/**
	 * The default prefix to use when creating a new node blueprint.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Blueprints")
	FString DefaultNodeBlueprintNamePrefix;
	
	/**
	 * Configure the editor-time construction script behavior.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Node Instances")
	ESMEditorConstructionScriptProjectSetting EditorNodeConstructionScriptSetting;

	/**
	 * Default class to be assigned when placing a new state node.
	 * A setting of None will use the system default classes.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Node Instances")
	TSoftClassPtr<USMStateInstance> DefaultStateClass;
	
	/**
	 * Default class to be assigned when placing a new state machine node.
	 * A setting of None will use the system default classes.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Node Instances")
	TSoftClassPtr<USMStateMachineInstance> DefaultStateMachineClass;
	
	/**
	 * Default class to be assigned when placing a new conduit node.
	 * A setting of None will use the system default classes.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Node Instances")
	TSoftClassPtr<USMConduitInstance> DefaultConduitClass;

	/**
	 * Default class to be assigned when placing a new transition.
	 * A setting of None will use the system default classes.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Node Instances")
	TSoftClassPtr<USMTransitionInstance> DefaultTransitionClass;

	/**
	 * Allow Logic Driver specific variable customization to show up on the variable details panel
	 * in the Node Blueprint Editor. If this is false then customization needs to be edited in the
	 * class defaults ExposedPropertyOverrides section.
	 *
	 * This is optional because Unreal (prior to 5.1) only allows one override to be present per property.
	 * Logic Driver has to override the FProperty customization which impacts all variables, but only
	 * does so when opening the Node Blueprint Editor.
	 *
	 * It is rare for a plugin to customize variables, and the plugin should be overriding this
	 * only when necessary so it is unlikely for this to cause issues.
	 *
	 * Note that UE properly supports this in 5.1+.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Node Instances")
	bool bEnableVariableCustomization;
	
	/**
	 * Newly placed state machine references will have their templates enabled by default.
	 * This allows custom node classes to be supported with references.
	 *
	 * State machine blueprints that have a custom root state machine node class assigned in their class defaults
	 * will always default to using a template, regardless of this setting.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Node Instances")
	bool bEnableReferenceTemplatesByDefault;

	/**
	 * Allow editor construction scripts to run on load when EditorNodeConstructionScriptSetting is set to Standard or Compile.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Node Instances", AdvancedDisplay)
	bool bRunConstructionScriptsOnLoad;

	/**
	 * Logic Driver can add support to select soft actor references from UEdGraphPins. Unreal by default does not support this.
	 * You can add support only to Logic Driver assets, to all blueprint assets, or disable completely.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Pins")
	ESMPinOverride OverrideActorSoftReferencePins;

	/**
	 * Logic Driver can add support for improved UObject based UEdGraphPins, displaying a thumbnail and longer asset name.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Pins", meta = (InvalidEnumValues = "AllBlueprints"))
	ESMPinOverride OverrideObjectPins;

	/**
	 * If thumbnails should be displayed by default when there is no meta specifier present.
	 * Requires OverrideObjectPins enabled.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Pins", meta = (EditCondition = "OverrideObjectPins != ESMPinOverride::None"))
	bool bDisplayThumbnailsByDefault;
	
	/**
	 * Enable the preview mode as an available editor mode.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Preview")
	bool bEnablePreviewMode;

	// UObject
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// ~UObject
};
