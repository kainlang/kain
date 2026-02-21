// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphPin.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"

#include "SearchFilterViewModel.generated.h"

class USMInstance;

UENUM()
enum class ESMPropertyTypeTemplate
{
	None,
	Text,
	Enum
};

UCLASS(NotBlueprintable, NotPlaceable, config = EditorPerProjectUserSettings)
class USearchFilterPropertiesViewModel : public UObject
{
	GENERATED_BODY()

public:
	/** The default template type. */
	UPROPERTY(Config)
	ESMPropertyTypeTemplate PropertyTypeTemplate = ESMPropertyTypeTemplate::Text;

	/** The pin types to load when not using a type template. */
	UPROPERTY(Config)
	TArray<FEdGraphPinType> PinTypes;

	/** Limit the search to properties matching these names. */
	UPROPERTY(EditAnywhere, Config, Category = Properties)
	TSet<FName> Names;
};

UCLASS(NotBlueprintable, NotPlaceable, config = EditorPerProjectUserSettings)
class USearchFilterAssetsViewModel : public UObject
{
	GENERATED_BODY()

public:
	/** Limit the search to these directories. */
	UPROPERTY(EditAnywhere, Config, Category = Assets)
	TArray<FDirectoryPath> Directories;

	/** Limit the search to state machines of the given types. */
	UPROPERTY(EditAnywhere, Config, Category = Assets)
	TSet<TSoftClassPtr<USMInstance>> StateMachines;

	/**
	 * Include children classes.
	 */
	UPROPERTY(EditAnywhere, Config, Category = Assets)
	bool bSubClasses = false;
};
