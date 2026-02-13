// Copyright 2026 K-Studio. All Rights Reserved.

// KClonerActorDetails.h
#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class UKClonerModifier;

class FKClonerActorDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	/** Callback for Bake Animation button */
	FReply OnBakeAnimationClicked();

	/** Quick-key all Interp properties on a modifier to Sequencer */
	FReply OnQuickKeyModifierClicked(UKClonerModifier* Modifier);

	/** Pointers to valid objects being customized */
	TWeakObjectPtr<class AKClonerActor> SelectedCloner;
};
