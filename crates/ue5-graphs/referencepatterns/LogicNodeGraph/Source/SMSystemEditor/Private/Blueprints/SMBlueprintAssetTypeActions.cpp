// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMBlueprintAssetTypeActions.h"

#include "SMBlueprintEditor.h"

#include "Blueprints/SMBlueprint.h"
#include "SMInstance.h"

#define LOCTEXT_NAMESPACE "SMBlueprintAssetTypeActions"

FSMAssetTypeActions_Base::FSMAssetTypeActions_Base(uint32 Categories) : MyAssetCategory(Categories)
{
}

uint32 FSMAssetTypeActions_Base::GetCategories()
{
	return MyAssetCategory;
}

FSMBlueprintAssetTypeActions::FSMBlueprintAssetTypeActions(uint32 InAssetCategory)
	: FSMAssetTypeActions_Base(InAssetCategory)
{
}

FText FSMBlueprintAssetTypeActions::GetName() const
{
	return LOCTEXT("FSMBlueprintAssetTypeActions", "State Machine");
}

FColor FSMBlueprintAssetTypeActions::GetTypeColor() const
{
	return FColor(118, 119, 118);
}

UClass* FSMBlueprintAssetTypeActions::GetSupportedClass() const
{
	return USMBlueprint::StaticClass();
}

void FSMBlueprintAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (USMBlueprint* StateMachineBP = Cast<USMBlueprint>(*ObjIt))
		{
			TSharedRef<FSMBlueprintEditor> BlueprintEditor(new FSMBlueprintEditor());
			BlueprintEditor->InitSMBlueprintEditor(Mode, EditWithinLevelEditor, StateMachineBP);
		}
	}
}

FSMInstanceAssetTypeActions::FSMInstanceAssetTypeActions(EAssetTypeCategories::Type InAssetCategory) : FSMAssetTypeActions_Base(InAssetCategory)
{
}

FText FSMInstanceAssetTypeActions::GetName() const
{
	return LOCTEXT("FSMGraphAssetTypeActions", "State Machine Instance");
}

FColor FSMInstanceAssetTypeActions::GetTypeColor() const
{
	return FColor(0, 0, 0);
}

UClass* FSMInstanceAssetTypeActions::GetSupportedClass() const
{
	return USMInstance::StaticClass();
}


FSMNodeInstanceAssetTypeActions::FSMNodeInstanceAssetTypeActions(uint32 InAssetCategory)
	: FSMAssetTypeActions_Base(InAssetCategory)
{
}

FText FSMNodeInstanceAssetTypeActions::GetName() const
{
	return LOCTEXT("FSMNodeInstanceAssetTypeActions", "Node Class");
}

FColor FSMNodeInstanceAssetTypeActions::GetTypeColor() const
{
	return FColor(118, 119, 118);
}

UClass* FSMNodeInstanceAssetTypeActions::GetSupportedClass() const
{
	return USMNodeBlueprint::StaticClass();
}

void FSMNodeInstanceAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (USMNodeBlueprint* Blueprint = Cast<USMNodeBlueprint>(*ObjIt))
		{
			TArray<UBlueprint*> Blueprints;
			Blueprints.Add(Cast<UBlueprint>(Blueprint));
			
			TSharedRef<FSMNodeBlueprintEditor> BlueprintEditor(new FSMNodeBlueprintEditor());
			BlueprintEditor->InitNodeBlueprintEditor(Mode, EditWithinLevelEditor, Blueprints, false);
		}
	}
}

#undef LOCTEXT_NAMESPACE
