// Copyright Narrative Tools 2022. 


#include "DialogueEditorSettings.h"
#include "UObject/ConstructorHelpers.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "DialogueNodeUserWidget.h"

UDialogueEditorSettings::UDialogueEditorSettings()
{
	RootNodeColor = FLinearColor(0.1f, 0.1f, 0.1f);
	PlayerNodeColor = FLinearColor(0.65f, 0.28f, 0.f);
	NPCNodeColor = FLinearColor(0.2f, 0.2f, 0.2f);
	BacklinkWireColor = FLinearColor(0.254970, 0.548547, 1.000000, 0.800000);

	DefaultNPCDialogueClass = UDialogueNode_NPC::StaticClass();
	DefaultPlayerDialogueClass = UDialogueNode_Player::StaticClass();
	DefaultDialogueClass = UDialogue::StaticClass();

	auto DialogueNodeUserWidgetFinder = ConstructorHelpers::FClassFinder<UDialogueNodeUserWidget>(TEXT("WidgetBlueprint'/Narrative/NarrativeUI/Widgets/Editor/WBP_DefaultDialogueNode.WBP_DefaultDialogueNode_C'"));
	if (DialogueNodeUserWidgetFinder.Succeeded())
	{
		DefaultDialogueWidgetClass = DialogueNodeUserWidgetFinder.Class;
	}

	bEnableWarnings = true;
	bWarnMissingSoundCues = true;

	ForwardSplineHorizontalDeltaRange = 1000.f;
	ForwardSplineVerticalDeltaRange = 1000.f;
	BackwardSplineHorizontalDeltaRange = 200.f;
	BackwardSplineVerticalDeltaRange = 200.f;

	ForwardSplineTangentFromHorizontalDelta = FVector2D(1.f, 0.f);
	ForwardSplineTangentFromVerticalDelta = FVector2D(1.f, 0.f);

	BackwardSplineTangentFromVerticalDelta = FVector2D(1.5f, 0.f);
	BackwardSplineTangentFromHorizontalDelta = FVector2D(2.f, 0.f);
}
