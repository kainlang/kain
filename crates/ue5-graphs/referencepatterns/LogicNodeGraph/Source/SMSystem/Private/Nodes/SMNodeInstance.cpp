// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMNodeInstance.h"
#include "ISMEditorGraphNodeInterface.h"
#include "SMCustomVersion.h"
#include "SMInstance.h"
#include "SMLogging.h"
#include "SMUtils.h"

#include "Components/InputComponent.h"
#include "CoreGlobals.h"
#include "Engine/GameInstance.h"
#include "Engine/InputDelegateBinding.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

DEFINE_STAT(STAT_NodeInstances);

void USMCompilerLog::Log(ESMCompilerLogType Severity, const FString& Message)
{
#if WITH_EDITOR
	OnCompilerLogEvent.ExecuteIfBound(Severity, Message);
#endif
}

void USMCompilerLog::LogProperty(FName PropertyName, const USMNodeInstance* NodeInstance, const FString& Message,
                                 ESMCompilerLogType Severity, bool bHighlight, bool bSilent, int32 ArrayIndex)
{
#if WITH_EDITOR
	ensureMsgf(NodeInstance != nullptr, TEXT("NodeInstance is null; this needs to be set manually if calling from C++."));
	OnCompilerLogPropertyEvent.ExecuteIfBound(PropertyName, ArrayIndex, Message, Severity, bHighlight, bSilent, NodeInstance);
#endif
}

USMNodeInstance::USMNodeInstance() : Super(), bEvalDefaultProperties(true), bAutoEvalExposedProperties(true),
                                     bResetVariablesOnInitialize(false), bBlockInput(false), RunInitializedFrame(0), bIsInitialized(false), OwningNode(nullptr)
{
	INC_DWORD_STAT(STAT_NodeInstances)

	NodeIconTintColor = FLinearColor(1.f, 1.f, 1.f, 1.f);

#if WITH_EDITORONLY_DATA
	// TODO: Read editor settings.
	NodeColor = FLinearColor(1.f, 1.f, 1.f, 0.7f);
	NodeDescription.Category = FText::FromString("User");
	bSkipNativeEditorConstructionScripts = false;
	bIsEditorThreadSafe = true;
	bIsEditorExecution = false;
	bHideFromDropDownIfRulesFail = false;
#endif

	bIsThreadSafe = true;

#if WITH_EDITOR
	ResetArrayCheck();
	bIsNodePinChanging = false;
#endif
}

void USMNodeInstance::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	Ar.UsingCustomVersion(FSMNodeInstanceCustomVersion::GUID);
}

UWorld* USMNodeInstance::GetWorld() const
{
	if (UObject* Context = GetContext())
	{
		return Context->GetWorld();
	}

	return nullptr;
}

void USMNodeInstance::BeginDestroy()
{
	Super::BeginDestroy();
	DEC_DWORD_STAT(STAT_NodeInstances)
}

UObject* USMNodeInstance::GetContext() const
{
	if (USMInstance* SMInstance = GetStateMachineInstance())
	{
		return SMInstance->GetContext();
	}

	return nullptr;
}

void USMNodeInstance::NativeInitialize()
{
	RunInitializedFrame = GFrameCounter;
	EnableInput();
	bIsInitialized = true;
}

void USMNodeInstance::NativeShutdown()
{
	RunInitializedFrame = 0;
	DisableInput();
	bIsInitialized = false;
}

void USMNodeInstance::RunConstructionScript()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMNodeInstance::RunConstructionScript"), STAT_SMNodeInstance_RunConstructionScript, STATGROUP_LogicDriver);

	RestoreArchetypeValuesPriorToConstruction();
	ConstructionScript();
}

void USMNodeInstance::RestoreArchetypeValuesPriorToConstruction()
{
#if WITH_EDITORONLY_DATA
	const USMNodeInstance* Archetype = CastChecked<USMNodeInstance>(GetArchetype());
	// Reset exposed property overrides to defaults per construction script run.
	// These could have been modified by construction script functions like SetVariableReadOnly.
	ExposedPropertyOverrides = Archetype->ExposedPropertyOverrides;
	NodeDescription = Archetype->NodeDescription;
#endif
}

USMInstance* USMNodeInstance::GetStateMachineInstance(bool bTopMostInstance) const
{
	if (USMInstance* Instance = Cast<USMInstance>(GetOuter()))
	{
		if (bTopMostInstance)
		{
			return Instance->GetPrimaryReferenceOwner();
		}

		return Instance;
	}

#if WITH_EDITORONLY_DATA
	if (IsEditorExecution())
	{
		FString ClassName = GetClass()->GetName();
		ClassName.RemoveFromEnd(TEXT("_C"));
		LD_LOG_VERBOSE(
			TEXT("GetStateMachineInstance() does not work when called from editor construction scripts for node '%s' of class '%s'.\n"
		"The state machine class is being generated so the instance is not available. This warning may also display when using other functions unavailable during editor construction, such as GetContext()."),
			*GetNodeName(), *ClassName);
	}
#endif

	return nullptr;
}

