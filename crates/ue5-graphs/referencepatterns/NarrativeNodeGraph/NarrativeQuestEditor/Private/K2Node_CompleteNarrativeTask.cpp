// Copyright Narrative Tools 2022. 


#include "K2Node_CompleteNarrativeTask.h"
#include "EdGraphSchema_K2_Actions.h"
#include "KismetCompiler.h"
#include "BlueprintActionMenuBuilder.h"
#include "NarrativeFunctionLibrary.h"
#include "NarrativeComponent.h"
#include "NarrativeDataTask.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"

#define LOCTEXT_NAMESPACE "UK2Node_CompleteNarrativeAction"

namespace NarrativePinNames
{
	static const FName TargetPinName(TEXT("Target"));
	static const FName EventPinName(TEXT("Task"));
	static const FName ArgumentPinName(TEXT("Argument"));
	static const FName QuantityPinName(TEXT("Quantity"));
	static const FName UpdatedQuestPinName(TEXT("UpdatedQuest"));
};

void UK2Node_CompleteNarrativeTask::ReconstructNode()
{
	Super::ReconstructNode();

	//Force action pin to update since PinDefaultValueChanged isnt called on init
	PinDefaultValueChanged(GetActionPin());
}

void UK2Node_CompleteNarrativeTask::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	// Execution pins
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UNarrativeComponent::StaticClass(), NarrativePinNames::TargetPinName);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UNarrativeDataTask::StaticClass(), NarrativePinNames::EventPinName);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, NarrativePinNames::ArgumentPinName);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, NarrativePinNames::QuantityPinName);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, NarrativePinNames::UpdatedQuestPinName)->PinFriendlyName = LOCTEXT("CompleteNarrativeActionK2Node_UpdatedQuestPin", "Updated Quest?");

	GetQuantityPin()->DefaultValue = "1";

}

FText UK2Node_CompleteNarrativeTask::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("CompleteNarrativeActionK2Node_Title", "Complete Narrative Data Task");
}

FText UK2Node_CompleteNarrativeTask::GetTooltipText() const
{
	return LOCTEXT("CompleteNarrativeActionK2Node_Tooltip", "Tells Narrative to log a data task, and remember it. Also may update any quests waiting for that task to be completed.");
}

FName UK2Node_CompleteNarrativeTask::GetCornerIcon() const
{
	return TEXT("Graph.Replication.AuthorityOnly");
}
FText UK2Node_CompleteNarrativeTask::GetToolTipHeading() const
{
	return LOCTEXT("ServerOnlyFunc", "Server Only");
}

FSlateIcon UK2Node_CompleteNarrativeTask::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("QuestEditorStyle", "ClassIcon.NarrativeDataTask");
	return Icon;
}


void UK2Node_CompleteNarrativeTask::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	if (Pin == GetActionPin())
	{
		UEdGraphPin* ArgumentPin = GetArgumentPin();
		UEdGraphPin* ActionPin = GetActionPin();
		if (ArgumentPin && ActionPin)
		{
			//Use the same argument text in Blueprint nodes as what shows in the quest tool to make life easy for designers.
			if (UNarrativeDataTask* QA = Cast<UNarrativeDataTask>(ActionPin->DefaultObject))
			{
				CachedArgumentPinFriendlyName.SetCachedText(FText::FromString(QA->ArgumentName), this);
			}
			else
			{
				CachedArgumentPinFriendlyName.SetCachedText(FText::FromString("Argument"), this);
			}
			ArgumentPin->PinFriendlyName = CachedArgumentPinFriendlyName.GetCachedText();
		}
	}
}

void UK2Node_CompleteNarrativeTask::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	Super::GetMenuActions(ActionRegistrar);

	UClass* Action = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(Action))
	{
		UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(GetClass());
		check(Spawner != nullptr);
		ActionRegistrar.AddBlueprintAction(Action, Spawner);
	}
}

FText UK2Node_CompleteNarrativeTask::GetMenuCategory() const
{
	return LOCTEXT("CompleteNarrativeActionK2Node_MenuCategory", "Narrative");
}

void UK2Node_CompleteNarrativeTask::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);


	UK2Node_CallFunction* CallFunction = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallFunction->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UNarrativeFunctionLibrary, CompleteNarrativeDataTask), UNarrativeFunctionLibrary::StaticClass());
	CallFunction->AllocateDefaultPins();

	CompilerContext.MessageLog.NotifyIntermediateObjectCreation(CallFunction, this);

	//Input
	if (UEdGraphPin* TargetPin = GetTargetPin())
	{
		CompilerContext.MovePinLinksToIntermediate(*TargetPin, *CallFunction->FindPinChecked(TEXT("Target")));
	}

	if (UEdGraphPin* ActionPin = GetActionPin())
	{
		CompilerContext.MovePinLinksToIntermediate(*ActionPin, *CallFunction->FindPinChecked(TEXT("Task")));
	}

	if (UEdGraphPin* ArgumentPin = GetArgumentPin())
	{
		CompilerContext.MovePinLinksToIntermediate(*ArgumentPin, *CallFunction->FindPinChecked(TEXT("Argument")));
	}

	if (UEdGraphPin* QuantityPin = GetQuantityPin())
	{
		CompilerContext.MovePinLinksToIntermediate(*QuantityPin, *CallFunction->FindPinChecked(TEXT("Quantity")));
	}

	//Output
	if (UEdGraphPin* UpdatedQuestPin = GetUpdatedQuestPin())
	{
		CompilerContext.MovePinLinksToIntermediate(*UpdatedQuestPin, *CallFunction->GetReturnValuePin());
	}

	//Exec pins
	UEdGraphPin* NodeExec = GetExecPin();
	UEdGraphPin* NodeThen = FindPin(UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* InternalExec = CallFunction->GetExecPin();
	CompilerContext.MovePinLinksToIntermediate(*NodeExec, *InternalExec);
	UEdGraphPin* InternalThen = CallFunction->GetThenPin();
	CompilerContext.MovePinLinksToIntermediate(*NodeThen, *InternalThen);
	//After we are done we break all links to this node (not the internally created one)
	BreakAllNodeLinks();
}

class FNodeHandlingFunctor* UK2Node_CompleteNarrativeTask::CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const
{
	return new FNodeHandlingFunctor(CompilerContext);
}

UEdGraphPin* UK2Node_CompleteNarrativeTask::GetTargetPin()
{
	UEdGraphPin* Pin = FindPin(NarrativePinNames::TargetPinName);
	check(Pin);
	return Pin;
}

UEdGraphPin* UK2Node_CompleteNarrativeTask::GetActionPin()
{
	UEdGraphPin* Pin = FindPin(NarrativePinNames::EventPinName);
	check(Pin);
	return Pin;
}

UEdGraphPin* UK2Node_CompleteNarrativeTask::GetArgumentPin()
{
	UEdGraphPin* Pin = FindPin(NarrativePinNames::ArgumentPinName);
	check(Pin);
	return Pin;
}

UEdGraphPin* UK2Node_CompleteNarrativeTask::GetUpdatedQuestPin()
{
	UEdGraphPin* Pin = FindPin(NarrativePinNames::UpdatedQuestPinName);
	check(Pin);
	return Pin;
}

UEdGraphPin* UK2Node_CompleteNarrativeTask::GetQuantityPin()
{
	UEdGraphPin* Pin = FindPin(NarrativePinNames::QuantityPinName);
	check(Pin);
	return Pin;
}

#undef LOCTEXT_NAMESPACE