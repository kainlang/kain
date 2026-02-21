// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Widgets/SWindow.h"

struct FSMSystemInfo;

class SSMSupportDialog : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SSMSupportDialog)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<FSMSystemInfo>& InSupportInfo);

private:
	FReply OnCopyClicked();
	
private:
	TSharedPtr<FSMSystemInfo> SystemInfo;
};
