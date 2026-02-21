// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMInstance.h"
#include "SMStateInstance.h"
#include "SMConduitInstance.h"
#include "SMStateMachineInstance.h"
#include "SMTransitionInstance.h"

#include "SMTextGraphProperty.h"

#include "SMAssetTestInstance.generated.h"

UCLASS()
class USMAssetTestInstance : public USMInstance
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, Category=Test)
	int32 OurTestInt = 0;
};

UENUM()
enum class EAssetTestEnum : uint8
{
	None = 0,
	ValueOne,
	ValueTwo
};

UCLASS()
class USMAssetTestPropertyStateInstance : public USMStateInstance
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Test)
	FSMTextGraphProperty TextGraph;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Test)
	TArray<FSMTextGraphProperty> TextGraphArray;

	UPROPERTY(EditAnywhere, Category=Test)
	FString NonExposedString;

	UPROPERTY(EditAnywhere, Category=Test)
	TArray<FString> NonExposedStringArray;

	UPROPERTY(EditAnywhere, Category=Test)
	FText NonExposedText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Test)
	FString ExposedString;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Test)
	FText ExposedText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Test)
	int32 ExposedInt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Test)
	bool ExposedBool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Test)
	EAssetTestEnum ExposedEnum;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Test)
	TArray<FString> ExposedStringArray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Test)
	TArray<FText> ExposedTextArray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Test)
	TSoftObjectPtr<UObject> ExposedSoftObject;
};

UCLASS()
class USMAssetTestPropertyStateTextGraphInstance : public USMStateInstance
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Test)
	FSMTextGraphProperty TextGraph;
};

UCLASS()
class USMAssetTestBasicStateInstance : public USMStateInstance
{
	GENERATED_BODY()
public:

};

UCLASS()
class USMAssetTestConduitInstance : public USMConduitInstance
{
	GENERATED_BODY()

protected:
	virtual bool CanEnterTransition_Implementation() const override
	{
		return true;
	}

};

UCLASS()
class USMAssetTestStateMachineInstance : public USMStateMachineInstance
{
	GENERATED_BODY()

};

UCLASS()
class USMAssetTestTransitionInstance : public USMTransitionInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category=Test)
	FString StringValue;

	UPROPERTY(EditAnywhere, Category=Test)
	TArray<int32> IntArray;
};