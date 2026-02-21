// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelNodes/VoxelNodeHelperMacros.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "VoxelNode.h"

namespace FVoxelExecPassthroughComputeNode
{
	using FLocalVoxelComputeNode = FVoxelPassthroughComputeNode;
}
namespace FVoxelBoolPassthroughComputeNode
{
	GENERATED_COMPUTENODE
	(
		DEFINE_INPUTS(bool),
		DEFINE_OUTPUTS(bool),
		_O0 = _I0;
	)
}
namespace FVoxelIntPassthroughComputeNode
{
	GENERATED_COMPUTENODE
	(
		DEFINE_INPUTS(int32),
		DEFINE_OUTPUTS(int32),
		_O0 = _I0;
	)
};
namespace FVoxelFloatPassthroughComputeNode
{
	GENERATED_COMPUTENODE
	(
		DEFINE_INPUTS(v_flt),
		DEFINE_OUTPUTS(v_flt),
		_O0 = _I0;
	)
};
namespace FVoxelMaterialPassthroughComputeNode
{
	GENERATED_COMPUTENODE
	(
		DEFINE_INPUTS(FVoxelMaterial),
		DEFINE_OUTPUTS(FVoxelMaterial),
		_O0 = _I0;
	)
};
namespace FVoxelSeedPassthroughComputeNode
{
	class FLocalVoxelComputeNode : public FVoxelSeedComputeNode
	{
	public:
		using FVoxelSeedComputeNode::FVoxelSeedComputeNode;

		void Init(FVoxelGraphSeed Inputs[], FVoxelGraphSeed Outputs[], const FVoxelGeneratorInit& InitStruct) override
		{
			Outputs[0] = Inputs[0];
		}
		void InitCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = " + Inputs[0] + ";");
		}
	};
};

template<typename T, EVoxelPinCategory Category>
class TVoxelPassthroughCompilationNode : public TVoxelCompilationNode<EVoxelCompilationNodeType::Passthrough, TVoxelPassthroughCompilationNode<T, Category>, FVoxelPassthroughCompilationNode>
{
public:
	using Parent = TVoxelCompilationNode<EVoxelCompilationNodeType::Passthrough, TVoxelPassthroughCompilationNode<T, Category>, FVoxelPassthroughCompilationNode>;
	using Parent::Parent;

	TVoxelPassthroughCompilationNode(const TArray<const UVoxelNode*>& InSourceNodes)
		: Parent(*GetDefault<UVoxelNode>(), TArray<EVoxelPinCategory>({ Category }), TArray<EVoxelPinCategory>({ Category }))
	{
		this->SourceNodes = InSourceNodes;
	}
	virtual TVoxelSharedPtr<FVoxelComputeNode> GetComputeNode() const override
	{
		return MakeVoxelShared<T>(this->Node, *this);
	}
};

using FVoxelExecPassthrough = TVoxelPassthroughCompilationNode<FVoxelExecPassthroughComputeNode::FLocalVoxelComputeNode, EC::Exec>;
using FVoxelBoolPassthrough = TVoxelPassthroughCompilationNode<FVoxelBoolPassthroughComputeNode::FLocalVoxelComputeNode, EC::Boolean>;
using FVoxelIntPassthrough = TVoxelPassthroughCompilationNode<FVoxelIntPassthroughComputeNode::FLocalVoxelComputeNode, EC::Int>;
using FVoxelFloatPassthrough = TVoxelPassthroughCompilationNode<FVoxelFloatPassthroughComputeNode::FLocalVoxelComputeNode, EC::Float>;
using FVoxelMaterialPassthrough = TVoxelPassthroughCompilationNode<FVoxelMaterialPassthroughComputeNode::FLocalVoxelComputeNode, EC::Material>;
using FVoxelColorPassthrough = TVoxelPassthroughCompilationNode<FVoxelMaterialPassthroughComputeNode::FLocalVoxelComputeNode, EC::Color>;
using FVoxelSeedPassthrough = TVoxelPassthroughCompilationNode<FVoxelSeedPassthroughComputeNode::FLocalVoxelComputeNode, EC::Seed>;

