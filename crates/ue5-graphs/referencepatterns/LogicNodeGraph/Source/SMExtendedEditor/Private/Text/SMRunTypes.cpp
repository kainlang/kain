// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMRunTypes.h"
#include "SMRichTextPropertyLink.h"
#include "Configuration/SMExtendedEditorStyle.h"
#include "Configuration/SMTextGraphEditorSettings.h"

#include "Utilities/SMBlueprintEditorUtils.h"

#include "BlueprintVariableNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Text/RunUtils.h"
#include "Framework/Text/ShapedTextCache.h"
#include "Framework/Text/WidgetLayoutBlock.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

TSharedRef< FSMPropertyRun > FSMPropertyRun::Create(const FRunInfo& InRunInfo, const TSharedRef< const FString >& InButtonText,
                                                    const FButtonStyle& InStyle, FTextBlockStyle InTextStyle, FOnClick NavigateDelegate, FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate)
{
	return MakeShareable(new FSMPropertyRun(InRunInfo, InButtonText, InStyle, InTextStyle, NavigateDelegate, InTooltipDelegate, InTooltipTextDelegate));
}

TSharedRef< FSMPropertyRun > FSMPropertyRun::Create(const FRunInfo& InRunInfo, const TSharedRef< const FString >& InButtonText,
	const FButtonStyle& InStyle, FTextBlockStyle InTextStyle, FOnClick NavigateDelegate, FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate,
	const FTextRange& InRange)
{
	return MakeShareable(new FSMPropertyRun(InRunInfo, InButtonText, InStyle, InTextStyle, NavigateDelegate, InTooltipDelegate, InTooltipTextDelegate, InRange));
}

FTextRange FSMPropertyRun::GetTextRange() const
{
	return Range;
}

void FSMPropertyRun::SetTextRange(const FTextRange& Value)
{
	Range = Value;
}

int16 FSMPropertyRun::GetBaseLine(float Scale) const
{
	const TSharedRef< FSlateFontMeasure > FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	return FontMeasure->GetBaseline(TextStyle.Font, Scale) - FMath::Min(0.0f, TextStyle.ShadowOffset.Y * Scale);
}

int16 FSMPropertyRun::GetMaxHeight(float Scale) const
{
	const TSharedRef< FSlateFontMeasure > FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	return FontMeasure->GetMaxCharacterHeight(TextStyle.Font, Scale) + FMath::Abs(TextStyle.ShadowOffset.Y * Scale);
}

FVector2D FSMPropertyRun::Measure(int32 StartIndex, int32 EndIndex, float Scale, const FRunTextContext& TextContext) const
{
	const FVector2D ShadowOffsetToApply((EndIndex == Range.EndIndex) ? FMath::Abs(TextStyle.ShadowOffset.X * Scale) : 0.0f, FMath::Abs(TextStyle.ShadowOffset.Y * Scale));

	if (EndIndex - StartIndex == 0)
	{
		return FVector2D(ShadowOffsetToApply.X * Scale, GetMaxHeight(Scale));
	}

	FVector2D Result = ShapedTextCacheUtil::MeasureShapedText(TextContext.ShapedTextCache, FCachedShapedTextKey(FTextRange(0, ButtonText->Len()), Scale, TextContext, TextStyle.Font), FTextRange(0, ButtonText->Len()), **ButtonText) + ShadowOffsetToApply;

	Result.X += 10;

	return Result;
}

int8 FSMPropertyRun::GetKerning(int32 CurrentIndex, float Scale, const FRunTextContext& TextContext) const
{
	return 0;
}

TSharedRef< ILayoutBlock > FSMPropertyRun::CreateBlock(int32 StartIndex, int32 EndIndex, FVector2D Size, const FLayoutBlockTextContext& TextContext, const TSharedPtr< IRunRenderer >& Renderer)
{
	FText ToolTipText;
	TSharedPtr<IToolTip> ToolTip;

	if (TooltipDelegate.IsBound())
	{
		ToolTip = TooltipDelegate.Execute(RunInfo.MetaData);
	}
	else
	{
		const FString* Url = RunInfo.MetaData.Find(TEXT("href"));
		if (TooltipTextDelegate.IsBound())
		{
			ToolTipText = TooltipTextDelegate.Execute(RunInfo.MetaData);
		}
		else if (Url != nullptr)
		{
			ToolTipText = FText::FromString(*Url);
		}
	}

	TSharedRef<SWidget> Widget = SNew(SSMRichTextPropertyLink, ViewModel)
		.ButtonStyle(&ButtonStyle)
		.TextStyle(&TextStyle)
		.ButtonColor(GetBackgroundColor())
		.Text(FText::FromString(FString(*ButtonText)))
		.ToolTip(ToolTip)
		.ToolTipText(ToolTipText)
		.OnPressed(this, &FSMPropertyRun::OnNavigate);

	Widget->SlatePrepass();

	Children.Add(Widget);

	return FWidgetLayoutBlock::Create(SharedThis(this), Widget, FTextRange(StartIndex, EndIndex), Size, TextContext, Renderer);
}

