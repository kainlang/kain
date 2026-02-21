// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMNodeWidgetInfo.h"

#include "Styling/SlateTypes.h"
#include "Templates/SubclassOf.h"

#include "SMTextNodeWidgetInfo.generated.h"

USTRUCT()
struct SMEXTENDEDRUNTIME_API FSMTextNodeWidgetInfo : public FSMTextDisplayWidgetInfo
{
	GENERATED_BODY()

	FSMTextNodeWidgetInfo();

	/** Style to apply for the text graph widget display on the node. */
	UPROPERTY(EditDefaultsOnly, Category = Style)
	FInlineEditableTextBlockStyle EditableTextStyle;

	/** When to the wrap the text on the node widget. */
	UPROPERTY(EditDefaultsOnly, Category = Style)
	float WrapTextAt;
};

USTRUCT()
struct SMEXTENDEDRUNTIME_API FSMTextNodeRichTextInfo
{
	GENERATED_BODY()

	FSMTextNodeRichTextInfo() {}

	/** Style to apply for rich text formatting. Only valid in the editor. */
	UPROPERTY(EditDefaultsOnly, Category = RichText, meta=(RequiredAssetDataTags = "RowStructure=/Script/UMG.RichTextStyleRow"))
	TObjectPtr<class UDataTable> RichTextStyleSet = nullptr;

	/** Decorators for rich text formatting. Only valid in the editor. */
	UPROPERTY(EditDefaultsOnly, Category = RichText)
	TArray<TSubclassOf<class URichTextBlockDecorator>> RichTextDecoratorClasses;
};