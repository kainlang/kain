// © 2025 mrbaconvn. All Rights Reserved.

#include "Widget/ComboGraphDebugSelectorWidget.h"
#include "ComboManager/ComboManagerComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

void SComboGraphDebugSelector::Construct(const FArguments& InArgs)
{
	DebugComboGraphChanged = InArgs._DebugComboGraphChanged;

    ComboBoxWidget = SNew(SComboBox<TWeakObjectPtr<UComboManagerComponent>>)
        .OptionsSource(&DebugObjects)
        .InitiallySelectedItem(GetDebugObjectInstance())
        .OnComboBoxOpening(this, &SComboGraphDebugSelector::GenerateDebugObjectInstances)
        .OnSelectionChanged(this, &SComboGraphDebugSelector::DebugObjectSelectionChanged)
        .OnGenerateWidget(this, &SComboGraphDebugSelector::CreateDebugObjectItemWidget)
        .ContentPadding(FMargin(4.f, 4.f))
        [
            SNew(STextBlock)
                .Text(this, &SComboGraphDebugSelector::GetSelectedDebugObjectTextLabel)
        ];

    ChildSlot
        [
            ComboBoxWidget.ToSharedRef()
        ];
}

void SComboGraphDebugSelector::GenerateDebugObjectInstances()
{
	if (!IsValid(GEditor))
	{
		return;
	}

	auto WorldContext = GEditor->GetWorldContexts(); 
	DebugObjects.Empty();

	for(const FWorldContext& Context : WorldContext)
	{
	    UWorld* World = Context.World();
	    if (!IsValid(World) || !World->IsGameWorld())
		{
			continue;
	    }  
		for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
		{
			APawn* Pawn = Cast<APawn>(*ActorItr);
	        if (!IsValid(Pawn))
	        {
	            continue;
	        }
			UComboManagerComponent* ComboManager = Pawn->FindComponentByClass<UComboManagerComponent>();
			if (IsValid(ComboManager))
			{
				if(Pawn->IsLocallyControlled())
				{
					DebugObjects.Insert(ComboManager, 0);	
				}
				else
				{
					DebugObjects.Add(ComboManager);
				}
			}
		}
	}

    if (ComboBoxWidget.IsValid())
    {
		if (DebugObjects.Num() > 0)
		{
			ComboBoxWidget->SetSelectedItem(DebugObjects[0]);
		}
        ComboBoxWidget.Get()->RefreshOptions();
    }
}

void SComboGraphDebugSelector::DebugObjectSelectionChanged(TWeakObjectPtr<UComboManagerComponent> NewSelection, ESelectInfo::Type SelectInfo)
{
    DebugComboGraphChanged.ExecuteIfBound(NewSelection);
}

TSharedRef<SWidget> SComboGraphDebugSelector::CreateDebugObjectItemWidget(TWeakObjectPtr<UComboManagerComponent> Item) const
{ 
    return SNew(STextBlock)
        .Text(FText::FromString(GetDebugObjectName(Item)));
}

FString SComboGraphDebugSelector::GetDebugObjectName(TWeakObjectPtr<UComboManagerComponent> DebugObject) const
{ 
	if (DebugObject.IsValid() && IsValid(DebugObject->GetOwner()))
	{
		FString IsServerOrClient = DebugObject->GetOwner()->HasAuthority() ? "Server" : "Client";
		return DebugObject->GetOwner()->GetName() + " ("+IsServerOrClient+")";
	}
	return "No Debug Object Selected";
}

FText SComboGraphDebugSelector::GetSelectedDebugObjectTextLabel() const
{
	return FText::FromString(GetDebugObjectName(GetDebugObjectInstance()));
}

TWeakObjectPtr<UComboManagerComponent> SComboGraphDebugSelector::GetDebugObjectInstance() const
{
	if (!ComboBoxWidget.IsValid())
	{
		return nullptr;
	}
    return ComboBoxWidget->GetSelectedItem();
}