void FSMPropertyRun::OnNavigate()
{
	NavigateDelegate.Execute(RunInfo.MetaData);
}

FLinearColor FSMPropertyRun::GetBackgroundColor() const
{
	if (!IsRunValid())
	{
		return FLinearColor::Red;
	}

	if (const FString* ColorStr = RunInfo.MetaData.Find(RUN_INFO_METADATA_COLOR))
	{
		FLinearColor Color;
		if (Color.InitFromString(*ColorStr))
		{
			return Color;
		}
	}
	
	return FLinearColor(0.05f, 0.833f, 0.f, 0.843f);
}

int32 FSMPropertyRun::OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const TSharedRef< FWidgetLayoutBlock > WidgetBlock = StaticCastSharedRef< FWidgetLayoutBlock >(TextArgs.Block);

	// The block size and offset values are pre-scaled, so we need to account for that when converting the block offsets into paint geometry
	const float InverseScale = Inverse(AllottedGeometry.Scale);

	const FGeometry WidgetGeometry = AllottedGeometry.MakeChild(TransformVector(InverseScale, TextArgs.Block->GetSize()), FSlateLayoutTransform(TransformPoint(InverseScale, TextArgs.Block->GetLocationOffset())));
	return WidgetBlock->GetWidget()->Paint(PaintArgs, WidgetGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

const TArray< TSharedRef<SWidget> >& FSMPropertyRun::GetChildren()
{
	return Children;
}

void FSMPropertyRun::ArrangeChildren(const TSharedRef< ILayoutBlock >& Block, const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const TSharedRef< FWidgetLayoutBlock > WidgetBlock = StaticCastSharedRef< FWidgetLayoutBlock >(Block);

	// The block size and offset values are pre-scaled, so we need to account for that when converting the block offsets into paint geometry
	const float InverseScale = Inverse(AllottedGeometry.Scale);

	ArrangedChildren.AddWidget(
		AllottedGeometry.MakeChild(WidgetBlock->GetWidget(), TransformVector(InverseScale, Block->GetSize()), FSlateLayoutTransform(TransformPoint(InverseScale, Block->GetLocationOffset())))
	);
}

int32 FSMPropertyRun::GetTextIndexAt(const TSharedRef< ILayoutBlock >& Block, const FVector2D& Location, float Scale, ETextHitPoint* const OutHitPoint) const
{
	const FVector2D& BlockOffset = Block->GetLocationOffset();
	const FVector2D& BlockSize = Block->GetSize();

	const float Left = BlockOffset.X;
	const float Top = BlockOffset.Y;
	const float Right = BlockOffset.X + BlockSize.X;
	const float Bottom = BlockOffset.Y + BlockSize.Y;

	const bool ContainsPoint = Location.X >= Left && Location.X < Right && Location.Y >= Top && Location.Y < Bottom;

	if (!ContainsPoint)
	{
		return INDEX_NONE;
	}

	const FTextRange BlockRange = Block->GetTextRange();
	const FLayoutBlockTextContext BlockTextContext = Block->GetTextContext();

	// Use the full text range (rather than the run range) so that text that spans runs will still be shaped correctly
	const int32 Index = ShapedTextCacheUtil::FindCharacterIndexAtOffset(BlockTextContext.ShapedTextCache, FCachedShapedTextKey(FTextRange(0, ButtonText->Len()), Scale, BlockTextContext, TextStyle.Font), BlockRange, **ButtonText, Location.X - BlockOffset.X);
	if (OutHitPoint)
	{
		*OutHitPoint = RunUtils::CalculateTextHitPoint(Index, BlockRange, BlockTextContext.TextDirection);
	}

	return Index;
}

FVector2D FSMPropertyRun::GetLocationAt(const TSharedRef< ILayoutBlock >& Block, int32 Offset, float Scale) const
{
	const FVector2D& BlockOffset = Block->GetLocationOffset();
	const FTextRange& BlockRange = Block->GetTextRange();
	const FLayoutBlockTextContext BlockTextContext = Block->GetTextContext();

	// Use the full text range (rather than the run range) so that text that spans runs will still be shaped correctly
	const FTextRange RangeToMeasure = RunUtils::CalculateOffsetMeasureRange(Offset, BlockRange, BlockTextContext.TextDirection);
	const FVector2D OffsetLocation = ShapedTextCacheUtil::MeasureShapedText(BlockTextContext.ShapedTextCache, FCachedShapedTextKey(FTextRange(0, ButtonText->Len()), Scale, BlockTextContext, TextStyle.Font), RangeToMeasure, **ButtonText);

	return BlockOffset + OffsetLocation;
}

void FSMPropertyRun::Move(const TSharedRef<FString>& NewText, const FTextRange& NewRange)
{
	Text = NewText;
	Range = NewRange;
}

TSharedRef<IRun> FSMPropertyRun::Clone() const
{
	return FSMPropertyRun::Create(RunInfo, ButtonText, ButtonStyle, TextStyle, NavigateDelegate, TooltipDelegate, TooltipTextDelegate);
}

void FSMPropertyRun::AppendTextTo(FString& AppendToText) const
{
	AppendToText.Append(**Text + Range.BeginIndex, Range.Len());
}

void FSMPropertyRun::AppendTextTo(FString& AppendToText, const FTextRange& PartialRange) const
{
	// Called when copying.

	check(Range.BeginIndex <= PartialRange.BeginIndex);
	check(Range.EndIndex >= PartialRange.EndIndex);

	const FString AsText = FString::Printf(TEXT("{%s}"), *ButtonText.Get());
	AppendToText.Append(*AsText, AsText.Len());
}

const FRunInfo& FSMPropertyRun::GetRunInfo() const
{
	return RunInfo;
}

ERunAttributes FSMPropertyRun::GetRunAttributes() const
{
	return ERunAttributes::None;
}

bool FSMPropertyRun::IsRunValid() const
{
	const FString* Function = RunInfo.MetaData.Find(RUN_INFO_METADATA_FUNCTION);
	if (Function)
	{
		return true;
	}
	
	const FString* GuidStr = RunInfo.MetaData.Find(RUN_INFO_METADATA_PROPERTY_GUID);
	if (GuidStr && !GuidStr->IsEmpty())
	{
		return true;
	}

	return false;
}

FString FSMPropertyRun::GetRunName(const FTextRunParseResults& RunParseResult, const FString& OriginalText)
{
	if (const FTextRange* const MetaDataNameRange = RunParseResult.MetaData.Find(TEXT(RUN_INFO_METADATA_PROPERTY)))
	{
		const FString MetaDataName = OriginalText.Mid(MetaDataNameRange->BeginIndex, MetaDataNameRange->EndIndex - MetaDataNameRange->BeginIndex);
		return MetaDataName;
	}
	if (const FTextRange* const MetaDataNameRange = RunParseResult.MetaData.Find(TEXT(RUN_INFO_METADATA_FUNCTION)))
	{
		const FString MetaDataName = OriginalText.Mid(MetaDataNameRange->BeginIndex, MetaDataNameRange->EndIndex - MetaDataNameRange->BeginIndex);
		return MetaDataName;
	}

	return FString();
}

FSMPropertyRun::FSMPropertyRun(const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText,
                               const FButtonStyle& InStyle, FTextBlockStyle InTextStyle, FOnClick InNavigateDelegate, FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate)
	: RunInfo(InRunInfo)
	, Text(MakeShareable(new FString(TEXT("\x200B"))))
	, ButtonText(InText)
	, Range(0, Text->Len())
	, ButtonStyle(InStyle)
	, TextStyle(InTextStyle)
	, NavigateDelegate(InNavigateDelegate)
	, TooltipDelegate(InTooltipDelegate)
	, TooltipTextDelegate(InTooltipTextDelegate)
	, ViewModel(MakeShareable(new FSMPropertyRun::FWidgetViewModel()))
	, Children()
{
}

FSMPropertyRun::FSMPropertyRun(const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText,
	const FButtonStyle& InStyle, FTextBlockStyle InTextStyle, FOnClick InNavigateDelegate, FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate,
	const FTextRange& InRange)
	: RunInfo(InRunInfo)
	, Text(MakeShareable(new FString(TEXT("\x200B"))))
	, ButtonText(InText)
	, Range(InRange)
	, ButtonStyle(InStyle)
	, TextStyle(InTextStyle)
	, NavigateDelegate(InNavigateDelegate)
	, TooltipDelegate(InTooltipDelegate)
	, TooltipTextDelegate(InTooltipTextDelegate)
	, ViewModel(MakeShareable(new FSMPropertyRun::FWidgetViewModel()))
	, Children()
{

}

FSMPropertyRun::FSMPropertyRun(const FSMPropertyRun& Run)
	: RunInfo(Run.RunInfo)
	, Text(Run.Text)
	, ButtonText(Run.ButtonText)
	, Range(Run.Range)
	, ButtonStyle(Run.ButtonStyle)
	, NavigateDelegate(Run.NavigateDelegate)
	, TooltipDelegate(Run.TooltipDelegate)
	, TooltipTextDelegate(Run.TooltipTextDelegate)
	, ViewModel(MakeShareable(new FSMPropertyRun::FWidgetViewModel()))
	, Children()
{

}


bool FPropertyDecorator::Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const
{
	const FTextRange* const MetaDataIdRange = RunParseResult.MetaData.Find(TEXT("id"));
	FString MetaDataId;
	if (MetaDataIdRange)
	{
		MetaDataId = Text.Mid(MetaDataIdRange->BeginIndex, MetaDataIdRange->EndIndex - MetaDataIdRange->BeginIndex);
	}

	return (RunParseResult.Name == RUN_INFO_METADATA_PROPERTY && MetaDataId == Id);
}

TSharedRef< FPropertyDecorator > FPropertyDecorator::Create(FString Id, const FSlateHyperlinkRun::FOnClick& NavigateDelegate, const FSMPropertyRun::FOnGetTooltipText& InToolTipTextDelegate, const FSMPropertyRun::FOnGenerateTooltip& InToolTipDelegate)
{
	return MakeShareable(new FPropertyDecorator(Id, NavigateDelegate, InToolTipTextDelegate, InToolTipDelegate));
}

TSharedRef< ISlateRun > FPropertyDecorator::Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText, const TSharedRef< FString >& InOutModelText, const ISlateStyle* Style)
{
	FString ButtonStyleName = TEXT("button");
	FString TextStyleName = TEXT("");
	FString VarName = TEXT("INVALID");

	const FTextRange* const MetaDataStyleNameRange = RunParseResult.MetaData.Find(RUN_INFO_METADATA_BUTTON_STYLE);
	if (MetaDataStyleNameRange != nullptr)
	{
		const FString MetaDataStyleName = OriginalText.Mid(MetaDataStyleNameRange->BeginIndex, MetaDataStyleNameRange->EndIndex - MetaDataStyleNameRange->BeginIndex);
		ButtonStyleName = *MetaDataStyleName;
	}

	const FTextRange* const MetaDataTextStyleNameRange = RunParseResult.MetaData.Find(RUN_INFO_METADATA_TEXT_STYLE);
	if (MetaDataTextStyleNameRange != nullptr)
	{
		const FString MetaDataTextStyleName = OriginalText.Mid(MetaDataTextStyleNameRange->BeginIndex, MetaDataTextStyleNameRange->EndIndex - MetaDataTextStyleNameRange->BeginIndex);
		TextStyleName = *MetaDataTextStyleName;
	}

	const FString FoundName = FSMPropertyRun::GetRunName(RunParseResult, OriginalText);
	if (!FoundName.IsEmpty())
	{
		VarName = FoundName;
	}
	
	if (!Style->HasWidgetStyle<FButtonStyle>(FName(*ButtonStyleName)))
	{
		Style = &*FSMExtendedEditorStyle::Get();
	}

	FTextRange ModelRange;
	ModelRange.BeginIndex = InOutModelText->Len();
	*InOutModelText += TEXT('\x200B'); // Zero-Width Breaking Space
	ModelRange.EndIndex = InOutModelText->Len();

	FRunInfo RunInfo(RunParseResult.Name);
	for (const TPair<FString, FTextRange>& Pair : RunParseResult.MetaData)
	{
		RunInfo.MetaData.Add(Pair.Key, OriginalText.Mid(Pair.Value.BeginIndex, Pair.Value.EndIndex - Pair.Value.BeginIndex));
	}

	const FButtonStyle ButtonStyle = Style->GetWidgetStyle<FButtonStyle>(FName(*ButtonStyleName));
	const FTextBlockStyle TextStyle = Style->GetWidgetStyle<FTextBlockStyle>(FName(*TextStyleName));
	
	return FSMPropertyRun::Create(RunInfo, MakeShareable(new FString(VarName)), ButtonStyle, TextStyle, NavigateDelegate, ToolTipDelegate, ToolTipTextDelegate, ModelRange);
}

