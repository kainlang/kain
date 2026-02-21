// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Compilation/Passes/VoxelFunctionsPass.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Compilation/VoxelCompilationNode.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelNode.h"

struct FVoxelFunctionTree
{
	FVoxelFunctionSeparatorCompilationNode& Separator;

	FVoxelFunctionTree* const Parent;
	TArray<TUniquePtr<FVoxelFunctionTree>> Children;
	
	TSet<FVoxelCompilationNode*> Nodes;

	struct FPinToForward
	{
		FVoxelCompilationPin* From;
		FVoxelCompilationPin* To;
	};
	TArray<FPinToForward> PinsToForward;
	// Pin in PinsToForward.To to separator output pin
	// Use To because it's unique, unlike From
	TMap<FVoxelCompilationPin*, FVoxelCompilationPin*> ForwardedPins;
		
public:
	explicit FVoxelFunctionTree(FVoxelFunctionSeparatorCompilationNode& Separator, FVoxelFunctionTree* Parent)
		: Separator(Separator)
		, Parent(Parent)
	{
		TArray<FVoxelCompilationNode*> ExecNodeQueue;
		ExecNodeQueue.Add(&Separator);
		while (ExecNodeQueue.Num() > 0)
		{
			auto* ExecNode = ExecNodeQueue.Pop(UE_505_SWITCH(false, EAllowShrinking::No));
			for (auto& OutputPin : ExecNode->IteratePins<EVoxelPinIter::Output>())
			{
				if (OutputPin.PinCategory == EVoxelPinCategory::Exec && OutputPin.NumLinkedTo() > 0)
				{
					check(OutputPin.NumLinkedTo() == 1);
					auto& OtherNode = OutputPin.GetLinkedTo(0).Node;

					if (auto* OtherFunctionSeparator = CastVoxel<FVoxelFunctionSeparatorCompilationNode>(&OtherNode))
					{
						bool bAlreadyAdded = false;
						for (auto& Child : Children)
						{
							if (&Child->Separator == OtherFunctionSeparator)
							{
								bAlreadyAdded = true;
								break;
							}
						}
						if (!bAlreadyAdded)
						{
							Children.Emplace(MakeUnique<FVoxelFunctionTree>(*OtherFunctionSeparator, this));
						}
					}
					else
					{
						ExecNodeQueue.Add(&OtherNode);
					}
				}
			}
		}
	}

	void Validate(FVoxelGraphErrorReporter& ErrorReporter, TMap<FVoxelFunctionSeparatorCompilationNode*, FVoxelFunctionSeparatorCompilationNode*>& Parents)
	{
		for (auto& Child : Children)
		{
			auto*& ExistingParent = Parents.FindOrAdd(&Child->Separator);
			if (ExistingParent)
			{
				ErrorReporter.AddError("separator has multiple separators in its predecessors that are not following each others");
				ErrorReporter.AddMessageToNode(ExistingParent, "separator A", EVoxelGraphNodeMessageType::Info);
				ErrorReporter.AddMessageToNode(&Separator, "separator B", EVoxelGraphNodeMessageType::Info);
				ErrorReporter.AddMessageToNode(&Child->Separator, "separator is child of both A and B", EVoxelGraphNodeMessageType::Error);
				return;
			}
			else
			{
				ExistingParent = &Separator;
			}
		}
		
		for (auto& Child : Children)
		{
			Child->Validate(ErrorReporter, Parents);
		}
	}
	
public:
	void FindUsedNodes(const TSet<FVoxelCompilationNode*>& ParentsNodes)
	{
		auto& OutputPin = Separator.GetOutputPin(0);
		if (OutputPin.NumLinkedTo() > 0)
		{
			FVoxelGraphCompilerHelpers::GetFunctionNodes(&OutputPin.GetLinkedTo(0).Node, Nodes);
		}

		for (auto* Node : ParentsNodes)
		{
			Nodes.Remove(Node);
		}

		auto ParentsNodesCopy = ParentsNodes;
		ParentsNodesCopy.Append(Nodes);
		for (auto& Child : Children)
		{
			Child->FindUsedNodes(ParentsNodesCopy);
		}
	}
	
	void FindPinsToForward()
	{
		for (auto* Node : Nodes)
		{
			for (auto& InputPin : Node->IteratePins<EVoxelPinIter::Input>())
			{
				if (InputPin.PinCategory != EVoxelPinCategory::Exec && InputPin.NumLinkedTo() > 0)
				{
					check(InputPin.NumLinkedTo() == 1);
					auto& LinkedTo = InputPin.GetLinkedTo(0);
					auto* LinkedToNode = &LinkedTo.Node;
					if (!Nodes.Contains(LinkedToNode))
					{
						// If this pin is linked to a node we don't own, we need to ask for that pin to be forwarded
						bool bFound = false;
						for (auto* It = this; It; It = It->Parent)
						{
							if (It->Nodes.Contains(LinkedToNode))
							{
								// No need to go further up
								bFound = true;
								break;
							}
							else
							{
								It->PinsToForward.Add({ &LinkedTo, &InputPin });
							}
						}
						ensure(bFound);
					}
				}
			}
		}
		
		for (auto& Child : Children)
		{
			Child->FindPinsToForward();
		}
	}

