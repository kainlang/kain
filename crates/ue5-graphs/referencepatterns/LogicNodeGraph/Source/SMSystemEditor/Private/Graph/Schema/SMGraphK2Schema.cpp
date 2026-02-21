// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Schema/SMGraphK2Schema.h"

#include "Blueprints/SMBlueprintEditor.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_RootNode.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateMachineSelectNode.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/GenericCommands.h"
#include "K2Node_Composite.h"
#include "K2Node_Select.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_Switch.h"
#include "K2Node_ActorBoundEvent.h"
#include "K2Node_Variable.h"
#include "K2Node_SetFieldsInStruct.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "GraphEditorActions.h"
#include "BlueprintEditorSettings.h"
#include "ToolMenusEditor.h"

#define LOCTEXT_NAMESPACE "SMGraphK2Schema"

const FName USMGraphK2Schema::PC_StateMachine(TEXT("statemachine"));
const FName USMGraphK2Schema::GN_StateMachineDefinitionGraph(TEXT("StateMachineGraph"));

USMGraphK2Schema::USMGraphK2Schema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Schema::GetContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	// We don't want to stop pins from working normally. This will disable Promote To Variable which is very useful.
	if (Context->Pin != nullptr)
	{
		Super::GetContextMenuActions(Menu, Context);
		return;
	}

	/*
	 * Root nodes are configured not to be collapsed in any way. However collapsing to a sub-graph (Collapse Nodes) can't be
	 * prevented by overriding CanCollapseNode of the schema. It fails due to UE4 only checking the default schema.
	 * This plugin has handling to ensure deleting collapsed graphs will not delete the root node, but the next problem is
	 * that you can right click on the collapsed graph and then choose to collapse or promote that graph to a function or macro,
	 * at which point you can delete said function or macro deleting the root node with it. So what we are doing here is
	 * checking to see if any collapsed graph is selected and if the collapsed graph has any root nodes. If so we are
	 * constructing our own context menu preventing collapsing this selection into a macro or function.
	 *
	 * We could prevent Collapse Nodes from showing up here, but since there is already handling for it let's leave it
	 * as an option unless problems are discovered.
	 *
	 * TODO: Collapsed Graph Revamp
	 */

	const UEdGraph* CurrentGraph = Context->Graph;
	const UEdGraphNode* InGraphNode = Context->Node;
	const UEdGraphPin* InGraphPin = Context->Pin;
	const bool bIsDebugging = Context->bIsDebugging;

	check(CurrentGraph);
	UBlueprint* OwnerBlueprint = FSMBlueprintEditorUtils::FindBlueprintForGraphChecked(CurrentGraph);
	FSMBlueprintEditor* Editor = FSMBlueprintEditorUtils::GetStateMachineEditor(OwnerBlueprint);
	if (!Editor)
	{
		Super::GetContextMenuActions(Menu, Context);
		return;
	}

	// Just functions / macros
	bool bRestrictCollapseToFunction = false;
	// All collapsing
	bool bRestrictCollapse = false;

	// For each object currently selected.
	TSet<UObject*> SelectedNodes = Editor->GetSelectedNodes();
	if (SelectedNodes.Num() == 0)
	{
		// No other selection.
		SelectedNodes.Add((UObject*)(InGraphNode));
	}

	for (UObject* SelectedObject : SelectedNodes)
	{
		if (bRestrictCollapseToFunction && bRestrictCollapse)
		{
			break;
		}

		// If this is a collapsed graph node.
		if (const UK2Node_Composite* CompositeNode = Cast<UK2Node_Composite>(SelectedObject))
		{
			/* Can happen on delete undo errors. */
			if (!CompositeNode->BoundGraph)
			{
				continue;
			}

			// Find any root node within any nested graph of this collapsed node.
			TArray<USMGraphK2Node_Base*> Nodes;
			FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_Base>(CompositeNode->BoundGraph, Nodes);

			if (Nodes.Num())
			{
				for (USMGraphK2Node_Base* Node : Nodes)
				{
					if (!Node->CanCollapseNode())
					{
						bRestrictCollapse = true;
						bRestrictCollapseToFunction = true;
						break;
					}
					if (!Node->CanCollapseToFunctionOrMacro())
					{
						bRestrictCollapseToFunction = true;
					}
				}
			}
		}
		// This is itself a root node.
		else if (const USMGraphK2Node_Base* Node = Cast<USMGraphK2Node_Base>(SelectedObject))
		{
			if (!Node->CanCollapseNode())
			{
				bRestrictCollapse = true;
				bRestrictCollapseToFunction = true;
				break;
			}
			if (!Node->CanCollapseToFunctionOrMacro())
			{
				bRestrictCollapseToFunction = true;
			}
		}
	}

	// Safe to perform all context actions.
	if (!bRestrictCollapseToFunction && !bRestrictCollapse)
	{
		Super::GetContextMenuActions(Menu, Context);
		return;
	}

	// Not safe, make sure we cannot collapse to function or macro.
	// TODO: See if this is still necessary. It would be nice to remove this all together since it's just a recreation of EdGraphSchema_K2 minus collapse to function / macro. New menu tools might allow to remove sections?
	if (InGraphPin != nullptr)
	{
		FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaPinActions", LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
		{
			if (!bIsDebugging)
			{
				// Break pin links
				if (InGraphPin->LinkedTo.Num() > 1)
				{
					Section.AddMenuEntry(FGraphEditorCommands::Get().BreakPinLinks);
				}

				// Add the change pin type action, if this is a select node
				if (InGraphNode->IsA(UK2Node_Select::StaticClass()))
				{
					Section.AddMenuEntry(FGraphEditorCommands::Get().ChangePinType);
				}

				// add sub menu for break link to
				if (InGraphPin->LinkedTo.Num() > 0)
				{
					Section.AddMenuEntry(NAME_None,
						InGraphPin->Direction == EEdGraphPinDirection::EGPD_Input ? LOCTEXT("SelectAllInputNodes", "Select All Input Nodes") : LOCTEXT("SelectAllOutputNodes", "Select All Output Nodes"),
						InGraphPin->Direction == EEdGraphPinDirection::EGPD_Input ? LOCTEXT("SelectAllInputNodesTooltip", "Adds all input Nodes linked to this Pin to selection") : LOCTEXT("SelectAllOutputNodesTooltip", "Adds all output Nodes linked to this Pin to selection"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateUObject((UEdGraphSchema_K2*const)this, &UEdGraphSchema_K2::SelectAllNodesInDirection, InGraphPin->Direction, const_cast<UEdGraph*>(CurrentGraph), const_cast<UEdGraphPin*>(InGraphPin))));

					if (InGraphPin->LinkedTo.Num() > 1)
					{
						Section.AddSubMenu(
							"BreakLinkTo",
							LOCTEXT("BreakLinkTo", "Break Link To..."),
							LOCTEXT("BreakSpecificLinks", "Break a specific link..."),
							FNewToolMenuDelegate::CreateUObject((USMGraphK2Schema*const)this, &USMGraphK2Schema::GetBreakLinkToSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));

						Section.AddSubMenu(
							"JumpToConnection",
							LOCTEXT("JumpToConnection", "Jump to Connection..."),
							LOCTEXT("JumpToSpecificConnection", "Jump to specific connection..."),
							FNewToolMenuDelegate::CreateUObject((USMGraphK2Schema*const)this, &USMGraphK2Schema::GetJumpToConnectionSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));

						Section.AddSubMenu(
							"StraightenConnection",
							LOCTEXT("StraightenConnection", "Straighten Connection To..."),
							LOCTEXT("StraightenConnection_Tip", "Straighten a specific connection"),
							FNewToolMenuDelegate::CreateUObject(this, &USMGraphK2Schema::GetStraightenConnectionToSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));
					}
					else
					{
						((USMGraphK2Schema*const)this)->GetBreakLinkToSubMenuActions(Menu, const_cast<UEdGraphPin*>(InGraphPin));
						((USMGraphK2Schema*const)this)->GetJumpToConnectionSubMenuActions(Menu, const_cast<UEdGraphPin*>(InGraphPin));

						UEdGraphPin* Pin = InGraphPin->LinkedTo[0];
						FText PinName = Pin->GetDisplayName();
						FText NodeName = Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView);

						Section.AddMenuEntry(
							FGraphEditorCommands::Get().StraightenConnections,
							FText::Format(LOCTEXT("StraightenDescription_SinglePin", "Straighten Connection to {0} ({1})"), NodeName, PinName),
							FText::Format(LOCTEXT("StraightenDescription_SinglePin_Node_Tip", "Straighten the connection between this pin, and {0} ({1})"), NodeName, PinName),
							FSlateIcon(NAME_None, NAME_None, NAME_None)
						);
					}
				}
			}
		}
	}
	else if (InGraphNode != nullptr)
	{
		FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
		if (!bIsDebugging)
		{
			// Replaceable node display option
			AddSelectedReplaceableNodes(Section, OwnerBlueprint, InGraphNode);

			// Node contextual actions
			Section.AddMenuEntry(FGenericCommands::Get().Delete);
			Section.AddMenuEntry(FGenericCommands::Get().Cut);
			Section.AddMenuEntry(FGenericCommands::Get().Copy);
			Section.AddMenuEntry(FGenericCommands::Get().Duplicate);
			Section.AddMenuEntry(FGraphEditorCommands::Get().ReconstructNodes);
			Section.AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);

			// Conditionally add the action to add an execution pin, if this is an execution node
			if (InGraphNode->IsA(UK2Node_ExecutionSequence::StaticClass()) || InGraphNode->IsA(UK2Node_Switch::StaticClass()))
			{
				Section.AddMenuEntry(FGraphEditorCommands::Get().AddExecutionPin);
			}

			// Conditionally add the action to create a super function call node, if this is an event or function entry
			if (InGraphNode->IsA(UK2Node_Event::StaticClass()) || InGraphNode->IsA(UK2Node_FunctionEntry::StaticClass()))
			{
				Section.AddMenuEntry(FGraphEditorCommands::Get().AddParentNode);
			}

			// Conditionally add the actions to add or remove an option pin, if this is a select node
			if (InGraphNode->IsA(UK2Node_Select::StaticClass()))
			{
				Section.AddMenuEntry(FGraphEditorCommands::Get().AddOptionPin);
				Section.AddMenuEntry(FGraphEditorCommands::Get().RemoveOptionPin);
			}

			// Don't show the "Assign selected Actor" option if more than one actor is selected
			if (InGraphNode->IsA(UK2Node_ActorBoundEvent::StaticClass()) && GEditor->GetSelectedActorCount() == 1)
			{
				Section.AddMenuEntry(FGraphEditorCommands::Get().AssignReferencedActor);
			}
		}

		// If the node has an associated definition (for some loose sense of the word), allow going to it (same action as double-clicking on a node)
		if (InGraphNode->CanJumpToDefinition())
		{
			Section.AddMenuEntry(FGraphEditorCommands::Get().GoToDefinition);
		}

		// show search for references for everyone
		Section.AddMenuEntry(FGraphEditorCommands::Get().FindReferences);

		if (!bIsDebugging)
		{
			if (InGraphNode->IsA(UK2Node_Variable::StaticClass()))
			{
				GetReplaceVariableMenu(Section, InGraphNode, OwnerBlueprint, true);
			}

			if (InGraphNode->IsA(UK2Node_SetFieldsInStruct::StaticClass()))
			{
				Section.AddMenuEntry(FGraphEditorCommands::Get().RestoreAllStructVarPins);
			}

			Section.AddMenuEntry(FGenericCommands::Get().Rename, LOCTEXT("Rename", "Rename"), LOCTEXT("Rename_Tooltip", "Renames selected function or variable in blueprint."));
		}

		// Select referenced actors in the level
		Section.AddMenuEntry(FGraphEditorCommands::Get().SelectReferenceInLevel);
	}

	if (!bIsDebugging && InGraphNode != nullptr)
	{
		// Collapse/expand nodes
		{
			FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaOrganization", LOCTEXT("OrganizationHeader", "Organization"));
			Section.AddMenuEntry(FGraphEditorCommands::Get().CollapseNodes);
			Section.AddMenuEntry(FGraphEditorCommands::Get().ExpandNodes);

			if (InGraphNode->IsA(UK2Node_FunctionEntry::StaticClass()))
			{
				Section.AddMenuEntry(FGraphEditorCommands::Get().ConvertFunctionToEvent);
			}

			if (InGraphNode->IsA(UK2Node_Event::StaticClass()))
			{
				Section.AddMenuEntry(FGraphEditorCommands::Get().ConvertEventToFunction);
			}

			if (InGraphNode->IsA(UK2Node_Composite::StaticClass()))
			{
				Section.AddMenuEntry(FGraphEditorCommands::Get().PromoteSelectionToFunction);
				Section.AddMenuEntry(FGraphEditorCommands::Get().PromoteSelectionToMacro);
			}

			Section.AddSubMenu("Alignment", LOCTEXT("AlignmentHeader", "Alignment"), FText(), FNewToolMenuDelegate::CreateLambda([](UToolMenu* AlignmentMenu)
			{
				{
					FToolMenuSection& InSection = AlignmentMenu->AddSection("EdGraphSchemaAlignment", LOCTEXT("AlignHeader", "Align"));
					InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesTop);
					InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesMiddle);
					InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesBottom);
					InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesLeft);
					InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesCenter);
					InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesRight);
					InSection.AddMenuEntry(FGraphEditorCommands::Get().StraightenConnections);
				}

				{
					FToolMenuSection& InSection = AlignmentMenu->AddSection("EdGraphSchemaDistribution", LOCTEXT("DistributionHeader", "Distribution"));
					InSection.AddMenuEntry(FGraphEditorCommands::Get().DistributeNodesHorizontally);
					InSection.AddMenuEntry(FGraphEditorCommands::Get().DistributeNodesVertically);
				}
			}));
		}
	}

	if (const UK2Node* K2Node = Cast<const UK2Node>(InGraphNode))
	{
		if (!K2Node->IsNodePure())
		{
			if (!bIsDebugging && GetDefault<UBlueprintEditorSettings>()->bAllowExplicitImpureNodeDisabling)
			{
				// Don't expose the enabled state for disabled nodes that were not explicitly disabled by the user
				if (!K2Node->IsAutomaticallyPlacedGhostNode())
				{
					// Add compile options
					{
						FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaCompileOptions", LOCTEXT("CompileOptionsHeader", "Compile Options"));
						Section.AddMenuEntry(
							FGraphEditorCommands::Get().DisableNodes,
							LOCTEXT("DisableCompile", "Disable (Do Not Compile)"),
							LOCTEXT("DisableCompileToolTip", "Selected node(s) will not be compiled."));

						{
							const FUIAction* SubMenuUIAction = Menu->Context.GetActionForCommand(FGraphEditorCommands::Get().EnableNodes);
							if (ensure(SubMenuUIAction))
							{
								Section.AddSubMenu(
									"EnableCompileSubMenu",
									LOCTEXT("EnableCompileSubMenu", "Enable Compile"),
									LOCTEXT("EnableCompileSubMenuToolTip", "Options to enable selected node(s) for compile."),
									FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
								{
									FToolMenuSection& SubMenuSection = SubMenu->AddSection("Section");
									SubMenuSection.AddMenuEntry(
										FGraphEditorCommands::Get().EnableNodes_Always,
										LOCTEXT("EnableCompileAlways", "Always"),
										LOCTEXT("EnableCompileAlwaysToolTip", "Always compile selected node(s)."));
									SubMenuSection.AddMenuEntry(
										FGraphEditorCommands::Get().EnableNodes_DevelopmentOnly,
										LOCTEXT("EnableCompileDevelopmentOnly", "Development Only"),
										LOCTEXT("EnableCompileDevelopmentOnlyToolTip", "Compile selected node(s) for development only."));
								}),
									*SubMenuUIAction,
									FGraphEditorCommands::Get().EnableNodes->GetUserInterfaceType());
							}
						}
					}
				}
			}

			// Add breakpoint actions
			{
				FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaBreakpoints", LOCTEXT("BreakpointsHeader", "Breakpoints"));
				Section.AddMenuEntry(FGraphEditorCommands::Get().ToggleBreakpoint);
				Section.AddMenuEntry(FGraphEditorCommands::Get().AddBreakpoint);
				Section.AddMenuEntry(FGraphEditorCommands::Get().RemoveBreakpoint);
				Section.AddMenuEntry(FGraphEditorCommands::Get().EnableBreakpoint);
				Section.AddMenuEntry(FGraphEditorCommands::Get().DisableBreakpoint);
			}
		}
	}

	{
		FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaDocumentation", LOCTEXT("DocumentationHeader", "Documentation"));
		Section.AddMenuEntry(FGraphEditorCommands::Get().GoToDocumentation);
	}
}

