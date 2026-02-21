/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "DialogueMasterStatistic.h"

#define LOCTEXT_NAMESPACE "DialogueMaster"

UDialogueMasterStatistic::UDialogueMasterStatistic()
{
	StatName = FName::NameToDisplayString(GetName(), false);

	int idx;
	bool ContainsSpace = StatName.FindLastChar(' ', idx);

	DefaultEntityName = StatName.RightChop(idx + 1);

	if (ContainsSpace)
	{
		CountedEntityName = DefaultEntityName + " Name";
	}

	Description = FText::Format(LOCTEXT("DefaultDescriptionText", "{0}. Argument is the {1}"), FText::FromString(StatName), FText::FromString(CountedEntityName));
}

FString UDialogueMasterStatistic::GenerateStatKey(const FString& StatName, const FString& EntityName)
{
	// Add a to upper to avoid case sensitive typos ...
	FString result = ("DialogueMasterStat_" + StatName + "_" + EntityName).ToUpper();
	result.RemoveSpacesInline();	// And trim the spaces for the same reason.
	return result;
}

FString UDialogueMasterStatistic::GenerateStatKey(const FString& EntityName) const
{
	return GenerateStatKey(StatName, EntityName);
}

FText UDialogueMasterStatistic::GetDisplayText() const
{
	return FText::FromString(CountedEntityName);
}

#undef LOCTEXT_NAMESPACE