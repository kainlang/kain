// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/SMGraphNode_AnyStateNode.h"

#include "Configuration/SMEditorSettings.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "SMStateInstance.h"

#define LOCTEXT_NAMESPACE "SMGraphAnyStateNode"

USMGraphNode_AnyStateNode::USMGraphNode_AnyStateNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), bOverrideColor(false), bAllowInitialReentry(false)
{
	NodeName = LOCTEXT("AnyStateNodeTitle", "Any State");

	const USMEditorSettings* EditorSettings = FSMBlueprintEditorUtils::GetEditorSettings();
	AnyStateColor = EditorSettings->AnyStateDefaultColor;
}

void USMGraphNode_AnyStateNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, TEXT("Transition"), TEXT("Out"));
}

void USMGraphNode_AnyStateNode::PostPlacedNewNode()
{
	// Skip state base so we don't create a graph.
	USMGraphNode_Base::PostPlacedNewNode();
}

void USMGraphNode_AnyStateNode::PostPasteNode()
{
	// Skip state because it relies on a graph being present.
	USMGraphNode_Base::PostPasteNode();
}

void USMGraphNode_AnyStateNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	bPostEditChangeConstructionRequiresFullRefresh = false;
	Super::PostEditChangeProperty(PropertyChangedEvent);
	bPostEditChangeConstructionRequiresFullRefresh = true;
}

FText USMGraphNode_AnyStateNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NodeName;
}

void USMGraphNode_AnyStateNode::OnRenameNode(const FString& NewName)
{
	NodeName = FText::FromString(NewName);
	RefreshConnectedTransitions();
}

void USMGraphNode_AnyStateNode::ResetCachedValues()
{
	Super::ResetCachedValues();
	CachedColor.Reset();
}

void USMGraphNode_AnyStateNode::SetNodeName_Direct(const FString& InNewName)
{
	NodeName = FText::FromString(InNewName);
}

UClass* USMGraphNode_AnyStateNode::GetNodeClass() const
{
	return USMAnyStateInstance::StaticClass();
}

FString USMGraphNode_AnyStateNode::GetStateName() const
{
	return NodeName.ToString();
}

FLinearColor USMGraphNode_AnyStateNode::GetAnyStateColor() const
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("USMGraphNode_AnyStateNode::GetAnyStateColor"), STAT_GetAnyStateColor, STATGROUP_LogicDriverEditor);

	if (CachedColor.IsSet())
	{
		return *CachedColor;
	}

	FLinearColor FinalColor;
	
	if (bOverrideColor)
	{
		FinalColor = AnyStateColor;
	}
	else if (!AnyStateTagQuery.IsEmpty())
	{
		FGameplayTagQueryExpression Expression;
		AnyStateTagQuery.GetQueryExpr(Expression);

		TArray<uint8> TokenStream;
		TArray<FGameplayTag> TagDictionary;

		Expression.EmitTokens(TokenStream, TagDictionary);

		FString StringToHash = "";
		for (const uint8 Token : TokenStream)
		{
			StringToHash += FString::FromInt(Token);
		}
		for (const FGameplayTag& TagKeyVal : TagDictionary)
		{
			StringToHash += TagKeyVal.GetTagName().ToString();
		}

		const uint8 Hash = static_cast<uint8>(TextKeyUtil::HashString(StringToHash) % 359);
		FinalColor = FLinearColor::MakeFromHSV8(Hash, 255, 255);
	}
	else
	{
		const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
		FinalColor = Settings->AnyStateDefaultColor;
	}

	CachedColor = FinalColor;
	
	return FinalColor;
}

FLinearColor USMGraphNode_AnyStateNode::Internal_GetBackgroundColor() const
{
	const FLinearColor DefaultColor = GetAnyStateColor();
	return DefaultColor;
}

#undef LOCTEXT_NAMESPACE
