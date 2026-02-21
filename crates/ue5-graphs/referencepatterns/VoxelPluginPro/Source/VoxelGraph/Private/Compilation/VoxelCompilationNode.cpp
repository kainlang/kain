// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Compilation/VoxelCompilationNode.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "Runtime/VoxelComputeNode.h"
#include "VoxelNode.h"
#include "VoxelGraphErrorReporter.h"

FVoxelCompilationPin::FVoxelCompilationPin(FVoxelCompilationNode& Node, int32 Index, EVoxelPinDirection Direction, EVoxelPinCategory PinCategory, const FName& Name)
	: Node(Node)
	, Index(Index)
	, Direction(Direction)
	, PinCategory(PinCategory)
	, Name(Name)
{
}

void FVoxelCompilationPin::Check(FVoxelGraphErrorReporter& ErrorReporter)
{
#define ensureCheck(Expr) if (!ensure(Expr)) { ErrorReporter.AddMessageToNode(&Node, "Internal pin error: " + FString(#Expr), EVoxelGraphNodeMessageType::Error); }
	for (auto& LinkedToPin : LinkedTo)
	{
		// Only one occurrence
		{
			const int32 FromStartIndex = LinkedTo.Find(LinkedToPin);
			const int32 FromEndIndex = LinkedTo.FindLast(LinkedToPin);
			ensureCheck(FromStartIndex == FromEndIndex && FromStartIndex >= 0);
		}
		{
			auto& OtherLinkedTo = LinkedToPin->LinkedTo;
			const int32 FromStartIndex = OtherLinkedTo.Find(this);
			const int32 FromEndIndex = OtherLinkedTo.FindLast(this);
			ensureCheck(FromStartIndex == FromEndIndex && FromStartIndex >= 0);
		}
	}

	if (PinCategory == EVoxelPinCategory::Exec)
	{
		if (Direction == EVoxelPinDirection::Output)
		{
			ensureCheck(NumLinkedTo() <= 1);
		}
	}
	else
	{
		if (Direction == EVoxelPinDirection::Input)
		{
			ensureCheck(NumLinkedTo() <= 1);
		}
	}
#undef ensureCheck
}

FVoxelCompilationPin FVoxelCompilationPin::Clone(FVoxelCompilationNode& NewNode) const
{
	auto NewPin = FVoxelCompilationPin(NewNode, Index, Direction, PinCategory, Name);
	NewPin.DefaultValue = DefaultValue;
	NewPin.LinkedTo = LinkedTo;
	return NewPin;
}

FVoxelCompilationNode::FVoxelCompilationNode(EVoxelCompilationNodeType Type, const UVoxelNode& Node)
	: Node(Node)
	, PrivateType(Type)
{
	SourceNodes.Add(&Node);

	const int32 InputCount = FMath::Clamp(Node.InputPinCount, Node.GetMinInputPins(), Node.GetMaxInputPins()); // To work with default classes too
	const int32 OutputCount = Node.GetOutputPinsCount();

	for (int32 Index = 0; Index < InputCount; Index++)
	{
		const auto PinCategory = Node.GetInputPinCategory(Index);
		const FName PinName = Node.GetInputPinName(Index);
		Pins.Emplace(*this, Index, EVoxelPinDirection::Input, PinCategory, PinName);
		if (PinCategory != EVoxelPinCategory::Exec)
		{
			InputIds.Add(-1);
		}
	}
	for (int32 Index = 0; Index < OutputCount; Index++)
	{
		const auto PinCategory = Node.GetOutputPinCategory(Index);
		const FName PinName = Node.GetOutputPinName(Index);
		Pins.Emplace(*this, Index, EVoxelPinDirection::Output, PinCategory, PinName);
		if (PinCategory != EVoxelPinCategory::Exec)
		{
			OutputIds.Add(-1);
		}
	}
	RebuildPinsArray();
}

