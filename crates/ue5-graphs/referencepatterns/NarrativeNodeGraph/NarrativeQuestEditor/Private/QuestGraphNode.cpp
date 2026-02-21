// Copyright Narrative Tools 2022. 

#include "QuestGraphNode.h"
#include "QuestGraph.h"
#include "EdGraph/EdGraphSchema.h"

void UQuestGraphNode::OnEntered(class UQuestNode* UpdatedQuestNode, bool bActivated)
{
	//Empty, just a template for a custom K2_node
}

void UQuestGraphNode::PostPlacedNewNode()
{
	if (UQuestGraph* QuestGraph = Cast<UQuestGraph>(GetGraph()))
	{
		QuestGraph->NodeAdded(this);
	}
}

void UQuestGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	Super::AutowireNewNode(FromPin);


	if (FromPin != nullptr)
	{
		UEdGraphPin* OutputPin = GetOutputPin();

		if (GetSchema()->TryCreateConnection(FromPin, GetInputPin()))
		{
			FromPin->GetOwningNode()->NodeConnectionListChanged();
		}
		else if (OutputPin != nullptr && GetSchema()->TryCreateConnection(OutputPin, FromPin))
		{
			NodeConnectionListChanged();
		}
	}
}

void UQuestGraphNode::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (UQuestGraph* QuestGraph = Cast<UQuestGraph>(GetGraph()))
	{
		QuestGraph->PinRewired(this, Pin);
	}
}

UEdGraphPin* UQuestGraphNode::GetInputPin(int32 InputIndex /*= 0*/) const
{
	check(InputIndex >= 0);

	for (int32 PinIndex = 0, FoundInputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Input)
		{
			if (InputIndex == FoundInputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundInputs++;
			}
		}
	}

	return nullptr;
}

UEdGraphPin* UQuestGraphNode::GetOutputPin(int32 InputIndex /*= 0*/) const
{
	check(InputIndex >= 0);

	for (int32 PinIndex = 0, FoundInputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Output)
		{
			if (InputIndex == FoundInputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundInputs++;
			}
		}
	}

	return nullptr;
}
