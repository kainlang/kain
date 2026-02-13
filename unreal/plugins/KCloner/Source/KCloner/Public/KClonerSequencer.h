// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneNameableTrack.h"
#include "MovieSceneSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneBoolChannel.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "UObject/NoExportTypes.h"
#include "KClonerSequencer.generated.h"

class AKClonerActor;
class UKClonerModifier;

/**
 * Float parameter keyframe data for K-Cloner modifiers
 */
USTRUCT()
struct FKClonerFloatParam
{
	GENERATED_BODY()
	
	UPROPERTY()
	FGuid ModifierGuid;
	
	UPROPERTY()
	FName PropertyName;
	
	UPROPERTY()
	FMovieSceneFloatChannel Channel;
};

/**
 * Bool parameter keyframe data for K-Cloner modifiers
 */
USTRUCT()
struct FKClonerBoolParam
{
	GENERATED_BODY()
	
	UPROPERTY()
	FGuid ModifierGuid;
	
	UPROPERTY()
	FName PropertyName;
	
	UPROPERTY()
	FMovieSceneBoolChannel Channel;
};

/**
 * Vector parameter keyframe data for K-Cloner modifiers (3 float channels)
 */
USTRUCT()
struct FKClonerVectorParam
{
	GENERATED_BODY()
	
	UPROPERTY()
	FGuid ModifierGuid;
	
	UPROPERTY()
	FName PropertyName;
	
	UPROPERTY()
	FMovieSceneFloatChannel X;
	
	UPROPERTY()
	FMovieSceneFloatChannel Y;
	
	UPROPERTY()
	FMovieSceneFloatChannel Z;
};

// Forward declare
class UMovieSceneKClonerModifierSection;

/**
 * Movie Scene Section for K-Cloner modifier keyframes
 * Stores all keyframed parameters per modifier
 */
UCLASS()
class KCLONER_API UMovieSceneKClonerModifierSection : public UMovieSceneSection
{
	GENERATED_BODY()
	
public:
	UMovieSceneKClonerModifierSection();

	/** Float property channels */
	UPROPERTY()
	TArray<FKClonerFloatParam> FloatParams;
	
	/** Bool property channels */
	UPROPERTY()
	TArray<FKClonerBoolParam> BoolParams;
	
	/** Vector property channels */
	UPROPERTY()
	TArray<FKClonerVectorParam> VectorParams;

	// UMovieSceneSection interface
	virtual TOptional<TRange<FFrameNumber>> GetAutoSizeRange() const override;
	virtual void SetBlendType(EMovieSceneBlendType InBlendType) override { /* No blending for properties */ }
	
	/** Evaluate all channels at the given time and apply to the cloner */
	void EvaluateAndApply(AKClonerActor* Cloner, FFrameTime Time) const;
	
private:
	/** Find modifier by GUID in cloner */
	static UKClonerModifier* FindModifierByGuid(AKClonerActor* Cloner, const FGuid& Guid);
};

/**
 * Evaluation template - THIS IS THE ENGINE THAT ACTUALLY RUNS THE KEYFRAMES
 * Without this, the Section data just sits there doing nothing lol
 */
USTRUCT()
struct FKClonerModifierSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FKClonerModifierSectionTemplate() : SectionPtr(nullptr) {}
	FKClonerModifierSectionTemplate(const UMovieSceneKClonerModifierSection& InSection);
	
	// The magic function - called every frame during playback
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, 
		const FMovieSceneContext& Context, 
		const FPersistentEvaluationData& PersistentData, 
		FMovieSceneExecutionTokens& ExecutionTokens) const override;
	
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	
private:
	// Can't use UPROPERTY for raw pointers in templates, store as weak ref
	UPROPERTY()
	TWeakObjectPtr<const UMovieSceneKClonerModifierSection> SectionPtr;
};

/**
 * Movie Scene Track for K-Cloner modifier animations
 */
UCLASS()
class KCLONER_API UMovieSceneKClonerModifierTrack : public UMovieSceneNameableTrack
{
	GENERATED_BODY()
	
public:
	// UMovieSceneTrack interface
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override { return Sections; }
	virtual void AddSection(UMovieSceneSection& Section) override { Sections.Add(&Section); }
	virtual void RemoveSection(UMovieSceneSection& Section) override { Sections.Remove(&Section); }
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override { return Sections.Contains(&Section); }
	virtual void RemoveAllAnimationData() override { Sections.Reset(); }
	virtual bool IsEmpty() const override { return Sections.Num() == 0; }
	
	// THE CRITICAL FUNCTION - creates the evaluation template for each section
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override { return NSLOCTEXT("KCloner", "TrackName", "K-Cloner Modifiers"); }
#endif

protected:
	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneSection>> Sections;
};

