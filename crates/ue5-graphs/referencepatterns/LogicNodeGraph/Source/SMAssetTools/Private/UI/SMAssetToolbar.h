// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FMenuBuilder;
class FUICommandList;
class FExtender;

class FSMAssetToolbar
{
public:
	static void Initialize();
	static void Shutdown();

private:
	static void ConstructExportMenu(FMenuBuilder& InMenuBuilder, const TArray<UObject*> ContextSensitiveObjects);
	static TSharedRef<FExtender> HandleMenuExtensibilityGetExtender(const TSharedRef<FUICommandList> CommandList, const TArray<UObject*> ContextSensitiveObjects);

	static void OnAssetExport(const TArray<UObject*> ContextSensitiveObjects);
	static void OnAssetImport(const TArray<UObject*> ContextSensitiveObjects);

private:
	static FDelegateHandle ExtenderHandle;
};