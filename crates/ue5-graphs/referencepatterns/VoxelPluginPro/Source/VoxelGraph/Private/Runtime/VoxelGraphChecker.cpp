// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Runtime/VoxelGraphChecker.h"
#include "Runtime/VoxelGraph.h"
#include "Runtime/VoxelGraphFunction.h"
#include "Runtime/VoxelComputeNodeTree.h"
#include "Runtime/VoxelComputeNode.h"
#include "Runtime/VoxelDefaultComputeNodes.h"
#include "VoxelGraphErrorReporter.h"

struct FVoxelDefinition
{
	const FVoxelComputeNode* Node = nullptr;
	int32 PinIndex = 0;

	FVoxelDefinition() = default;
	FVoxelDefinition(const FVoxelComputeNode* Node, int32 PinIndex)
		: Node(Node)
		, PinIndex(PinIndex)
	{
	}

	inline bool IsValid() const
	{
		return Node != nullptr;
	}
};

struct FVoxelFrames
{
	FVoxelFrames() = default;

	inline void Push()
	{
		Definitions.Emplace();
	}
	inline void Pop()
	{
		Definitions.Pop();
	}

	void AddDefinition(int32 Id, const FVoxelDefinition& Definition)
	{
		ensure(Definition.IsValid());
		auto& CurrentDefinitions = Definitions.Last();
		if (Id >= CurrentDefinitions.Num())
		{
			CurrentDefinitions.SetNum(Id + 1);
		}
		check(!FindDefinition(Id).IsValid());
		CurrentDefinitions[Id] = Definition;
	}

	FVoxelDefinition FindDefinition(int32 Id) const
	{
		for (int32 Index = Definitions.Num() - 1; Index >= 0; Index--)
		{
			if (Definitions[Index].IsValidIndex(Id))
			{
				const FVoxelDefinition& Definition = Definitions[Index][Id];
				if (Definition.IsValid())
				{
					return Definition;
				}
			}
		}
		return {};
	}

private:
	TArray<TArray<FVoxelDefinition>> Definitions;
};

inline void CheckNode(const FVoxelComputeNode& Node, FVoxelGraphErrorReporter& ErrorReporter, FVoxelFrames& Frames, bool bIsInit)
{
	for (int32 Index = 0; Index < Node.InputCount; Index++)
	{
		if ((Node.GetInputCategory(Index) == EVoxelPinCategory::Seed) == bIsInit)
		{
			const int32 InputId = Node.GetInputId(Index);
			if (InputId != -1 && !Frames.FindDefinition(InputId).IsValid())
			{
				ErrorReporter.AddMessageToNode(&Node, FString::Printf(TEXT("input pin %d is using an undefined variable!"), Index), EVoxelGraphNodeMessageType::Error);
			}
		}
	}
	for (int32 Index = 0; Index < Node.OutputCount; Index++)
	{
		if ((Node.GetOutputCategory(Index) == EVoxelPinCategory::Seed) == bIsInit)
		{
			const int32 OutputId = Node.GetOutputId(Index);
			if (OutputId < 0)
			{
				check(OutputId == -1);
				// unused output
			}
			else
			{
				auto PreviousDefinition = Frames.FindDefinition(OutputId);
				if (PreviousDefinition.IsValid())
				{
					// Functions init are allowed to redefine variables, and seed too as long as it's the same node
					if (!(Node.Type == EVoxelComputeNodeType::Exec && static_cast<const FVoxelExecComputeNode&>(Node).ExecType == EVoxelComputeNodeExecType::FunctionInit) &&
						!(Node.Type == EVoxelComputeNodeType::Seed && PreviousDefinition.Node == &Node))
					{
						ErrorReporter.AddMessageToNode(&Node, FString::Printf(TEXT("output pin %d is redefining a variable! definition id: %d"), Index, OutputId), EVoxelGraphNodeMessageType::Error);
						ErrorReporter.AddMessageToNode(PreviousDefinition.Node, FString::Printf(TEXT("output pin %d definition id: %d"), PreviousDefinition.PinIndex, OutputId), EVoxelGraphNodeMessageType::Info, false);
					}
				}
				else
				{
					Frames.AddDefinition(OutputId, FVoxelDefinition(&Node, Index));
				}
			}
		}
	}
}

