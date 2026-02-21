// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMAssetManager.h"

#include "Blueprints/SMBlueprint.h"

#include "Blueprints/SMBlueprintFactory.h"
#include "Construction/ISMEditorConstructionManager.h"

#include "AssetToolsModule.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "ISourceControlModule.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "SMAssetManager"

USMBlueprint* FSMAssetManager::CreateStateMachineBlueprint(const FCreateStateMachineBlueprintArgs& InArgs)
{
	if (!ensureMsgf(!InArgs.Name.IsNone(), TEXT("No asset name provided to CreateStateMachineBlueprint.")))
	{
		return nullptr;
	}

	const FString Path = !InArgs.Path.IsEmpty() ? InArgs.Path : TEXT("/Game/");

	const FString SanitizedObject = ObjectTools::SanitizeObjectName(InArgs.Name.ToString());
	const FString TentativePackagePath = UPackageTools::SanitizePackageName(FPaths::Combine(Path, SanitizedObject));
	const FString DefaultSuffix;
	FString AssetName;
	FString PackageName;

	const FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(TentativePackagePath, DefaultSuffix, /*out*/ PackageName, /*out*/ AssetName);

	UPackage* Package = CreatePackage(*PackageName);

	TArray<UPackage*> TopLevelPackages;
	TopLevelPackages.Add(Package->GetOutermost());
	if (!UPackageTools::HandleFullyLoadingPackages(TopLevelPackages, NSLOCTEXT("UnrealEd", "CreateANewObject", "Create a new object")))
	{
		return nullptr;
	}

	const FName BPName(*FPackageName::GetLongPackageAssetName(AssetName));

	/* // This can cause issues when the name is the same as another asset but being saved to a different directory.
	if (!ensureMsgf(!StaticFindObject(UObject::StaticClass(), ANY_PACKAGE, *BPName.ToString()),
		TEXT("Cannot create asset %s because it already exists."), *BPName.ToString()))
	{
		return nullptr;
	}*/

	USMBlueprintFactory* Factory = NewObject<USMBlueprintFactory>();
	Factory->SetParentClass(InArgs.ParentClass);

	if (USMBlueprint* NewBlueprint = Cast<USMBlueprint>(Factory->FactoryCreateNew(USMBlueprint::StaticClass(), Package, BPName,
		RF_Public | RF_Standalone, nullptr, GWarn)))
	{
		FAssetRegistryModule::AssetCreated(NewBlueprint);
		Package->MarkPackageDirty();

		return NewBlueprint;
	}

	return nullptr;
}

void FSMAssetManager::PopulateClassDefaults(UBlueprint* InBlueprint, UObject* InNewClassDefaults)
{
	check(InBlueprint && InBlueprint->GeneratedClass && InBlueprint->GeneratedClass->ClassDefaultObject);
	check(InNewClassDefaults);

	ensureMsgf(InBlueprint->GeneratedClass->ClassDefaultObject->GetClass()->IsChildOf(InNewClassDefaults->GetClass()),
		TEXT("The CDO class is not equal to or is not a child of the new defaults."));

	UEngine::CopyPropertiesForUnrelatedObjects(InNewClassDefaults, InBlueprint->GeneratedClass->ClassDefaultObject);
	InBlueprint->MarkPackageDirty();
}

