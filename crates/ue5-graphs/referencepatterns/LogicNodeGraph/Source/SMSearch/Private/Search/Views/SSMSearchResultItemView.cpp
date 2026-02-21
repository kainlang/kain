// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMSearchResultItemView.h"

#include "SSMSearchView.h"

#include "Graph/Nodes/SMGraphNode_Base.h"

#include "SMInstance.h"

#include "EdGraphSchema_K2.h"
#include "EditorStyleSet.h"
#include "Engine/AssetManager.h"
#include "FindInBlueprintManager.h"
#include "FindInBlueprints.h"
#include "SMUnrealTypeDefs.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/SRichTextBlock.h"

#define LOCTEXT_NAMESPACE "SMSearchResultItem"

FName SSMSearchResultItemView::ColumnName_Error = TEXT("Error");
FName SSMSearchResultItemView::ColumnName_Asset = TEXT("Asset");
FName SSMSearchResultItemView::ColumnName_Node = TEXT("Node");
FName SSMSearchResultItemView::ColumnName_Property = TEXT("Property");
FName SSMSearchResultItemView::ColumnName_Value = TEXT("Value");

void SSMSearchResultItemView::Construct(const FArguments& InArgs, TSharedPtr<SSMSearchView> InSearchView, TSharedPtr<ISMSearch::FSearchResult> InSearchResult,
	const TSharedRef<STableViewBase>& InOwnerTableView, const FString& InSearchString)
{
	SearchViewOwner = InSearchView;
	Item = InSearchResult;
	SearchString = InSearchString;

	Item->FiBResult->Finalize();
	Item->TryResolveObjects();

	SMultiColumnTableRow<TSharedPtr<ISMSearch::FSearchResult>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

/**
 * Retrieves the pin type as a string value. See FindInBlueprints.cpp
 *
 * @param InPinType		The pin type to look at
 *
 * @return				The pin type as a string in format [category]'[sub-category object]'
 */
FString GetPinTypeAsString(const FEdGraphPinType& InPinType)
{
	FString Result = InPinType.PinCategory.ToString();
	if(const UObject* SubCategoryObject = InPinType.PinSubCategoryObject.Get())
	{
		Result += FString(" '") + SubCategoryObject->GetName() + "'";
	}
	else if (!InPinType.PinSubCategory.IsNone())
	{
		Result += FString(" '") + InPinType.PinSubCategory.ToString() + "'";
	}

	return Result;
}

TSharedRef<SWidget> SSMSearchResultItemView::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (Item.IsValid())
	{
		if (ColumnName == ColumnName_Error)
		{
			if (Item->HasError())
			{
				return SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FSMUnrealAppStyle::Get().GetBrush(TEXT("Icons.Error")))
					.ColorAndOpacity(FLinearColor::White)
					.ToolTipText(Item->ReplaceResult->ErrorMessage)
				];
			}
		}
		else if (ColumnName == ColumnName_Asset)
		{
			const FText Tooltip = FText::FromString(*Item->BlueprintPath);

			return SNew(SHorizontalBox)
			.ToolTipText(Tooltip)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SCircularThrobber)
					.Radius(6.0f)
					.Visibility_Lambda([this]()
					{
						return IsAssetLoading() ? EVisibility::Visible : EVisibility::Collapsed;
					})
					.ToolTipText(LOCTEXT("Loading_Tooltip", "Asset is loading..."))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
				+SOverlay::Slot()
				[
					SNew(SImage)
					.Visibility_Lambda([this]()
					{
						return !IsAssetLoading() ? EVisibility::Visible : EVisibility::Collapsed;
					})
					.Image_Lambda([this]()
					{
						const UObject* CDO = (Item->Blueprint.IsValid() && Item->Blueprint->GeneratedClass) ? Item->Blueprint->GeneratedClass->ClassDefaultObject : nullptr;
						return FSlateIconFinder::FindIconBrushForClass(CDO ? CDO->GetClass() : USMInstance::StaticClass());
					})
				]
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.AutoWidth()
			.Padding(0.15f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(*Item->GetBlueprintName()))
			];
		}
		else if (ColumnName == ColumnName_Node)
		{
			const FText NodeName = FText::FromString(Item->FiBResult->NodeName);

			return SNew(SHorizontalBox)
			.ToolTipText_Lambda([this, NodeName]()
			{
				if (Item->GraphNode.IsValid())
				{
					const USMGraphNode_Base* SMGraphNode = Cast<USMGraphNode_Base>(Item->GraphNode.Get());
					const FText Tooltip = FText::FromString(SMGraphNode ? *SMGraphNode->GetFriendlyNodeName().ToString() : *Item->GraphNode->GetName());
					return Tooltip;
				}
				return NodeName;
			})
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SScaleBox)
				.HAlign(HAlign_Fill)
				.Stretch(EStretch::ScaleToFit)
				.StretchDirection(EStretchDirection::Both)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(16.f)
					.HeightOverride(16.f)
					[
						SNew(SImage)
						.Image_Lambda([this]() -> const FSlateBrush*
						{
							if (const USMGraphNode_Base* GraphNode = Cast<USMGraphNode_Base>(Item->GraphNode))
							{
								return GraphNode->GetNodeIcon();
							}

							return nullptr;
						})
					]
				]
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.AutoWidth()
			.Padding(0.15f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(NodeName)
			];
		}
		else if (ColumnName == ColumnName_Property)
		{
			FString PropertyName = Item->GetPropertyName();
			const int32 PropertyIndex = Item->GetPropertyIndex();
			if (PropertyIndex != INDEX_NONE)
			{
				PropertyName = FString::Printf(TEXT("%s (%s)"), *PropertyName, *FString::FromInt(PropertyIndex));
			}
			const TSharedRef<SWidget> Icon = Item->FiBResult->GraphPin.IsValid() ? Item->FiBResult->GraphPin->CreateIcon() : SNullWidget::NullWidget;

			return SNew(SHorizontalBox)
			.ToolTipText_Lambda([this]()
			{
				if (Item->Property)
				{
					const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
					FEdGraphPinType PinType;
					K2Schema->ConvertPropertyToPinType(Item->Property, PinType);
					const FText Tooltip = FText::FromString(GetPinTypeAsString(PinType));
					return Tooltip;
				}

				return FText::GetEmpty();
			})
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				Icon
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.Padding(0.15f, 0.f, 0.f, 0.f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(*PropertyName))
			];
		}
		else if (ColumnName == ColumnName_Value)
		{
			return SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FSMUnrealAppStyle::Get().GetBrush(TEXT("Icons.Info")))
					.Visibility_Lambda([&]()
					{
						return Item->ReplaceResult.IsValid() && Item->ReplaceResult->ErrorMessage.IsEmpty() ?
							EVisibility::Visible : EVisibility::Collapsed;
					})
					.ColorAndOpacity(FLinearColor::White)
					.ToolTipText_Lambda([&]()
					{
						return Item->ReplaceResult.IsValid() ?
							FText::FromString(FString::Printf(TEXT("Value updated to '%s'."), *Item->ReplaceResult->NewValue)) :
							FText::GetEmpty();
					})
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(SRichTextBlock)
					.Text(FText::FromString(MakeStringSnippet(*Item->PropertyValue)))
					.ToolTipText(FText::FromString(Item->PropertyValue))
					.HighlightText(FText::FromString(SearchString))
				];
		}
	}

	return SNullWidget::NullWidget;
}

FString SSMSearchResultItemView::MakeStringSnippet(const FString& InString) const
{
	check(Item.IsValid());

	FString Result;

	TArray<FTextRange> LineRanges;
	FTextRange::CalculateLineRangesFromString(InString, LineRanges);

	check(LineRanges.Num() > 0);

	// Find only the full lines that contain the matches.
	for (int32 Idx = 0; Idx < LineRanges.Num(); ++Idx)
	{
		const FTextRange& Line = LineRanges[Idx];
		const int32 MatchedTextRangeIndex = Item->FindMatchedTextRangeIntersectingRange(Line);
		if (MatchedTextRangeIndex != INDEX_NONE)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT("\n");
			}

			Result.Append(*InString + Line.BeginIndex, Line.Len());
		}
	}

	return MoveTemp(Result);
}

bool SSMSearchResultItemView::IsAssetLoading() const
{
	return SearchViewOwner.IsValid() && SearchViewOwner.Pin()->IsAssetLoading(Item->BlueprintPath);
}

#undef LOCTEXT_NAMESPACE
