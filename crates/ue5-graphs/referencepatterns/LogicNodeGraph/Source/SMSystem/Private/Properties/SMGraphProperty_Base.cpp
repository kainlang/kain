// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphProperty_Base.h"
#include "SMUtils.h"
#include "SMLogging.h"
#include "ExposedFunctions/SMExposedFunctionHelpers.h"
#if WITH_EDITORONLY_DATA
#include "Misc/PackageName.h"
#endif


FSMGraphProperty_Base_Runtime::FSMGraphProperty_Base_Runtime(): GraphEvaluator(nullptr), LinkedProperty(nullptr),
                                                                bIsDefaultValueOnly(0),
                                                                OwningNode(nullptr)
{
}

void FSMGraphProperty_Base_Runtime::Initialize(FSMNode_Base* InOwningNode)
{
	check(InOwningNode);
	OwningNode = InOwningNode;

	const FSMNode_FunctionHandlers* FunctionHandlers = OwningNode->GetFunctionHandlers();
	check(FunctionHandlers);
	check(FunctionHandlers->ExposedFunctionsOwner);

	if (TArray<FSMExposedFunctionHandler>* ExposedFunctionHandler = /* LinkedProperty can be true for custom graph properties */
		FunctionHandlers->ExposedFunctionsOwner->FindExposedGraphPropertyFunctionHandler(LinkedProperty ? GetOwnerGuid() : GetGuid()))
	{
		LD::ExposedFunctions::InitializeGraphFunctions(*ExposedFunctionHandler, OwningNode->GetOwningInstance(), nullptr);
		GraphEvaluator = ExposedFunctionHandler;
	}
}

void FSMGraphProperty_Base_Runtime::Execute(void* Params)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMGraphProperty_Base::Execute"), STAT_SMGraphProperty_Base_Execute, STATGROUP_LogicDriver);

	if (GraphEvaluator)
	{
		LD::ExposedFunctions::ExecuteGraphFunctions(*GraphEvaluator, OwningNode->GetOwningInstance(), nullptr, Params);
	}

	// If set then the graph evaluator is actually executing a graph from the linked property.
	// We should copy the result value into this property.
	if (LinkedProperty)
	{
		SetResult(LinkedProperty->GetResult());
	}
}

const FGuid& FSMGraphProperty_Base_Runtime::SetGuid(const FGuid& NewGuid)
{
	Guid = NewGuid;
	return Guid;
}

const FGuid& FSMGraphProperty_Base_Runtime::SetOwnerGuid(const FGuid& NewGuid)
{
	OwnerGuid = NewGuid;
	return OwnerGuid;
}


FSMGraphProperty_Runtime::FSMGraphProperty_Runtime()
{
}


FSMGraphProperty_Base::FSMGraphProperty_Base(): bIsInArray(false), bReadOnly(false), bHidden(false), GuidIndex(-1)
{
#if WITH_EDITORONLY_DATA
	GraphModuleClassName = "SMSystemEditor";
	GraphClassName = "SMPropertyGraph";
	GraphSchemaClassName = "SMPropertyGraphSchema";
	CachedGraphClass = nullptr;
	CachedSchemaClass = nullptr;
	ArrayIndex = 0;
#endif
}

const FGuid& FSMGraphProperty_Base::SetGuid(const FGuid& NewGuid)
{
	const FString GuidString = NewGuid.ToString();
	GuidUnmodified = NewGuid;
	GuidIndex = -1;
	
	const FString GuidWithTemplate = GuidString + TemplateGuid.ToString();
	USMUtils::PathToGuid(GuidWithTemplate, &Guid);
	return Guid;
}

const FGuid& FSMGraphProperty_Base::SetGuid(const FGuid& NewGuid, int32 Index, bool bCountTemplate)
{
	FString GuidWithIndex = NewGuid.ToString() + FString::FromInt(Index);
	GuidUnmodified = NewGuid;
	GuidIndex = Index;
	
	if (bCountTemplate)
	{
		GuidWithIndex += TemplateGuid.ToString();
	}
	
	USMUtils::PathToGuid(GuidWithIndex, &Guid);
	return Guid;
}

const FGuid& FSMGraphProperty_Base::GenerateNewGuid()
{
	SetGuid(FGuid::NewGuid());
	return Guid;
}

const FGuid& FSMGraphProperty_Base::GenerateNewGuidIfNotValid()
{
	if (!Guid.IsValid())
	{
		GenerateNewGuid();
	}

	return Guid;
}

void FSMGraphProperty_Base::InvalidateGuid()
{
	Guid.Invalidate();
}

const FGuid& FSMGraphProperty_Base::SetTemplateGuid(const FGuid& NewGuid, bool bRefreshGuid)
{
	TemplateGuid = NewGuid;

	if (bRefreshGuid)
	{
		if (GuidIndex >= 0)
		{
			SetGuid(GetUnmodifiedGuid(), GuidIndex);
		}
		else
		{
			SetGuid(GetUnmodifiedGuid());
		}
	}
	
	return TemplateGuid;
}

bool FSMGraphProperty_Base::IsVariableReadOnly() const
{
	if (FProperty* Property = MemberReference.ResolveMember<FProperty>())
	{
		if (Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
		{
			return true;
		}
	}

	return bReadOnly;
}

bool FSMGraphProperty_Base::IsVariableHidden() const
{
	return bHidden;
}

#if WITH_EDITORONLY_DATA

UClass* FSMGraphProperty_Base::GetGraphClass(UObject* Outer) const
{
	if (!CachedGraphClass)
	{
		CachedGraphClass = FindObject<UClass>(Outer, *GraphClassName.ToString());
	}

	return CachedGraphClass;
}

UClass* FSMGraphProperty_Base::GetGraphSchemaClass(UObject* Outer) const
{
	if (!CachedSchemaClass)
	{
		CachedSchemaClass = FindObject<UClass>(Outer, *GraphSchemaClassName.ToString());
	}

	return CachedSchemaClass;
}

const FString& FSMGraphProperty_Base::GetGraphModuleName() const
{
	return GraphModuleClassName;
}

UPackage* FSMGraphProperty_Base::GetEditorModule() const
{
	const FString& ModuleName = GetGraphModuleName();
	const FString LongName = FPackageName::ConvertToLongScriptPackageName(*ModuleName);
	return Cast<UPackage>(StaticFindObjectFast(UPackage::StaticClass(), nullptr, *LongName));
}

const FString& FSMGraphProperty_Base::GetPropertyDisplayName() const
{
	static FString DefaultName("");
	return DefaultName;
}

FText FSMGraphProperty_Base::GetDisplayName() const
{
	const FString& DisplayName = GetPropertyDisplayName();
	if (!DisplayName.IsEmpty())
	{
		return FText::FromString(DisplayName);
	}

	return RealDisplayName;
}
#endif

FSMGraphProperty::FSMGraphProperty() : Super()
{
}
