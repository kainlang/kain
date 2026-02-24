#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

DECLARE_DELEGATE_TwoParams(FOnSelectionChanged, TSharedPtr<FString>, ESelectInfo::Type);

class SSSFluidSimulationDashboard : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSSFluidSimulationDashboard) {}
        SLATE_ARGUMENT(FOnSelectionChanged, on_quality_changed)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
};