void USMNodeInstance::SetOwningNode(FSMNode_Base* Node, bool bInIsEditorExecution)
{
	OwningNode = Node;
#if WITH_EDITORONLY_DATA
	bIsEditorExecution = bInIsEditorExecution;
#endif
}

USMStateMachineInstance* USMNodeInstance::GetOwningStateMachineNodeInstance() const
{
	if (const FSMNode_Base* Node = GetOwningNode())
	{
		if (FSMNode_Base* NodeOwner = Node->GetOwnerNode())
		{
			return Cast<USMStateMachineInstance>(NodeOwner->GetOrCreateNodeInstance());
		}
	}

	return nullptr;
}

TScriptInterface<ISMStateMachineNetworkedInterface> USMNodeInstance::GetNetworkInterface() const
{
	if (USMInstance* Instance = GetStateMachineInstance())
	{
		return Instance->GetNetworkInterface();
	}

	return nullptr;
}

float USMNodeInstance::GetTimeInState() const
{
	return OwningNode ? OwningNode->TimeInState : 0.f;
}

bool USMNodeInstance::IsInEndState() const
{
	return OwningNode ? OwningNode->bIsInEndState : false;
}

bool USMNodeInstance::HasUpdated() const
{
	return OwningNode ? OwningNode->bHasUpdated : false;
}

bool USMNodeInstance::IsActive() const
{
	return OwningNode ? OwningNode->IsActive() : false;
}

const FString& USMNodeInstance::GetNodeName() const
{
	static FString EmptyString;
	if (const FSMNode_Base* Node = GetOwningNodeContainer())
	{
		return Node->GetNodeName();
	}
	return EmptyString;
}

const FGuid& USMNodeInstance::GetGuid() const
{
	static FGuid BlankGuid;
	return OwningNode ? OwningNode->GetGuid() : BlankGuid;
}

void USMNodeInstance::EvaluateGraphProperties(bool bTargetOnly)
{
	if (const FSMNode_Base* Node = GetOwningNode())
	{
		const_cast<FSMNode_Base*>(Node)->ExecuteGraphProperties(this, bTargetOnly ? &GetTemplateGuid() : nullptr);
	}
}

const FVector2D& USMNodeInstance::GetNodePosition() const
{
	if (const FSMNode_Base* Node = GetOwningNode())
	{
		return Node->NodePosition;
	}

	static FVector2D EmptyVector(0.f, 0.f);
	return EmptyVector;
}

bool USMNodeInstance::IsInitializedAndReadyForInputEvents() const
{
	if (!IsInitialized())
	{
		return false;
	}

	// Do not allow processing on the same frame input was initialized. This fixes the case where a key transition event
	// switches to another state that uses the same key to transition to another state. The same key press would be recognized
	// twice otherwise and the states would transition in the same tick.
	const bool bReadyForInput = RunInitializedFrame != GFrameCounter;
	return bReadyForInput;
}

void USMNodeInstance::EnableInput()
{
	if (AutoReceiveInput == ESMNodeInput::Type::Disabled || !GetWorld() || !UInputDelegateBinding::SupportsInputDelegate(GetClass()))
	{
		// Node has disabled input.
		return;
	}

	APlayerController* PlayerController = nullptr;
	int32 ChosenPriority = InputPriority;
	bool bChosenBlock = bBlockInput;

	bool bIsUsingContext = false;
	if (AutoReceiveInput == ESMNodeInput::UseContextController)
	{
		PlayerController = USMUtils::FindControllerFromContext<APlayerController>(GetContext());
		bIsUsingContext = true;
	}
	else if (AutoReceiveInput == ESMNodeInput::Type::UseOwningStateMachine)
	{
		if (USMInstance* StateMachineOwner = GetStateMachineInstance())
		{
			PlayerController = StateMachineOwner->GetInputController();
			ChosenPriority = StateMachineOwner->GetInputPriority();
			bChosenBlock = StateMachineOwner->GetBlockInput();

			bIsUsingContext = StateMachineOwner->GetInputType() == ESMStateMachineInput::UseContextController;
		}
	}
	else
	{
		// Node values.
		const int32 PlayerIndex = static_cast<int32>(AutoReceiveInput.GetValue()) - ESMNodeInput::Player0;
		PlayerController = UGameplayStatics::GetPlayerController(this, PlayerIndex);
	}

	if (PlayerController)
	{
		UObject* Context = GetContext();
		USMUtils::EnableInputForObject(PlayerController, this, InputComponent, ChosenPriority, bChosenBlock, !Context || !Context->IsA<APawn>());
	}

	if (bIsUsingContext)
	{
		// Context controller could change throughout the game.
		if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
		{
			GameInstance->GetOnPawnControllerChanged().AddUniqueDynamic(this, &USMNodeInstance::OnContextPawnControllerChanged);
		}
	}
}

