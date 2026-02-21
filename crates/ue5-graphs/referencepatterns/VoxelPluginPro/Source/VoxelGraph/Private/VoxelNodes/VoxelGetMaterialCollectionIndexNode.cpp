// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelGetMaterialCollectionIndexNode.h"
#include "VoxelNodes/VoxelNodeVariables.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "CppTranslation/VoxelVariables.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "NodeFunctions/VoxelNodeFunctions.h"
#include "VoxelGenerators/VoxelGeneratorInit.h"
#include "VoxelRender/MaterialCollections/VoxelMaterialCollectionBase.h"
#include "VoxelGraphGenerator.h"

#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "UObject/Package.h"
#include "AssetRegistry/AssetData.h"
#include "VoxelGraphPreviewSettings.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"

UVoxelNode_GetMaterialCollectionIndex::UVoxelNode_GetMaterialCollectionIndex()
{
	SetOutputs(EVoxelPinCategory::Int);
}

FText UVoxelNode_GetMaterialCollectionIndex::GetTitle() const
{
	return FText::Format(VOXEL_LOCTEXT("Get Material Collection Index: {0}"), Super::GetTitle());
}

UObject* UVoxelNode_GetMaterialCollectionIndex::GetAsset() const
{
	return Material;
}

UClass* UVoxelNode_GetMaterialCollectionIndex::GetAssetClass() const
{
	return UObject::StaticClass();
}

void UVoxelNode_GetMaterialCollectionIndex::SetAsset(UObject* Object)
{
	Material = Cast<UMaterialInterface>(Object);

	UpdatePreview(false);
}

bool UVoxelNode_GetMaterialCollectionIndex::ShouldFilterAsset(const FAssetData& Asset) const
{
	if (!ensure(Graph) || !Graph->PreviewSettings->MaterialCollection)
	{
		return true;
	}

	return Graph->PreviewSettings->MaterialCollection->GetMaterialIndex(Asset.AssetName) == -1;
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_GetMaterialCollectionIndex::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_GetMaterialCollectionIndex& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Name(Node.GetParameter<UMaterialInterface*>() ? Node.GetParameter<UMaterialInterface*>() ->GetFName() : "")
			, IndexVariable("int32", UniqueName.ToString() + "_Index")
			, ExposedVariable(MakeShared<FVoxelMaterialInterfaceVariable>(Node, Node.Material))
		{
		}

		virtual void Init(FVoxelGraphSeed Inputs[], const FVoxelGeneratorInit& InitStruct) override
		{
			if (InitStruct.MaterialCollection)
			{
				Index = InitStruct.MaterialCollection->GetMaterialIndex(Name);
				if (Index == -1)
				{
					TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning);
					Message->AddToken(FTextToken::Create(FText::Format(VOXEL_LOCTEXT("GetMaterialCollectionIndex: Material {0} not found in "), FText::FromName(Name))));
					Message->AddToken(FUObjectToken::Create(InitStruct.MaterialCollection));

					Message->AddToken(FTextToken::Create(VOXEL_LOCTEXT("Graph:")));
					
					for (auto& SourceNode : SourceNodes)
					{
						if (SourceNode.IsValid())
						{
							Message->AddToken(FUObjectToken::Create(SourceNode->Graph));
						}
					}
					if (Node.IsValid())
					{
						FVoxelGraphErrorReporter ErrorReporter(Node->Graph);
						ErrorReporter.AddMessageToNode(Node.Get(), "material index not found", EVoxelGraphNodeMessageType::Warning);
						ErrorReporter.Apply(false);
					}
					FMessageLog("PIE").AddMessage(Message);
				}
			}
			else
			{
				if (Node.IsValid())
				{
					FVoxelGraphErrorReporter ErrorReporter(Node->Graph);
					ErrorReporter.AddMessageToNode(Node.Get(), "material collection is null\nYou can set in the Preview Settings", EVoxelGraphNodeMessageType::Warning);
					ErrorReporter.Apply(false);
				}
			}
		}
		virtual void InitCpp(const TArray<FString> & Inputs, FVoxelCppConstructor & Constructor) const override
		{
			Constructor.AddLinef(TEXT("if (%s.MaterialCollection)"), *FVoxelCppIds::InitStruct);
			Constructor.StartBlock();
			Constructor.AddLinef(TEXT("%s = %s.MaterialCollection->GetMaterialIndex(%s);"), *IndexVariable.CppName, *FVoxelCppIds::InitStruct, *ExposedVariable->CppName);
			Constructor.EndBlock();
			Constructor.AddLine("else");
			Constructor.StartBlock();
			Constructor.AddLinef(TEXT("%s = -1;"), *IndexVariable.CppName);
			Constructor.EndBlock();
		}

		virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<int32>() = Index;
		}
		virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<int32>() = Index;
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s;"), *Outputs[0], *IndexVariable.CppName);
		}
		virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}

		virtual void SetupCpp(FVoxelCppConfig& Config) const override
		{
			Config.AddExposedVariable(ExposedVariable);
		}
		virtual void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const override
		{
			PrivateVariables.Add(IndexVariable);
		}

	private:
		int32 Index = -1;
		const FName Name;
		const FVoxelVariable IndexVariable;
		const TSharedRef<FVoxelMaterialInterfaceVariable> ExposedVariable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}