FPropertyDecorator::FPropertyDecorator(FString InId, const FSMPropertyRun::FOnClick& InNavigateDelegate, const FSMPropertyRun::FOnGetTooltipText& InToolTipTextDelegate, const FSMPropertyRun::FOnGenerateTooltip& InToolTipDelegate)
	: NavigateDelegate(InNavigateDelegate)
	, Id(InId)
	, ToolTipTextDelegate(InToolTipTextDelegate)
	, ToolTipDelegate(InToolTipDelegate)
{

}

FRunInfo FRunTypeUtils::CreatePropertyRunInfo(FName PropertyName, FBPVariableDescription* Variable)
{
	FName TextStyleName = TEXT("SMExtendedEditor.Graph.Property.Text");
	FName ButtonStyleName = TEXT("SMExtendedEditor.Graph.Property.Button");

	// Create the correct meta-information for this run, so that valid source rich-text formatting can be generated for it
	FRunInfo RunInfo(TEXT(RUN_INFO_METADATA_PROPERTY));
	RunInfo.MetaData.Add(TEXT("id"), RUN_INFO_METADATA_PROPERTY);
	//RunInfo.MetaData.Add(TEXT("href"), "https://recursoft.com/");
	RunInfo.MetaData.Add(TEXT(RUN_INFO_METADATA_BUTTON_STYLE), ButtonStyleName.ToString());
	RunInfo.MetaData.Add(TEXT(RUN_INFO_METADATA_TEXT_STYLE), TextStyleName.ToString());
	RunInfo.MetaData.Add(TEXT(RUN_INFO_METADATA_PROPERTY), PropertyName.ToString());

	if (Variable != nullptr)
	{
		RunInfo.MetaData.Add(TEXT(RUN_INFO_METADATA_PROPERTY_GUID), Variable->VarGuid.ToString());

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		const FLinearColor PinColor = Schema->GetPinTypeColor(Variable->VarType);
		
		RunInfo.MetaData.Add(TEXT(RUN_INFO_METADATA_COLOR), PinColor.ToString());
	}

	return RunInfo;
}

