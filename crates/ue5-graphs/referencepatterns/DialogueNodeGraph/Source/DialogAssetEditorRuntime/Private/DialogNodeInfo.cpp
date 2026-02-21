/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "DialogNodeInfo.h"
#include "DialogAsset.h"
#include "DialogueMasterComponent.h"

FName UDialogNodeInfo::GetNodeSwitchVariableName()
{
	UDialogAsset* asset = GetTypedOuter<UDialogAsset>();
	FString assetPrefix = "";
	if(asset != nullptr)
	{
		assetPrefix = asset->AssetId.ToString();
	}
	return FName(assetPrefix + NodeId.ToString());
}

FString UAdvancedPrerequisiteBase::GetDescription_Implementation()
{
    return "Not implemented AdvancedPrerequisite description getter !";
}

bool UDialogNodeInfo::checkPrerequisites(UDialogueMasterComponent * DialogueMasterComponent, AActor * NPC, AActor * Player)
{
    if(PlayableOnlyOnce)
    {
        bool alreadyPlayed = ISwitchAndCounterPrerequisitesInterface::Execute_getSwitchValue(DialogueMasterComponent, GetNodeSwitchVariableName());
        if(alreadyPlayed)
            return false;
    }

    
    for(const FSwitchDialogPrerequisite req : Replique.SwitchPrerequisites)
    {
        if(ISwitchAndCounterPrerequisitesInterface::Execute_getSwitchValue(DialogueMasterComponent, req.SwitchPrerequisiteName) != req.NeededValue)
            return false;
    }

    for(const FCounterDialogPrerequisite req : Replique.CounterPrerequisites)
    {
        int currentValue = ISwitchAndCounterPrerequisitesInterface::Execute_getCounterValue(DialogueMasterComponent, req.CounterPrerequisiteName);
        switch(req.ValueCheckOperator)
        {
        case Equals:
            if(!(currentValue == req.NeededValue))
                return false;
            break;

        case NotEquals:
            if(!(currentValue != req.NeededValue))
                return false;
            break;

        case Greater:
            if(!(currentValue > req.NeededValue))
                return false;
            break;

        case GreaterOrEquals:
            if(!(currentValue >= req.NeededValue))
                return false;
            break;

        case Smaller:
            if(!(currentValue < req.NeededValue))
                return false;
            break;

        case SmallerOrEquals:
            if(!(currentValue <= req.NeededValue))
                return false;
            break;
        }
    }

    for(UAdvancedPrerequisiteBase* prerequisite : Replique.AdvancedPrerequisites)
    {
        bool result;
        
        result = prerequisite->CheckPrerequisite(NPC, Player, DialogueMasterComponent);

        if(!result)
            return false;
    }
    
    return true;
}