void USMGraphK2Schema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	// Create the default state machine node.
	FGraphNodeCreator<USMGraphK2Node_StateMachineNode> StateMachineNodeCreator(Graph);
	USMGraphK2Node_StateMachineNode* StateMachineNode = StateMachineNodeCreator.CreateNode();
	StateMachineNodeCreator.Finalize();
	SetNodeMetaData(StateMachineNode, FNodeMetadata::DefaultGraphNode);

	// The select node.
	FGraphNodeCreator<USMGraphK2Node_StateMachineSelectNode> SelectNodeCreator(Graph);
	USMGraphK2Node_StateMachineSelectNode* StateMachineSelectNode = SelectNodeCreator.CreateNode();
	SelectNodeCreator.Finalize();
	SetNodeMetaData(StateMachineSelectNode, FNodeMetadata::DefaultGraphNode);
	StateMachineSelectNode->NodePosX = 400;

	// Wire the connection.
	StateMachineNode->GetOutputPin()->MakeLinkTo(StateMachineSelectNode->GetInputPin());
}

void USMGraphK2Schema::HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const
{
	// This is a nested collapsed graph. TODO: Collapsed Graph Revamp
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(&GraphBeingRemoved))
	{
		if (UK2Node_Composite* Composite = Cast<UK2Node_Composite>(GraphBeingRemoved.GetOuter()))
		{
			TArray<USMGraphK2Node_RootNode*> RootNodes;
			GraphBeingRemoved.GetNodesOfClass<USMGraphK2Node_RootNode>(RootNodes);

			// If a root node is being deleted we want to move it up a level.
			for (USMGraphK2Node_RootNode* RootNode : RootNodes)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, RootNode, true);
				RootNode->Rename(nullptr, Composite->GetGraph());
				Composite->GetGraph()->AddNode(RootNode, false, false);
				RootNode->NodePosX = Composite->NodePosX;
				RootNode->NodePosY = Composite->NodePosY;
			}
		}

		if (FSMBlueprintEditor* Editor = FSMBlueprintEditorUtils::GetStateMachineEditor(&GraphBeingRemoved))
		{
			// 4.21 has issues closing tabs on deleted nodes and we're adding handling to get around this.
			Editor->CloseInvalidTabs();
		}
	}

	Super::HandleGraphBeingDeleted(GraphBeingRemoved);
}

