// © 2025 mrbaconvn. All Rights Reserved.

#include "Graph/Node/ComboNodeBase.h"
#include "Framework/Commands/GenericCommands.h"
#include "GraphData/ComboGraphData.h"
#include "GraphEditorActions.h"
#include "Graph/ComboGraphSchema.h"

#define FADE_TIME 1.f

void UComboNodeBase::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetNodeContextMenuActions(Menu, Context);

	FToolMenuSection& Section = Menu->AddSection(TEXT("DefaultActions"), FText::FromString(TEXT("Default actions")));

	Section.AddMenuEntry(FGenericCommands::Get().Delete);
	Section.AddMenuEntry(FGenericCommands::Get().Duplicate);
	Section.AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks); 
}

UEdGraphPin* UComboNodeBase::CreateCustomPin(EEdGraphPinDirection Direction, FName Name, FName Subcategory)
{
	FName Category = (Direction == EEdGraphPinDirection::EGPD_Input) ? TEXT("Inputs") : TEXT("Outputs");

	UEdGraphPin* pin = CreatePin(
		Direction,
		Category,
		Name
	);
	pin->PinType.PinSubCategory = Subcategory;

	return pin;
}

FLinearColor UComboNodeBase::GetPinColor() const
{
	return FLinearColor::Gray;
}

FString UComboNodeBase::GetNodePopUpMessage() const
{
	if (IsNodeActive())
	{
		return FString::Printf(TEXT("%.2f seconds"), CurrentActiveTime);
	}
	return FString();
}

FLinearColor UComboNodeBase::GetNodePopUpColor() const
{
	return GetNodeBodyTintColor();
}

FLinearColor UComboNodeBase::GetNodeBodyTintColor() const
{
	const FLinearColor DefaultColor = FLinearColor::Black;
	const FLinearColor ActiveColor = FLinearColor(FColor::Yellow);
	if (CurrentActiveTime <= 0)
	{
		return DefaultColor;
	}
	if (IsNodeActive())
	{
		return ActiveColor;
	}
 
	float LerpAlpha = FMath::Clamp(CurrentActiveTime / FADE_TIME, 0.0f, 1.0f);
	return FLinearColor( FMath::Lerp(DefaultColor.R, ActiveColor.R, LerpAlpha),
						 FMath::Lerp(DefaultColor.G, ActiveColor.G, LerpAlpha),
						 FMath::Lerp(DefaultColor.B, ActiveColor.B, LerpAlpha), 1);
}

bool UComboNodeBase::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const
{
	return Schema->IsA<UComboGraphSchema>();
}

void UComboNodeBase::UpdateTime(float DeltaTime)
{
	if (bIsNodeActive)
	{
		CurrentActiveTime += DeltaTime; 
	}
	else
	{
		CurrentActiveTime = FMath::Max(0, CurrentActiveTime - DeltaTime);
	}
}

void UComboNodeBase::SetNodeActive(bool bActive)
{
	bIsNodeActive = bActive;
	if (bIsNodeActive)
	{
		CurrentActiveTime = 0;
	}
	else
	{
		CurrentActiveTime = FADE_TIME;
	}
}

bool UComboNodeBase::IsNodeActive() const
{
	return bIsNodeActive;
}

int UComboNodeBase::GetPinCount(EEdGraphPinDirection Direction)
{
	int Result = 0;
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin == nullptr)
		{
			continue;
		}

		if (Pin->Direction == Direction)
		{
			Result++;
		}
	}
	return Result;
}

void UComboNodeBase::ReservePins(EEdGraphPinDirection Direction, int TargetCount)
{
	int CurrentPinCount = GetPinCount(Direction);

	if (TargetCount > CurrentPinCount)
	{
		for (int i = CurrentPinCount; i < TargetCount; i++)
		{
			CreateCustomPin(Direction, UNINITIALIZED_PIN_NAME);
		}
		return;
	}
	
	if (TargetCount < CurrentPinCount)
	{
		for (int i = CurrentPinCount - 1; i >= TargetCount; i--)
		{
			RemovePinAt(i, Direction);
		}
	}
}

FText UComboNodeBase::GetNodeCreationText() const
{
	return FText::GetEmpty();
}

TSharedRef<SWidget> UComboNodeBase::CreateMiddleNodeContent() const
{
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> UComboNodeBase::CreateBottomNodeContent() const
{
	return SNullWidget::NullWidget;
}

void UComboNodeBase::CreateDefaultPins()
{
}

bool UComboNodeBase::ValidateNode()
{
	RebuildNode();
	return true;
}

bool UComboNodeBase::InitPinConnection(UEdGraphPin* OtherNodePin)
{
	UEdGraph* Graph = GetGraph();
	if (!IsValid(Graph))
	{
		return false;
	}

	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (!IsValid(Schema))
	{
		return false;
	}
	
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->Direction == EEdGraphPinDirection::EGPD_Input && Schema->TryCreateConnection(OtherNodePin, Pin))
		{
			return true;
		}
	}
	return false;
}

void UComboNodeBase::InitNodeData(UObject* Outer)
{
	check(false);
}

void UComboNodeBase::SetNodeData(UComboGraphNodeData* NodeData)
{
	check(false);
}

UComboNodeBase::UComboNodeBase()
{ 
	CreateNewGuid();
}

UComboGraphNodeData* UComboNodeBase::GetNodeData() const
{
	check(false);
	return nullptr;
} 

void UComboNodeBase::RebuildNode()
{
}
