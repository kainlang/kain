// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Styling/SlateTypes.h"

class FSMSearchStyle
{
public:
	// Register with the system.
	static void Initialize();

	// Unregister from the system.
	static void Shutdown();

	/** Gets the singleton instance. */
	static TSharedPtr<ISlateStyle> Get() { return StyleSetInstance; }

	static FName GetStyleSetName() { return TEXT("SMSearchStyle"); }

	static FTextBlockStyle NormalText;
	static FString InResources(const FString& RelativePath, const ANSICHAR* Extension);
	
protected:
	static void SetIcons();

private:
	// Singleton instance.
	static TSharedPtr<FSlateStyleSet> StyleSetInstance;

};
