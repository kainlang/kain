// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Framework/Text/IRun.h"
#include "Framework/Text/ITextDecorator.h"
#include "Framework/Text/SlateHyperlinkRun.h"

#define RUN_INFO_METADATA_PROPERTY "property"
#define RUN_INFO_METADATA_FUNCTION "function"
#define RUN_INFO_METADATA_PROPERTY_GUID "guid"
#define RUN_INFO_METADATA_TEXT_STYLE "textstyle"
#define RUN_INFO_METADATA_BUTTON_STYLE "buttonstyle"
#define RUN_INFO_METADATA_COLOR "color"

struct FBPVariableDescription;

/**
 * Used for displaying properties in text boxes.
 */
class FSMPropertyRun : public ISlateRun, public TSharedFromThis< FSMPropertyRun >
{
public:

	typedef TMap< FString, FString > FMetadata;
	DECLARE_DELEGATE_OneParam(FOnClick, const FMetadata& /*Metadata*/);
	DECLARE_DELEGATE_RetVal_OneParam(FText, FOnGetTooltipText, const FMetadata& /*Metadata*/);
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<IToolTip>, FOnGenerateTooltip, const FMetadata& /*Metadata*/);

public:

	class FWidgetViewModel
	{
	public:
		FWidgetViewModel(): bIsPressed(false), bIsHovered(false)
		{
		}

		bool IsPressed() const { return bIsPressed; }
		bool IsHovered() const { return bIsHovered; }

		void SetIsPressed(bool Value) { bIsPressed = Value; }
		void SetIsHovered(bool Value) { bIsHovered = Value; }

	private:

		bool bIsPressed;
		bool bIsHovered;
	};

public:

	static TSharedRef< FSMPropertyRun > Create(const FRunInfo& InRunInfo, const TSharedRef< const FString >& InButtonText, const FButtonStyle& InStyle,
		FTextBlockStyle InTextStyle, FOnClick NavigateDelegate, FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate);
	static TSharedRef< FSMPropertyRun > Create(const FRunInfo& InRunInfo, const TSharedRef< const FString >& InButtonText, const FButtonStyle& InStyle,
		FTextBlockStyle InTextStyle, FOnClick NavigateDelegate, FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate, const FTextRange& InRange);
public:

	virtual ~FSMPropertyRun() override {}

	// ISlateRun
	virtual FTextRange GetTextRange() const override;
	virtual void SetTextRange(const FTextRange& Value) override;
	virtual int16 GetBaseLine(float Scale) const override;
	virtual int16 GetMaxHeight(float Scale) const override;
	virtual FVector2D Measure(int32 StartIndex, int32 EndIndex, float Scale, const FRunTextContext& TextContext) const override;
	virtual int8 GetKerning(int32 CurrentIndex, float Scale, const FRunTextContext& TextContext) const override;
	virtual TSharedRef< ILayoutBlock > CreateBlock(int32 StartIndex, int32 EndIndex, FVector2D Size, const FLayoutBlockTextContext& TextContext, const TSharedPtr< IRunRenderer >& Renderer) override;
	virtual int32 OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual const TArray< TSharedRef<SWidget> >& GetChildren() override;
	virtual void ArrangeChildren(const TSharedRef< ILayoutBlock >& Block, const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual int32 GetTextIndexAt(const TSharedRef< ILayoutBlock >& Block, const FVector2D& Location, float Scale, ETextHitPoint* const OutHitPoint = nullptr) const override;
	virtual FVector2D GetLocationAt(const TSharedRef< ILayoutBlock >& Block, int32 Offset, float Scale) const override;
	virtual void BeginLayout() override { Children.Empty(); }
	virtual void EndLayout() override {}
	virtual void Move(const TSharedRef<FString>& NewText, const FTextRange& NewRange) override;
	virtual TSharedRef<IRun> Clone() const override;

	virtual void AppendTextTo(FString& AppendToText) const override;
	virtual void AppendTextTo(FString& AppendToText, const FTextRange& PartialRange) const override;

	virtual const FRunInfo& GetRunInfo() const override;
	virtual ERunAttributes GetRunAttributes() const override;
	// ~ ISlateRun

	bool IsRunValid() const;

	/** Find the run name checking properties and functions. */
	static FString GetRunName(const FTextRunParseResults& RunParseResult, const FString& OriginalText);
protected:
	FSMPropertyRun(const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FButtonStyle& InStyle, FTextBlockStyle InTextStyle, FOnClick InNavigateDelegate,
		FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate);

	FSMPropertyRun(const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FButtonStyle& InStyle, FTextBlockStyle InTextStyle, FOnClick InNavigateDelegate,
		FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate, const FTextRange& InRange);


	FSMPropertyRun(const FSMPropertyRun& Run);

protected:
	void OnNavigate();

	FLinearColor GetBackgroundColor() const;

protected:
	FRunInfo RunInfo;
	TSharedRef< const FString > Text;
	TSharedRef< const FString > ButtonText;
	FTextRange Range;
	FButtonStyle ButtonStyle;
	FTextBlockStyle TextStyle;
	FOnClick NavigateDelegate;
	FOnGenerateTooltip TooltipDelegate;
	FOnGetTooltipText TooltipTextDelegate;

	TSharedRef< FWidgetViewModel > ViewModel;
	TArray< TSharedRef<SWidget> > Children;
};

