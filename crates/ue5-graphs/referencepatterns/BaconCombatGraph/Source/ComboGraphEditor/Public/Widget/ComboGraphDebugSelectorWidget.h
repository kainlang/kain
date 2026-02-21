// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h" 
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"

class UComboManagerComponent; 

DECLARE_DELEGATE_OneParam(FOnDebugComboGraphChanged, TWeakObjectPtr<UComboManagerComponent>);

class SComboGraphDebugSelector : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SComboGraphDebugSelector) {}
        SLATE_EVENT(FOnDebugComboGraphChanged, DebugComboGraphChanged);
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

public:
    void GenerateDebugObjectInstances();

    void DebugObjectSelectionChanged(TWeakObjectPtr<UComboManagerComponent> NewSelection, ESelectInfo::Type SelectInfo);

    TSharedRef<SWidget> CreateDebugObjectItemWidget(TWeakObjectPtr<UComboManagerComponent> Item) const;
    
    FString GetDebugObjectName(TWeakObjectPtr<UComboManagerComponent> DebugObject) const;

    FText GetSelectedDebugObjectTextLabel() const;

    TWeakObjectPtr<UComboManagerComponent> GetDebugObjectInstance() const;

protected:
    TSharedPtr<SComboBox<TWeakObjectPtr<UComboManagerComponent>>> ComboBoxWidget;
    TArray<TWeakObjectPtr<UComboManagerComponent>> DebugObjects;
    FOnDebugComboGraphChanged DebugComboGraphChanged;
};