FVoxelCompilationNode::FVoxelCompilationNode(
	EVoxelCompilationNodeType Type, const UVoxelNode& Node, 
	const TArray<EVoxelPinCategory>& InputCategories, 
	const TArray<EVoxelPinCategory>& OutputCategories)
	: Node(Node)
	, PrivateType(Type)
{
	SourceNodes.Add(&Node);
	
	const int32 InputCount = InputCategories.Num();
	const int32 OutputCount = OutputCategories.Num();

	for (int32 Index = 0; Index < InputCount; Index++)
	{
		const auto PinCategory = InputCategories[Index];
		const FName PinName = "INTERNAL";
		Pins.Emplace(*this, Index, EVoxelPinDirection::Input, PinCategory, PinName);
		if (PinCategory != EVoxelPinCategory::Exec)
		{
			InputIds.Add(-1);
		}
	}
	for (int32 Index = 0; Index < OutputCount; Index++)
	{
		const auto PinCategory = OutputCategories[Index];
		const FName PinName = "INTERNAL";
		Pins.Emplace(*this, Index, EVoxelPinDirection::Output, PinCategory, PinName);
		if (PinCategory != EVoxelPinCategory::Exec)
		{
			OutputIds.Add(-1);
		}
	}
	RebuildPinsArray();
}

TArray<EVoxelPinCategory> FVoxelCompilationNode::GetInputPinCategories() const
{
	TArray<EVoxelPinCategory> Result;
	for (auto& Pin : IteratePins<EVoxelPinIter::Input>())
	{
		Result.Add(Pin.PinCategory);
	}
	return Result;
}

TArray<EVoxelPinCategory> FVoxelCompilationNode::GetOutputPinCategories() const
{
	TArray<EVoxelPinCategory> Result;
	for (auto& Pin : IteratePins<EVoxelPinIter::Output>())
	{
		Result.Add(Pin.PinCategory);
	}
	return Result;
}

FString FVoxelCompilationNode::GetPrettyName() const
{
	if (SourceNodes.Num() == 0)
	{
		return "Class: " + GetClassName();
	}
	else
	{
		FString Result = SourceNodes.Last()->GetTitle().ToString();
		for (int32 Index = SourceNodes.Num() - 2; Index >= 0; Index--)
		{
			Result += "." + SourceNodes[Index]->GetTitle().ToString();
		}
		return Result;
	}
}

bool FVoxelCompilationNode::IsSeedNode() const
{
	for (auto& Pin : IteratePins<EVoxelPinIter::Output>())
	{
		if (Pin.PinCategory == EVoxelPinCategory::Seed)
		{
			return true;
		}
	}
	return false;
}

void FVoxelCompilationNode::BreakAllLinks()
{
	FVoxelGraphCompilerHelpers::BreakNodeLinks<EVoxelPinIter::All>(*this);;
}

bool FVoxelCompilationNode::IsLinkedTo(const FVoxelCompilationNode* OtherNode) const
{
	for (auto& Pin : IteratePins<EVoxelPinIter::All>())
	{
		for (auto& LinkedTo : Pin.IterateLinkedTo())
		{
			if (&LinkedTo.Node == OtherNode)
			{
				return true;
			}
		}
	}
	return false;
}

void FVoxelCompilationNode::CheckIsNotLinked(FVoxelGraphErrorReporter& ErrorReporter) const
{
	for (auto& Pin : IteratePins<EVoxelPinIter::All>())
	{
		ensureVoxelGraph(Pin.NumLinkedTo() == 0, this);
	}
}

TVoxelSharedPtr<FVoxelComputeNode> FVoxelCompilationNode::GetComputeNode() const
{
	return Node.GetComputeNode(*this);
}

void FVoxelCompilationNode::Check(FVoxelGraphErrorReporter& ErrorReporter) const
{

}

void FVoxelCompilationNode::CopyPropertiesToNewNode(const TSharedRef<FVoxelCompilationNode>& NewNode, bool bFixLinks) const
{
	NewNode->Dependencies = Dependencies;
	NewNode->SourceNodes = SourceNodes;
	ensure(NewNode->Pins.Num() == 0);
	for (auto& Pin : Pins)
	{
		NewNode->Pins.Add(Pin.Clone(*NewNode));
	}
	NewNode->RebuildPinsArray();
	NewNode->InputIds = InputIds;
	NewNode->OutputIds = OutputIds;

	if (bFixLinks)
	{
		for (auto& Pin : NewNode->Pins)
		{
			for (auto& LinkedToPin : Pin.LinkedTo)
			{
				LinkedToPin->LinkedTo.Add(&Pin);
			}
		}
	}
}

void FVoxelCompilationNode::RebuildPinsArray()
{
	CachedInputPins.Empty();
	CachedOutputPins.Empty();
	for (auto& Pin : Pins)
	{
		if (Pin.Direction == EVoxelPinDirection::Input)
		{
			CachedInputPins.Add(&Pin);
		}
		else
		{
			CachedOutputPins.Add(&Pin);
		}
	}
}
