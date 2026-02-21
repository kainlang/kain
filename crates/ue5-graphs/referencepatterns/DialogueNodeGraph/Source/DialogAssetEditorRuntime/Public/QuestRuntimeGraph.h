/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */
#pragma once

#include <ThirdParty/ShaderConductor/ShaderConductor/External/DirectXShaderCompiler/include/dxc/DXIL/DxilConstants.h>

#include "CoreMinimal.h"
#include "QuestNodeInfoBase.h"
#include "QuestNodeType.h"
#include "UObject/Object.h"
#include "Layout/SlateRect.h"
#include "QuestRuntimeGraph.generated.h"

/**
 * 
 */

UCLASS()
class DIALOGASSETEDITORRUNTIME_API UQuestRuntimePin : public UObject {
	GENERATED_BODY()

public:
	UPROPERTY()
	FName PinName;

	UPROPERTY()
	FGuid PinId;

	UPROPERTY()
	UQuestRuntimePin* Connection = nullptr;

	UPROPERTY()
	class UQuestRuntimeNode* Parent = nullptr;
};

UCLASS()
class DIALOGASSETEDITORRUNTIME_API UQuestRuntimeNode : public UObject {
	GENERATED_BODY()

public:
	UPROPERTY()
	EQuestNodeType NodeType = EQuestNodeType::Unknown;

	UPROPERTY()
	UQuestRuntimePin* InputPin;

	UPROPERTY()
	TArray<UQuestRuntimePin*> OutputPins;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	UQuestNodeInfoBase* NodeInfo = nullptr;
};

USTRUCT()
struct FQuestCommentBounds
{
	GENERATED_USTRUCT_BODY()

public:
	FQuestCommentBounds() { minX = maxX = minY = maxY = 0; }
	FQuestCommentBounds(float minX, float maxX, float minY, float maxY)
	{
		this->minX = minX;
		this->maxX = maxX;
		this->minY = minY;
		this->maxY = maxY;
	}
	
	UPROPERTY()
	float minX;

	UPROPERTY()
	float maxX;

	UPROPERTY()
	float minY;

	UPROPERTY()
	float maxY;

	
	FSlateRect GetSlateBounds()
	{
		return FSlateRect(minX, minY, maxX, maxY);
	}
};

UCLASS()
class DIALOGASSETEDITORRUNTIME_API UQuestCommentNode : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FQuestCommentBounds Bounds;

	UPROPERTY()
	FString CommentText;

	//FSlateColor CommentColor;
};


UCLASS()
class DIALOGASSETEDITORRUNTIME_API UQuestRuntimeGraph : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<UQuestRuntimeNode*> Nodes;

	// Added that, but it is still unused because
	// it does not work as expected. I still have to
	// research about this topic.
	UPROPERTY()
	TArray<UQuestCommentNode*> Comments;
};