inline void CheckTree(
	const FVoxelComputeNodeTree& Tree,
	FVoxelGraphErrorReporter& InitErrorReporter,
	FVoxelGraphErrorReporter& ComputeErrorReporter,
	FVoxelFrames& InitFrames,
	FVoxelFrames& ComputeFrames,
	const TArray<bool>& Branches,
	int32 Depth,
	TSet<const FVoxelGraphFunction*>& OutFunctionsCalled)
{
	InitFrames.Push();
	ComputeFrames.Push();

	// Init
	for (auto* SeedNode : Tree.GetSeedNodes())
	{
		check(SeedNode);
		CheckNode(*SeedNode, InitErrorReporter, InitFrames, true);
	}
	for (auto* DataNode : Tree.GetDataNodes())
	{
		check(DataNode);
		CheckNode(*DataNode, InitErrorReporter, InitFrames, true);
	}

	// Compute
	for (auto* DataNode : Tree.GetDataNodes())
	{
		check(DataNode);
		CheckNode(*DataNode, ComputeErrorReporter, ComputeFrames, false);
	}
	if (auto* ExecNode = Tree.GetExecNode())
	{
		CheckNode(*ExecNode, ComputeErrorReporter, ComputeFrames, false);
	}

	// Children
	auto& Children = Tree.GetChildren();
	check(Children.Num() <= 2);
	if (Children.Num() > 0)
	{
		auto& Child = Children.Num() == 1 ? Children[0] : Children[Branches[Depth]];
		CheckTree(Child, InitErrorReporter, ComputeErrorReporter, InitFrames, ComputeFrames, Branches, Children.Num() == 1 ? Depth : (Depth + 1), OutFunctionsCalled);
	}

	// Find functions
	if (auto* ExecNode = Tree.GetExecNode())
	{
		if (ExecNode->ExecType == EVoxelComputeNodeExecType::FunctionCall)
		{
			auto* FunctionCall = static_cast<const FVoxelFunctionCallComputeNode*>(ExecNode);
			OutFunctionsCalled.Add(FunctionCall->GetFunction());
			check(Tree.GetChildren().Num() == 0);
		}
	}
}

inline void CheckFunction(
	const FVoxelGraphFunction& Function, 
	FVoxelGraphErrorReporter& ErrorReporter, 
	FVoxelFrames& InitFrames, 
	FVoxelFrames& ComputeFrames,
	const TArray<bool>& Branches)
{
	TSet<const FVoxelGraphFunction*> FunctionsCalled;
	{
		FVoxelGraphErrorReporter FunctionErrorReporter(ErrorReporter, "function " + FString::FromInt(Function.FunctionId));
		FVoxelGraphErrorReporter InitErrorReporter(FunctionErrorReporter, "init");
		FVoxelGraphErrorReporter ComputeErrorReporter(FunctionErrorReporter, "compute");
		CheckTree(Function.GetTree(), InitErrorReporter, ComputeErrorReporter, InitFrames, ComputeFrames, Branches, 0, FunctionsCalled);
	}
	check(!FunctionsCalled.Contains(&Function));
	for (auto* FunctionCalled : FunctionsCalled)
	{
		EVoxelFunctionAxisDependencies Dependencies = FunctionCalled->Dependencies;
		switch (Function.Dependencies)
		{
		case EVoxelFunctionAxisDependencies::X:
			check(Dependencies == EVoxelFunctionAxisDependencies::X);
			break;
		case EVoxelFunctionAxisDependencies::XYWithCache:
			check(Dependencies == EVoxelFunctionAxisDependencies::XYWithCache || Dependencies == EVoxelFunctionAxisDependencies::XYWithoutCache);
			break;
		case EVoxelFunctionAxisDependencies::XYWithoutCache:
			check(Dependencies == EVoxelFunctionAxisDependencies::XYWithoutCache);
			break;
		case EVoxelFunctionAxisDependencies::XYZWithCache:
			check(Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache || Dependencies == EVoxelFunctionAxisDependencies::XYZWithoutCache);
			break;
		case EVoxelFunctionAxisDependencies::XYZWithoutCache:
			check(Dependencies == EVoxelFunctionAxisDependencies::XYZWithoutCache);
			break;
		default:
			check(false);
		}
	}
}

