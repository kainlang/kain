/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "AnimUtils.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimCurveTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"

void UAnimUtils::CopyMetaHumanCurvesToARKit(UAnimSequence* AnimSequence)
{
	 if (!AnimSequence)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid AnimSequence!"));
        return;
    }

    // Check if asset is loaded:
    if (AnimSequence->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
    {
        UE_LOG(LogTemp, Error, TEXT("AnimSequence not ready. Process cancelled."));
        return;
    }
    
    // Save the asset on disk before processing anything:
    AnimSequence->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(AnimSequence);
    AnimSequence->GetOutermost()->MarkPackageDirty();
    
    
    const IAnimationDataModel* DataModel = AnimSequence->GetDataModel();
    if(!DataModel)
    {
        UE_LOG(LogTemp, Error, TEXT("No DataModel on this animation!"));
        return;
    }

    struct FCurveMapping
    {
        FString SourceCurve;
        FString TargetCurve;
    };

    // Mapping list between MetaHuman curves and ARKit curves:
    TArray<FCurveMapping> Mappings = {
        // Eye Left :
        { "CTRL_expressions_eyeBlinkL", "eyeBlinkLeft" },
        { "CTRL_expressions_eyeLookDownL", "eyeLookDownLeft" },
        { "CTRL_expressions_eyeLookRightL", "eyeLookInLeft" },
        { "CTRL_expressions_eyeLookLeftL", "eyeLookOutLeft" },
        { "CTRL_expressions_eyeLookUpL", "eyeLookUpLeft" },
        { "CTRL_expressions_eyeSquintInnerL", "eyeSquintLeft" },
        { "CTRL_expressions_eyeWidenL", "eyeWideLeft" },

        // Eye Right :
        { "CTRL_expressions_eyeBlinkR", "eyeBlinkRight" },
        { "CTRL_expressions_eyeLookDownR", "eyeLookDownRight" },
        { "CTRL_expressions_eyeLookLeftR", "eyeLookInRight" },
        { "CTRL_expressions_eyeLookRightR", "eyeLookOutRight" },
        { "CTRL_expressions_eyeLookUpR", "eyeLookUpRight" },
        { "CTRL_expressions_eyeSquintInnerR", "eyeSquintRight" },
        { "CTRL_expressions_eyeWidenR", "eyeWideRight" },

        // Jaw :
        { "CTRL_expressions_jawFwd", "jawForward" },
        { "CTRL_expressions_jawLeft", "jawLeft" },
        { "CTRL_expressions_jawRight", "jawRight" },
        { "CTRL_expressions_jawOpen", "jawOpen" },

        // Mouth :
        { "CTRL_expressions_mouthFunnelUL", "mouthFunnel" },
        { "CTRL_expressions_mouthLipsPurseUL", "mouthPucker" },
        { "head_cm3_color_head_wm3_smile_L", "mouthLeft" },
        { "head_cm3_color_head_wm3_smile_R", "mouthRight" },
        { "head_cm3_color_head_wm3_smile_L", "mouthSmileLeft" },
        { "head_cm3_color_head_wm3_smile_R", "mouthSmileRight" },
        { "CTRL_expressions_mouthCornerDepressL", "mouthFrownLeft" },
        { "CTRL_expressions_mouthCornerDepressR", "mouthFrownRight" },
        { "CTRL_expressions_mouthDimpleL", "mouthDimpleLeft" },
        { "CTRL_expressions_mouthDimpleR", "mouthDimpleRight" },
        { "CTRL_expressions_mouthStretchL", "mouthStretchLeft" },
        { "CTRL_expressions_mouthStretchR", "mouthStretchRight" },
        { "CTRL_expressions_mouthLowerLipRollInL", "mouthRollLower" },
        { "CTRL_expressions_mouthUpperLipRollInL", "mouthRollUpper" },
        { "CTRL_expressions_jawChinRaiseDL", "mouthShrugLower" },
        { "CTRL_expressions_jawChinRaiseUL", "mouthShrugUpper" },
        { "CTRL_expressions_mouthPressUL", "mouthPressLeft" },
        { "CTRL_expressions_mouthPressUR", "mouthPressRight" },
        { "CTRL_expressions_mouthLowerLipDepressL", "mouthLowerDownLeft" },
        { "CTRL_expressions_mouthLowerLipDepressR", "mouthLowerDownRight" },
        { "CTRL_expressions_mouthUpperLipRaiseL", "mouthUpperUpLeft" },
        { "CTRL_expressions_mouthUpperLipRaiseR", "mouthUpperUpRight" },

        // Brow :
        { "CTRL_expressions_browDownL", "browDownLeft" },
        { "CTRL_expressions_browDownR", "browDownRight" },
        { "CTRL_expressions_browLateralL", "browInnerUp" },
        { "CTRL_expressions_browRaiseOuterL", "browOuterUpLeft" },
        { "CTRL_expressions_browRaiseOuterR", "browOuterUpRight" },

        // Cheek :
        { "CTRL_expressions_mouthCheekBlowL", "cheekPuff" },
        { "CTRL_expressions_eyeCheekRaiseL", "cheekSquintLeft" },
        { "CTRL_expressions_eyeCheekRaiseR", "cheekSquintRight" },

        // Nose :
        { "CTRL_expressions_noseWrinkleL", "noseSneerLeft" },
        { "CTRL_expressions_noseWrinkleR", "noseSneerRight" },

        // Tongue :
        { "CTRL_expressions_tongueDown", "tongueOut" },
    };

    USkeleton* Skeleton = AnimSequence->GetSkeleton();
    if (!Skeleton)
    {
        UE_LOG(LogTemp, Error, TEXT("No valid skeleton on this AnimSequence."));
        return;
    }

    for (const FCurveMapping& Mapping : Mappings)
    {
        FName SourceCurveName(*Mapping.SourceCurve);
        FName TargetCurveName(*Mapping.TargetCurve);

        // Check if source curve exists:
        FAnimationCurveIdentifier SourceId(SourceCurveName, ERawCurveTrackTypes::RCT_Float);
        FFloatCurve* SourceCurve = const_cast<FFloatCurve*>(DataModel->FindFloatCurve(SourceId));
        if (!SourceCurve)
        {
            UE_LOG(LogTemp, Warning, TEXT("Source curve not found : %s"), *Mapping.SourceCurve);
            continue;
        }

        // Save the keys :
        TArray<FRichCurveKey> Keys;
        for (const FRichCurveKey& Key : SourceCurve->FloatCurve.GetConstRefOfKeys())
        {
            Keys.Add(Key);
        }

        // Check if target curve exists in skeleton
        if (!Skeleton->GetCurveMetaData(TargetCurveName))
        {
            // Add curve metadata if not existing
            Skeleton->AddCurveMetaData(TargetCurveName);
        }
        
        FAnimationCurveIdentifier TargetId(TargetCurveName, ERawCurveTrackTypes::RCT_Float);

        IAnimationDataController& Controller = AnimSequence->GetController();
        if (DataModel->FindFloatCurve(TargetId))
        {
            Controller.RemoveCurve(TargetId, false); // Remove the curve if already here (avoid conflict)
        }

        Controller.AddCurve(TargetId, AACF_DriveTrack, false);
        Controller.SetCurveKeys(TargetId, Keys, false);

        UE_LOG(LogTemp, Log, TEXT("Copied curve : %s -> %s"), *Mapping.SourceCurve, *Mapping.TargetCurve);
    }

    // Mark asset as modified to save it.
    AnimSequence->MarkPackageDirty();
}
