// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * AlphaGen Slate Style
 * 
 * Manages toolbar and menu icons for the AlphaGen plugin.
 */
class FAlphaGenStyle
{
public:
	/** Initialize the style set */
	static void Initialize();
	
	/** Shutdown and cleanup */
	static void Shutdown();
	
	/** Reloads textures used by style set */
	static void ReloadTextures();
	
	/** Get the style set name */
	static FName GetStyleSetName();
	
	/** Get the style set */
	static const ISlateStyle& Get();

private:
	/** Create the style set */
	static TSharedRef<FSlateStyleSet> Create();
	
	/** Singleton instance */
	static TSharedPtr<FSlateStyleSet> StyleInstance;
};