FRunInfo FRunTypeUtils::CreateFunctionRunInfo(UFunction* Function)
{
	FName TextStyleName = TEXT("SMExtendedEditor.Graph.Property.Text");
	FName ButtonStyleName = TEXT("SMExtendedEditor.Graph.Property.Button");

	// Create the correct meta-information for this run, so that valid source rich-text formatting can be generated for it
	FRunInfo RunInfo(TEXT(RUN_INFO_METADATA_PROPERTY));
	RunInfo.MetaData.Add(TEXT("id"), RUN_INFO_METADATA_PROPERTY);
	RunInfo.MetaData.Add(TEXT(RUN_INFO_METADATA_BUTTON_STYLE), ButtonStyleName.ToString());
	RunInfo.MetaData.Add(TEXT(RUN_INFO_METADATA_TEXT_STYLE), TextStyleName.ToString());

	//FString FunctionPath = FString::Printf(TEXT("%s.%s"), *Function->GetOuterUClass()->GetPathName(), *Function->GetName());
	RunInfo.MetaData.Add(TEXT(RUN_INFO_METADATA_FUNCTION), Function->GetName());

	if (Function != nullptr)
	{
		TArray<FProperty*> Outputs;
		if (FSMBlueprintEditorUtils::GetOutputProperties(Function, Outputs))
		{
			const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
			FEdGraphPinType PinType;
			Schema->ConvertPropertyToPinType(Outputs[0], PinType);
			const FLinearColor PinColor = Schema->GetPinTypeColor(PinType);

			RunInfo.MetaData.Add(TEXT(RUN_INFO_METADATA_COLOR), PinColor.ToString());
		}
	}
	
	return RunInfo;
}

bool FRunTypeUtils::TryGetRunName(TSharedRef<IRun> Run, FString& OutName)
{
	OutName.Empty();

	if (const FString* PropertyNameString = Run->GetRunInfo().MetaData.Find(TEXT(RUN_INFO_METADATA_PROPERTY)))
	{
		OutName = *PropertyNameString;
		return true;
	}
	if (const FString* FunctionNameString = Run->GetRunInfo().MetaData.Find(TEXT(RUN_INFO_METADATA_FUNCTION)))
	{
		OutName = *FunctionNameString;
		return true;
	}

	return false;
}

bool FRunTypeUtils::TryGetRunNameAsFormatArgument(TSharedRef<IRun> Run, FString& OutName)
{
	if (TryGetRunName(Run, OutName))
	{
		OutName = FString::Printf(TEXT("{%s}"), *OutName);
		return true;
	}

	return false;
}