const FPinConnectionResponse USMGraphK2Schema::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	const bool bNodeAIsSelect = PinA->GetOwningNode()->IsA(USMGraphK2Node_StateMachineSelectNode::StaticClass());
	const bool bNodeBIsSelect = PinB->GetOwningNode()->IsA(USMGraphK2Node_StateMachineSelectNode::StaticClass());

	const bool bNodeAIsStateMachine = PinA->GetOwningNode()->IsA(USMGraphK2Node_StateMachineNode::StaticClass());
	const bool bNodeBIsStateMachine = PinB->GetOwningNode()->IsA(USMGraphK2Node_StateMachineNode::StaticClass());

	if (bNodeAIsSelect || bNodeBIsSelect || bNodeAIsStateMachine || bNodeBIsStateMachine)
	{
		if (bNodeAIsSelect && bNodeBIsStateMachine)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, TEXT(""));
		}

		if (bNodeBIsSelect && bNodeAIsStateMachine)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, TEXT(""));
		}

		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorNotAllowed", "A state machine select node must wire to a state machine directly."));
	}

	return Super::CanCreateConnection(PinA, PinB);
}

bool USMGraphK2Schema::CanEncapuslateNode(UEdGraphNode const& TestNode) const
{
	if (const USMGraphK2Node_Base* Node = Cast<USMGraphK2Node_Base>(&TestNode))
	{
		return Node->CanCollapseNode();
	}

	return true;
}

