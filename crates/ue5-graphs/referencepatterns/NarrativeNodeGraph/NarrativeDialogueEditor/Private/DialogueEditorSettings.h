// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DialogueEditorSettings.generated.h"

/**
 * 
 */
UCLASS(config=Engine, defaultconfig)
class NARRATIVEDIALOGUEEDITOR_API UDialogueEditorSettings : public UObject
{
	GENERATED_BODY()

public:

	UDialogueEditorSettings();

	UPROPERTY(EditAnywhere, config, Category = "Graph Style")
	FLinearColor RootNodeColor;

	UPROPERTY(EditAnywhere, config, Category = "Graph Style")
	FLinearColor PlayerNodeColor;

	UPROPERTY(EditAnywhere, config, Category = "Graph Style")
	FLinearColor NPCNodeColor;

	UPROPERTY(EditAnywhere, config, Category = "Graph Style")
	FLinearColor BacklinkWireColor;

	UPROPERTY(EditAnywhere, config, Category = "Graph Defaults", noclear, meta = (MetaClass="/Script/Narrative.DialogueNode_NPC"))
	FSoftClassPath DefaultNPCDialogueClass;

	UPROPERTY(EditAnywhere, config, Category = "Graph Defaults", noclear, meta = (MetaClass = "/Script/Narrative.DialogueNode_Player"))
	FSoftClassPath DefaultPlayerDialogueClass;

	UPROPERTY(EditAnywhere, config, Category = "Graph Defaults", noclear, meta = (MetaClass = "/Script/Narrative.Dialogue"))
	FSoftClassPath DefaultDialogueClass;

	UPROPERTY(EditAnywhere, config, Category = "Graph Defaults", noclear, meta = (MetaClass = "/Script/NarrativeDialogueEditor.DialogueNodeUserWidget"))
	TSoftClassPtr<class UDialogueNodeUserWidget> DefaultDialogueWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category = "Graph Options")
	bool bEnableWarnings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category = "Graph Options")
	bool bWarnMissingSoundCues;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category = "Graph Options")
	bool bWarnMissingAnims;

	
	/** The maximum value to clamp the absolute value of the horizontal distance between endpoints when calculating tangents (when the wire is moving forward) */
	UPROPERTY(config, EditAnywhere, Category=Splines, AdvancedDisplay)
	float ForwardSplineHorizontalDeltaRange;

	/** The maximum value to clamp the absolute value of the vertical distance between endpoints when calculating tangents (when the wire is moving forward) */
	UPROPERTY(config, EditAnywhere, Category=Splines, AdvancedDisplay)
	float ForwardSplineVerticalDeltaRange;

	/** The amount that the horizontal delta affects the generated tangent handle of splines (when the wire is moving forward) */
	UPROPERTY(config, EditAnywhere, Category=Splines, AdvancedDisplay)
	FVector2D ForwardSplineTangentFromHorizontalDelta;

	/** The amount that the vertical delta affects the generated tangent handle of splines (when the wire is moving forward) */
	UPROPERTY(config, EditAnywhere, Category=Splines, AdvancedDisplay)
	FVector2D ForwardSplineTangentFromVerticalDelta;

	/** The maximum value to clamp the absolute value of the horizontal distance between endpoints when calculating tangents (when the wire is moving backwards) */
	UPROPERTY(config, EditAnywhere, Category=Splines, AdvancedDisplay)
	float BackwardSplineHorizontalDeltaRange;

	/** The maximum value to clamp the absolute value of the vertical distance between endpoints when calculating tangents (when the wire is moving backwards) */
	UPROPERTY(config, EditAnywhere, Category=Splines, AdvancedDisplay)
	float BackwardSplineVerticalDeltaRange;

	/** The amount that the horizontal delta affects the generated tangent handle of splines (when the wire is moving backwards) */
	UPROPERTY(config, EditAnywhere, Category=Splines, AdvancedDisplay)
	FVector2D BackwardSplineTangentFromHorizontalDelta;

	/** The amount that the vertical delta affects the generated tangent handle of splines (when the wire is moving backwards) */
	UPROPERTY(config, EditAnywhere, Category=Splines, AdvancedDisplay)
	FVector2D BackwardSplineTangentFromVerticalDelta;

};
