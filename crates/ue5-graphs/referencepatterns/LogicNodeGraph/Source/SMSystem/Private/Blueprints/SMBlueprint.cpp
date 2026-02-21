// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "SMInstance.h"
#include "SMNodeInstance.h"
#include "SMStateInstance.h"
#include "SMStateMachineInstance.h"
#include "SMTransitionInstance.h"

#if WITH_EDITORONLY_DATA
#include "ISMPreviewEditorModule.h"
#endif

USMBlueprint::USMBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), AssetVersion(0), PluginVersion(0)
{
	BlueprintType = BPTYPE_Normal;
#if WITH_EDITORONLY_DATA
	BlueprintCategory = "State Machines";
	PreviewObject = nullptr;

	NumConstructionScriptPasses = 1;
	bAllowEditorConstructionScripts = true;
	bEnableNodeValidation = true;
	bEnableReferenceNodeValidation = false;
#endif
}

#if WITH_EDITOR

UClass* USMBlueprint::GetBlueprintClass() const
{
	return USMBlueprintGeneratedClass::StaticClass();
}

void USMBlueprint::GetReparentingRules(TSet<const UClass*>& AllowedChildrenOfClasses,
	TSet<const UClass*>& DisallowedChildrenOfClasses) const
{
	AllowedChildrenOfClasses.Add(USMInstance::StaticClass());
}

USMBlueprint::FOnRenameGraph USMBlueprint::OnRenameGraphEvent;

void USMBlueprint::NotifyGraphRenamed(UEdGraph* Graph, FName OldName, FName NewName)
{
	Super::NotifyGraphRenamed(Graph, OldName, NewName);
	OnRenameGraphEvent.Broadcast(this, Graph, OldName, NewName);
}

bool USMBlueprint::SupportsInputEvents() const
{
	if (GeneratedClass)
	{
		if (const USMInstance* Instance = Cast<USMInstance>(GeneratedClass->GetDefaultObject(false)))
		{
			return Instance->GetInputType() != ESMStateMachineInput::Disabled;
		}
	}
	return true;
}

USMBlueprintGeneratedClass* USMBlueprint::GetGeneratedClass() const
{
	return Cast<USMBlueprintGeneratedClass>(*GeneratedClass);
}

USMBlueprint* USMBlueprint::FindOldestParentBlueprint() const
{
	USMBlueprint* ParentBP = nullptr;

	// Find the root State Machine.
	for (const UClass* NextParentClass = ParentClass; NextParentClass && (UObject::StaticClass() != NextParentClass); NextParentClass = NextParentClass->GetSuperClass())
	{
		if (USMBlueprint* TestBP = Cast<USMBlueprint>(NextParentClass->ClassGeneratedBy))
		{
			ParentBP = TestBP;
		}
	}

	return ParentBP;
}

#endif

#if WITH_EDITORONLY_DATA

USMPreviewObject* USMBlueprint::GetPreviewObject(bool bCreateIfNeeded)
{
	if (!PreviewObject && bCreateIfNeeded)
	{
		// Let the preview module instantiate the object. We're not including the entire module just the public headers to prevent circular referencing.
		ISMPreviewEditorModule& PreviewModule = FModuleManager::LoadModuleChecked<ISMPreviewEditorModule>(LOGICDRIVER_PREVIEW_MODULE_NAME);
		PreviewObject = reinterpret_cast<UObject*>(PreviewModule.CreatePreviewObject(this));
	}
	
	return reinterpret_cast<USMPreviewObject*>(PreviewObject.Get());
}

void USMBlueprint::RecreatePreviewObject()
{
	if (PreviewObject)
	{
		ISMPreviewEditorModule& PreviewModule = FModuleManager::LoadModuleChecked<ISMPreviewEditorModule>(LOGICDRIVER_PREVIEW_MODULE_NAME);
		PreviewObject = reinterpret_cast<UObject*>(PreviewModule.RecreatePreviewObject(reinterpret_cast<USMPreviewObject*>(PreviewObject.Get())));
	}
}

#endif

USMNodeBlueprint::USMNodeBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), AssetVersion(0), PluginVersion(0)
{
	BlueprintType = BPTYPE_Normal;
}

#if WITH_EDITOR

UClass* USMNodeBlueprint::GetBlueprintClass() const
{
	return USMNodeBlueprintGeneratedClass::StaticClass();
}

void USMNodeBlueprint::GetReparentingRules(TSet<const UClass*>& AllowedChildrenOfClasses,
	TSet<const UClass*>& DisallowedChildrenOfClasses) const
{
	if (ParentClass)
	{
		if (ParentClass->IsChildOf(USMStateInstance::StaticClass()))
		{
			AllowedChildrenOfClasses.Add(USMStateInstance::StaticClass());
		}
		else if (ParentClass->IsChildOf(USMTransitionInstance::StaticClass()))
		{
			AllowedChildrenOfClasses.Add(USMTransitionInstance::StaticClass());
		}
		else if (ParentClass->IsChildOf(USMStateMachineInstance::StaticClass()))
		{
			AllowedChildrenOfClasses.Add(USMStateMachineInstance::StaticClass());
		}
	}

	if (AllowedChildrenOfClasses.Num() == 0)
	{
		AllowedChildrenOfClasses.Add(USMNodeInstance::StaticClass());
	}
}

bool USMNodeBlueprint::SupportsInputEvents() const
{
	if (GeneratedClass)
	{
		if (const USMNodeInstance* Instance = Cast<USMNodeInstance>(GeneratedClass->GetDefaultObject(false)))
		{
			return Instance->GetInputType() != ESMNodeInput::Disabled;
		}
	}
	return true;
}

#endif

USMNodeBlueprintGeneratedClass* USMNodeBlueprint::GetGeneratedClass() const
{
	return Cast<USMNodeBlueprintGeneratedClass>(*GeneratedClass);
}
