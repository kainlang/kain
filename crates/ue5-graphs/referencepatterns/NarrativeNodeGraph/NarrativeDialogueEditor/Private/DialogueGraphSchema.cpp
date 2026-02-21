// Copyright Narrative Tools 2022. 

#include "DialogueGraphSchema.h"
#include "DialogueGraphNode_Root.h"
#include "ScopedTransaction.h"
#include "DialogueEditorTypes.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EdGraphNode_Comment.h"
#include "DialogueBlueprint.h"
#include "DialogueConnectionDrawingPolicy.h"
#include "DialogueGraph.h"
#include "DialogueGraphNode_NPC.h"
#include "DialogueGraphNode_Player.h"
#include "DialogueGraphNode.h"
#include "DialogueGraphEditor.h"
#include "Dialogue.h"
#include "DialogueSM.h"

#define LOCTEXT_NAMESPACE "DialogueGraphSchema"

static int32 NodeDistance = 60;

UEdGraphNode* FDialogueSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UEdGraphNode* ResultNode = NULL;

	// If there is a template, we actually use it
	if (NodeTemplate != NULL)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddNode", "Add Node"));
		ParentGraph->Modify();
		if (FromPin)
		{
			FromPin->Modify();
		}

		NodeTemplate->SetFlags(RF_Transactional);

		// set outer to be the graph so it doesn't go away
		NodeTemplate->Rename(NULL, ParentGraph, REN_NonTransactional);
		ParentGraph->AddNode(NodeTemplate, true);

		NodeTemplate->CreateNewGuid();
		NodeTemplate->PostPlacedNewNode();

		// For input pins, new node will generally overlap node being dragged off
		// Work out if we want to visually push away from connected node
		int32 XLocation = Location.X;
		if (FromPin && FromPin->Direction == EGPD_Input)
		{
			UEdGraphNode* PinNode = FromPin->GetOwningNode();
			const float XDelta = FMath::Abs(PinNode->NodePosX - Location.X);

			if (XDelta < NodeDistance)
			{
				// Set location to edge of current node minus the max move distance
				// to force node to push off from connect node enough to give selection handle
				XLocation = PinNode->NodePosX - NodeDistance;
			}
		}

		NodeTemplate->NodePosX = XLocation;
		NodeTemplate->NodePosY = Location.Y;
		NodeTemplate->SnapToGrid(16);

		// setup pins after placing node in correct spot, since pin sorting will happen as soon as link connection change occurs
		NodeTemplate->AllocateDefaultPins();
		NodeTemplate->AutowireNewNode(FromPin);

		if (UDialogueGraphNode_NPC* NPCGraphNode = Cast<UDialogueGraphNode_NPC>(NodeTemplate))
		{
			if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(NPCGraphNode->DialogueNode))
			{
				NPCNode->SetSpeakerID(SpeakerInfo.GetSpeakerID());
			}
		}

		ResultNode = NodeTemplate;
	}

	return ResultNode;
}

UEdGraphNode* FDialogueSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode)
{
	UEdGraphNode* ResultNode = NULL;
	if (FromPins.Num() > 0)
	{
		ResultNode = PerformAction(ParentGraph, FromPins[0], Location);

		// Try autowiring the rest of the pins
		for (int32 Index = 1; Index < FromPins.Num(); ++Index)
		{
			ResultNode->AutowireNewNode(FromPins[Index]);
		}
	}
	else
	{
		ResultNode = PerformAction(ParentGraph, NULL, Location, bSelectNewNode);
	}

	return ResultNode;
}

void FDialogueSchemaAction_NewNode::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEdGraphSchemaAction::AddReferencedObjects(Collector);

	// These don't get saved to disk, but we want to make sure the objects don't get GC'd while the action array is around
	Collector.AddReferencedObject(NodeTemplate);
}

void UDialogueGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	if (UDialogueGraph* DialogueGraph = CastChecked<UDialogueGraph>(&Graph))
	{
		FGraphNodeCreator<UDialogueGraphNode_Root> RootNodeCreator(Graph);
		UDialogueGraphNode_Root* RootNode = RootNodeCreator.CreateNode();
		RootNodeCreator.Finalize();
		SetNodeMetaData(RootNode, FNodeMetadata::DefaultGraphNode);
	}
}

