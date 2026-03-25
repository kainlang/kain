/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "DialogNodeInfo.h"
#include "QuestNodeInfo.h"
#include "QuestStartGraphNode.h"
#include "SGraphNode.h"

class SRichTextBlock;

/**
 * 
 */
class QUESTASSETEDITOR_API SQuestGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SQuestGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode);

	// SGraphNode Interface
	virtual void UpdateGraphNode() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	virtual TSharedRef<SWidget> CreateNodeContentArea() override;
	// Fin de l'interface
	
protected:
	TSharedPtr<SRichTextBlock> NodeTextBlock;

private:
	TSharedPtr<SLevelOfDetailBranchNode> TitleLODBranchNode;

	/*
	FString GetComparatorString(TEnumAsByte<EValueCheckOperator> EnumAsByte);
	FString ConstructConditionsText(UDialogNodeInfo* NodeInfo);
	FString GetCounterUpdateActionString(FCounterChangeValue action);
	FString ConstructActionsText(UDialogNodeInfo* NodeInfo);
	FString ConstructFlagsInfoText(UDialogNodeInfo* NodeInfo);
	FString ConstructWarningsText(UDialogNodeInfo* NodeInfo);
	*/
	FText ConstructDetailsText(UQuestNodeInfo* NodeInfo);
};
