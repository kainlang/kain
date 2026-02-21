//  Copyright Narrative Tools 2022.


#include "DialogueNodeUserWidget.h"
#include <Components/VerticalBox.h>
#include <Components/Overlay.h>
#include <Components/VerticalBoxSlot.h>
#include <Blueprint/WidgetTree.h>


void UDialogueNodeUserWidget::InitializeFromNode(class UDialogueNode* InNode, class UDialogue* InDialogue)
{
	if (InNode)
	{
		Node = InNode;
		Dialogue = InDialogue;

		OnNodeInitialized(InNode, InDialogue);
	}
}
