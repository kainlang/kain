// © 2025 mrbaconvn. All Rights Reserved.

#include "ComboGraphFactory.h"

#include "ClassViewerModule.h"
#include "ComboGraphAsset.h"
#include "Kismet2/SClassPickerDialog.h"

#define LOCTEXT_NAMESPACE "ComboGraphFactory"
#define PICK_BASE_CLASS_TITLE LOCTEXT("PickBaseClass", "Pick a Combo Graph base class")

/////////////////////////////////////////////////////
// FComboGraphBaseClassFilter

bool FComboGraphBaseClassFilter::IsClassAllowed(
    const FClassViewerInitializationOptions& InInitOptions,
    const UClass* InClass,
    TSharedRef<FClassViewerFilterFuncs> InFilterFuncs)
{
    return InClass->IsChildOf(UComboGraphAsset::StaticClass()) && !InClass->HasAnyClassFlags(CLASS_Abstract);
}

bool FComboGraphBaseClassFilter::IsUnloadedClassAllowed(
    const FClassViewerInitializationOptions& InInitOptions,
    const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData,
    TSharedRef<FClassViewerFilterFuncs> InFilterFuncs)
{
    return InUnloadedClassData->IsChildOf(UComboGraphAsset::StaticClass()) && !InUnloadedClassData->HasAnyClassFlags(CLASS_Abstract);
}

/////////////////////////////////////////////////////
// UComboGraphFactory

UComboGraphFactory::UComboGraphFactory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SupportedClass = UComboGraphAsset::StaticClass();
    bCreateNew = true;
    bEditAfterNew = true;
}

bool UComboGraphFactory::ConfigureProperties()
{
    ChosenBaseClass = nullptr;

    FClassViewerInitializationOptions Options;
    Options.Mode = EClassViewerMode::ClassPicker;
    Options.DisplayMode = EClassViewerDisplayMode::TreeView;
    Options.bShowUnloadedBlueprints = true;
    Options.bShowNoneOption = false;

    Options.ClassFilters.Add(MakeShared<FComboGraphBaseClassFilter>());

    UClass* PickedClass = nullptr;
    const bool bPressedOk = SClassPickerDialog::PickClass(
        PICK_BASE_CLASS_TITLE,
        Options,
        PickedClass,
        UComboGraphAsset::StaticClass()
    );

    if (bPressedOk)
    {
        ChosenBaseClass = PickedClass;
        return true;
    }

    return false;
}

UObject* UComboGraphFactory::FactoryCreateNew(
    UClass* Class,
    UObject* InParent,
    FName Name,
    EObjectFlags Flags,
    UObject* Context,
    FFeedbackContext* Warn)
{
    if (ChosenBaseClass == nullptr)
    {
        return nullptr;
    }

    return NewObject<UObject>(InParent, ChosenBaseClass, Name, Flags);
}

bool UComboGraphFactory::CanCreateNew() const
{
    return true;
}

#undef PICK_BASE_CLASS_TITLE
#undef LOCTEXT_NAMESPACE