void USMNodeInstance::DisableInput()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	USMUtils::DisableInput(World, InputComponent);

	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		GameInstance->GetOnPawnControllerChanged().RemoveDynamic(this, &USMNodeInstance::OnContextPawnControllerChanged);
	}
}

void USMNodeInstance::OnContextPawnControllerChanged(APawn* Pawn, AController* NewController)
{
	USMUtils::HandlePawnControllerChange(Pawn, NewController, this, InputComponent, InputPriority, bBlockInput);
}

UTexture2D* USMNodeInstance::GetNodeIcon_Implementation() const
{
	return NodeIcon;
}

FVector2D USMNodeInstance::GetNodeIconSize_Implementation() const
{
	return NodeIconSize;
}

FLinearColor USMNodeInstance::GetNodeIconTintColor_Implementation() const
{
	return NodeIconTintColor;
}

void USMNodeInstance::SetDisplayName(FName NewDisplayName)
{
#if WITH_EDITORONLY_DATA
	NodeDescription.Name = MoveTemp(NewDisplayName);
#endif
}

void USMNodeInstance::SetNodeDescriptionText(FText NewDescription)
{
#if WITH_EDITORONLY_DATA
	NodeDescription.Description = MoveTemp(NewDescription);
#endif
}

FText USMNodeInstance::GetNodeDescriptionText() const
{
#if WITH_EDITORONLY_DATA
	if (!NodeDescription.Description.IsEmpty())
	{
		return NodeDescription.Description;
	}

	return GetClass()->GetToolTipText(true);
#else
	return FText::GetEmpty();
#endif
}

void USMNodeInstance::SetNodeColor(FLinearColor NewColor)
{
#if WITH_EDITORONLY_DATA
	NodeColor = NewColor;
#endif
}

void USMNodeInstance::SetUseCustomColor(bool bValue)
{
#if WITH_EDITORONLY_DATA
	bUseCustomColors = bValue;
#endif
}

void USMNodeInstance::SetUseCustomIcon(bool bValue)
{
#if WITH_EDITORONLY_DATA
	bDisplayCustomIcon = bValue;
#endif
}

void USMNodeInstance::SetVariableReadOnly(FName VariableName, bool bSetIsReadOnly)
{
#if WITH_EDITORONLY_DATA

	FSMGraphProperty* GraphPropertyOverride = FindOrAddExposedPropertyOverrideByName(VariableName);
	check(GraphPropertyOverride);

	GraphPropertyOverride->bReadOnly = bSetIsReadOnly;

#endif
}

void USMNodeInstance::SetVariableHidden(FName VariableName, bool bSetHidden)
{
#if WITH_EDITORONLY_DATA

	FSMGraphProperty* GraphPropertyOverride = FindOrAddExposedPropertyOverrideByName(VariableName);
	check(GraphPropertyOverride);

	GraphPropertyOverride->bHidden = bSetHidden;

#endif
}

bool USMNodeInstance::IsEditorExecution() const
{
#if WITH_EDITORONLY_DATA
	return bIsEditorExecution;
#else
	return false;
#endif
}

void USMNodeInstance::WithExecutionEnvironment(ESMExecutionEnvironment& ExecutionEnvironment)
{
	ExecutionEnvironment = IsEditorExecution() ? ESMExecutionEnvironment::EditorExecution : ESMExecutionEnvironment::GameExecution;
}

TScriptInterface<ISMEditorGraphNodeInterface> USMNodeInstance::GetOwningEditorGraphNode() const
{
#if WITH_EDITOR
	if (const UObject* Outer = GetOuter())
	{
		if (Outer->Implements<USMEditorGraphNodeInterface>())
		{
			return GetOuter();
		}
	}
#endif
	return nullptr;
}

void USMNodeInstance::K2_TryGetOwningEditorGraphNode(TScriptInterface<ISMEditorGraphNodeInterface>& EditorNode,
	ESMValidEditorNode& IsValidNode) const
{
	EditorNode = GetOwningEditorGraphNode();
	IsValidNode = EditorNode.GetObject() != nullptr ? ESMValidEditorNode::IsValidEditorNode : ESMValidEditorNode::IsNotValidEditorNode;
}