void USMGraphK2Schema::GetGraphDisplayInformation(const UEdGraph& Graph, FGraphDisplayInfo& DisplayInfo) const
{
	Super::GetGraphDisplayInformation(Graph, DisplayInfo);

	DisplayInfo.Tooltip = FText::FromName(Graph.GetFName());
	DisplayInfo.DocExcerptName = nullptr;
}

UEdGraphPin* USMGraphK2Schema::GetThenPin(UEdGraphNode* Node)
{
	return Node->FindPin(PN_Then, EGPD_Output);
}

bool USMGraphK2Schema::IsThenPin(UEdGraphPin* Pin)
{
	return Pin && Pin->PinName == PN_Then && Pin->PinType.PinCategory == PC_Exec;
}

void USMGraphK2Schema::GetBreakLinkToSubMenuActions(UToolMenu* Menu, UEdGraphPin* InGraphPin)
{
	FToolMenuSection& Section = Menu->FindOrAddSection("EdGraphSchemaPinActions");

	// Make sure we have a unique name for every entry in the list
	TMap< FString, uint32 > LinkTitleCount;

	// Add all the links we could break from
	for (TArray<class UEdGraphPin*>::TConstIterator Links(InGraphPin->LinkedTo); Links; ++Links)
	{
		UEdGraphPin* Pin = *Links;
		FText Title = Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView);
		FString TitleString = Title.ToString();
		const FText PinDisplayName = Pin->GetDisplayName();
		if (!PinDisplayName.IsEmpty())
		{
			TitleString = FString::Printf(TEXT("%s (%s)"), *TitleString, *PinDisplayName.ToString());

			// Add name of connection if possible
			FFormatNamedArguments Args;
			Args.Add(TEXT("NodeTitle"), Title);
			Args.Add(TEXT("PinName"), PinDisplayName);
			Title = FText::Format(LOCTEXT("BreakDescPin", "{NodeTitle} ({PinName})"), Args);
		}

		uint32& Count = LinkTitleCount.FindOrAdd(TitleString);

		FText Description;
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeTitle"), Title);
		Args.Add(TEXT("NumberOfNodes"), Count);

		if (Count == 0)
		{
			Description = FText::Format(LOCTEXT("BreakDesc", "Break Link to {NodeTitle}"), Args);
		}
		else
		{
			Description = FText::Format(LOCTEXT("BreakDescMulti", "Break Link to {NodeTitle} ({NumberOfNodes})"), Args);
		}
		++Count;

		Section.AddMenuEntry(NAME_None, Description, Description, FSlateIcon(), FUIAction(
			FExecuteAction::CreateUObject(this, &UEdGraphSchema_K2::BreakSinglePinLink, const_cast<UEdGraphPin*>(InGraphPin), *Links)));
	}
}

