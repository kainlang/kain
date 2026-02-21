// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

#include "Configuration/SMEditorStyle.h"

class FSMExtendedEditorStyle : FSMEditorStyle
{
public:
	// Register with the system.
	static void Initialize();

	// Unregister from the system.
	static void Shutdown();

	/** Gets the singleton instance. */
	static TSharedPtr<ISlateStyle> Get() { return StyleSetInstance; }

	static FName GetStyleSetName() { return TEXT("SMExtendedEditorStyle"); }

protected:
	static void SetGraphStyles();
	static void SetBrushes();
	static void SetIcons();
private:
	// Singleton instance.
	static TSharedPtr<FSlateStyleSet> StyleSetInstance;
};
