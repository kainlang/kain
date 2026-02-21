// © 2025 mrbaconvn. All Rights Reserved.


#include "Graph/Node/ComboExecutionNode.h"
#include "GraphData/NodeData/ComboExecutionNodeData.h" 
#include "Action/ComboActionPass.h"  
#include "AssetRegistry/AssetRegistryModule.h"
#include "ThumbnailRendering/ThumbnailManager.h"

#define LOCTEXT_NAMESPACE "ComboGraphSchema"
#define NODE_TITLE_EMPTY LOCTEXT("Node.ComboExecutionNode.Title", "Execute nothing")
#define NODE_TITLE LOCTEXT("Node.ComboExecutionNode.Title", "Execute {0} passes")
#define SUCCESS_EXECUTION_PIN "Success"
#define FAILED_EXECUTION_PIN "Fail"
#define NODE_CREATION_TEXT LOCTEXT("Node.ComboConditionNode.CreationText", "Create execution node")

FText UComboExecutionNode::GetNodeCreationText() const
{
    return NODE_CREATION_TEXT;
}

FText UComboExecutionNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (!IsValid(NodeInfo) || NodeInfo->ExecutionPasses.Num() == 0)
	{
		return NODE_TITLE_EMPTY;
	}
	return FText::Format(NODE_TITLE, NodeInfo->ExecutionPasses.Num());
}

FLinearColor UComboExecutionNode::GetNodeTitleColor() const
{
	return FLinearColor::Red;
}

void UComboExecutionNode::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetNodeContextMenuActions(Menu, Context); 
}

void UComboExecutionNode::CreateDefaultPins()
{
	CreateCustomPin(EEdGraphPinDirection::EGPD_Input, INPUT_PIN_NAME);
	CreateCustomPin(EEdGraphPinDirection::EGPD_Output, SUCCESS_EXECUTION_PIN);
	CreateCustomPin(EEdGraphPinDirection::EGPD_Output, FAILED_EXECUTION_PIN);
}

TSharedRef<SWidget> UComboExecutionNode::CreateBottomNodeContent() const
{
    if (!IsValid(NodeInfo))
    {
        return SNullWidget::NullWidget;
    }

    const float NODE_WIDTH = 250;
    TSharedRef<SVerticalBox> VerticalNodeContent = SNew(SVerticalBox);
    
    if (NodeInfo->bBreakWhenPassFailed)
    {
        VerticalNodeContent->AddSlot()
            .AutoHeight()
            .Padding(FMargin(5, 5, 0, 0))
            [
                SNew(STextBlock)
                    .Text(FText::FromString("Breaks when any pass failed"))
                    .ColorAndOpacity(FLinearColor(0.5f, 0.1f, 0.1f))
                    .AutoWrapText(true)
            ];
    }

    const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    
    for (const TObjectPtr<UComboActionPass>& Pass : NodeInfo->ExecutionPasses)
    {
        if (!IsValid(Pass))
        {
            continue;
        }
        
        FString PassInfo = Pass->GetPassInformation();
        TArray<FSoftObjectPath> PreviewedAsset = Pass->GetPreviewAssets();

        TSharedRef<SHorizontalBox> ThumbnailRow = SNew(SHorizontalBox); 
        TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox); 
        
        for (const FSoftObjectPath& Asset : PreviewedAsset)
        {
            FAssetData Path = AssetRegistryModule.Get().GetAssetByObjectPath(Asset);
            if (!Path.IsValid())
            {
                continue;
            }
            const float THUMBNAIL_WIDTH = (NODE_WIDTH - 20) / PreviewedAsset.Num();
            const TSharedPtr<FAssetThumbnail> AssetThumbnail = MakeShareable(new FAssetThumbnail(Path, THUMBNAIL_WIDTH, THUMBNAIL_WIDTH, UThumbnailManager::Get().GetSharedThumbnailPool()));
            
            ThumbnailRow->AddSlot()
                .FillWidth(1.0f)
                [
                    AssetThumbnail->MakeThumbnailWidget()
                ];
        }

        if(PreviewedAsset.Num() > 0)
        {
            VerticalBox->AddSlot()
                .AutoHeight() 
                [
                    ThumbnailRow
                ];
        }
        
        VerticalBox->AddSlot()
            .AutoHeight()
            .Padding(10, 10, 10, 0)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(PassInfo))
                    .ColorAndOpacity(FLinearColor::White)
                    .LineHeightPercentage(1.5f)
                    .AutoWrapText(true)
            ];

        VerticalNodeContent->AddSlot()
            .AutoHeight()
            .Padding(0, 5, 0 ,0)
            [
                SNew(SBorder) 
                .BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f))
                .BorderImage(FCoreStyle::Get().GetBrush("ToolTip.BrightBackground")) 
                [
                    VerticalBox
                ]
            ];
    }

    return SNew(SBox)
        .Padding(FMargin(10, 10))
        .WidthOverride(NODE_WIDTH)
        [
            VerticalNodeContent
        ];
}

void UComboExecutionNode::RebuildNode()
{
	ReservePins(EEdGraphPinDirection::EGPD_Output, 2);
	GetPinWithDirectionAt(0, EEdGraphPinDirection::EGPD_Output)->PinName = SUCCESS_EXECUTION_PIN;
	GetPinWithDirectionAt(1, EEdGraphPinDirection::EGPD_Output)->PinName = FAILED_EXECUTION_PIN;
}

void UComboExecutionNode::InitNodeData(UObject* Outer)
{
	NodeInfo = NewObject<UComboExecutionNodeData>(Outer);
} 

void UComboExecutionNode::SetNodeData(UComboGraphNodeData* InNodeData)
{
	NodeInfo = Cast<UComboExecutionNodeData>(InNodeData);
}

UComboGraphNodeData* UComboExecutionNode::GetNodeData() const
{
	return NodeInfo;
}

#undef NODE_TITLE_EMPTY
#undef NODE_TITLE
#undef SUCCESS_EXECUTION_PIN
#undef FAILED_EXECUTION_PIN
#undef NODE_CREATION_TEXT    

#undef LOCTEXT_NAMESPACE