void FSMAssetManager::CompileBlueprints(const FCompileBlueprintArgs& InArgs, const FOnCompileBlueprintsCompletedSignature& InOnCompileBlueprintsCompletedDelegate)
{
	if (IsCompilingBlueprints())
	{
		return;
	}

	const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry")).Get();

	TArray<FAssetData> OutAssets;
	AssetRegistry.GetAssets(InArgs.AssetFilter, OutAssets);

	BlueprintsToLoadAndCompile.Reset(OutAssets.Num());

	for (const FAssetData& Asset : OutAssets)
	{
		if (Asset.IsRedirector())
		{
			continue;
		}

		BlueprintsToLoadAndCompile.Add(Asset.ToSoftObjectPath());
	}

	EAppReturnType::Type ReturnValue = EAppReturnType::Yes;

	// Display a warning message so the user can cancel out.
	if (InArgs.bShowWarningMessage)
	{
		if (BlueprintsToLoadAndCompile.Num() == 0)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("CompileNoBlueprintsFoundMessage", "There are no blueprints to compile."));
			return;
		}

		const FText DialogTitle = !InArgs.CustomWarningTitle.IsEmpty() ? InArgs.CustomWarningTitle : LOCTEXT("ConfirmCompileAllTitle", "Compile Blueprints");
		FFormatNamedArguments Args;
		Args.Add(TEXT("BlueprintCount"), BlueprintsToLoadAndCompile.Num());

		const FText FormatText = !InArgs.CustomWarningMessage.IsEmpty() ? InArgs.CustomWarningMessage :
		LOCTEXT("CompileAllConfirmationMessage",
				"This process can take a long time and the editor may become unresponsive; there are {BlueprintCount} blueprints to load and compile.\n\nWould you like to checkout, load, and save all blueprints?");

		const FText DialogDisplayText = FText::Format(FormatText, Args);

		ReturnValue = FMessageDialog::Open(EAppMsgType::YesNoCancel, DialogDisplayText, DialogTitle);
	}

	if (ReturnValue == EAppReturnType::Yes)
	{
		CompileArgs = InArgs;
		OnCompileBlueprintsCompletedEvent = InOnCompileBlueprintsCompletedDelegate;

		const bool bIsSourceControlEnabled = ISourceControlModule::Get().IsEnabled();
		if (!bIsSourceControlEnabled && CompileArgs.bSave)
		{
			// Offer to start up Source Control
			ISourceControlModule::Get().ShowLoginDialog(FSourceControlLoginClosed::CreateRaw(this,
				&FSMAssetManager::CompileBlueprints_Internal), ELoginWindowMode::Modeless, EOnLoginWindowStartup::PreserveProvider);
		}
		else
		{
			CompileBlueprints_Internal(bIsSourceControlEnabled);
		}
	}
	else
	{
		BlueprintsToLoadAndCompile.Empty();
	}
}

void FSMAssetManager::CancelCompileBlueprints()
{
	if (StreamingHandle.IsValid())
	{
		StreamingHandle->CancelHandle();
		StreamingHandle.Reset();
	}

	BlueprintsCompiling.Empty();
	CurrentIndex = 0;
}

bool FSMAssetManager::IsCompilingBlueprints() const
{
	return StreamingHandle.IsValid() || BlueprintsCompiling.Num() > 0;
}

float FSMAssetManager::GetCompileBlueprintsPercent() const
{
	// Loading happens first, then compiling. Treat the total percentage as 1.f.

	if (StreamingHandle.IsValid())
	{
		return StreamingHandle->GetProgress() / 2.f;
	}

	if (BlueprintsCompiling.Num() == 0)
	{
		return 1.f;
	}

	return 0.5f + ((static_cast<float>(CurrentIndex) / static_cast<float>(BlueprintsCompiling.Num())) / 2.f);
}

void FSMAssetManager::Tick(float DeltaTime)
{
	UpdateCompileBlueprints();
}

bool FSMAssetManager::IsTickable() const
{
	return BlueprintsCompiling.Num() > 0;
}

TStatId FSMAssetManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FSMAssetManager, STATGROUP_Tickables);
}

void FSMAssetManager::CompileBlueprints_Internal(bool bInSourceControlActive)
{
	TArray<FString> UncachedAssetStrings;
	UncachedAssetStrings.Reserve(BlueprintsToLoadAndCompile.Num());
	for (const FSoftObjectPath& BlueprintPath : BlueprintsToLoadAndCompile)
	{
		const FString BlueprintPathString = BlueprintPath.ToString();
		UncachedAssetStrings.Add(BlueprintPathString);

		// Loading can be significantly slower if the blueprint is running construction scripts.
		// These will run during compile anyway.
		ISMEditorConstructionManager::Get().SetAllowConstructionScriptsOnLoadForBlueprint(BlueprintPathString, false);
	}

	StreamingHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		BlueprintsToLoadAndCompile, FStreamableDelegate::CreateLambda([this, bInSourceControlActive, UncachedAssetStrings]()
		{
			for (const FString& BlueprintPath : UncachedAssetStrings)
			{
				ISMEditorConstructionManager::Get().SetAllowConstructionScriptsOnLoadForBlueprint(BlueprintPath, true);
			}

			if(bInSourceControlActive && CompileArgs.bSave)
			{
				FEditorFileUtils::CheckoutPackages(UncachedAssetStrings);
			}

			BlueprintsCompiling = BlueprintsToLoadAndCompile;
			StreamingHandle.Reset();
		}));
}

