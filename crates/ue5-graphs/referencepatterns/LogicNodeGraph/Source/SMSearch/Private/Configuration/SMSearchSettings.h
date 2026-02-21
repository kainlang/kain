// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SMSearchSettings.generated.h"

UENUM()
enum class ESMAssetLoadType
{
	/** When selecting an asset. */
	OnDemand,
	/** When an asset becomes viewable in the list. */
	OnViewable
};

UCLASS(config = EditorPerProjectUserSettings)
class USMSearchSettings : public UObject
{
	GENERATED_BODY()

public:
	USMSearchSettings();

	/**
	 * The local status of the deferred indexer when using Logic Driver search. Unreal Engine defaults this to on,
	 * but Logic Driver defaults it to off because it is buggy and can stall indexing when a blueprint is compiled.
	 *
	 * When search is activated this replaces the current UE configuration. This does not permanently override
	 * the GEditorIni status of the deferred indexer.
	 *
	 * Restarting the project without opening search will instead use the UE default from GEditorIni.
	 */
	UPROPERTY(config, EditAnywhere, Category = AssetIndexing)
	bool bEnableDeferredIndexing = false;

	/** The strategy for search to use when loading assets. */
	UPROPERTY(config, EditAnywhere, Category = AssetLoad)
	ESMAssetLoadType AssetLoadType = ESMAssetLoadType::OnDemand;

	/** If assets should load async or blocking. If you experience crashes while loading assets try turning this off. */
	UPROPERTY(config, EditAnywhere, Category = AssetLoad)
	bool bAsyncLoad = true;

	/**
	 * Allow construction scripts to run when an asset is loaded from search. This is
	 * disabled for performance.
	 */
	UPROPERTY(config, EditAnywhere, Category = AssetLoad)
	bool bAllowConstructionScriptsOnLoad = false;

	/**
	 * The color to highlight properties on graph nodes when a match is found.
	 */
	UPROPERTY(config, EditAnywhere, Category = Color)
	FLinearColor PropertyHighlightColor;
};