const FPinConnectionResponse UDialogueGraphSchema::CanCreateConnection(const UEdGraphPin* NodeA, const UEdGraphPin* NodeB) const
{
	if (!NodeA || !NodeB)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("NullError", "Nodes were null."));
	}

	bool bPinAIsNPC = NodeA->GetOwningNode()->IsA<UDialogueGraphNode_NPC>();
	bool bPinBIsNPC = NodeB->GetOwningNode()->IsA<UDialogueGraphNode_NPC>();

	if (NodeA == NodeB)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorSelfDisallow", "Cannot connect a node to itself."));
	}

	if (!bPinAIsNPC && !bPinBIsNPC)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorPlayerDisallow", "Cannot connect player response to another player response."));
	}

	if (NodeA->Direction != NodeB->Direction)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("PinConnect", "Connect nodes"));
	}
	else
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("InputsMatching", "Can't connect an input to an input, or an output to an output."));
	}

}

class FConnectionDrawingPolicy* UDialogueGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	return new FDialogueGraphConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

void UDialogueGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const FName PinCategory = ContextMenuBuilder.FromPin ?
		ContextMenuBuilder.FromPin->PinType.PinCategory :
		UDialogueEditorTypes::PinCategory_SingleNode;

	const bool bNoParent = (ContextMenuBuilder.FromPin == NULL);
	const bool bIsNPC = ContextMenuBuilder.FromPin ? ContextMenuBuilder.FromPin->GetOwningNode()->IsA<UDialogueGraphNode_NPC>() : false;

	FCategorizedGraphActionListBuilder ActionBuilder(TEXT("Next we should..."));

	//Add the NPC context options
	if (ContextMenuBuilder.CurrentGraph)
	{
		//Add an option for each NPC speaker
		if (UDialogueBlueprint* const DialogueBlueprint = Cast<UDialogueBlueprint>(ContextMenuBuilder.CurrentGraph->GetOuter()))
		{
			if (DialogueBlueprint->GeneratedClass)
			{
				if (UDialogue* DialogueCDO = Cast<UDialogue>(DialogueBlueprint->GeneratedClass->GetDefaultObject()))
				{
					for (auto& Speaker : DialogueCDO->Speakers)
					{
						FString OptionString = FString::Printf(TEXT("Add dialogue line for %s."), *Speaker.SpeakerID.ToString());

						TSharedPtr<FDialogueSchemaAction_NewNode> AddNPC = UDialogueGraphSchema::AddNewNodeAction(ActionBuilder, FText::GetEmpty(), FText::FromString(OptionString), FText::GetEmpty());
						UDialogueGraphNode* NodeNPC = NewObject<UDialogueGraphNode>(ContextMenuBuilder.OwnerOfTemporaries, UDialogueGraphNode_NPC::StaticClass());
						AddNPC->NodeTemplate = NodeNPC;
						AddNPC->SpeakerInfo = Speaker;
						ContextMenuBuilder.Append(ActionBuilder);
					}
				}
			}
		}
	}

	//Player responses can only be after an NPC response
	if (bIsNPC)
	{
		TSharedPtr<FDialogueSchemaAction_NewNode> AddPlayer = UDialogueGraphSchema::AddNewNodeAction(ActionBuilder, FText::GetEmpty(), FText::FromString("Add player response..."), FText::GetEmpty());
		UDialogueGraphNode* NodeP = NewObject<UDialogueGraphNode>(ContextMenuBuilder.OwnerOfTemporaries, UDialogueGraphNode_Player::StaticClass());
		AddPlayer->NodeTemplate = NodeP;

		ContextMenuBuilder.Append(ActionBuilder);
	}

	if (bNoParent)
	{
		FCategorizedGraphActionListBuilder CommentActionBuilder(TEXT("Comments"));
		TSharedPtr<FDialogueSchemaAction_AddComment> AddComment = UDialogueGraphSchema::AddCommentAction(ContextMenuBuilder, FText::FromString("Add Comment..."), FText::GetEmpty());
		ContextMenuBuilder.Append(CommentActionBuilder);
	}

	UEdGraphSchema::GetGraphContextActions(ContextMenuBuilder);
}


