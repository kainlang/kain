// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerFilter.h"
#include "Factories/Factory.h"
#include "ComboGraphFactory.generated.h"

/////////////////////////////////////////////////////
// FComboGraphBaseClassFilter

class FComboGraphBaseClassFilter : public IClassViewerFilter
{
public:
    virtual bool IsClassAllowed(
        const FClassViewerInitializationOptions& InInitOptions,
        const UClass* InClass,
        TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override;

    virtual bool IsUnloadedClassAllowed(
        const FClassViewerInitializationOptions& InInitOptions,
        const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData,
        TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override;
};

/////////////////////////////////////////////////////
// UComboGraphFactory

UCLASS()
class COMBOGRAPHEDITOR_API UComboGraphFactory : public UFactory
{
    GENERATED_BODY()

public:
    UComboGraphFactory(const FObjectInitializer& ObjectInitializer);

public: // UFactory interface
    virtual bool ConfigureProperties() override;

    virtual UObject* FactoryCreateNew(
        UClass* Class,
        UObject* InParent,
        FName Name,
        EObjectFlags Flags,
        UObject* Context,
        FFeedbackContext* Warn) override;

    virtual bool CanCreateNew() const override;

protected:
    UPROPERTY()
    TSubclassOf<class UComboGraphAsset> ChosenBaseClass;
};