TSharedRef<FVoxelCompilationNode> FVoxelGraphCompilerHelpers::GetPassthroughNode(EVoxelPinCategory Category, const TArray<const UVoxelNode*>& SourceNodes)
{
	switch (Category)
	{
	case EVoxelPinCategory::Exec:
		return MakeShared<FVoxelExecPassthrough>(SourceNodes);
	case EVoxelPinCategory::Boolean:
		return MakeShared<FVoxelBoolPassthrough>(SourceNodes);
	case EVoxelPinCategory::Int:
		return MakeShared<FVoxelIntPassthrough>(SourceNodes);
	case EVoxelPinCategory::Float:
		return MakeShared<FVoxelFloatPassthrough>(SourceNodes);
	case EVoxelPinCategory::Material:
		return MakeShared<FVoxelMaterialPassthrough>(SourceNodes);
	case EVoxelPinCategory::Color:
		return MakeShared<FVoxelColorPassthrough>(SourceNodes);
	case EVoxelPinCategory::Seed:
		return MakeShared<FVoxelSeedPassthrough>(SourceNodes);
	default:
		check(false);
		return TSharedPtr<FVoxelCompilationNode>().ToSharedRef();
	}
}

FVoxelCompilationNode* FVoxelGraphCompilerHelpers::AddPassthrough(FVoxelGraphCompiler& Compiler, FVoxelCompilationPin& Pin, FVoxelCompilationNode* SourceNode)
{
	if (!SourceNode)
	{
		SourceNode = &Pin.Node;
	}

	const auto Passthrough = GetPassthroughNode(Pin.PinCategory, SourceNode->SourceNodes);
	Compiler.AddNode(Passthrough);

	auto& PassthroughInputPin = Passthrough->GetInputPin(0);
	auto& PassthroughOutputPin = Passthrough->GetOutputPin(0);
	if (Pin.Direction == EVoxelPinDirection::Input)
	{
		PassthroughInputPin.SetDefaultValue(Pin.GetDefaultValue());
	}

	auto& PassthroughPinConnectedToPin = Pin.Direction == EVoxelPinDirection::Input ? PassthroughOutputPin : PassthroughInputPin;
	auto& PassthroughPinNotConnectedToPin = Pin.Direction == EVoxelPinDirection::Output ? PassthroughOutputPin : PassthroughInputPin;

	for (auto& LinkedToPin : Pin.IterateLinkedTo())
	{
		LinkedToPin.LinkTo(PassthroughPinNotConnectedToPin);
	}
	Pin.BreakAllLinks();

	PassthroughPinConnectedToPin.LinkTo(Pin);

	return &Passthrough.Get();
}

struct FVoxelCompilationPinLink
{
	FVoxelCompilationPin* From;
	FVoxelCompilationPin* To;

	FVoxelCompilationPinLink(FVoxelCompilationPin* From, FVoxelCompilationPin* To)
		: From(From)
		, To(To)
	{
	}
};

void DuplicateNodeAndQueueLinks(
	FVoxelGraphCompiler& Compiler,
	FVoxelCompilationNode* Node, 
	TArray<FVoxelCompilationPinLink>& OutLinksToCreate,
	TMap<FVoxelCompilationPin*, FVoxelCompilationPin*>& OldPinsToNewPins,
	TMap<FVoxelCompilationNode*, FVoxelCompilationNode*>& OldNodesToNewNodes)
{
	auto* NewNode = Compiler.AddNode(Node->Clone(true));
	OldNodesToNewNodes.Add(Node, NewNode);

	auto** ParentParent = Compiler.Parents.Find(Node);
	auto* Parent = ParentParent ? *ParentParent : Node;
	Compiler.Parents.Add(NewNode, Parent);
	Compiler.DuplicateCounts.FindOrAdd(Parent)++;
	for (auto& It : Compiler.DuplicateCounts)
	{
		if (It.Key && It.Value > 256)
		{
			if (!Compiler.ErrorReporter.HasError())
			{
				Compiler.ErrorReporter.AddError("You need to add a function separator after your Select, FastLerp or FlowMerge node, and make all the data links go through it");
			}
			Compiler.ErrorReporter.AddMessageToNode(It.Key, FString::Printf(TEXT("duplicated %d times: function separator needed"), It.Value), EVoxelGraphNodeMessageType::Error);
		}
	}
	
	for (auto& Pin : NewNode->IteratePins<EVoxelPinIter::All>())
	{
		for (auto& LinkedTo : Pin.IterateLinkedTo())
		{
			// From MUST be new pin
			OutLinksToCreate.Emplace(&Pin, &LinkedTo);
		}
	}

	for (int32 Index = 0; Index < NewNode->GetNumPins(); Index++)
	{
		auto& NewPin = NewNode->GetPin(Index);
		auto& OldPin = Node->GetPin(Index);

		OldPinsToNewPins.Add(&OldPin, &NewPin);
	}

	NewNode->BreakAllLinks();
}

