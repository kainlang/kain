// Copyright Narrative Tools 2022. 

#include "QuestConnectionDrawingPolicy.h"
#include "QuestGraphNode_Action.h"
#include "QuestSM.h"

void FQuestGraphConnectionDrawingPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params)
{
	Params.WireColor = FLinearColor::White;
	Params.WireThickness = 2.f;

	UQuestNode* QuestBranch = nullptr;

	if (InputPin && OutputPin)
	{
		if (UQuestGraphNode_Action* OutActionNode = Cast<UQuestGraphNode_Action>(OutputPin->GetOwningNode()))
		{
			QuestBranch = Cast<UQuestNode>(OutActionNode->QuestNode);
		}
		else if (UQuestGraphNode_Action* InActionNode = Cast<UQuestGraphNode_Action>(InputPin->GetOwningNode()))
		{
			QuestBranch = Cast<UQuestNode>(InActionNode->QuestNode);
		}
	}

	if (QuestBranch)
	{
		//FTimespan Span = FDateTime::Now() - QuestBranch->LastExecTime;

		//float TimeSinceExec = Span.GetTotalMilliseconds();

		//if (TimeSinceExec <= 1000.f)
		//{
		//	const float Delta = FMath::Abs(1.f - FMath::Abs(TimeSinceExec - 500.f) / 500.f);

		//	Params.WireColor = FMath::Lerp<FLinearColor>(FLinearColor::White, TransitionColor, Delta);
		//	Params.WireThickness = FMath::Lerp<float>(2.f, 5.f, Delta);
		//	Params.bDrawBubbles = true;
		//}
	}
}