//
//void UDialogueGraphSchema::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const
//{
//	//Allow breaking links with context menu
//	if (InGraphPin)
//	{
//		MenuBuilder->BeginSection("DialogueGraphSchemaPinActions", LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
//
//		if (InGraphPin->LinkedTo.Num() > 0)
//		{
//			MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().BreakPinLinks);
//		}
//
//		MenuBuilder->EndSection();
//	}
//	else if (InGraphNode)
//	{
//		MenuBuilder->BeginSection("DialogueGraphSchemaNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
//		MenuBuilder->AddMenuEntry(FGenericCommands::Get().Delete);
//		MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
//		MenuBuilder->EndSection();
//	}
//	Super::GetContextMenuActions(CurrentGraph, InGraphNode, InGraphPin, MenuBuilder, bIsDebugging);
//}
//
//void UDialogueGraphSchema::GetContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
//{
//	//Allow breaking links with context menu
//	if (Context)
//	{
//	//	if (Context->Pin)
//	//	{
//	//		Menu->AddSection(FName("DialogueGraphSchemaPinActions"), LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
//
//	//		if (InGraphPin->LinkedTo.Num() > 0)
//	//		{
//	//			MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().BreakPinLinks);
//	//		}
//
//	//		MenuBuilder->EndSection();
//	//	}
//	//	else if (InGraphNode)
//	//	{
//	//		MenuBuilder->BeginSection("DialogueGraphSchemaNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
//	//		MenuBuilder->AddMenuEntry(FGenericCommands::Get().Delete);
//	//		MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
//	//		MenuBuilder->EndSection();
//	//	}
//	}
//	Super::GetContextMenuActions(Menu, Context);
//}

FLinearColor UDialogueGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor::White;
}

TSharedPtr<FEdGraphSchemaAction> UDialogueGraphSchema::GetCreateCommentAction() const
{
	return TSharedPtr<FEdGraphSchemaAction>(static_cast<FEdGraphSchemaAction*>(new FDialogueSchemaAction_AddComment));
}


void UDialogueGraphSchema::SetNodePosition(UEdGraphNode* Node, const FVector2D& Position) const
{
	Super::SetNodePosition(Node, Position);

	if (UDialogueGraphNode* DNode = Cast<UDialogueGraphNode>(Node))
	{
		if (DNode->DialogueNode)
		{
			DNode->DialogueNode->NodePos = Position;
		}
	}
}


EGraphType UDialogueGraphSchema::GetGraphType(const UEdGraph* TestEdGraph) const
{	
	//Just return MAX to change the default icon - TODO this probably should be changed but changing icons isn't well exposed by the editor 
	return GT_MAX;
}

TSharedPtr<FDialogueSchemaAction_NewNode> UDialogueGraphSchema::AddNewNodeAction(FGraphActionListBuilderBase& ContextMenuBuilder, const FText& Category, const FText& MenuDesc, const FText& Tooltip)
{
	TSharedPtr<FDialogueSchemaAction_NewNode> NewAction = TSharedPtr<FDialogueSchemaAction_NewNode>(new FDialogueSchemaAction_NewNode(Category, MenuDesc, Tooltip, 0));
	ContextMenuBuilder.AddAction(NewAction);

	return NewAction;
}

TSharedPtr<FDialogueSchemaAction_AddComment> UDialogueGraphSchema::AddCommentAction(FGraphActionListBuilderBase& ContextMenuBuilder, const FText& MenuDesc, const FText& Tooltip)
{
	TSharedPtr<FDialogueSchemaAction_AddComment> NewComment = TSharedPtr<FDialogueSchemaAction_AddComment>(new FDialogueSchemaAction_AddComment(MenuDesc, Tooltip));
	ContextMenuBuilder.AddAction(NewComment);
	return NewComment;
}

UEdGraphNode* FDialogueSchemaAction_AddComment::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode /*= true*/)
{
	UEdGraphNode_Comment* const CommentTemplate = NewObject<UEdGraphNode_Comment>();

	FVector2D SpawnLocation = Location;

	FDialogueGraphEditor* DialogueEditor = nullptr;
	if (UDialogueBlueprint* const DialogueBlueprint = Cast<UDialogueBlueprint>(ParentGraph->GetOuter()))
	{
		if (UAssetEditorSubsystem* AESubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			DialogueEditor = static_cast<FDialogueGraphEditor*>(AESubsystem->FindEditorForAsset(DialogueBlueprint, false));
		}
	}

	FSlateRect Bounds;
	if (DialogueEditor && DialogueEditor->Dialogue_GetBoundsForSelectedNodes(Bounds, 50.0f))
	{
		CommentTemplate->SetBounds(Bounds);
		SpawnLocation.X = CommentTemplate->NodePosX;
		SpawnLocation.Y = CommentTemplate->NodePosY;
	}

	UEdGraphNode* const NewNode = FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(ParentGraph, CommentTemplate, SpawnLocation, bSelectNewNode);

	return NewNode;
}

#undef LOCTEXT_NAMESPACE
