// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ISMAssetManager.h"

#include "TickableEditorObject.h"

class FSMAssetManager final : public ISMAssetManager, public FTickableEditorObject
{
public:
	virtual ~FSMAssetManager() override {}

	// ISMAssetManager
	virtual USMBlueprint* CreateStateMachineBlueprint(const FCreateStateMachineBlueprintArgs& InArgs) override;
	virtual void PopulateClassDefaults(UBlueprint* InBlueprint, UObject* InNewClassDefaults) override;
	virtual void CompileBlueprints(const FCompileBlueprintArgs& InArgs, const FOnCompileBlueprintsCompletedSignature& InOnCompileBlueprintsCompletedDelegate) override;
	virtual void CancelCompileBlueprints() override;
	virtual bool IsCompilingBlueprints() const override;
	virtual float GetCompileBlueprintsPercent() const override;
	// ~ISMAssetManager

	// FTickableEditorObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual TStatId GetStatId() const override;
	// ~FTickableEditorObject

private:
	void CompileBlueprints_Internal(bool bInSourceControlActive);
	void UpdateCompileBlueprints();

private:
	FCompileBlueprintArgs CompileArgs;
	FOnCompileBlueprintsCompletedSignature OnCompileBlueprintsCompletedEvent;
	TSharedPtr<struct FStreamableHandle> StreamingHandle;

	TArray<FSoftObjectPath> BlueprintsToLoadAndCompile;
	TArray<FSoftObjectPath> BlueprintsCompiling;
	int32 CurrentIndex = 0;
};