void InitVoxelFrames(
	FVoxelGraphErrorReporter& ErrorReporter,
	FVoxelFrames& InitFrames,
	FVoxelFrames& ComputeFrames,
	const FVoxelGraph& Graph)
{
	InitFrames.Push();
	ComputeFrames.Push();
	
	// Init
	FVoxelGraphErrorReporter InitErrorReporter(ErrorReporter, "constant init");
	for (auto& SeedNode : Graph.SeedComputeNodes)
	{
		CheckNode(*SeedNode, InitErrorReporter, InitFrames, true);
	}
	for (auto& DataNode : Graph.ConstantComputeNodes)
	{
		CheckNode(*DataNode, InitErrorReporter, InitFrames, true);
	}

	// Compute
	FVoxelGraphErrorReporter ComputeErrorReporter(ErrorReporter, "constant compute");
	for (auto& DataNode : Graph.ConstantComputeNodes)
	{
		CheckNode(*DataNode, ComputeErrorReporter, ComputeFrames, false);
	}
}
inline int32 GetNumBranches(const FVoxelComputeNodeTree& Tree)
{
	int32 Num = 0;
	auto& Children = Tree.GetChildren();
	if (Children.Num() > 1)
	{
		check(Children.Num() == 2);
		Num++;
	}
	int32 MaxChild = 0;
	for (auto& Child : Children)
	{
		MaxChild = FMath::Max(MaxChild, GetNumBranches(Child));
	}
	return Num + MaxChild;
}

inline bool IncreaseBranches(TArray<bool>& Branches)
{
	for (int32 Index = 0; Index < Branches.Num() ; Index++)
	{
		if (!Branches[Index])
		{
			Branches[Index] = true;
			return true;
		}
		else
		{
			Branches[Index] = false;
		}
	}
	return false;
}

void FVoxelGraphChecker::CheckGraph(FVoxelGraphErrorReporter& ErrorReporter, const FVoxelGraph& Graph)
{
	for (auto& Functions : Graph.AllFunctions)
	{
		TArray<bool> Branches;
		Branches.SetNum(GetNumBranches(Functions.FunctionXYZWithoutCache->GetTree()));
		int32 Iterations = 0;
		do
		{
			Iterations++;
			{
				FVoxelFrames InitFrames;
				FVoxelFrames ComputeFrames;
				InitVoxelFrames(ErrorReporter, InitFrames, ComputeFrames, Graph);
				TSet<const FVoxelGraphFunction*> FunctionsAlreadyCalled;
				{
					FVoxelGraphErrorReporter LocalErrorReporter(ErrorReporter, "X");
					CheckFunction(Functions.Get(EVoxelFunctionAxisDependencies::X), LocalErrorReporter, InitFrames, ComputeFrames, Branches);
				}
				{
					FVoxelGraphErrorReporter LocalErrorReporter(ErrorReporter, "XYWithCache");
					CheckFunction(Functions.Get(EVoxelFunctionAxisDependencies::XYWithCache), LocalErrorReporter, InitFrames, ComputeFrames, Branches);
				}
				{
					FVoxelGraphErrorReporter LocalErrorReporter(ErrorReporter, "XYZWithCache");
					CheckFunction(Functions.Get(EVoxelFunctionAxisDependencies::XYZWithCache), LocalErrorReporter, InitFrames, ComputeFrames, Branches);
				}
			}
			{
				FVoxelFrames InitFrames;
				FVoxelFrames ComputeFrames;
				InitVoxelFrames(ErrorReporter, InitFrames, ComputeFrames, Graph);
				TSet<const FVoxelGraphFunction*> FunctionsAlreadyCalled;
				{
					FVoxelGraphErrorReporter LocalErrorReporter(ErrorReporter, "XYWithoutCache");
					CheckFunction(Functions.Get(EVoxelFunctionAxisDependencies::XYWithoutCache), LocalErrorReporter, InitFrames, ComputeFrames, Branches);
				}
				{
					FVoxelGraphErrorReporter LocalErrorReporter(ErrorReporter, "XYZWithCache");
					CheckFunction(Functions.Get(EVoxelFunctionAxisDependencies::XYZWithCache), LocalErrorReporter, InitFrames, ComputeFrames, Branches);
				}
			}
			{
				FVoxelFrames InitFrames;
				FVoxelFrames ComputeFrames;
				InitVoxelFrames(ErrorReporter, InitFrames, ComputeFrames, Graph);
				TSet<const FVoxelGraphFunction*> FunctionsAlreadyCalled;
				{
					FVoxelGraphErrorReporter LocalErrorReporter(ErrorReporter, "XYZWithoutCache");
					CheckFunction(Functions.Get(EVoxelFunctionAxisDependencies::XYZWithoutCache), LocalErrorReporter, InitFrames, ComputeFrames, Branches);
				}
			}
		} while (IncreaseBranches(Branches));
		check(Iterations == 1 << Branches.Num());
	}
}
