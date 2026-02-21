// © 2025 mrbaconvn. All Rights Reserved.

#include "Graph/Factory/ComboNodeFactory.h" 
#include "PropertyCustomizationHelpers.h"
#include "Graph/Node/ComboNodeBase.h"

TSharedPtr<class SGraphNode> FComboNodeFactory::CreateNode(UEdGraphNode* InNode) const
{
    UComboNodeBase* ComboNode = Cast<UComboNodeBase>(InNode);
    if (!IsValid(ComboNode))
    {
        return nullptr;
    }

    return SNew(SComboGraphNode, InNode); 
}

void SComboGraphNode::Construct(const FArguments& InArgs, UEdGraphNode* InNode)
{
    GraphNode = InNode;
    UpdateGraphNode();
}

void SComboGraphNode::GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const
{
	UComboNodeBase* ComboNode = Cast<UComboNodeBase>(GraphNode);

    if (!IsValid(ComboNode))
    {
		return;
    }
    const FString Message = ComboNode->GetNodePopUpMessage();
	if (Message.IsEmpty())
	{
		return;
	}
    new (Popups) FGraphInformationPopupInfo(nullptr, ComboNode->GetNodePopUpColor(), Message);
}

const FSlateBrush* SComboGraphNode::GetNodeBodyBrush() const
{
	return FAppStyle::GetBrush("Graph.Node.TintedBody");
}

void SComboGraphNode::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SGraphNode::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    UComboNodeBase* ComboNode = Cast<UComboNodeBase>(GraphNode);

    if (!IsValid(ComboNode))
    {
        return;
    }
	ComboNode->UpdateTime(InDeltaTime);
}

TSharedRef<SWidget> SComboGraphNode::CreateNodeContentArea()
{
    UComboNodeBase* ComboNode = Cast<UComboNodeBase>(GraphNode);

	TSharedRef<SWidget> BottomNodeContent = SNullWidget::NullWidget;
	TSharedRef<SWidget> MiddleNodeContent = SNullWidget::NullWidget;

    if (IsValid(ComboNode))
    {
        BottomNodeContent = ComboNode->CreateBottomNodeContent();
        MiddleNodeContent = ComboNode->CreateMiddleNodeContent();
    }

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("NoBorder"))
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        .Padding(FMargin(0, 3))
        [
            SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .HAlign(HAlign_Fill)
                .FillHeight(1.0f)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .HAlign(HAlign_Left)
                        .FillWidth(1.0f)
                        [
                            SAssignNew(LeftNodeBox, SVerticalBox)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .HAlign(HAlign_Fill)
                        [
                            MiddleNodeContent
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .HAlign(HAlign_Right)
                        [
                            SAssignNew(RightNodeBox, SVerticalBox)
                        ]
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Fill)
                [
                    BottomNodeContent
                ]
        ];
}