// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_Blueprint.h"

class FAssetTypeActions_NarrativeQuestTask : public FAssetTypeActions_Blueprint
{
public:

	FAssetTypeActions_NarrativeQuestTask(uint32 InAssetCategory);

	uint32 Category;

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_NarrativeQuestTask", "Task"); };
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;

};

//Just put conditions and events in here too 
class FAssetTypeActions_NarrativeCondition : public FAssetTypeActions_Blueprint
{
public:

	FAssetTypeActions_NarrativeCondition(uint32 InAssetCategory);

	uint32 Category;

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_NarrativeCondition", "Narrative Condition"); };
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;

	/** Return the factory responsible for creating this type of Blueprint */
	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const;
};


//Just put Events and events in here too 
class FAssetTypeActions_NarrativeEvent : public FAssetTypeActions_Blueprint
{
public:

	FAssetTypeActions_NarrativeEvent(uint32 InAssetCategory);

	uint32 Category;

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_NarrativeEvent", "Narrative Event"); };
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;

	/** Return the factory responsible for creating this type of Blueprint */
	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const;
};
