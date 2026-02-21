// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMEditorSettings.h"

USMEditorSettings::USMEditorSettings()
{
	StateDefaultColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.f);
	EndStateColor = FLinearColor::Red;
	StateWithLogicColor = FLinearColor::Green;

	StateMachineDefaultColor = FLinearColor(0.4f, 0.4f, 0.4f, 1.f);
	StateMachineWithLogicColor = FLinearColor(0.3f, 0.7f, 0.8f);
	StateMachineParentDefaultColor = FLinearColor(1.f, 0.2f, 0.f, 1.f);
	AnyStateDefaultColor = FLinearColor(0.36f, 0.1f, 0.68f, 1.f);
	MaxAnyStateIcons = 3;
	StateConnectionSize = 8;

	bDisplayStateStackClassNames = true;

	bDisableVisualCues = false;
	
	StateMachineContentPadding = FMargin(4.f, 2.f, 4.f, 2.f);
	StateContentPadding = FMargin(4.f, 0.f, 4.f, 0.f);
	
	TransitionEmptyColor = FLinearColor(0.5f, 0.5f, 0.5f, 0.5f);
	TransitionValidColor = FLinearColor(1.f, 1.f, 1.f, 1.f);
	TransitionHoverColor = FLinearColor(0.724f, 0.256f, 0.0f, 1.0f);
	bEnableTransitionWithEntryLogicColor = false;
	TransitionWithEntryLogicColor = FLinearColor(0.2f, 0.8f, 1.f, 1.f);

	DisplayDefaultTransitionName = ESMNodeInfoVisibility::Never;
	DisplayCustomTransitionName = ESMNodeInfoVisibility::OnHover;
	
	bDisplayTransitionPriority = true;
	bDisplayTransitionIconWhenRerouted = false;

	bCollapseCategoriesByDefault = false;
	PropertyPinColorModifier = FLinearColor(1.f, 1.f, 1.f, 0.35f);
	
	ActiveStateColor = FLinearColor(1.f, 0.6f, 0.35f);
	ActiveTransitionColor = FLinearColor::Red;
	
	TimeToDisplayLastActiveState = 2.f;
	TimeToFadeLastActiveState = 0.25f;
	TimeToFadeLastActiveTransition = 0.7f;

	bDisplayTransitionEvaluation = true;
	EvaluatingTransitionColor = FLinearColor(0.92f, .2f, .92f);

	bEnableAnimations = true;
	bDisplayFastPath = true;

	StateDoubleClickBehavior = ESMJumpToGraphBehavior::PreferLocalGraph;
	TransitionDoubleClickBehavior = ESMJumpToGraphBehavior::PreferLocalGraph;
	ConduitDoubleClickBehavior = ESMJumpToGraphBehavior::PreferLocalGraph;
	ReferenceDoubleClickBehavior = ESMJumpToGraphBehavior::PreferExternalGraph;
	
	bEnableBlueprintMenuExtenders = true;
	bEnableBlueprintToolbarExtenders = true;

	bEnableUtilityLauncherToolbar = true;
}