	void CreatePins(FVoxelGraphCompiler& Compiler)
	{
		TArray<EVoxelPinCategory> PinCategories;
		PinCategories.Add(EVoxelPinCategory::Exec);
		for (auto& PinToForward : PinsToForward)
		{
			ensure(PinToForward.From->PinCategory == PinToForward.To->PinCategory);
			PinCategories.Add(PinToForward.From->PinCategory);
		}

		// Create new separator
		auto* NewSeparator = Compiler.AddNode(MakeShared<FVoxelFunctionSeparatorCompilationNode>(Separator.Node, PinCategories, PinCategories));
		NewSeparator->SourceNodes = Separator.SourceNodes;

		// Forward exec pin
		FVoxelGraphCompilerHelpers::MovePin(Separator.GetInputPin(0), NewSeparator->GetInputPin(0));
		FVoxelGraphCompilerHelpers::MovePin(Separator.GetOutputPin(0), NewSeparator->GetOutputPin(0));

		// Forward pins
		int32 PinIndex = 1;
		for (auto& PinToForward : PinsToForward)
		{			
			auto& InputPin = NewSeparator->GetInputPin(PinIndex);
			auto& OutputPin = NewSeparator->GetOutputPin(PinIndex);
			PinIndex++;

			// TRICKY: the separators are at the _start_, so to link the input pin we must look in the parent nodes
			if (Parent && Parent->Nodes.Contains(&PinToForward.From->Node))
			{
				// This is one of our parent nodes, so directly create the link
				InputPin.LinkTo(*PinToForward.From);
			}
			else
			{
				// We need to find our parent pin
				check(Parent);
				InputPin.LinkTo(*Parent->ForwardedPins[PinToForward.To]);
			}

			if (Nodes.Contains(&PinToForward.To->Node))
			{
				// It's forwarded to one of our nodes, so just link it
				PinToForward.From->BreakLinkTo(*PinToForward.To);
				OutputPin.LinkTo(*PinToForward.To);
			}
			else
			{
				// Queue it for children to link
				ForwardedPins.Add(PinToForward.To, &OutputPin);
			}
		}
		
		// Delete our old node
		if (Compiler.FirstNode == &Separator)
		{
			Compiler.FirstNode = NewSeparator;
			ensure(Compiler.FirstNodePinIndex == 0);
		}
		Separator.BreakAllLinks();
		Compiler.RemoveNode(&Separator);

		// Call children
		for (auto& Child : Children)
		{
			Child->CreatePins(Compiler);
		}
	}
};

void FVoxelFillFunctionSeparatorsPass::Apply(FVoxelGraphCompiler& Compiler)
{
	auto& FirstSeparator = CastCheckedVoxel<FVoxelFunctionSeparatorCompilationNode>(Compiler.FirstNode);
	FVoxelFunctionTree Tree(FirstSeparator, nullptr);

	{
		TMap<FVoxelFunctionSeparatorCompilationNode*, FVoxelFunctionSeparatorCompilationNode*> Parents;
		Tree.Validate(Compiler.ErrorReporter, Parents);

		if (Compiler.ErrorReporter.HasError())
		{
			return;
		}
	}
	
	Tree.FindUsedNodes({});
	Tree.FindPinsToForward();
	Tree.CreatePins(Compiler);
}

