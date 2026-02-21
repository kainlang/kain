// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelGraphCompileToCpp.h"
#include "VoxelGraphGenerator.h"

#include "DesktopPlatformModule.h"
#include "Widgets/SWindow.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "HAL/PlatformFilemanager.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "GameProjectGenerationModule.h"
#include "GameProjectUtils.h"

static const FString GeneratedBuildText(
	R"(using UnrealBuildTool;

public class GeneratedWorldGenerators : ModuleRules
{
	public GeneratedWorldGenerators(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Voxel", "VoxelGraph" });
        PrivatePCHHeaderFile = "GeneratedWorldGeneratorsPCH.h";
	}
}
)");

static const FString GeneratedModuleHeaderText(
R"(#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FGeneratedWorldGeneratorsModule : public IModuleInterface
{
};
)");

static const FString GeneratedModuleCppText(
R"(#include "GeneratedWorldGenerators.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FGeneratedWorldGeneratorsModule, GeneratedWorldGenerators);
)");

static const FString GeneratedModulePCHText = "#include \"VoxelGeneratedWorldGeneratorsPCH.h\"";

static void Popup(const FString& Text, bool bSuccess = true)
{
	FNotificationInfo Info(FText::FromString(Text));
	Info.ExpireDuration = 10.f;
	Info.CheckBoxState = bSuccess ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	FSlateNotificationManager::Get().AddNotification(Info);
}

static void UpdateTextFileIfNeeded(const FString& Path, const FString& Text)
{
	FString OldText;
	if (FFileHelper::LoadFileToString(OldText, *Path))
	{
		if (OldText == Text)
		{
			Popup(Path + " didn't change: not updating it");
		}
		else
		{
			if (FFileHelper::SaveStringToFile(Text, *Path))
			{
				Popup(Path + " was successfully updated");
			}
			else
			{
				Popup(Path + " was NOT successfully updated");
			}
		}
	}
	else
	{
		if (FFileHelper::SaveStringToFile(Text, *Path))
		{
			Popup(Path + " was successfully created");
		}
		else
		{
			Popup(Path + " was NOT successfully created");
		}
	}
}

inline bool IsUnderDirectory(const FString& InPath, const FString& InDirectory)
{
	return FPaths::IsUnderDirectory(InPath, InDirectory);
}