void FSMAssetManager::UpdateCompileBlueprints()
{
	if (ensure(BlueprintsCompiling.Num() > 0 && CurrentIndex >= 0 && CurrentIndex < BlueprintsCompiling.Num()))
	{
		const FSoftObjectPath& Path = BlueprintsCompiling[CurrentIndex];
		if (UBlueprint* Blueprint = Cast<UBlueprint>(Path.ResolveObject()))
		{
			FKismetEditorUtilities::CompileBlueprint(Blueprint);

			if (CompileArgs.bSave)
			{
				Blueprint->MarkPackageDirty();

				// Save block from FindInBlueprintManager.cpp
				const FAssetRegistryModule* AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
				const FAssetData AssetData = AssetRegistryModule->Get().GetAssetByObjectPath(Path);
				if (AssetData.IsValid())
				{
					const bool bIsWorldAsset = AssetData.AssetClassPath == UWorld::StaticClass()->GetClassPathName();

					// Construct a full package filename with path so we can query the read only status and save to disk
					FString FinalPackageFilename = FPackageName::LongPackageNameToFilename(AssetData.PackageName.ToString());
					if (FinalPackageFilename.Len() > 0 && FPaths::GetExtension(FinalPackageFilename).Len() == 0)
					{
						FinalPackageFilename += bIsWorldAsset ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
					}
					FText ErrorMessage;
					bool bValidFilename = FFileHelper::IsFilenameValidForSaving(FinalPackageFilename, ErrorMessage);
					if (bValidFilename)
					{
						bValidFilename = bIsWorldAsset ? FEditorFileUtils::IsValidMapFilename(FinalPackageFilename, ErrorMessage) : FPackageName::IsValidLongPackageName(FinalPackageFilename, false, &ErrorMessage);
					}

					const bool bIsAssetReadOnlyOnDisk = IFileManager::Get().IsReadOnly(*FinalPackageFilename);
					if (!bIsAssetReadOnlyOnDisk)
					{
						// Assume the package was correctly checked out from SCC
						bool bOutPackageLocallyWritable = true;

						UPackage* Package = Blueprint->GetPackage();

						ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
						// Trusting the SCC status in the package file cache to minimize network activity during save.
						const FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(Package, EStateCacheUsage::Use);
						// If the package is in the depot, and not recognized as editable by source control, and not read-only, then we know the user has made the package locally writable!
						const bool bSCCCanEdit = !SourceControlState.IsValid() || SourceControlState->CanCheckIn() || SourceControlState->IsIgnored() || SourceControlState->IsUnknown();
						const bool bSCCIsCheckedOut = SourceControlState.IsValid() && SourceControlState->IsCheckedOut();
						const bool bInDepot = SourceControlState.IsValid() && SourceControlState->IsSourceControlled();
						if (!bSCCCanEdit && bInDepot && !bIsAssetReadOnlyOnDisk && SourceControlProvider.UsesLocalReadOnlyState() && !bSCCIsCheckedOut)
						{
							bOutPackageLocallyWritable = false;
						}

						// Save the package if the file is writable
						if (bOutPackageLocallyWritable && GEditor)
						{
							// Save the package
							FSavePackageArgs SaveArgs;
							SaveArgs.Error = GError;
							SaveArgs.TopLevelFlags = RF_Standalone;

							GEditor->SavePackage(Package, nullptr, *FinalPackageFilename, MoveTemp(SaveArgs));
						}
					}
				}
			}
		}
		CurrentIndex++;
	}

	if (CurrentIndex == BlueprintsCompiling.Num())
	{
		BlueprintsCompiling.Empty();
		CurrentIndex = 0;
		OnCompileBlueprintsCompletedEvent.ExecuteIfBound();
	}
}

#undef LOCTEXT_NAMESPACE