bool USMNodeInstance::IsInitializationThreadSafe() const
{
#if WITH_EDITORONLY_DATA
	return bIsThreadSafe && bIsEditorThreadSafe;
#else
	return bIsThreadSafe;
#endif
}

void USMNodeInstance::ResetVariables()
{
	check(OwningNode);

	if (USMInstance* SMInstance = GetStateMachineInstance())
	{
		if (const UObject* Archetype = USMUtils::FindTemplateFromInstance(SMInstance, OwningNode->GetTemplateName((this))))
		{
			check(Archetype->GetClass() == GetClass());
			
			for (TFieldIterator<FProperty> Prop(GetClass()); Prop; ++Prop)
			{
				if (!Prop->ContainsInstancedObjectProperty() && !Prop->IsA<FDelegateProperty>()
					&& !Prop->IsA<FMulticastDelegateProperty>())
				{
					if (const FStructProperty* StructProperty = CastField<FStructProperty>(*Prop))
					{
						if (StructProperty->Struct->IsChildOf(FSMGraphProperty_Base::StaticStruct())
							|| StructProperty->GetFName() == TEXT("TemplateGuid"))
						{
							// Graph properties don't need to be reset.
							continue;
						}
					}

					Prop->CopyCompleteValue_InContainer(this, Archetype);
				}
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
FString USMNodeInstance::GetNodeDisplayName() const
{
	if (NodeDescription.Name.IsNone())
	{
		FString ClassName = GetClass()->GetMetaData(TEXT("DisplayName"));
		if (ClassName.IsEmpty())
		{
			ClassName = GetClass()->GetName();
		}
		else
		{
			ClassName = FName::NameToDisplayString(ClassName, false);
		}

		ClassName.RemoveFromEnd(TEXT("_C"));
		return ClassName;
	}
	return NodeDescription.Name.ToString();
}
#endif

#if WITH_EDITOR
FSMGraphProperty* USMNodeInstance::FindExposedPropertyOverrideByName(const FName& VariableName) const
{
	return const_cast<FSMGraphProperty*>(ExposedPropertyOverrides.FindByPredicate([VariableName](const FSMGraphProperty& InGraphPropertyOverride)
	{
		return InGraphPropertyOverride.VariableName == VariableName;
	}));
}

FSMGraphProperty* USMNodeInstance::FindOrAddExposedPropertyOverrideByName(const FName& VariableName)
{
	if (FSMGraphProperty* GraphPropertyOverride = FindExposedPropertyOverrideByName(VariableName))
	{
		return GraphPropertyOverride;
	}

	FSMGraphProperty NewGraphProperty;
	NewGraphProperty.VariableName = VariableName;
	return &ExposedPropertyOverrides.Add_GetRef(NewGraphProperty);
}

void USMNodeInstance::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (IsNodePinChanging())
	{
		// Parent method only calls FCoreUObjectDelegates::OnObjectPropertyChanged which should be fine,
		// but we don't need to do any other work. This case is mostly for users to overload and handle.
		return;
	}

	/*
	 * Hack: Helpers for determining if an array property was changed in the editor. Ideally this would be under
	 * the editor module in SMGraphNode_Base's PostEditChangeChainProperty. That method doesn't appear to have
	 * a good way of finding the specific changes that are available here.
	 */
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayRemove ||
		PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd ||
		PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet ||
		PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate) // Clear not needed.
	{
		ArrayPropertyChanged = PropertyChangedEvent.GetPropertyName();
		ArrayChangeType = PropertyChangedEvent.ChangeType;
		ArrayIndexChanged = PropertyChangedEvent.GetArrayIndex(ArrayPropertyChanged.ToString());
	}
	else
	{
		ResetArrayCheck();
	}
}

bool USMNodeInstance::WasArrayPropertyModified(const FName& PropertyName) const
{
	return ArrayPropertyChanged == PropertyName &&
		(ArrayChangeType == EPropertyChangeType::ArrayRemove ||
			ArrayChangeType == EPropertyChangeType::ArrayAdd ||
			ArrayChangeType == EPropertyChangeType::ValueSet ||
			ArrayChangeType == EPropertyChangeType::Duplicate) && ArrayIndexChanged >= 0;
}

bool USMNodeInstance::IsNodePinChanging() const
{
	return bIsNodePinChanging;
}

void USMNodeInstance::ResetArrayCheck()
{
	ArrayPropertyChanged = NAME_None;
	ArrayChangeType = EPropertyChangeType::Unspecified;
	ArrayIndexChanged = INDEX_NONE;
}
#endif