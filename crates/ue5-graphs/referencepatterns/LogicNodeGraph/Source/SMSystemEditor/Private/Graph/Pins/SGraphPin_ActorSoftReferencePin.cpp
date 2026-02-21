// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SGraphPin_ActorSoftReferencePin.h"

#include "Configuration/SMProjectEditorSettings.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "SGraphPin_SMDefaults.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "SMUnrealTypeDefs.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SMenuAnchor.h"

#define LOCTEXT_NAMESPACE "SMActorSoftReferencePin"

TSharedPtr<SGraphPin> FSMActorSoftReferencePinFactory::CreatePin(UEdGraphPin* InPin) const
{
	if (!InPin)
	{
		return nullptr;
	}

	UEdGraphNode* OwningNode = InPin->GetOwningNodeUnchecked();
	if (OwningNode == nullptr)
	{
		return nullptr;
	}

	const ESMPinOverride OverridePins = FSMBlueprintEditorUtils::GetProjectEditorSettings()->OverrideActorSoftReferencePins;
	
	if ((!OwningNode->IsA<USMGraphK2Node_PropertyNode_Base>() &&
		OverridePins != ESMPinOverride::AllBlueprints)
		|| OverridePins == ESMPinOverride::None)
	{
		// User has opted not to override generic soft actor reference pins.
		return nullptr;
	}
	
	UClass* TheClass;
	if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject &&
		InPin->PinType.PinSubCategoryObject.IsValid() &&
		(TheClass = Cast<UClass>(InPin->PinType.PinSubCategoryObject.Get())) != nullptr &&
		TheClass->IsChildOf<AActor>())
	{
		return SNew(SGraphPin_ActorSoftReferencePin, InPin);
	}

	return nullptr;
}

void FSMActorSoftReferencePinFactory::RegisterFactory()
{
	FEdGraphUtilities::RegisterVisualPinFactory(MakeShared<FSMActorSoftReferencePinFactory>());
}

