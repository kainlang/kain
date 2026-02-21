// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMNodeWidgetInfo.h"

#include "SMTypes.h"

FSMNodeWidgetInfo::FSMNodeWidgetInfo()
{
#if WITH_EDITORONLY_DATA
	MinWidth = 150;
	MaxWidth = 450;
	MinHeight = 50;
	MaxHeight = 450;
	DisplayOrder_DEPRECATED = 0;
	BackgroundColor = FLinearColor(0.1f, 0.128f, 0.2f, 0.5f);
	Clipping = EWidgetClipping::ClipToBounds;
	bConsiderForDefaultWidget = false;
	DisplayThumbnail = ESMOptionalBool::Unset;
#endif
}

FSMTextDisplayWidgetInfo::FSMTextDisplayWidgetInfo() : Super()
{
#if WITH_EDITORONLY_DATA
	DefaultTextStyle = FTextBlockStyle::GetDefault();
	DefaultTextStyle.Font.OutlineSettings.OutlineSize = 1;
	DefaultTextStyle.ColorAndOpacity = FLinearColor(0.7f, 0.7f, 0.7f, 0.7f);
	OnDropBackgroundColor = FLinearColor(0.3f, 0.328f, 0.5f, 0.75f);
	MinHeight = 25;
	MaxHeight = 450;
#endif
}
