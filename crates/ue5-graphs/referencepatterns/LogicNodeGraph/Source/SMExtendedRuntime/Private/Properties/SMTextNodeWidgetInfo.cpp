// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTextNodeWidgetInfo.h"

#include "Styling/CoreStyle.h"

FSMTextNodeWidgetInfo::FSMTextNodeWidgetInfo()
{
#if WITH_EDITORONLY_DATA
	MinWidth = 150;
	MaxWidth = 300;
	MinHeight = 50;
	MaxHeight = 250;
	bConsiderForDefaultWidget = false;

	if (IsInGameThread())
	{
		// CoreStyle isn't safe to access from other threads and isn't required apart from the editor.
		EditableTextStyle = FCoreStyle::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>("InlineEditableTextBlockStyle");
	}
	EditableTextStyle.EditableTextBoxStyle.BackgroundColor = FLinearColor(0.71f, 0.71f, 0.71f);
	EditableTextStyle.TextStyle.Font.Size = 11;
	EditableTextStyle.TextStyle.Font.OutlineSettings.OutlineSize = 2;

	WrapTextAt = 0;
#endif
}
