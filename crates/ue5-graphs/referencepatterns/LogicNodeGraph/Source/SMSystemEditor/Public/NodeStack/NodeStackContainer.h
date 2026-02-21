// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMStateInstance.h"
#include "SMTransitionInstance.h"

#include "NodeStackContainer.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct FNodeStackContainer
{
	GENERATED_BODY()

	/** The instanced template to use as an archetype. */
	UPROPERTY(VisibleDefaultsOnly, Instanced, Category = "Class", meta = (DisplayName = Template))
	TObjectPtr<USMNodeInstance> NodeStackInstanceTemplate;
	
	UPROPERTY()
	FGuid TemplateGuid;
	
	explicit FNodeStackContainer(USMNodeInstance* InTemplate = nullptr) : NodeStackInstanceTemplate(InTemplate) {}

	virtual ~FNodeStackContainer() = default;
	
	/** The class to assign the template for this node stack. */
	virtual TSubclassOf<USMNodeInstance> GetNodeClass() { return nullptr; }
	
	void InitTemplate(UObject* Owner, bool bForceInit = false, bool bForceNewGuid = false);
	void DestroyTemplate();

	/** Format a friendly name given a node instance and optional index. */
	static FText FormatStackInstanceName(const USMNodeInstance* InNodeInstance, int32 InIndex = INDEX_NONE);
};

USTRUCT(BlueprintInternalUseOnly)
struct FStateStackContainer : public FNodeStackContainer
{
	GENERATED_BODY()
	
	/** The class to assign the template for this state stack. */
	UPROPERTY(EditAnywhere, Category = "Class", meta = (BlueprintBaseOnly))
	TSubclassOf<USMStateInstance> StateStackClass;

	FStateStackContainer() : Super(nullptr), StateStackClass(nullptr) {}
	explicit FStateStackContainer(TSubclassOf<USMStateInstance> InClass, USMNodeInstance* InTemplate = nullptr) : Super(InTemplate), StateStackClass(InClass) {}

	virtual TSubclassOf<USMNodeInstance> GetNodeClass() override { return StateStackClass; }
};

UENUM(BlueprintInternalUseOnly)
enum class ESMExpressionMode : uint8
{
	AND,
	OR
};

USTRUCT(BlueprintInternalUseOnly)
struct FTransitionStackContainer : public FNodeStackContainer
{
	GENERATED_BODY()

	/** NOT the result when auto formatting the graph. */
	UPROPERTY(EditAnywhere, Category = "Transitions")
	bool bNOT;

	/** The operation to auto format the graph to. */
	UPROPERTY(EditAnywhere, Category = "Transitions")
	ESMExpressionMode Mode;
	
	/** The class to assign the template for this transition stack. */
	UPROPERTY(EditAnywhere, Category = "Class", meta = (BlueprintBaseOnly))
	TSubclassOf<USMTransitionInstance> TransitionStackClass;

	UPROPERTY(Transient)
	FSlateBrush CachedBrush;

	UPROPERTY(Transient)
	FString CachedTexture;

	UPROPERTY(Transient)
	FVector2D CachedTextureSize;

	UPROPERTY(Transient)
	FLinearColor CachedNodeTintColor;

	TSharedPtr<class SImage> IconImage;
	
	FTransitionStackContainer() : Super(nullptr), bNOT(false), Mode(), TransitionStackClass(nullptr),
	CachedTextureSize(ForceInitToZero), CachedNodeTintColor(ForceInitToZero)
	{
	}

	explicit FTransitionStackContainer(TSubclassOf<USMTransitionInstance> InClass, USMNodeInstance* InTemplate = nullptr) :
	Super(InTemplate), bNOT(false), Mode(), TransitionStackClass(InClass),
	CachedTextureSize(ForceInitToZero), CachedNodeTintColor(ForceInitToZero)
	{
	}

	virtual TSubclassOf<USMNodeInstance> GetNodeClass() override { return TransitionStackClass; }
};