void USMGraphK2Schema::GetJumpToConnectionSubMenuActions(UToolMenu* Menu, UEdGraphPin* InGraphPin)
{
	FToolMenuSection& Section = Menu->FindOrAddSection("EdGraphSchemaPinActions");

	// Make sure we have a unique name for every entry in the list
	TMap< FString, uint32 > LinkTitleCount;

	// Add all the links we could break from
	for (const UEdGraphPin* PinLink : InGraphPin->LinkedTo)
	{
		FText Title = PinLink->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView);
		FString TitleString = Title.ToString();
		const FText PinDisplayName = PinLink->GetDisplayName();
		if (!PinDisplayName.IsEmpty())
		{
			TitleString = FString::Printf(TEXT("%s (%s)"), *TitleString, *PinDisplayName.ToString());

			// Add name of connection if possible
			FFormatNamedArguments Args;
			Args.Add(TEXT("NodeTitle"), Title);
			Args.Add(TEXT("PinName"), PinDisplayName);
			Title = FText::Format(LOCTEXT("JumpToDescPin", "{NodeTitle} ({PinName})"), Args);
		}

		uint32& Count = LinkTitleCount.FindOrAdd(TitleString);

		FText Description;
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeTitle"), Title);
		Args.Add(TEXT("NumberOfNodes"), Count);

		if (Count == 0)
		{
			Description = FText::Format(LOCTEXT("JumpDesc", "Jump to {NodeTitle}"), Args);
		}
		else
		{
			Description = FText::Format(LOCTEXT("JumpDescMulti", "Jump to {NodeTitle} ({NumberOfNodes})"), Args);
		}
		++Count;

		Section.AddMenuEntry(NAME_None, Description, Description, FSlateIcon(), FUIAction(
			FExecuteAction::CreateStatic(&FKismetEditorUtilities::BringKismetToFocusAttentionOnPin, PinLink)));
	}
}

