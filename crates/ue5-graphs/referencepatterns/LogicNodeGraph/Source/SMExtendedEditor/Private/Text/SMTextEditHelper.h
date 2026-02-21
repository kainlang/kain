// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Runtime/Slate/Private/Framework/Text/TextEditHelper.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"

/**
 * Recreation of FTextEditHelper which isn't fully exported.
 */
class FSMTextEditHelper
{
public:
	static float GetFontHeight(const FSlateFontInfo& FontInfo)
	{
		const TSharedRef< FSlateFontMeasure > FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return FontMeasure->GetMaxCharacterHeight(FontInfo);
	}

	static float CalculateCaretWidth(const float FontMaxCharHeight)
	{
		// We adjust the width of the caret to avoid it becoming too wide on smaller or larger fonts and overlapping the characters it's next to.
		// We clamp the lower limit to 1 to avoid it being invisible, and the upper limit to 2 to avoid tall fonts having very wide carets.
		return FMath::Clamp(EditableTextDefs::CaretWidthPercent * FontMaxCharHeight, 1.0f, 2.0f);
	}
};