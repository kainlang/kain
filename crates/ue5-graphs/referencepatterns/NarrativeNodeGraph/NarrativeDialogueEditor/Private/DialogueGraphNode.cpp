// Copyright Narrative Tools 2022. 

#include "DialogueGraphNode.h"
#include "DialogueGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "DialogueGraphSchema.h"

FText UDialogueGraphNode::GetNodeText() const
{
	if (DialogueNode)
	{
		return DialogueNode->Line.Text;
	}

	return FText::GetEmpty();
}

void UDialogueGraphNode::PostPlacedNewNode()
{
	if (UDialogueGraph* DialogueGraph = Cast<UDialogueGraph>(GetGraph()))
	{
		DialogueGraph->NodeAdded(this);
	}
}

void UDialogueGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
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

void UDialogueGraphNode::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (UDialogueGraph* DialogueGraph = Cast<UDialogueGraph>(GetGraph()))
	{
		DialogueGraph->PinRewired(this, Pin);
	}
}


bool UDialogueGraphNode::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const
{
	return DesiredSchema->GetClass()->IsChildOf(UDialogueGraphSchema::StaticClass());
}

UEdGraphPin* UDialogueGraphNode::GetInputPin(int32 InputIndex /*= 0*/) const
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

UEdGraphPin* UDialogueGraphNode::GetOutputPin(int32 InputIndex /*= 0*/) const
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

void UDialogueGraphNode::OnStartedOrFinished(UDialogueNode* Node, bool bStarted)
{

}
