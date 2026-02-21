// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ISMSearchModule.h"

class FSpawnTabArgs;
class SDockTab;

class FSMSearchModule : public ISMSearchModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual TSharedPtr<ISMSearch> GetSearchInterface() const override;
	virtual const FName& GetSearchTabName() const override { return TabName; }

private:
	void RegisterSettings();
	void UnregisterSettings();
	
	static TSharedRef<SDockTab> SpawnSearchInTab(const FSpawnTabArgs& SpawnTabArgs);

private:
	mutable TSharedPtr<ISMSearch> SearchInterface;
	static const FName TabName;
};
