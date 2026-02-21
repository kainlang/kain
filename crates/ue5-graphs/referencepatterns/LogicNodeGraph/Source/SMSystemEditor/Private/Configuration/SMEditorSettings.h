// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"

#include "SMEditorSettings.generated.h"

UENUM()
enum class ESMJumpToGraphBehavior : uint8
{
	/** The local or intermediate graph within the same blueprint. */
	PreferLocalGraph,
	/** An external blueprint's graph. */
	PreferExternalGraph
};

UENUM()
enum class ESMNodeInfoVisibility : uint8
{
	/** Never display information. */
	Never,
	/** Always display information. */
	Always,
	/** Only display information when hovered. */
	OnHover
};

UCLASS(config = EditorPerProjectUserSettings)
class SMSYSTEMEDITOR_API USMEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	USMEditorSettings();

	UPROPERTY(config, EditAnywhere, Category = "States")
	FLinearColor StateDefaultColor;

	/** When a state has internal logic. */
	UPROPERTY(config, EditAnywhere, Category = "States")
	FLinearColor StateWithLogicColor;

	/** When a node is an end state. */
	UPROPERTY(config, EditAnywhere, Category = "States")
	FLinearColor EndStateColor;

	UPROPERTY(config, EditAnywhere, Category = "States")
	FLinearColor StateMachineDefaultColor;

	UPROPERTY(config, EditAnywhere, Category = "States")
	FLinearColor StateMachineParentDefaultColor;
	
	/** When a state machine has states. */
	UPROPERTY(config, EditAnywhere, Category = "States")
	FLinearColor StateMachineWithLogicColor;

	/** The default color for Any States. */
	UPROPERTY(config, EditAnywhere, Category = "States")
	FLinearColor AnyStateDefaultColor;

	/** The maximum amount of Any State icons to display per state. */
	UPROPERTY(config, EditAnywhere, Category = "States", meta = (ClampMin = "0", ClampMax = "10"))
	int32 MaxAnyStateIcons;
	
	/** The buffer size which accepts pins to create transitions. Open editors will need to be refreshed. */
	UPROPERTY(config, EditAnywhere, Category = "States", meta = (ClampMin = "4.0", ClampMax = "15.0"))
	float StateConnectionSize;

	/** Displays the class name above state stack classes. */
	UPROPERTY(config, EditAnywhere, Category = "States")
	bool bDisplayStateStackClassNames;
	
	/** The padding around the main content box for state machines. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "States")
	FMargin StateMachineContentPadding;
	
	/** The padding around the main content box. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "States")
	FMargin StateContentPadding;
	
	/** Prevent nodes from displaying overlay widgets indicating a special status, such as for intermediate graphs or waiting for an end state. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "States")
	bool bDisableVisualCues;
	
	/** When a transition evaluator has no input. */
	UPROPERTY(config, EditAnywhere, Category = "Transitions")
	FLinearColor TransitionEmptyColor;

	/** When the transition evaluator has input. */
	UPROPERTY(config, EditAnywhere, Category = "Transitions")
	FLinearColor TransitionValidColor;

	/** When a transition is hovered. */
	UPROPERTY(config, EditAnywhere, Category = "Transitions")
	FLinearColor TransitionHoverColor;

	/** Allow transitions with On Transition Entered logic to have a special color. */
	UPROPERTY(config, EditAnywhere, Category = "Transitions")
	bool bEnableTransitionWithEntryLogicColor;
	
	/** When a transition has On Transition Entered logic. */
	UPROPERTY(config, EditAnywhere, Category = "Transitions", meta = (EditCondition = "bEnableTransitionWithEntryLogicColor"))
	FLinearColor TransitionWithEntryLogicColor;

	/** When a system generated default transition name should be displayed. */
	UPROPERTY(config, EditAnywhere, Category = "Transitions")
	ESMNodeInfoVisibility DisplayDefaultTransitionName;
	
	/** When a custom user-defined transition name should be displayed. */
	UPROPERTY(config, EditAnywhere, Category = "Transitions")
	ESMNodeInfoVisibility DisplayCustomTransitionName;
	
	/** Display the transition priority next to transition icons. */
	UPROPERTY(config, EditAnywhere, Category = "Transitions")
	bool bDisplayTransitionPriority;

	/**
	 * False: When a transition is rerouted the icon is hidden on the transition and displayed on the reroute node instead.
	 * True: When a transition is rerouted the icon is displayed on both the transition and the reroute node.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Transitions")
	bool bDisplayTransitionIconWhenRerouted;

	/**
	 * Automatically collapse exposed property categories on state nodes by default.
	 * Manually expanding or collapsing categories will continue to save to a config file as normal.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Properties")
	bool bCollapseCategoriesByDefault;

	/**
	 * Change the color of exposed property pins on state nodes. This modifies the default pin color. Set the
	 * alpha to 0 to turn off the color completely.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Properties")
	FLinearColor PropertyPinColorModifier;

	/** When a state is active during debug. */
	UPROPERTY(config, EditAnywhere, Category = "Debug")
	FLinearColor ActiveStateColor;

	/** When a transition is active during debug. */
	UPROPERTY(config, EditAnywhere, Category = "Debug")
	FLinearColor ActiveTransitionColor;
	
	/** Time in seconds before the last state starts to fade. */
	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ClampMin = "0.0"))
	float TimeToDisplayLastActiveState;

	/** The fade time in seconds to hide the last active state. */
	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ClampMin = "0.0"))
	float TimeToFadeLastActiveState;

	/** The fade time in seconds to hide the last active transition. */
	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ClampMin = "0.0"))
	float TimeToFadeLastActiveTransition;

	/** Transitions that are evaluating will be highlighted in the editor during debug. */
	UPROPERTY(config, EditAnywhere, Category = "Debug")
	bool bDisplayTransitionEvaluation;
	
	/** When a transition is evaluating during debug. */
	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (EditCondition = "bDisplayTransitionEvaluation"))
	FLinearColor EvaluatingTransitionColor;

	/**
	 * The behavior when double clicking on states.
	 * Either prefer the local graph or the node blueprint.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Node Double Click")
	ESMJumpToGraphBehavior StateDoubleClickBehavior;

	/**
	 * The behavior when double clicking on transitions.
	 * Either prefer the local graph or the node blueprint.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Node Double Click")
	ESMJumpToGraphBehavior TransitionDoubleClickBehavior;

	/**
	 * The behavior when double clicking on conduits.
	 * Either prefer the local graph or the node blueprint.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Node Double Click")
	ESMJumpToGraphBehavior ConduitDoubleClickBehavior;
	
	/**
	 * The behavior when double clicking on state machine references.
	 * The local graph is available when the intermediate graph is enabled.
	 *
	 * If a node class is assigned it will still have to be accessed through the context menu.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Node Double Click")
	ESMJumpToGraphBehavior ReferenceDoubleClickBehavior;
	
	/** Show quick animations such as when a state is placed on a graph. */
	UPROPERTY(config, EditAnywhere, Category = "Visual")
	bool bEnableAnimations;

	/** Show fast path icons on all applicable nodes. */
	UPROPERTY(config, EditAnywhere, Category = "Visual")
	bool bDisplayFastPath;
	
	/**
	 * Add all Kismet toolbar extenders to the state machine editor. This allows items from third party plugins to be visible in Logic Driver.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Plugin Compatibility")
	bool bEnableBlueprintToolbarExtenders;

	/**
	 * Add all Kismet menu extenders to the state machine editor. This allows items from third party plugins to be visible in Logic Driver.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Plugin Compatibility")
	bool bEnableBlueprintMenuExtenders;

	/**
	 * Display the Logic Driver utility launcher button in the level editor toolbar. Requires editor restart.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Utility Launcher")
	bool bEnableUtilityLauncherToolbar;
	
	/** The last position on the new asset dialog box. */
	UPROPERTY(config)
	int32 NewAssetIndex;
};