void FVoxelGraphCompilerHelpers::DuplicateNodes(
	FVoxelGraphCompiler& Compiler, 
	const TSet<FVoxelCompilationNode*>& Nodes, 
	TMap<FVoxelCompilationPin*, FVoxelCompilationPin*>& OutOldPinsToNewPins,
	TMap<FVoxelCompilationNode*, FVoxelCompilationNode*>& OldNodesToNewNodes)
{
	TArray<FVoxelCompilationPinLink> LinksToCreate;

	// Duplicate all nodes
	for (auto& Node : Nodes)
	{
		DuplicateNodeAndQueueLinks(Compiler, Node, LinksToCreate, OutOldPinsToNewPins, OldNodesToNewNodes);
		if (Compiler.ErrorReporter.HasError())
		{
			return;
		}
	}

	// Link them back together
	for (auto& LinkToCreate : LinksToCreate)
	{
		auto* From = LinkToCreate.From;
		auto* To = LinkToCreate.To;

		check(!OutOldPinsToNewPins.Contains(From));

		auto** NewTo = OutOldPinsToNewPins.Find(To);
		if (NewTo)
		{
			To = *NewTo;
		}

		if (!From->IsLinkedTo(*To))
		{
			From->LinkTo(*To);
		}
	}
}

inline void GetSortedExecNodesImpl(FVoxelCompilationNode* Node, TArray<FVoxelCompilationNode*>& Nodes, TSet<FVoxelCompilationNode*>& VisitedNodes)
{
	if (!VisitedNodes.Contains(Node))
	{
		VisitedNodes.Add(Node);

		for (auto& Pin : Node->IteratePins<EVoxelPinIter::Output>())
		{
			if (Pin.PinCategory == EVoxelPinCategory::Exec)
			{
				for (auto& LinkedToPin : Pin.IterateLinkedTo())
				{
					GetSortedExecNodesImpl(&LinkedToPin.Node, Nodes, VisitedNodes);
				}
			}
		}

		Nodes.Add(Node);
	}
}

void FVoxelGraphCompilerHelpers::GetSortedExecNodes(FVoxelCompilationNode* FirstNode, TArray<FVoxelCompilationNode*>& OutNodes)
{
	TSet<FVoxelCompilationNode*> VisitedNodes;
	GetSortedExecNodesImpl(FirstNode, OutNodes, VisitedNodes);
}

void FVoxelGraphCompilerHelpers::GetAllSuccessors(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& OutNodes)
{
	if (OutNodes.Contains(Node))
	{
		return;
	}
	OutNodes.Add(Node);

	for (auto& Pin : Node->IteratePins<EVoxelPinIter::Output>())
	{
		for (auto& LinkedToPin : Pin.IterateLinkedTo())
		{
			GetAllSuccessors(&LinkedToPin.Node, OutNodes);
		}
	}
}

void FVoxelGraphCompilerHelpers::GetAllPredecessors(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& OutNodes)
{
	if (OutNodes.Contains(Node))
	{
		return;
	}
	OutNodes.Add(Node);

	for (auto& Pin : Node->IteratePins<EVoxelPinIter::Input>())
	{
		for (auto& LinkedToPin : Pin.IterateLinkedTo())
		{
			GetAllPredecessors(&LinkedToPin.Node, OutNodes);
		}
	}
}

