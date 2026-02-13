// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerSequencer.h"
#include "KClonerActor.h"
#include "KClonerModifier.h"
#include "UObject/UnrealType.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Evaluation/MovieSceneEvaluationOperand.h"
#include "Evaluation/MovieSceneExecutionTokens.h"
#include "IMovieScenePlayer.h"

// ============================================================================
// UMovieSceneKClonerModifierSection
// ============================================================================

UMovieSceneKClonerModifierSection::UMovieSceneKClonerModifierSection()
{
	SetRange(TRange<FFrameNumber>::All());
}

TOptional<TRange<FFrameNumber>> UMovieSceneKClonerModifierSection::GetAutoSizeRange() const
{
	// figure out how long the track should be based on keys
	// iterate all params and find the min/max frame range
	FFrameNumber MinFrame = TNumericLimits<FFrameNumber>::Max();
	FFrameNumber MaxFrame = TNumericLimits<FFrameNumber>::Lowest();

	for (const FKClonerFloatParam& P : FloatParams)
	{
		TRange<FFrameNumber> Range = P.Channel.ComputeEffectiveRange();
		if (!Range.IsEmpty())
		{
			MinFrame = FMath::Min(MinFrame, Range.GetLowerBoundValue());
			MaxFrame = FMath::Max(MaxFrame, Range.GetUpperBoundValue());
		}
	}
	for (const FKClonerBoolParam& P : BoolParams)
	{
		TRange<FFrameNumber> Range = P.Channel.ComputeEffectiveRange();
		if (!Range.IsEmpty())
		{
			MinFrame = FMath::Min(MinFrame, Range.GetLowerBoundValue());
			MaxFrame = FMath::Max(MaxFrame, Range.GetUpperBoundValue());
		}
	}
	for (const FKClonerVectorParam& P : VectorParams)
	{
		TRange<FFrameNumber> RX = P.X.ComputeEffectiveRange();
		TRange<FFrameNumber> RY = P.Y.ComputeEffectiveRange();
		TRange<FFrameNumber> RZ = P.Z.ComputeEffectiveRange();
		if (!RX.IsEmpty())
		{
			MinFrame = FMath::Min(MinFrame, RX.GetLowerBoundValue());
			MaxFrame = FMath::Max(MaxFrame, RX.GetUpperBoundValue());
		}
		if (!RY.IsEmpty())
		{
			MinFrame = FMath::Min(MinFrame, RY.GetLowerBoundValue());
			MaxFrame = FMath::Max(MaxFrame, RY.GetUpperBoundValue());
		}
		if (!RZ.IsEmpty())
		{
			MinFrame = FMath::Min(MinFrame, RZ.GetLowerBoundValue());
			MaxFrame = FMath::Max(MaxFrame, RZ.GetUpperBoundValue());
		}
	}

	if (MinFrame <= MaxFrame)
	{
		return TRange<FFrameNumber>(MinFrame, MaxFrame + 1);
	}
	return TOptional<TRange<FFrameNumber>>();
}