void FVoxelFindFunctionsPass::Apply(FVoxelGraphCompiler& Compiler, TArray<FVoxelCompilationFunctionDescriptor>& OutFunctions)
{
	check(Compiler.FirstNode);
	check(Compiler.FirstNode->IsA<FVoxelFunctionSeparatorCompilationNode>());

	for (auto& Node : Compiler.GetAllNodes())
	{
		if (auto* FunctionCall = CastVoxel<FVoxelFunctionSeparatorCompilationNode>(Node))
		{
			OutFunctions.Add(FVoxelCompilationFunctionDescriptor(FunctionCall->FunctionId, FunctionCall));
		}
	}

	for (auto& Function : OutFunctions)
	{
		Function.Nodes.Add(Function.FirstNode);

		auto& OutputPin = Function.FirstNode->GetOutputPin(0);
		if (OutputPin.NumLinkedTo() == 0)
		{
			continue;
		}
		check(OutputPin.NumLinkedTo() == 1);

		FVoxelGraphCompilerHelpers::GetFunctionNodes(&OutputPin.GetLinkedTo(0).Node, Function.Nodes);
	}

	TSet<FVoxelCompilationNode*> FunctionSeparators;
	for (auto& Function : OutFunctions)
	{
		FunctionSeparators.Add(Function.FirstNode);
	}

	for (auto& FunctionA : OutFunctions)
	{
		for (auto& FunctionB : OutFunctions)
		{
			if (FunctionA.FirstNode != FunctionB.FirstNode)
			{
				auto IntersectionNodes = FunctionA.Nodes.Intersect(FunctionB.Nodes).Difference(FunctionSeparators);
				if (IntersectionNodes.Num() > 0)
				{
					auto IntersectionNodesHead = FVoxelGraphCompilerHelpers::FilterHeads(IntersectionNodes);
					Compiler.ErrorReporter.AddError("INTERNAL ERROR: Nodes outputs are used in different functions! Try moving the function separators somewhere else");

					for (auto& Node : IntersectionNodesHead)
					{
						Compiler.ErrorReporter.AddMessageToNode(Node, "Node is used by FunctionA and FunctionB", EVoxelGraphNodeMessageType::Error);
					}

					auto NodesAToShow = FunctionA.Nodes.Difference(IntersectionNodes).Difference(FunctionSeparators);
					auto NodesBToShow = FunctionB.Nodes.Difference(IntersectionNodes).Difference(FunctionSeparators);
					bool bNodesAShown = false;
					bool bNodesBShown = false;
					for (auto& Node : NodesAToShow)
					{
						if (Node->IsLinkedToOne(IntersectionNodesHead))
						{
							bNodesAShown = true;
							Compiler.ErrorReporter.AddMessageToNode(Node, "FunctionA node", EVoxelGraphNodeMessageType::Info);
						}
					}
					for (auto& Node : NodesBToShow)
					{
						if (Node->IsLinkedToOne(IntersectionNodesHead))
						{
							bNodesBShown = true;
							Compiler.ErrorReporter.AddMessageToNode(Node, "FunctionB node", EVoxelGraphNodeMessageType::Info);
						}
					}
					if (!bNodesAShown)
					{
						for (auto& Node : NodesAToShow)
						{
							Compiler.ErrorReporter.AddMessageToNode(Node, "FunctionA node", EVoxelGraphNodeMessageType::Info);
						}
					}
					if (!bNodesBShown)
					{
						for (auto& Node : NodesBToShow)
						{
							Compiler.ErrorReporter.AddMessageToNode(Node, "FunctionB node", EVoxelGraphNodeMessageType::Info);
						}
					}
					auto* Separator = FVoxelGraphCompilerHelpers::IsDataNodeSuccessor(FunctionA.FirstNode, FunctionB.FirstNode) ? FunctionB.FirstNode : FunctionA.FirstNode;
					Compiler.ErrorReporter.AddMessageToNode(Separator, "separator", EVoxelGraphNodeMessageType::Info);
					return;
				}
			}
		}
	}
}

void FVoxelRemoveNodesOutsideFunction::Apply(FVoxelGraphCompiler& Compiler, TSet<FVoxelCompilationNode*>& FunctionNodes)
{
	for (auto& Node : Compiler.GetAllNodesCopy())
	{
		if (!FunctionNodes.Contains(Node))
		{
			Node->BreakAllLinks();
			Compiler.RemoveNode(Node);
		}
	}
}

void FVoxelAddFirstFunctionPass::Apply(FVoxelGraphCompiler& Compiler)
{
	if (Compiler.FirstNode)
	{
		auto& InputPin = Compiler.FirstNode->GetInputPin(Compiler.FirstNodePinIndex);
		InputPin.BreakAllLinks();

		TSharedRef<FVoxelCompilationNode> FunctionNode = MakeShareable(
			new FVoxelFunctionSeparatorCompilationNode(
				*GetDefault<UVoxelNode>(),
				{ EVoxelPinCategory::Exec },
				{ EVoxelPinCategory::Exec }));

		Compiler.AddNode(FunctionNode);
		FunctionNode->GetOutputPin(0).LinkTo(InputPin);
		Compiler.FirstNode = &FunctionNode.Get();
		Compiler.FirstNodePinIndex = 0;
	}
}

void FVoxelReplaceFunctionSeparatorsPass::Apply(FVoxelGraphCompiler& Compiler)
{
	check(Compiler.FirstNode);

	// Replace first node
	{
		auto* OldNode = Compiler.FirstNode;
		auto* NewNode = Compiler.AddNode(MakeShared<FVoxelFunctionInitCompilationNode>(CastCheckedVoxel<FVoxelFunctionSeparatorCompilationNode>(OldNode)));
		Compiler.FirstNode = NewNode;

		FVoxelGraphCompilerHelpers::MoveOutputPins(*OldNode, *NewNode);
		FVoxelGraphCompilerHelpers::BreakNodeLinks<EVoxelPinIter::Input>(*OldNode);
		OldNode->CheckIsNotLinked(Compiler.ErrorReporter);
		Compiler.RemoveNode(OldNode);
	}

	// Replace function calls
	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		if (auto* Separator = CastVoxel<FVoxelFunctionSeparatorCompilationNode>(Node))
		{
			auto* NewNode = Compiler.AddNode(MakeShared<FVoxelFunctionCallCompilationNode>(*Separator));
			FVoxelGraphCompilerHelpers::MoveInputPins(*Separator, *NewNode);
			FVoxelGraphCompilerHelpers::BreakNodeLinks<EVoxelPinIter::Output>(*Separator);
			Separator->CheckIsNotLinked(Compiler.ErrorReporter);
			Compiler.RemoveNode(Separator);
		}
	}
}
