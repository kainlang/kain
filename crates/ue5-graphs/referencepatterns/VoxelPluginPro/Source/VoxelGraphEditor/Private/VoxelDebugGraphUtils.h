// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelNode.h"
#include "Compilation/VoxelCompilationNode.h"
#include "VoxelDebugGraphUtils.generated.h"

class FVoxelGraphCompiler;
class UVoxelGraphGenerator;

UCLASS(NotPlaceable)
class UVoxelDebugNode : public UVoxelNode
{	
	GENERATED_BODY()

public:
	TSharedPtr<FVoxelCompilationNode> CompilationNode;
	FLinearColor Color = FLinearColor::Black;
	FText Name;
	FText Tooltip;
	bool bIsCompact = false;

public:
	int32 GetMaxInputPins() const override { return CompilationNode->GetInputCount(); }
	int32 GetMinInputPins() const override { return CompilationNode->GetInputCount(); }
	int32 GetOutputPinsCount() const override { return CompilationNode->GetOutputCount(); }

	FLinearColor GetColor() const override { return Color; }
	FText GetTitle() const override { return Name; }
	FText GetTooltip() const override { return Tooltip; }
	bool IsCompact() const override { return bIsCompact; }

	FName GetInputPinName(int32 PinIndex) const override { return CompilationNode->GetInputPin(PinIndex).Name; }
	FName GetOutputPinName(int32 PinIndex) const override { return CompilationNode->GetOutputPin(PinIndex).Name; }

	EVoxelPinCategory GetInputPinCategory(int32 PinIndex) const override { return CompilationNode->GetInputPin(PinIndex).PinCategory; }
	EVoxelPinCategory GetOutputPinCategory(int32 PinIndex) const override { return CompilationNode->GetOutputPin(PinIndex).PinCategory; }

	FString GetInputPinDefaultValue(int32 PinIndex) const override { return CompilationNode->GetInputPin(PinIndex).GetDefaultValue(); }
		
	bool CanUserDeleteNode() const override { return false; }
	bool CanDuplicateNode() const override { return false; }
};

namespace FVoxelDebugGraphUtils
{
	void DebugNodes(const TSet<FVoxelCompilationNode*>& Nodes, UVoxelGraphGenerator* Generator);
}
