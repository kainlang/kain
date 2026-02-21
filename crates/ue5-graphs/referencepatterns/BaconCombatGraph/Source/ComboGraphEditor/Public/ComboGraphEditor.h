// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

struct FComboPinFactory;
struct FComboNodeFactory;
class FSlateStyleSet;

class FComboGraphEditor : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected: 
	void InitializeStyleSet(); 

public:
	TSharedPtr<FComboPinFactory> PinFactory;
	TSharedPtr<FComboNodeFactory> NodeFactory;
	TSharedPtr<FSlateStyleSet> StyleSet;
};