void SGraphPin_ActorSoftReferencePin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget> SGraphPin_ActorSoftReferencePin::GetDefaultValueWidget()
{
	if (GraphPinObj == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	if (Schema == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	if (GraphPinObj->PinType.PinSubCategoryObject.IsValid())
	{
		PinObjectClass =
			Cast<UClass>(GraphPinObj->PinType.PinSubCategoryObject.Get());
	}

	if (PinObjectClass == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	if (Schema->ShouldShowAssetPickerForPin(GraphPinObj))
	{
		const TSharedRef<SWidget> ActorPicker =
			PropertyCustomizationHelpers::MakeInteractiveActorPicker(
				FOnGetAllowedClasses::CreateSP(
					this, &SGraphPin_ActorSoftReferencePin::OnGetAllowedClasses),
				FOnShouldFilterActor(),
				FOnActorSelected::CreateSP(
					this, &SGraphPin_ActorSoftReferencePin::OnActorSelected));
		ActorPicker->SetEnabled(!GraphPinObj->bDefaultValueIsReadOnly);

		return SNew(SHorizontalBox)
			.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0)
			.MaxWidth(200.0f)
			[
				SAssignNew(AssetPickerAnchor, SComboButton)
				.ButtonStyle(FSMUnrealAppStyle::Get(), "PropertyEditor.AssetComboStyle")
				.ForegroundColor(this, &SGraphPin_ActorSoftReferencePin::OnGetComboForeground)
				.ContentPadding(FMargin(2, 2, 2, 1))
				.ButtonColorAndOpacity(this, &SGraphPin_ActorSoftReferencePin::OnGetWidgetBackground)
				.MenuPlacement(MenuPlacement_BelowAnchor)
				.IsEnabled(this, &SGraphPin::IsEditingEnabled)
				.ButtonContent()
				[
					SNew(STextBlock)
					.ColorAndOpacity(this, &SGraphPin_ActorSoftReferencePin::OnGetComboForeground)
					.TextStyle(FSMUnrealAppStyle::Get(), "PropertyEditor.AssetClass")
					.Font(FSMUnrealAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
					.Text(this, &SGraphPin_ActorSoftReferencePin::OnGetComboTextValue)
					.ToolTipText(this, &SGraphPin_ActorSoftReferencePin::GetObjectToolTip)
				]
				.OnGetMenuContent(this, &SGraphPin_ActorSoftReferencePin::OnGetMenuContent)
			]
			+SHorizontalBox::Slot()
			.Padding(2.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &SGraphPin_ActorSoftReferencePin::OnBrowseToSelected))
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				ActorPicker
			];
	}

	return SNullWidget::NullWidget;
}

FText SGraphPin_ActorSoftReferencePin::GetDefaultComboText() const
{
	return LOCTEXT("DefaultComboText", "Select Actor");
}

FText SGraphPin_ActorSoftReferencePin::GetObjectToolTip() const { return GetValue(); }

FText SGraphPin_ActorSoftReferencePin::GetValue() const
{
	const FAssetData& CurrentAssetData = GetAssetData(true);
	FText Value;
	if (CurrentAssetData.IsValid())
	{
		Value = FText::FromString(CurrentAssetData.GetFullName());
	}
	else
	{
		if (GraphPinObj->GetSchema()->IsSelfPin(*GraphPinObj))
		{
			Value = FText::FromName(GraphPinObj->PinName);
		}
		else
		{
			Value = FText::GetEmpty();
		}
	}
	return Value;
}

FText SGraphPin_ActorSoftReferencePin::OnGetComboTextValue() const
{
	FText Value = GetDefaultComboText();

	if (GraphPinObj != nullptr)
	{
		const FAssetData& CurrentAssetData = GetAssetData(true);
		Value = FText::FromString(CurrentAssetData.AssetName.ToString());
	}
	return Value;
}

FSlateColor SGraphPin_ActorSoftReferencePin::OnGetComboForeground() const
{
	const float Alpha = (IsHovered() || bOnlyShowDefaultValue)
		                    ? ActiveComboAlpha
		                    : InActiveComboAlpha;
	return FSlateColor(FLinearColor(1.f, 1.f, 1.f, Alpha));
}

FSlateColor SGraphPin_ActorSoftReferencePin::OnGetWidgetBackground() const
{
	const float Alpha = (IsHovered() || bOnlyShowDefaultValue)
		                    ? ActivePinBackgroundAlpha
		                    : InactivePinBackgroundAlpha;
	return FSlateColor(FLinearColor(1.f, 1.f, 1.f, Alpha));
}

void SGraphPin_ActorSoftReferencePin::OnUse()
{
	if (UObject* Selection = GEditor->GetSelectedActors()->GetTop(PinObjectClass))
	{
		const FScopedTransaction Transaction(NSLOCTEXT("ActorSoftReferencePin", "OnUse", "Use Selected Actor"));
		GraphPinObj->Modify();
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, Selection->GetPathName());
	}
}

bool SGraphPin_ActorSoftReferencePin::IsFilteredActor(const AActor* const Actor) const
{
	return Actor && Actor->GetClass()->IsChildOf(PinObjectClass);
}

void SGraphPin_ActorSoftReferencePin::CloseComboButton()
{
	AssetPickerAnchor->SetIsOpen(false);
}

void SGraphPin_ActorSoftReferencePin::OnGetAllowedClasses(TArray<const UClass*>& AllowedClasses)
{
	AllowedClasses = { PinObjectClass };
}

void SGraphPin_ActorSoftReferencePin::OnActorSelected(AActor* InActor)
{
	if (!GraphPinObj || GraphPinObj->IsPendingKill())
	{
		return;
	}

	if (!InActor)
	{
		const FScopedTransaction Transaction(NSLOCTEXT("ActorSoftReferencePin", "ClearPinValue", "Clear Soft Reference"));
		GraphPinObj->Modify();
		GraphPinObj->ResetDefaultValue();
		return;
	}

	if (!GraphPinObj->GetDefaultAsString().Equals(InActor->GetPathName()))
	{
		const FScopedTransaction Transaction(NSLOCTEXT("ActorSoftReferencePin", "ChangePinValue", "Select Soft Reference"));
		GraphPinObj->Modify();
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, InActor->GetPathName());
	}
}

