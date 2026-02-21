// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "NodeStack/NodeStackContainer.h"

#include "Configuration/SMProjectEditorSettings.h"
#include "Graph/Nodes/SMGraphNode_Base.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Engine/Engine.h"

void FNodeStackContainer::InitTemplate(UObject* Owner, bool bForceInit, bool bForceNewGuid)
{
	const TSubclassOf<USMNodeInstance> NodeClass = GetNodeClass();
	if (NodeClass == nullptr)
	{
		DestroyTemplate();
		return;
	}

	if (!bForceInit && NodeStackInstanceTemplate && NodeStackInstanceTemplate->GetClass() == NodeClass)
	{
		return;
	}

	Owner->Modify();

	if (bForceNewGuid || !TemplateGuid.IsValid())
	{
		TemplateGuid = FGuid::NewGuid();
	}

	FString NodeName = Owner->GetName();
	NodeName = FSMBlueprintEditorUtils::GetSafeName(NodeName);
	
	const FString TemplateName = FString::Printf(TEXT("NODE_STACK_TEMPLATE_%s_%s_%s"),
		*NodeName, *NodeClass->GetName(), *TemplateGuid.ToString());
	
	USMNodeInstance* NewTemplate = NodeClass ? NewObject<USMNodeInstance>(Owner, NodeClass, *TemplateName,
		RF_ArchetypeObject | RF_Transactional | RF_Public) : nullptr;

	if (NodeStackInstanceTemplate)
	{
		if (NewTemplate && NewTemplate->GetClass() == NodeStackInstanceTemplate->GetClass())
		{
			// Only copy when they're the same class. Causes problems when there's a common base class between the new node template and original template.
			// Packaging won't find the template.
			UEngine::CopyPropertiesForUnrelatedObjects(NodeStackInstanceTemplate, NewTemplate);
		}

		if (USMGraphNode_Base* GraphNodeOwner = Cast<USMGraphNode_Base>(Owner))
		{
			// Destroy all old property graphs first. If the user is replacing a template parent class
			// with a child class, shared parent property graphs won't be properly updated for the child.
			GraphNodeOwner->RemoveGraphPropertyGraphsForTemplate(NodeStackInstanceTemplate);
		}
		
		// Original template isn't needed any more.
		DestroyTemplate();
	}

	NodeStackInstanceTemplate = NewTemplate;
	if (NodeStackInstanceTemplate)
	{
		NodeStackInstanceTemplate->SetTemplateGuid(TemplateGuid);

		const ESMEditorConstructionScriptProjectSetting ConstructionProjectSetting =
			FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting;
		if (ConstructionProjectSetting == ESMEditorConstructionScriptProjectSetting::SM_Legacy)
		{
			// On standard these will be run with the entire blueprint after this operation.
			NodeStackInstanceTemplate->RunConstructionScript();
		}
	}
}

void FNodeStackContainer::DestroyTemplate()
{
	if (NodeStackInstanceTemplate)
	{
		NodeStackInstanceTemplate->Modify();
		FSMBlueprintEditorUtils::TrashObject(NodeStackInstanceTemplate);
		NodeStackInstanceTemplate = nullptr;
	}
}

FText FNodeStackContainer::FormatStackInstanceName(const USMNodeInstance* InNodeInstance, int32 InIndex)
{
	FString ClassName = InNodeInstance ? InNodeInstance->GetClass()->GetName() : TEXT("None");
	ClassName.RemoveFromEnd("_C");
	return InIndex >= 0 ?
		FText::Format(NSLOCTEXT("NodeStackContainer", "FormattedNodeInstanceName", "{0} {1}"),
			FText::AsNumber(InIndex), FText::FromString(ClassName) )
			: FText::FromString(ClassName);
}