static bool CompileInternal(UVoxelGraphGenerator* Generator, bool bIsAutomaticCompile, FString& OutError)
{
	auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	auto& FileManager = IFileManager::Get();

	// cpp project
	if (!GameProjectUtils::ProjectHasCodeFiles())
	{
		auto Result = FMessageDialog::Open(EAppMsgType::OkCancel, VOXEL_LOCTEXT(
			"Your project is a blueprint only project. You need to convert it to a C++ one.\n"
			"In the next window, click Next and then Create Class, leaving everything to default."));
		if (Result == EAppReturnType::Cancel)
		{
			OutError = "Canceled";
			return false;
		}
		FGameProjectGenerationModule::Get().OpenAddCodeToProjectDialog();
		OutError = "Need to convert to a C++ project";
		return false;
	}

	// GeneratedWorldGenerators module
	const FString GeneratedDirectory(FPaths::GameSourceDir() + "GeneratedWorldGenerators/");
	if (!FPaths::DirectoryExists(GeneratedDirectory))
	{
		auto Result = FMessageDialog::Open(EAppMsgType::OkCancel, VOXEL_LOCTEXT(
			"The GeneratedWorldGenerators module is missing. It will be automatically created.\n"
			"A GeneratedWorldGenerators folder will be created in your game Source directory."));
		if (Result == EAppReturnType::Cancel)
		{
			OutError = "Canceled";
			return false;
		}

		PlatformFile.CreateDirectoryTree(*GeneratedDirectory);

		const FString GeneratedBuild = GeneratedDirectory + "GeneratedWorldGenerators.Build.cs";
		FFileHelper::SaveStringToFile(GeneratedBuildText, *GeneratedBuild);

		const FString GeneratedModuleHeader = GeneratedDirectory + "GeneratedWorldGenerators.h";
		FFileHelper::SaveStringToFile(GeneratedModuleHeaderText, *GeneratedModuleHeader);

		const FString GeneratedModuleCpp = GeneratedDirectory + "GeneratedWorldGenerators.cpp";
		FFileHelper::SaveStringToFile(GeneratedModuleCppText, *GeneratedModuleCpp);

		const FString GeneratedModulePCH = GeneratedDirectory + "GeneratedWorldGeneratorsPCH.h";
		FFileHelper::SaveStringToFile(GeneratedModulePCHText, *GeneratedModulePCH);

		Popup(GeneratedDirectory + " successfully created");
	}
	
	// GeneratedWorldGenerators module dependency
	{
		TArray<FString> FoundFiles;
		FileManager.FindFiles(FoundFiles, *FPaths::ProjectDir(), TEXT(".uproject"));

		if (FoundFiles.Num() != 1)
		{
			OutError = FString::Printf(TEXT("Error: %d .uproject found in the game directory"), FoundFiles.Num());
			return false;
		}

		for (const auto& File : FoundFiles)
		{
			const FString UProjectPath(FPaths::ProjectDir() + File);
			FString UProjectContent;
			FFileHelper::LoadFileToString(UProjectContent, *UProjectPath);
			
			TSharedPtr<FJsonObject> UProjectObject;
			TSharedRef<TJsonReader<>> UProjectReader = TJsonReaderFactory<>::Create(UProjectContent);
			if (!FJsonSerializer::Deserialize(UProjectReader, UProjectObject))
			{
				OutError = "Invalid .uproject: can't deserialize json";
				return false;
			}
			check(UProjectObject.IsValid());

			bool bModuleInUProject = false;

			const TArray<TSharedPtr<FJsonValue>>* ModulesPtr = nullptr;
			if (!UProjectObject->TryGetArrayField(TEXT("Modules"), ModulesPtr))
			{
				OutError = "Invalid .uproject: No Modules array";
				return false;
			}
			check(ModulesPtr);

			const auto& Modules = *ModulesPtr;
			for (auto& Module : Modules)
			{
				if (Module->AsObject()->GetStringField(TEXT("Name")) == "GeneratedWorldGenerators")
				{
					bModuleInUProject = true;
					break;
				}
			}

			if (!bModuleInUProject)
			{
				auto Result = FMessageDialog::Open(EAppMsgType::OkCancel, VOXEL_LOCTEXT(
					"The GeneratedWorldGenerators module dependency is missing from your .uproject. It will automatically added.\n"
					"A backup of your .uproject will be created next to it."));
				if (Result == EAppReturnType::Cancel)
				{
					OutError = "Canceled";
					return false;
				}

				auto NewModules = Modules;
				TSharedPtr<FJsonObject> GeneratedWorldGeneratorsModule = MakeShared<FJsonObject>();
				GeneratedWorldGeneratorsModule->Values.Add("Name", MakeShared<FJsonValueString>("GeneratedWorldGenerators"));
				GeneratedWorldGeneratorsModule->Values.Add("Type", MakeShared<FJsonValueString>("Runtime"));
				GeneratedWorldGeneratorsModule->Values.Add("LoadingPhase", MakeShared<FJsonValueString>("Default"));
				NewModules.Add(MakeShared<FJsonValueObject>(GeneratedWorldGeneratorsModule));

				UProjectObject->SetArrayField("Modules", NewModules);

				// Backup uproject
				const FString Backup = UProjectPath + ".bak";
				FileManager.Copy(*Backup, *UProjectPath);

				FString Json;
				TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&Json, 0);
				if (!FJsonSerializer::Serialize(UProjectObject.ToSharedRef(), JsonWriter))
				{
					OutError = "Failed to serialize new uproject";
					return false;
				}
				if (!FFileHelper::SaveStringToFile(Json, *UProjectPath))
				{
					OutError = "Failed to write new uproject";
					return false;
				}

				Popup("Module dependency successfully added");
			}
		}
	}

	{
		TArray<FString> FoundFiles;
		FileManager.FindFiles(FoundFiles, *FPaths::GameSourceDir(), TEXT(".Target.cs"));

		for (const auto& File : FoundFiles)
		{
			const FString TargetPath(FPaths::GameSourceDir() + File);
			FString TargetContent;
			FFileHelper::LoadFileToString(TargetContent, *TargetPath);
			if (!TargetContent.Contains("\"GeneratedWorldGenerators\""))
			{
				auto Result = FMessageDialog::Open(EAppMsgType::YesNoCancel, FText::FromString(File + " doesn't have the GeneratedWorldGenerators module dependency. Add it automatically? (You'll have to restart the editor)\n\nChoose Yes if you don't know"));
				switch (Result)
				{
				case EAppReturnType::No:
					continue;
				case EAppReturnType::Yes:
					break;
				case EAppReturnType::Cancel:
					OutError = "Canceled";
					return false;
				default:
					check(false);
					break;
				}

				int32 Position = TargetContent.Find("ExtraModuleNames.AddRange(");
				if (Position < 0)
				{
					Position = TargetContent.Find("ExtraModuleNames.Add(");
				}
				if (Position < 0)
				{
					FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Cannot add the GeneratedWorldGenerators module dependency. Please add the following code to " + File + ": \n\nExtraModuleNames.AddRange( new string[] { \"GeneratedWorldGenerators\" } );"));
				}
				else
				{
					TargetContent.InsertAt(Position, "ExtraModuleNames.AddRange( new string[] { \"GeneratedWorldGenerators\" } );\n\t\t");

					FileManager.Move(*(TargetPath + ".bak"), *TargetPath);
					FFileHelper::SaveStringToFile(TargetContent, *TargetPath);

					Popup("GeneratedWorldGenerators module dependency successfully added to " + File);
				}
			}
		}
	}
	
	// Show the file browser dialog
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		check(DesktopPlatform);

		TArray<FString> OutFiles;
		if (bIsAutomaticCompile)
		{
			if (Generator->SaveLocation.FilePath.IsEmpty())
			{
				OutError = "Save location is empty!";
				return false;
			}
			OutFiles.Add(FPaths::ProjectDir() / Generator->SaveLocation.FilePath);
		}
		else
		{
			if (DesktopPlatform->SaveFileDialog(
				FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
				"File to create",
				Generator->LastSavePath.IsEmpty()
				? GeneratedDirectory
				: FPaths::ProjectDir() / Generator->LastSavePath,
				Generator->GetName() + ".h",
				TEXT("C++ header (*.h)|*.h"),
				EFileDialogFlags::None,
				OutFiles))
			{
				check(OutFiles.Num() == 1);
			}
		}

		if (OutFiles.Num() > 0)
		{
			check(OutFiles.Num() == 1);

			const FString HeaderPath = FPaths::ConvertRelativePathToFull(OutFiles[0]);
			if (!HeaderPath.EndsWith(".h"))
			{
				OutError = "Filename must end with .h";
				return false;
			}

			const FString FolderPath = FPaths::GetPath(HeaderPath);
			const FString BaseName = FPaths::GetBaseFilename(HeaderPath);

			FString HeaderText;
			FString CppText;
			if (!Generator->CompileToCpp(HeaderText, CppText, BaseName))
			{
				Popup("Compilation failed!", false);
				return true;
			}

			const FString CppPath = FolderPath + "/" + BaseName + ".cpp";

			UpdateTextFileIfNeeded(HeaderPath, HeaderText);
			UpdateTextFileIfNeeded(CppPath, CppText);

			if (IsUnderDirectory(FolderPath, FPaths::ProjectDir()))
			{
				Generator->LastSavePath = FolderPath;
				FPaths::MakePathRelativeTo(Generator->LastSavePath, *FPaths::ProjectDir());
			}
			else
			{
				Generator->LastSavePath.Reset();
			}
			Generator->Modify();
		}
	}

	return true;
}

void FVoxelGraphCompileToCpp::Compile(UVoxelGraphGenerator* Generator, bool bIsAutomaticCompile)
{
	if (!ensure(Generator))
	{
		return;
	}
	
	FString Error;
	if (!CompileInternal(Generator, bIsAutomaticCompile, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Failed to compile " + Generator->GetName() + ":\n" + Error));
	}
}