struct FPropertyRunTypeDesc
{
	FPropertyRunTypeDesc(const FText& InText, const FText& InTooltipText, const FString& InId, FSMPropertyRun::FOnClick InOnClickedDelegate, FSMPropertyRun::FOnGetTooltipText InTooltipTextDelegate = FSMPropertyRun::FOnGetTooltipText(), FSMPropertyRun::FOnGenerateTooltip InTooltipDelegate = FSMPropertyRun::FOnGenerateTooltip())
		: Id(InId)
		, Text(InText)
		, TooltipText(InTooltipText)
		, OnClickedDelegate(InOnClickedDelegate)
		, TooltipTextDelegate(InTooltipTextDelegate)
		, TooltipDelegate(InTooltipDelegate)
	{
	}

	/** Tag used by this hyperlink's run */
	FString Id;

	/** Text to display in the UI */
	FText Text;

	/** Tooltip text to display in the UI */
	FText TooltipText;

	/** Delegate to execute for this hyperlink's run */
	FSMPropertyRun::FOnClick OnClickedDelegate;

	/** Delegate used to retrieve the text to display in the hyperlink's tooltip */
	FSMPropertyRun::FOnGetTooltipText TooltipTextDelegate;

	/** Delegate used to generate hyperlink's tooltip */
	FSMPropertyRun::FOnGenerateTooltip TooltipDelegate;
};

class FRunTypeUtils
{
public:
	static bool IsRunRestricted(TSharedRef<IRun> Run)
	{
		return Run->GetRunInfo().MetaData.Contains(RUN_INFO_METADATA_PROPERTY) || Run->GetRunInfo().MetaData.Contains(RUN_INFO_METADATA_FUNCTION);
	}

	/** Create run information from a property. If Variable is null no guid will be recorded and this will be assumed to be an error. */
	static FRunInfo CreatePropertyRunInfo(FName PropertyName, FBPVariableDescription* Variable);

	/** Create run information from a function. */
	static FRunInfo CreateFunctionRunInfo(UFunction* Function);

	/** Attempt to locate ether a property or function name. */
	static bool TryGetRunName(TSharedRef<IRun> Run, FString& OutName);

	/** Formats the result for use with a format text node. */
	static bool TryGetRunNameAsFormatArgument(TSharedRef<IRun> Run, FString& OutName);
};

/**
 * For the parsing rich text to reconstruct the decorator.
 */

class FPropertyDecorator : public ITextDecorator
{
public:

	static TSharedRef< FPropertyDecorator > Create(FString Id, const FSlateHyperlinkRun::FOnClick& NavigateDelegate, const FSMPropertyRun::FOnGetTooltipText& InToolTipTextDelegate = FSMPropertyRun::FOnGetTooltipText(), const FSMPropertyRun::FOnGenerateTooltip& InToolTipDelegate = FSMPropertyRun::FOnGenerateTooltip());
	virtual ~FPropertyDecorator() {}

public:

	virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override;

	virtual TSharedRef< ISlateRun > Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText, const TSharedRef< FString >& InOutModelText, const ISlateStyle* Style) override;

protected:

	FPropertyDecorator(FString InId, const FSMPropertyRun::FOnClick& InNavigateDelegate, const FSMPropertyRun::FOnGetTooltipText& InToolTipTextDelegate, const FSMPropertyRun::FOnGenerateTooltip& InToolTipDelegate);

protected:

	FSMPropertyRun::FOnClick NavigateDelegate;

protected:
	FString Id;
	FSMPropertyRun::FOnGetTooltipText ToolTipTextDelegate;
	FSMPropertyRun::FOnGenerateTooltip ToolTipDelegate;
};