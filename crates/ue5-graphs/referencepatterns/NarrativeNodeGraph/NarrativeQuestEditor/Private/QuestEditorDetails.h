// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 * 
 */
class FQuestEditorDetails : public IDetailCustomization
{

public:

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/**We cache a reference to this so we can do refreshes*/
	IDetailLayoutBuilder* LayoutBuilder;

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

	void OnArgumentChanged();
	void OnTaskChanged();

};