void FVoxelGraphCompilerHelpers::GetAllDataSuccessors(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& OutNodes)
{
	if (OutNodes.Contains(Node))
	{
		return;
	}
	OutNodes.Add(Node);

	for (auto& Pin : Node->IteratePins<EVoxelPinIter::Output>())
	{
		if (Pin.PinCategory != EVoxelPinCategory::Exec)
		{
			for (auto& LinkedToPin : Pin.IterateLinkedTo())
			{
				GetAllDataSuccessors(&LinkedToPin.Node, OutNodes);
			}
		}
	}
}

void FVoxelGraphCompilerHelpers::GetAllExecSuccessors(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& OutNodes)
{
	if (OutNodes.Contains(Node))
	{
		return;
	}
	OutNodes.Add(Node);

	for (auto& Pin : Node->IteratePins<EVoxelPinIter::Output>())
	{
		if (Pin.PinCategory == EVoxelPinCategory::Exec)
		{
			for (auto& LinkedToPin : Pin.IterateLinkedTo())
			{
				GetAllExecSuccessors(&LinkedToPin.Node, OutNodes);
			}
		}
	}
}

inline bool HasSetterOrFunctionCallSuccessorImpl(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& VisitedNodes)
{
	if (VisitedNodes.Contains(Node))
	{
		return false;
	}
	VisitedNodes.Add(Node);

	if (Node->IsA<FVoxelSetterCompilationNode>() || Node->IsA<FVoxelFunctionCallCompilationNode>())
	{
		return true;
	}

	for (auto& Pin : Node->IteratePins<EVoxelPinIter::Output>())
	{
		if (Pin.PinCategory == EVoxelPinCategory::Exec)
		{
			for (auto& LinkedToPin : Pin.IterateLinkedTo())
			{
				if (HasSetterOrFunctionCallSuccessorImpl(&LinkedToPin.Node, VisitedNodes))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool FVoxelGraphCompilerHelpers::HasSetterOrFunctionCallSuccessor(FVoxelCompilationNode* Node)
{
	check(Node);
	TSet<FVoxelCompilationNode*> VisitedNodes;
	return HasSetterOrFunctionCallSuccessorImpl(Node, VisitedNodes);
}

void FVoxelGraphCompilerHelpers::GetAllUsedNodes(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& OutNodes)
{
	check(Node);
	if (OutNodes.Contains(Node))
	{
		return;
	}
	OutNodes.Add(Node);

	for (auto& Pin : Node->IteratePins<EVoxelPinIter::Input>())
	{
		if (Pin.PinCategory != EVoxelPinCategory::Exec)
		{
			for (auto& LinkedToPin : Pin.IterateLinkedTo())
			{
				GetAllUsedNodes(&LinkedToPin.Node, OutNodes);
			}
		}
	}
	for (auto& Pin : Node->IteratePins<EVoxelPinIter::Output>())
	{
		if (Pin.PinCategory == EVoxelPinCategory::Exec)
		{
			for (auto& LinkedToPin : Pin.IterateLinkedTo())
			{
				GetAllUsedNodes(&LinkedToPin.Node, OutNodes);
			}
		}
	}
}

// Won't add function parameters as dependencies
void GetDataDependencies(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& OutNodes)
{
	if (OutNodes.Contains(Node))
	{
		return;
	}
	OutNodes.Add(Node);

	for (auto& Pin : Node->IteratePins<EVoxelPinIter::Input>())
	{
		if (Pin.PinCategory != EVoxelPinCategory::Exec)
		{
			for (auto& LinkedToPin : Pin.IterateLinkedTo())
			{
				if (!LinkedToPin.Node.IsA<FVoxelFunctionInitCompilationNode>() && !LinkedToPin.Node.IsA<FVoxelFunctionSeparatorCompilationNode>())
				{
					check(!LinkedToPin.Node.IsA<FVoxelFunctionCallCompilationNode>());
					GetDataDependencies(&LinkedToPin.Node, OutNodes);
				}
			}
		}
	}
}

inline bool IsDataNodeSuccessorImpl(FVoxelCompilationNode* DataNode, FVoxelCompilationNode* PossibleSuccessor, TSet<FVoxelCompilationNode*>& VisitedNodes)
{
	if (DataNode == PossibleSuccessor)
	{
		return true;
	}
	if (VisitedNodes.Contains(PossibleSuccessor))
	{
		return false;
	}
	VisitedNodes.Add(PossibleSuccessor);

	for (auto& Pin : PossibleSuccessor->IteratePins<EVoxelPinIter::Input>())
	{
		if (Pin.PinCategory != EVoxelPinCategory::Exec)
		{
			for (auto& LinkedToPin : Pin.IterateLinkedTo())
			{
				if (IsDataNodeSuccessorImpl(DataNode, &LinkedToPin.Node, VisitedNodes))
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool FVoxelGraphCompilerHelpers::IsDataNodeSuccessor(FVoxelCompilationNode* DataNode, FVoxelCompilationNode* PossibleSuccessor)
{
	TSet<FVoxelCompilationNode*> VisitedNodes;
	return IsDataNodeSuccessorImpl(DataNode, PossibleSuccessor, VisitedNodes);
}

bool FVoxelGraphCompilerHelpers::AreAllNodePredecessorsChildOfStartNodeExecOnly(FVoxelCompilationNode* Node, FVoxelCompilationNode* StartNode, FVoxelCompilationNode*& FaultyNode)
{
	if (Node == StartNode)
	{
		return true;
	}

	bool bIsChild = false;

	for (auto& Pin : Node->IteratePins<EVoxelPinIter::Input>())
	{
		if (Pin.PinCategory == EVoxelPinCategory::Exec)
		{
			for (auto& LinkedToPin : Pin.IterateLinkedTo())
			{
				if (AreAllNodePredecessorsChildOfStartNodeExecOnly(&LinkedToPin.Node, StartNode, FaultyNode))
				{
					bIsChild = true;
				}
				else
				{
					return false;
				}
			}
		}
	}

	if (!bIsChild)
	{
		FaultyNode = Node;
	}

	return bIsChild;
}

void FVoxelGraphCompilerHelpers::GetFunctionNodes(FVoxelCompilationNode* StartNode, TSet<FVoxelCompilationNode*>& OutNodes)
{
	if (OutNodes.Contains(StartNode))
	{
		return;
	}
	OutNodes.Add(StartNode);

	// Go along data input & exec outputs

	for (auto& Pin : StartNode->IteratePins<EVoxelPinIter::Input>())
	{
		if (Pin.PinCategory != EVoxelPinCategory::Exec && Pin.NumLinkedTo() > 0)
		{
			check(Pin.NumLinkedTo() == 1);
			GetFunctionNodes(&Pin.GetLinkedTo(0).Node, OutNodes);
		}
	}

	// Stop on separators
	if (!StartNode->IsA<FVoxelFunctionSeparatorCompilationNode>())
	{
		check(!StartNode->IsA<FVoxelFunctionCallCompilationNode>());
		for (auto& Pin : StartNode->IteratePins<EVoxelPinIter::Output>())
		{
			if (Pin.PinCategory == EVoxelPinCategory::Exec && Pin.NumLinkedTo() > 0)
			{
				check(Pin.NumLinkedTo() == 1);
				GetFunctionNodes(&Pin.GetLinkedTo(0).Node, OutNodes);
			}
		}
	}
}

inline bool AreDataNodesSorted(const TArray<FVoxelCompilationNode*>& Nodes)
{
	TMap<FVoxelCompilationNode*, int32> Indices;
	for (int32 Index = 0; Index < Nodes.Num() ; Index++)
	{
		Indices.Add(Nodes[Index], Index);
	}
	for (int32 Index = 0; Index < Nodes.Num(); Index++)
	{
		TSet<FVoxelCompilationNode*> Dependencies;
		GetDataDependencies(Nodes[Index], Dependencies);
		for (auto& Dependency : Dependencies)
		{
			if (int32* DependencyIndex = Indices.Find(Dependency))
			{
				if (!ensure(*DependencyIndex <= Index))
				{
					return false;
				}
			}
		}
	}
	return true;
}

void FVoxelGraphCompilerHelpers::SortNodes(TArray<FVoxelCompilationNode*>& Nodes)
{
	VOXEL_FUNCTION_COUNTER();
	
	TMap<FVoxelCompilationNode*, int32> Order;
	for (auto& Node : Nodes)
	{
		Order.Add(Node, 0);
	}

	bool bChanged = true;
	while (bChanged)
	{
		bChanged = false;
		for (auto& Node : Nodes)
		{
			for (auto& Pin : Node->IteratePins<EVoxelPinIter::Input>())
			{
				for (auto& LinkedTo : Pin.IterateLinkedTo())
				{
					auto& OtherNode = LinkedTo.Node;
					if (Order.Contains(&OtherNode) && Order[&OtherNode] >= Order[Node])
					{
						Order[Node] = Order[&OtherNode] + 1;
						bChanged = true;
					}
				}
			}
		}
	}
	Algo::Sort(Nodes, [&](auto* NodeA, auto* NodeB) { return Order[NodeA] < Order[NodeB]; });
	checkVoxelSlow(AreDataNodesSorted(Nodes));
}

inline void AddPreviousNodesToSetImpl(FVoxelCompilationNode* ExecNode, FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& Nodes)
{
	if (Nodes.Contains(Node) || (ExecNode != Node && Node->IsExecNode())) // Ignore exec nodes different from the first one
	{
		return;
	}
	Nodes.Add(Node);

	for (int32 I = 0; I < Node->GetInputCount(); I++)
	{
		auto& Pin = Node->GetInputPin(I);

		if (Pin.PinCategory != EVoxelPinCategory::Exec)
		{
			for (auto& LinkedToPin : Pin.IterateLinkedTo())
			{
				AddPreviousNodesToSetImpl(ExecNode, &LinkedToPin.Node, Nodes);
			}
		}
	}
}

void FVoxelGraphCompilerHelpers::AddPreviousNodesToSet(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& Nodes)
{
	AddPreviousNodesToSetImpl(Node, Node, Nodes);
}

inline FVoxelCompilationNode* GetPreviousFlowMergeOrFunctionSeparatorNodeImpl(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& VisitedNodes)
{
	check(!Node->IsA<FVoxelFunctionInitCompilationNode>() && !Node->IsA<FVoxelFunctionCallCompilationNode>());
	if (Node->IsA<FVoxelFlowMergeCompilationNode>() || Node->IsA<FVoxelFunctionSeparatorCompilationNode>())
	{
		return Node;
	}
	if (VisitedNodes.Contains(Node))
	{
		return nullptr;
	}
	VisitedNodes.Add(Node);

	for (auto& Pin : Node->IteratePins<EVoxelPinIter::Input>())
	{
		if (Pin.PinCategory != EVoxelPinCategory::Exec)
		{
			for (auto& LinkedToPin : Pin.IterateLinkedTo())
			{
				if (auto* Result = GetPreviousFlowMergeOrFunctionSeparatorNodeImpl(&LinkedToPin.Node, VisitedNodes))
				{
					return Result;
				}
			}
		}
	}

	return nullptr;
}

FVoxelCompilationNode* FVoxelGraphCompilerHelpers::GetPreviousFlowMergeOrFunctionSeparatorNode(FVoxelCompilationNode* Node)
{
	TSet<FVoxelCompilationNode*> Nodes;
	return GetPreviousFlowMergeOrFunctionSeparatorNodeImpl(Node, Nodes);
}

TSet<FVoxelCompilationNode*> FVoxelGraphCompilerHelpers::GetAllExecSuccessorsAndTheirDataDependencies(FVoxelCompilationNode* FirstNode, bool bAddFirstNodeDataDependencies)
{
	TSet<FVoxelCompilationNode*> Successors;
	FVoxelGraphCompilerHelpers::GetAllExecSuccessors(FirstNode, Successors);
	TSet<FVoxelCompilationNode*> OutNodes = Successors;
	for (auto& Successor : Successors)
	{
		if (bAddFirstNodeDataDependencies || Successor != FirstNode)
		{
			TSet<FVoxelCompilationNode*> Nodes;
			GetDataDependencies(Successor, Nodes);
			OutNodes.Append(Nodes);
		}
	}
	return OutNodes;
}

TSet<FVoxelCompilationNode*> FVoxelGraphCompilerHelpers::GetAlwaysComputedNodes(FVoxelCompilationNode* Node)
{
	check(Node->IsExecNode());
	check(!Node->IsA<FVoxelFunctionSeparatorCompilationNode>() && !Node->IsA<FVoxelFunctionInitCompilationNode>());

	TArray<TSet<FVoxelCompilationNode*>> SuccessorsNodes;
	for (int32 Index = 0; Index < Node->GetOutputCount() ; Index++)
	{
		auto& OutputPin = Node->GetOutputPin(Index);
		if (OutputPin.PinCategory == EVoxelPinCategory::Exec)
		{
			if (OutputPin.NumLinkedTo() > 0)
			{
				check(OutputPin.NumLinkedTo() == 1);
				SuccessorsNodes.Add(GetAlwaysComputedNodes(&OutputPin.GetLinkedTo(0).Node));
			}
		}
	}

	// First add children nodes
	TSet<FVoxelCompilationNode*> OutNodes;
	if (SuccessorsNodes.Num() > 0)
	{
		OutNodes = SuccessorsNodes[0];
		for (int32 Index = 1; Index < SuccessorsNodes.Num(); Index++)
		{
			OutNodes = OutNodes.Intersect(SuccessorsNodes[Index]);
		}
	}

	// Then our own data dependencies
	GetDataDependencies(Node, OutNodes);
	OutNodes.Remove(Node);

	return OutNodes;
}

TSet<FVoxelCompilationNode*> FVoxelGraphCompilerHelpers::FilterHeads(const TSet<FVoxelCompilationNode*>& Nodes)
{
	TSet<FVoxelCompilationNode*> Result;
	for (auto* Node : Nodes)
	{
		TSet<FVoxelCompilationNode*> Successors;
		GetAllDataSuccessors(Node, Successors);
		Successors.Remove(Node);
		if (Successors.Intersect(Nodes).Num() == 0)
		{
			Result.Add(Node);
		}
	}
	return Result;
}

void FVoxelGraphCompilerHelpers::MovePin(FVoxelCompilationPin& From, FVoxelCompilationPin& To)
{
	MovePin(From, { &To });
}

void FVoxelGraphCompilerHelpers::MovePin(FVoxelCompilationPin& From, const TArray<FVoxelCompilationPin*>& To)
{
	for (auto& ToPin : To)
	{
		check(From.Direction == ToPin->Direction);
		for (auto& LinkedTo : From.IterateLinkedTo())
		{
			LinkedTo.LinkTo(*ToPin);
		}
		if (From.Direction == EVoxelPinDirection::Input)
		{
			ToPin->SetDefaultValue(From.GetDefaultValue());
		}
	}
	From.BreakAllLinks();
}

void FVoxelGraphCompilerHelpers::MoveInputPins(FVoxelCompilationNode& From, FVoxelCompilationNode& To)
{
	check(From.GetInputCount() == To.GetInputCount());
	for (int32 Index = 0; Index < From.GetInputCount() ; Index++)
	{
		auto& FromPin = From.GetInputPin(Index);
		auto& ToPin = To.GetInputPin(Index);
		MovePin(FromPin, ToPin);
	}
}

void FVoxelGraphCompilerHelpers::MoveOutputPins(FVoxelCompilationNode& From, FVoxelCompilationNode& To)
{
	check(From.GetOutputCount() == To.GetOutputCount());
	for (int32 Index = 0; Index < From.GetOutputCount() ; Index++)
	{
		auto& FromPin = From.GetOutputPin(Index);
		auto& ToPin = To.GetOutputPin(Index);
		MovePin(FromPin, ToPin);
	}
}