void SGraphPin_ActorSoftReferencePin::OnBrowseToSelected()
{
	if (!GraphPinObj || GraphPinObj->IsPendingKill())
	{
		return;
	}

	bool bBrowseToClass = false;
	if (AActor* Actor = GetActorFromAssetData())
	{
		GEditor->SelectNone(true, true, false);
		GEditor->SelectActor(Actor, true, true);
		GEditor->MoveViewportCamerasToActor(*Actor, true);
	}
	else
	{
		bBrowseToClass = true;
	}

	if (bBrowseToClass)
	{
		const FAssetData& CurrentAssetData = GetAssetData(true);
		if (const UClass* Class = CurrentAssetData.GetClass())
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy);
			if (ensure(Blueprint))
			{
				TArray<UObject*> SyncObjects;
				SyncObjects.Add(Blueprint);
				GEditor->SyncBrowserToObjects(SyncObjects);
			}
		}
	}
}

TSharedRef<SWidget> SGraphPin_ActorSoftReferencePin::OnGetMenuContent()
{
	const FAssetData& CurrentAssetData = GetAssetData(true);
	AActor* Actor = nullptr;
	if (CurrentAssetData.IsValid())
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
		{
			FString ActorName = *ActorItr->GetPathName();
			FString AssetName = CurrentAssetData.GetObjectPathString();
			if (ActorName == AssetName)
			{
				Actor = *ActorItr;
				break;
			}
		}
	}

	return PropertyCustomizationHelpers::MakeActorPickerWithMenu(
		Actor, true,
		FOnShouldFilterActor::CreateSP(this, &SGraphPin_ActorSoftReferencePin::IsFilteredActor),
		FOnActorSelected::CreateSP(this, &SGraphPin_ActorSoftReferencePin::OnActorSelected),
		FSimpleDelegate::CreateSP(this, &SGraphPin_ActorSoftReferencePin::CloseComboButton),
		FSimpleDelegate::CreateSP(this, &SGraphPin_ActorSoftReferencePin::OnUse));
}

const FAssetData& SGraphPin_ActorSoftReferencePin::GetAssetData(bool bRuntimePath) const
{
	// For normal assets, the editor and runtime path are the same
	if (GraphPinObj->DefaultObject)
	{
		if (!GraphPinObj->DefaultObject->GetPathName().Equals(CachedAssetData.GetObjectPathString(),
			ESearchCase::CaseSensitive))
		{
			// This always uses the exact object pointed at
			CachedAssetData = FAssetData(GraphPinObj->DefaultObject, true);
		}
	}
	else if (!GraphPinObj->DefaultValue.IsEmpty())
	{
		const FString ObjectPath = *GraphPinObj->DefaultValue;
		if (ObjectPath != CachedAssetData.GetObjectPathString())
		{
			const FAssetRegistryModule& AssetRegistryModule =
				FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
					TEXT("AssetRegistry"));

			CachedAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(ObjectPath);

			if (!CachedAssetData.IsValid())
			{
				const FString PackageName = FPackageName::ObjectPathToPackageName(GraphPinObj->DefaultValue);
				const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
				const FString ObjectName = FPackageName::ObjectPathToObjectName(GraphPinObj->DefaultValue);

				// Fake one
				CachedAssetData = FAssetData(FName(*PackageName), FName(*PackagePath),
					           FName(*ObjectName), UObject::StaticClass()->GetClassPathName());
			}
		}
	}
	else if (CachedAssetData.IsValid())
	{
		CachedAssetData = FAssetData();
	}

	return CachedAssetData;
}

AActor* SGraphPin_ActorSoftReferencePin::GetActorFromAssetData() const
{
	const FAssetData& CurrentAssetData = GetAssetData(true);
	AActor* Actor = nullptr;
	if (CurrentAssetData.IsValid())
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
		{
			FString ActorName = *ActorItr->GetPathName();
			FString AssetName = CurrentAssetData.GetObjectPathString();
			if (ActorName == AssetName)
			{
				Actor = *ActorItr;
				break;
			}
		}
	}

	return Actor;
}

#undef LOCTEXT_NAMESPACE