// todo: this is a long way off ideal, but we can't pass context from our menu items onto the graph panel implementation
// It'd be better to be able to pass context through to menu/ui commands
namespace { UEdGraphPin* StraightenDestinationPin = nullptr; }
UEdGraphPin* USMGraphK2Schema::GetAndResetStraightenDestinationPin()
{
	UEdGraphPin* Temp = StraightenDestinationPin;
	StraightenDestinationPin = nullptr;
	return Temp;
}

void USMGraphK2Schema::GetStraightenConnectionToSubMenuActions(UToolMenu* Menu, UEdGraphPin* InGraphPin) const
{
	const FUIAction* StraightenConnectionsUIAction = Menu->Context.GetActionForCommand(FGraphEditorCommands::Get().StraightenConnections);
	if (!ensure(StraightenConnectionsUIAction))
	{
		return;
	}

	// Make sure we have a unique name for every entry in the list
	TMap<FString, uint32> LinkTitleCount;

	TMap<UEdGraphNode*, TArray<UEdGraphPin*>> NodeToPins;

	for (UEdGraphPin* Pin : InGraphPin->LinkedTo)
	{
		UEdGraphNode* Node = Pin->GetOwningNode();
		if (Node)
		{
			NodeToPins.FindOrAdd(Node).Add(Pin);
		}
	}

	FToolMenuSection& Section = Menu->FindOrAddSection("EdGraphSchemaPinActions");
	Section.AddMenuEntry(FGraphEditorCommands::Get().StraightenConnections,
		LOCTEXT("StraightenAllConnections", "All Connected Pins"),
		TAttribute<FText>(), FSlateIcon(NAME_None, NAME_None, NAME_None));

	for (const TPair<UEdGraphNode*, TArray<UEdGraphPin*>>& Pair : NodeToPins)
	{
		for (UEdGraphPin* Pin : Pair.Value)
		{
			FText Title = Pair.Key->GetNodeTitle(ENodeTitleType::ListView);
			FString TitleString = Title.ToString();
			const FText PinDisplayName = Pin->GetDisplayName();
			if (!PinDisplayName.IsEmpty())
			{
				TitleString = FString::Printf(TEXT("%s (%s)"), *TitleString, *PinDisplayName.ToString());

				// Add name of connection if possible
				FFormatNamedArguments Args;
				Args.Add(TEXT("NodeTitle"), Title);
				Args.Add(TEXT("PinName"), PinDisplayName);
				Title = FText::Format(LOCTEXT("StraightenToDescPin", "{NodeTitle} ({PinName})"), Args);
			}
			uint32& Count = LinkTitleCount.FindOrAdd(TitleString);

			FText Description;
			FFormatNamedArguments Args;
			Args.Add(TEXT("NodeTitle"), Title);
			Args.Add(TEXT("NumberOfNodes"), Count);

			if (Count == 0)
			{
				Description = FText::Format(LOCTEXT("StraightenDesc", "Straighten connection to {NodeTitle}"), Args);
			}
			else
			{
				Description = FText::Format(LOCTEXT("StraightendDescMulti", "Straighten connection to {NodeTitle} ({NumberOfNodes})"), Args);
			}
			++Count;

			Section.AddMenuEntry(
				NAME_None,
				Description,
				Description,
				FSlateIcon(),
				FToolMenuExecuteAction::CreateLambda([=](const FToolMenuContext& Context) {
				if (const FUIAction* UIAction = Context.GetActionForCommand(FGraphEditorCommands::Get().StraightenConnections))
				{
					StraightenDestinationPin = Pin;
					UIAction->ExecuteAction.Execute();
				}
			}));
		}
	}
}

#undef LOCTEXT_NAMESPACE
