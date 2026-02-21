// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMExtendedEditorCommands.h"

#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_TextPropertyNode.h"
#include "Graph/SMTextPropertyGraph.h"

#include "Blueprints/ISMStateMachineBlueprintEditor.h"

#define LOCTEXT_NAMESPACE "SMExtendedEditorCommands"

void FSMExtendedEditorCommands::RegisterCommands()
{
	UI_COMMAND(StartTextPropertyEdit, "Edit Text", "Edit text directly on the node", EUserInterfaceActionType::Button, FInputChord());
}

const FSMExtendedEditorCommands& FSMExtendedEditorCommands::Get()
{
	return TCommands<FSMExtendedEditorCommands>::Get();
}

void FSMExtendedEditorCommands::OnEditorCommandsCreated(ISMStateMachineBlueprintEditor* Editor, TSharedPtr<FUICommandList> CommandList)
{
	CommandList->MapAction(Get().StartTextPropertyEdit,
		FExecuteAction::CreateStatic(&FSMExtendedEditorCommands::EditText, Editor),
		FCanExecuteAction::CreateStatic(&FSMExtendedEditorCommands::CanEditText, Editor));
}

void FSMExtendedEditorCommands::EditText(ISMStateMachineBlueprintEditor* Editor)
{
	if (USMGraphK2Node_TextPropertyNode* TextNode = Cast<USMGraphK2Node_TextPropertyNode>(Editor->GetSelectedPropertyNode()))
	{
		if (USMTextPropertyGraph* TextGraph = Cast<USMTextPropertyGraph>(TextNode->GetPropertyGraph()))
		{
			TextGraph->SetTextEditMode(true);
		}
	}
}

bool FSMExtendedEditorCommands::CanEditText(ISMStateMachineBlueprintEditor* Editor)
{
	return Editor->IsSelectedPropertyNodeValid();
}

#undef LOCTEXT_NAMESPACE