void UMovieSceneKClonerModifierSection::EvaluateAndApply(AKClonerActor* Cloner, FFrameTime Time) const
{
	if (!Cloner) return;

	bool bAnyChanged = false;

	// check all floats (pulse freq, strength, etc)
	for (const FKClonerFloatParam& P : FloatParams)
	{
		float Val = 0.0f; 
		if (P.Channel.Evaluate(Time, Val))
		{
			if (UKClonerModifier* M = FindModifierByGuid(Cloner, P.ModifierGuid))
			{
				if (FFloatProperty* FP = CastField<FFloatProperty>(M->GetClass()->FindPropertyByName(P.PropertyName)))
				{
					FP->SetPropertyValue_InContainer(M, Val);
					bAnyChanged = true;
				}
				else if (FDoubleProperty* DP = CastField<FDoubleProperty>(M->GetClass()->FindPropertyByName(P.PropertyName)))
				{
					DP->SetPropertyValue_InContainer(M, (double)Val);
					bAnyChanged = true;
				}
			}
		}
	}

	// check all bools (invert, enabled, etc)
	for (const FKClonerBoolParam& P : BoolParams)
	{
		bool bVal = false;
		if (P.Channel.Evaluate(Time, bVal))
		{
			if (UKClonerModifier* M = FindModifierByGuid(Cloner, P.ModifierGuid))
			{
				if (FBoolProperty* BP = CastField<FBoolProperty>(M->GetClass()->FindPropertyByName(P.PropertyName)))
				{
					BP->SetPropertyValue_InContainer(M, bVal);
					bAnyChanged = true;
				}
			}
		}
	}

	// check vectors (direction, scale, etc)
	// handling structs in reflection is always a pain
	for (const FKClonerVectorParam& P : VectorParams)
	{
		float X = 0.0f, Y = 0.0f, Z = 0.0f;  // 
		const bool bx = P.X.Evaluate(Time, X);
		const bool by = P.Y.Evaluate(Time, Y);
		const bool bz = P.Z.Evaluate(Time, Z);
		
		if (bx || by || bz)
		{
			if (UKClonerModifier* M = FindModifierByGuid(Cloner, P.ModifierGuid))
			{
				if (FStructProperty* SP = CastField<FStructProperty>(M->GetClass()->FindPropertyByName(P.PropertyName)))
				{
					if (SP->Struct && SP->Struct->GetFName() == NAME_Vector)
					{
						FVector Val(X, Y, Z);
						SP->CopyCompleteValue_InContainer(M, &Val);
						bAnyChanged = true;
					}
				}
			}
		}
	}

	// tell the cloner to rebuild if we touched anything
	if (bAnyChanged)
	{
		Cloner->MarkModifiersDirty();
	}
}

UKClonerModifier* UMovieSceneKClonerModifierSection::FindModifierByGuid(AKClonerActor* Cloner, const FGuid& Guid)
{
	if (!Cloner) return nullptr;
	
	for (UKClonerModifier* Mod : Cloner->Modifiers)
	{
		if (Mod && Mod->ModifierGuid == Guid)
		{
			return Mod;
		}
	}
	return nullptr;
}

// ============================================================================
// FKClonerModifierSectionTemplate - THE ENGINE THAT RUNS KEYFRAMES
// ============================================================================

FKClonerModifierSectionTemplate::FKClonerModifierSectionTemplate(const UMovieSceneKClonerModifierSection& InSection)
	: SectionPtr(&InSection)
{
}

void FKClonerModifierSectionTemplate::Evaluate(
	const FMovieSceneEvaluationOperand& Operand,
	const FMovieSceneContext& Context,
	const FPersistentEvaluationData& PersistentData,
	FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const UMovieSceneKClonerModifierSection* Section = SectionPtr.Get();
	if (!Section) return;
	
	// Create an execution token that will run when it's time
	struct FKClonerExecutionToken : IMovieSceneExecutionToken
	{
		const UMovieSceneKClonerModifierSection* Section;
		FFrameTime Time;
		
		FKClonerExecutionToken(const UMovieSceneKClonerModifierSection* InSection, FFrameTime InTime)
			: Section(InSection), Time(InTime) {}
		
		virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, 
			FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
		{
			// Find the bound cloner actor and call EvaluateAndApply on it
			for (TWeakObjectPtr<> Object : Player.FindBoundObjects(Operand))
			{
				if (AKClonerActor* Cloner = Cast<AKClonerActor>(Object.Get()))
				{
					Section->EvaluateAndApply(Cloner, Time);
				}
			}
		}
	};
	
	// Add token to the execution queue
	ExecutionTokens.Add(FKClonerExecutionToken(Section, Context.GetTime()));
}

// ============================================================================
// UMovieSceneKClonerModifierTrack
// ============================================================================

bool UMovieSceneKClonerModifierTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneKClonerModifierSection::StaticClass();
}

UMovieSceneSection* UMovieSceneKClonerModifierTrack::CreateNewSection()
{
	return NewObject<UMovieSceneKClonerModifierSection>(this, NAME_None, RF_Transactional);
}

// THE CRITICAL FUNCTION - this tells Sequencer how to evaluate our sections
FMovieSceneEvalTemplatePtr UMovieSceneKClonerModifierTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	const UMovieSceneKClonerModifierSection* Section = Cast<const UMovieSceneKClonerModifierSection>(&InSection);
	if (Section)
	{
		return FKClonerModifierSectionTemplate(*Section);
	}
	return FMovieSceneEvalTemplatePtr();
}
