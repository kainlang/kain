// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMKismetCompiler.h"

#include "EdGraphUtilities.h"
#include "Engine/Engine.h"
#include "Framework/Notifications/NotificationManager.h"
#include "InputAction.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "Misc/App.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "K2Node_CallArrayFunction.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_InputAction.h"
#include "K2Node_InputAxisEvent.h"
#include "K2Node_InputAxisKeyEvent.h"
#include "K2Node_InputKey.h"
#include "K2Node_StructMemberGet.h"
#include "K2Node_StructMemberSet.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"

#include "Compilers/SMCompilerDelegates.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "Construction/ISMEditorConstructionManager.h"
#include "Construction/SMEditorInstance.h"
#include "Construction/SMEditorStateMachine.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_NodeInstance.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_TransitionEvent.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_StateReadNodes.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_StateWriteNodes.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_ConduitResultNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_IntermediateEntryNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_IntermediateRootStateMachineNodes.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateEndNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateEntryNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateMachineSelectNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateUpdateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionEnteredNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionInitializedNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionPostEvaluateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionPreEvaluateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionShutdownNode.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_AnyStateNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_LinkStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineParentNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMConduitGraph.h"
#include "Graph/SMGraph.h"
#include "Graph/SMIntermediateGraph.h"
#include "Graph/SMPropertyGraph.h"
#include "Graph/SMStateGraph.h"
#include "Graph/SMTransitionGraph.h"
#include "ISMSystemEditorModule.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMNodeInstanceUtils.h"
#include "Utilities/SMPropertyUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "ExposedFunctions/SMExposedFunctionHelpers.h"
#include "SMUtils.h"

#define LOCTEXT_NAMESPACE "SMKismetCompiler"

bool FSMKismetCompiler::CanCompile(const UBlueprint* Blueprint)
{
	return Blueprint->IsA<USMBlueprint>();
}

void FSMKismetCompiler::Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results)
{
	FSMKismetCompilerContext Compiler(Blueprint, Results, CompileOptions);
	Compiler.Compile();
}

bool FSMKismetCompiler::GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass,
	UClass*& OutBlueprintGeneratedClass) const
{
	if (ParentClass && ParentClass->IsChildOf<USMInstance>())
	{
		OutBlueprintClass = USMBlueprint::StaticClass();
		OutBlueprintGeneratedClass = USMBlueprintGeneratedClass::StaticClass();
		return true;
	}
	
	return false;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FOnStateMachineCompiledSignature FSMKismetCompilerContext::OnStateMachinePreCompiled;
FOnStateMachineCompiledSignature FSMKismetCompilerContext::OnStateMachinePostCompiled;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

FSMKismetCompilerContext::FSMKismetCompilerContext(UBlueprint* InBlueprint,
	FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions) :
	FKismetCompilerContext(InBlueprint, InMessageLog, InCompilerOptions), NewSMBlueprintClass(nullptr), NumberStates(0),
	NumberTransitions(0), InputConsumingEvent(nullptr)
{
	if (InBlueprint->HasAnyFlags(RF_NeedPostLoad))
	{
		/*
		 * Compile during loading may have duplicate IDs. This was brought over from anim blueprint compiler
		 * in an effort to fix an inheritance issue.
		 * Haven't been able to recreate the particular error this solves but am leaving it just in case
		 */
		FSMBlueprintEditorUtils::FixUpDuplicateGraphNodeGuids(InBlueprint);

		// Transition Guids before 1.6 could be copied and pasted when they should all be unique.
		FSMBlueprintEditorUtils::FixUpDuplicateRuntimeGuids(InBlueprint, &InMessageLog);
	}

	bBlueprintIsDerived = CastChecked<USMBlueprint>(InBlueprint)->FindOldestParentBlueprint() != nullptr;
}

void FSMKismetCompilerContext::MergeUbergraphPagesIn(UEdGraph* Ubergraph)
{
	Super::MergeUbergraphPagesIn(Ubergraph);

	// Make sure we expand any split pins here before we process state machine nodes.
	for (TArray<TObjectPtr<UEdGraphNode>>::TIterator NodeIt(ConsolidatedEventGraph->Nodes); NodeIt; ++NodeIt)
	{
		UK2Node* K2Node = Cast<UK2Node>(*NodeIt);
		if (K2Node != nullptr)
		{
			for (int32 PinIndex = K2Node->Pins.Num() - 1; PinIndex >= 0; --PinIndex)
			{
				UEdGraphPin* Pin = K2Node->Pins[PinIndex];
				if (Pin->SubPins.Num() == 0)
				{
					continue;
				}

				K2Node->ExpandSplitPin(this, ConsolidatedEventGraph, Pin);
			}
		}
	}

	// Locate the top level state machine definition.
	USMGraphK2Node_StateMachineNode* RootStateMachine = GetRootStateMachineNode();
	if (!RootStateMachine)
	{
		return;
	}

	FSMNode_Base* RootStateMachineNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(RootStateMachine->GetStateMachineGraph());
	check(RootStateMachineNode);

	// Record the guid so we can look it up later.
	NewSMBlueprintClass->SetRootGuid(RootStateMachineNode->GetNodeGuid());

	NumberStates = NumberTransitions = 0;
	
	USMGraph* RootStateMachineGraph = RootStateMachine->GetStateMachineGraph();
	ValidateAllNodes(RootStateMachineGraph);
	PreProcessStateMachineNodes(RootStateMachineGraph);
	PreProcessRuntimeReferences(RootStateMachineGraph);
	ExpandParentNodes(RootStateMachineGraph);
	ProcessStateMachineGraph(RootStateMachineGraph);
	ProcessPropertyNodes();
	ProcessInputNodes();
	ProcessEnhancedInputNodes();
	ProcessRuntimeContainers();
	ProcessRuntimeReferences();
}

void FSMKismetCompilerContext::SpawnNewClass(const FString& NewClassName)
{
	NewSMBlueprintClass = FindObject<USMBlueprintGeneratedClass>(Blueprint->GetOutermost(), *NewClassName);

	if (NewSMBlueprintClass == nullptr)
	{
		NewSMBlueprintClass = NewObject<USMBlueprintGeneratedClass>(Blueprint->GetOutermost(), FName(*NewClassName), RF_Public | RF_Transactional);
	}
	else
	{
		// Already existed, but wasn't linked in the Blueprint yet due to load ordering issues
		FBlueprintCompileReinstancer::Create(NewSMBlueprintClass);
	}
	NewClass = NewSMBlueprintClass;
}

void FSMKismetCompilerContext::OnNewClassSet(UBlueprintGeneratedClass* ClassToUse)
{
	NewSMBlueprintClass = CastChecked<USMBlueprintGeneratedClass>(ClassToUse);
}

void FSMKismetCompilerContext::CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& InOldCDO)
{
	Super::CleanAndSanitizeClass(ClassToClean, InOldCDO);

	// Fixes #151. CommandLet can cause a crash during BP modify.
	if (CompileOptions.CompileType != EKismetCompileType::BytecodeOnly)
	{
		RecompileChildren();
	}
	
	// Make sure our typed pointer is set
	check(ClassToClean == NewClass && NewSMBlueprintClass == NewClass);

	NewSMBlueprintClass->GeneratedNames.Empty();
}

void FSMKismetCompilerContext::CopyTermDefaultsToDefaultObject(UObject* DefaultObject)
{
	Super::CopyTermDefaultsToDefaultObject(DefaultObject);
	USMInstance* DefaultInstance = CastChecked<USMInstance>(DefaultObject);

	const USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetProjectEditorSettings();
	const bool bIsFromLinkerLoad = Settings->bLinkerLoadHandling ? OldLinker && OldGenLinkerIdx != INDEX_NONE &&
		Blueprint->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects) : false;
	
	if (!bIsFromLinkerLoad)
	{
		// Treat the CDO as a template at first so we can purge all templates which will be regenerated below.
		// References are likely correct when used from linker load.
		FSMBlueprintEditorUtils::CleanReferenceTemplates(DefaultInstance);
	}
	
	uint32 TotalSize = 0;
	TSet<FName> NamesChecked;

	// Don't modify persistent data in copy term defaults. This might be called more than once.
	TMap<FGuid, FSMExposedNodeFunctions> NodeExposedFunctionsCopy = NodeExposedFunctions;
	
	auto CheckPropertySize = [&](const FProperty* Property) -> uint32
	{
		if (NamesChecked.Contains(Property->GetFName()))
		{
			return 0;
		}

		NamesChecked.Add(Property->GetFName());
		return Property->GetSize();
	};
	
	// Patch up parent values so they can be accessed properly from the child.
	if (bBlueprintIsDerived)
	{
		USMBlueprintGeneratedClass* RootClass = NewSMBlueprintClass;
		while (USMBlueprintGeneratedClass* NextClass = Cast<USMBlueprintGeneratedClass>(RootClass->GetSuperClass()))
		{
			RootClass = NextClass;

			USMInstance* DefaultRootObject = CastChecked<USMInstance>(RootClass->GetDefaultObject());

			// Add parent exposed functions but only if the guids aren't already present. They can be if the parent graph
			// is called directly.
			TMap<FGuid, FSMExposedNodeFunctions>& ParentExposedFunctions = DefaultRootObject->GetNodeExposedFunctions();
			for (const TTuple<FGuid, FSMExposedNodeFunctions>& ExposedFuncKeyVal : ParentExposedFunctions)
			{
				if (!NodeExposedFunctionsCopy.Contains(ExposedFuncKeyVal.Key))
				{
					NodeExposedFunctionsCopy.Add(ExposedFuncKeyVal);
				}
			}

			for (TFieldIterator<FProperty> It(RootClass); It; ++It)
			{
				FProperty* RootProp = *It;

				if (FStructProperty* RootStructProp = CastField<FStructProperty>(RootProp))
				{
					if (RootStructProp->Struct->IsChildOf(FSMNode_Base::StaticStruct()))
					{
						FStructProperty* ChildStructProp = FindFProperty<FStructProperty>(NewSMBlueprintClass, *RootStructProp->GetName());
						check(ChildStructProp);
						uint8* SourcePtr = RootStructProp->ContainerPtrToValuePtr<uint8>(DefaultRootObject);
						uint8* DestPtr = ChildStructProp->ContainerPtrToValuePtr<uint8>(DefaultObject);
						check(SourcePtr && DestPtr);
						RootStructProp->CopyCompleteValue(DestPtr, SourcePtr);

						TotalSize += CheckPropertySize(ChildStructProp);
					}
				}
			}
		}
	}

	// Setup graph properties first so they can later be copied to their correct instance template.
	for (TFieldIterator<FProperty> It(DefaultObject->GetClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty* TargetProperty = *It;
		if (USMGraphK2Node_PropertyNode_Base* PropertyNode = Cast<USMGraphK2Node_PropertyNode_Base>(AllocatedNodePropertiesToNodes.FindRef(TargetProperty)))
		{
			const FStructProperty* SourceProperty = PropertyNode->GetRuntimePropertyNodeProperty();
			check(SourceProperty);

			uint8* DestinationPtr = TargetProperty->ContainerPtrToValuePtr<uint8>(DefaultObject);
			uint8* SourcePtr = SourceProperty->ContainerPtrToValuePtr<uint8>(PropertyNode);
			TargetProperty->CopyCompleteValue(DestinationPtr, SourcePtr);
			TotalSize += CheckPropertySize(TargetProperty);
		}
	}

	TSet<UObject*> TemplatesUsed;
	TMap<FGuid, UClass*> NodeGuidToNodeClassesUsed;
	TMap<FGuid, UClass*> PropertyGuidToPropertyTemplatesUsed;
	for (TFieldIterator<FProperty> It(DefaultObject->GetClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty* TargetProperty = *It;

		if (USMGraphK2Node_RuntimeNodeContainer* RuntimeContainerNode = Cast<USMGraphK2Node_RuntimeNodeContainer>(AllocatedNodePropertiesToNodes.FindRef(TargetProperty)))
		{
			const FStructProperty* SourceProperty = RuntimeContainerNode->GetRuntimeNodeProperty();
			check(SourceProperty);

			uint8* DestinationPtr = TargetProperty->ContainerPtrToValuePtr<uint8>(DefaultObject);
			uint8* SourcePtr = SourceProperty->ContainerPtrToValuePtr<uint8>(RuntimeContainerNode);
			TargetProperty->CopyCompleteValue(DestinationPtr, SourcePtr);
			TotalSize += CheckPropertySize(TargetProperty);
			
			FSMNode_Base* RuntimeNode = reinterpret_cast<FSMNode_Base*>(DestinationPtr);

			NodeGuidToNodeClassesUsed.Add(RuntimeNode->GetNodeGuid(),
				FSMBlueprintEditorUtils::GetMostUpToDateClass(RuntimeNode->GetNodeInstanceClass()));

			// Template Storage
			// Templates are manually placed directly on the CDO with the CDO as the property owner.
			// It is important that the final storage property be marked as Instanced. These conditions are necessary
			// for templates to work properly in all scenarios especially cooked builds with BP Nativization.

			// Set the template to use for the reference. This doesn't have to be completely unique per use.
			if (TArray<FTemplateContainer>* Templates = DefaultObjectTemplates.Find(RuntimeNode->GetNodeGuid()))
			{
				for (const FTemplateContainer& Template : *Templates)
				{
					UObject* TemplateInstance = Template.Template;
					if (!TemplateInstance)
					{
						continue;
					}

					// Can't deep copy properties from the reference template CDO if it's still being compiled.
					ensure(!TemplateInstance->GetClass()->bLayoutChanging);

					// Template name starts with class level in case of duplicate runtime nodes in the parent.
					FString NodeName = RuntimeNode->GetNodeName();
					NodeName = FSMBlueprintEditorUtils::GetSafeName(NodeName);
					FString TemplateName = FString::Printf(TEXT("TEMPLATE_%s_%s_%s"), *DefaultObject->GetClass()->GetName(), *NodeName, *RuntimeNode->GetNodeGuid().ToString());

					if (Template.TemplateType == FTemplateContainer::StackTemplate)
					{
						ensure(Template.TemplateGuid.IsValid());
						TemplateName += TEXT("_") + Template.TemplateGuid.ToString();
					}
					else if (Template.TemplateType == FTemplateContainer::ReferenceTemplate)
					{
						TemplateName += TEXT("_Reference");
					}

					UObject* TemplateArchetype = nullptr;

					if (UObject* ExistingObject = FindObject<UObject>(DefaultObject, *TemplateName))
					{
						if (bIsFromLinkerLoad && ExistingObject->GetClass() == TemplateInstance->GetClass())
						{
							// Object already processed, just update from our current template but use the original instance.
							UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
							Params.bDoDelta = false;
							UEngine::CopyPropertiesForUnrelatedObjects(TemplateInstance, ExistingObject, MoveTemp(Params));
							TemplateArchetype = ExistingObject;
						}
						else
						{
							FSMBlueprintEditorUtils::TrashObject(ExistingObject);
						}
					}

					// At this point the templates are still parented to their graph node which is necessary since they could have been copied while their
					// owner class has its layout generating (specifically Play in Stand Alone Game mode). Reinstantiate directly on the default object.
					if (TemplateArchetype == nullptr)
					{
						TemplateArchetype = NewObject<UObject>(DefaultObject, TemplateInstance->GetClass(), *TemplateName, RF_ArchetypeObject | RF_Public, TemplateInstance);
					}

					// Search for any instanced sub-objects that might be stored on the template. Transient flags are added during cook
					// which prevent the sub-objects from saving properly so we need to clear them, but only if the user hasn't actually
					// marked the owning UProperty transient.
					LD::PropertyUtils::ForEachInstancedSubObject(TemplateArchetype, [](UObject* SubObject)
					{
						SubObject->ClearFlags(RF_Transient);
					});

					// Check if this is a reference to another state machine blueprint.
					if (USMInstance* ReferenceTemplate = Cast<USMInstance>(TemplateArchetype))
					{
						ensure(Template.TemplateType == FTemplateContainer::ReferenceTemplate);
						ensure(SourceProperty->Struct->IsChildOf(FSMStateMachine::StaticStruct()));

						// These templates can contain other references which need to be cleaned.
						FSMBlueprintEditorUtils::CleanReferenceTemplates(Cast<USMInstance>(ReferenceTemplate));
						static_cast<FSMStateMachine*>(RuntimeNode)->SetReferencedTemplateName(TemplateArchetype->GetFName());
					}
					else
					{
						if (TMap<FGuid, USMGraphK2Node_Base*>* GraphPropertiesForTemplate = MappedTemplatesToNodeProperties.Find(TemplateInstance))
						{
							for (TFieldIterator<FProperty> PropertyIt(TemplateArchetype->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
							{
								// Regular class instance template which contains graph properties.
								TArray<FSMGraphProperty_Base*> GraphProperties;
								USMUtils::BlueprintPropertyToNativeProperty(*PropertyIt, TemplateArchetype, GraphProperties);
								for (FSMGraphProperty_Base* RuntimePropertyNode : GraphProperties)
								{
									if (TMap<FGuid, FGuid>* GuidMap = GraphPropertyRemap.Find(TemplateInstance))
									{
										FGuid* RemappedGuid = GuidMap->Find(RuntimePropertyNode->GetGuid());
										FGuid GuidToUse = RemappedGuid ? *RemappedGuid : RuntimePropertyNode->GetGuid();
										USMGraphK2Node_PropertyNode_Base* GraphPropertyNode = Cast<USMGraphK2Node_PropertyNode_Base>(GraphPropertiesForTemplate->FindRef(
											GuidToUse));

										PropertyGuidToPropertyTemplatesUsed.Add(GuidToUse,
											FSMBlueprintEditorUtils::GetMostUpToDateClass(TemplateArchetype->GetClass()));
										if (GraphPropertyNode)
										{
											FSMGraphProperty_Base* IntermediateRuntimeProperty = GraphPropertyNode->GetPropertyNodeChecked();
											RuntimePropertyNode->SetOwnerGuid(GuidToUse);
											RuntimePropertyNode->GraphEvaluator = IntermediateRuntimeProperty->GraphEvaluator;
										}
									}
								}
							}
							// Automatically created variable properties.
							for (const TTuple<FGuid, USMGraphK2Node_Base*>& KeyVal : *GraphPropertiesForTemplate)
							{
								if (USMGraphK2Node_PropertyNode_Base* GraphPropertyNode = Cast<USMGraphK2Node_PropertyNode_Base>(KeyVal.Value))
								{
									FSMGraphProperty_Base* PropertyNode = GraphPropertyNode->GetPropertyNodeChecked();
									if (PropertyNode->ShouldAutoAssignVariable())
									{
										FSMGraphProperty_Base* RuntimeGraphPropertyNode = GraphPropertyNode->
											GetPropertyNodeChecked();

										PropertyGuidToPropertyTemplatesUsed.Add(RuntimeGraphPropertyNode->GetGuid(),
											FSMBlueprintEditorUtils::GetMostUpToDateClass(TemplateArchetype->GetClass()));
										RuntimeNode->AddVariableGraphProperty(*RuntimeGraphPropertyNode, PropertyNode->GetTemplateGuid());
									}
								}
							}
						}

						if (Template.TemplateType == FTemplateContainer::NodeTemplate)
						{
							RuntimeNode->SetTemplateName(TemplateArchetype->GetFName());
						}
						else if (Template.TemplateType == FTemplateContainer::StackTemplate)
						{
							UClass* UpToDateTemplateClass = FSMBlueprintEditorUtils::GetMostUpToDateClass(TemplateArchetype->GetClass());
							RuntimeNode->AddStackTemplateName(TemplateArchetype->GetFName(), UpToDateTemplateClass);
						}
					}

					TemplatesUsed.Add(TemplateArchetype);
				}
			}
		}
	}

	// Cache exposed functions' UFunction property to save FindFunctionByName calls during run-time Initialization.
	for (TTuple<FGuid, FSMExposedNodeFunctions>& NodeFunctionsKeyVal : NodeExposedFunctionsCopy)
	{
		// Instead of FindChecked we only do Find because there are issues during a reinstancing where the class
		// may not be present. This never seems to carry over to the final class as the function is always cached.

		if (UClass** NodeClass = NodeGuidToNodeClassesUsed.Find(NodeFunctionsKeyVal.Key))
		{
			TArray<FSMExposedFunctionHandler*> AllHandlers = NodeFunctionsKeyVal.Value.GetFlattedArrayOfAllNodeFunctionHandlers();
			LD::ExposedFunctions::InitializeGraphFunctions(AllHandlers, DefaultInstance->GetClass(), *NodeClass);
		}
		for (TTuple<FGuid, FSMExposedFunctionContainer>& PropertyFunctionContainerKeyVal : NodeFunctionsKeyVal.Value.GraphPropertyFunctionHandlers)
		{
			if (UClass** NodeClassForProperty = PropertyGuidToPropertyTemplatesUsed.Find(PropertyFunctionContainerKeyVal.Key))
			{
				LD::ExposedFunctions::InitializeGraphFunctions(PropertyFunctionContainerKeyVal.Value.ExposedFunctionHandlers,
					DefaultInstance->GetClass(), *NodeClassForProperty);
			}
		}
	}

	// Load all exposed functions to the CDO. These are for all exposed functions directly in this instance
	// and will be mapped out to node instances and graph properties during Initialize.
	DefaultInstance->GetNodeExposedFunctions() = MoveTemp(NodeExposedFunctionsCopy);

	if (bIsFromLinkerLoad)
	{
		// Do not physically remove or call constructor on reference template items.
		// If an object isn't supposed to be here it is likely null (such as from a force delete).

		for (int32 Idx = 0; Idx < DefaultInstance->ReferenceTemplates.Num(); ++Idx)
		{
			UObject* Object = DefaultInstance->ReferenceTemplates[Idx];
			if (Object == nullptr || !TemplatesUsed.Contains(Object))
			{
				FSMBlueprintEditorUtils::TrashObject(Object);
				DefaultInstance->ReferenceTemplates[Idx] = nullptr;
			}
		}
	
		for (UObject* Template : TemplatesUsed)
		{
			DefaultInstance->ReferenceTemplates.AddUnique(Template);
		}
	}
	else
	{
		DefaultInstance->ReferenceTemplates = TemplatesUsed.Array();
	}
	
	DefaultInstance->RootStateMachineGuid = NewSMBlueprintClass->GetRootGuid();

	// Calculate path guids to save initialization time at run-time.
	CalculatePathGuids(DefaultInstance);

	if (Settings->bDisplayMemoryLimitsOnCompile)
	{
		const uint32 MaxSize = 0x7FFFF;
		uint32 Threshold = static_cast<uint32>(static_cast<float>(MaxSize) * Settings->StructMemoryLimitWarningThreshold);
		if (TotalSize >= Threshold)
		{
			FString SizeMessage = FString::Printf(TEXT("Total size of struct properties: %i / %i. You are approaching the maximum size allowed in Unreal Engine and will crash when this limit is reached.\
\nConsider refactoring the state machine to use references to improve performance and reduce memory usage."), TotalSize, MaxSize);
			MessageLog.Warning(*SizeMessage);
		}
		else if (Settings->bAlwaysDisplayStructMemoryUsage)
		{
			FString SizeMessage = FString::Printf(TEXT("Total size of struct properties: %i"), TotalSize);
			MessageLog.Note(*SizeMessage);
		}
	}
}

void FSMKismetCompilerContext::PreCompile()
{
	FSMBlueprintEditorUtils::FixUpDuplicateRuntimeGuids(Blueprint, &MessageLog);
	FSMBlueprintEditorUtils::FixUpMismatchedRuntimeGuids(Blueprint, &MessageLog);
	FSMBlueprintEditorUtils::FCacheInvalidationArgs Args;
	Args.bAllowDuringCompile = true;
	FSMBlueprintEditorUtils::InvalidateCaches(Blueprint, MoveTemp(Args));

	USMBlueprint* SMBlueprint = GetSMBlueprint();
	check(SMBlueprint);

	const USMGraph* Graph = FSMBlueprintEditorUtils::GetRootStateMachineGraph(SMBlueprint);
	if (Graph)
	{
		TArray<USMGraphK2Node_PropertyNode_Base*> PropertyNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_PropertyNode_Base>(Graph, PropertyNodes);
		for (USMGraphK2Node_PropertyNode_Base* Node : PropertyNodes)
		{
			// Only property nodes currently require this so highlights added during construction scripts can be optionally cleared.
			Node->PreCompileBeforeConstructionScripts(*this);
		}
	}

	ISMEditorConstructionManager::Get().RunAllConstructionScriptsForBlueprintImmediately(SMBlueprint, false);

	Super::PreCompile();
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OnStateMachinePreCompiled.Broadcast(*this);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	FSMCompilerDelegates::OnStateMachinePreCompiled.Broadcast(*this);
	
	if (Graph)
	{
		USMCompilerLog* CompilerLog = NewObject<USMCompilerLog>(GetTransientPackage());

		if (SMBlueprint->bEnableNodeValidation)
		{
			// Run OnPreCompileValidate for the root state machine. This won't have an SMGraphNode associated with it
			// and won't run by default. Note that we can't run property validation since there are no graph properties
			// associated with the node instance in this case.
			
			FSMEditorStateMachine RootStateMachine;
			if (ISMEditorConstructionManager::Get().TryGetEditorStateMachine(SMBlueprint, RootStateMachine)
				&& RootStateMachine.StateMachineEditorInstance != nullptr)
			{
				if (const USMNodeInstance* NodeInstance = RootStateMachine.StateMachineEditorInstance->GetRootStateMachineNodeInstance())
				{
					if (NodeInstance->GetClass() != USMStateMachineInstance::StaticClass())
					{
						CompilerLog->OnCompilerLogEvent.BindLambda([&](ESMCompilerLogType Severity, const FString& Message)
						{
							if (FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting == ESMEditorConstructionScriptProjectSetting::SM_Legacy)
							{
								LDEDITOR_LOG_WARNING(TEXT("OnPreCompileValidate called with EditorNodeConstructionScriptSetting set to Legacy. This will limit functionality. Set to 'Compile' or 'Standard'."))
							}
		
							LogCompilerMessage(MessageLog, Message, Severity, nullptr, nullptr);
						});

						USMGraphNode_Base::RunPreCompileValidateForNodeInstance(NodeInstance, CompilerLog);
					}
				}
			}
		}
		
		TArray<USMGraphNode_Base*> Nodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_Base>(Graph, Nodes);
		for (USMGraphNode_Base* Node : Nodes)
		{
			Node->PreCompile(*this);

			if (SMBlueprint->bEnableNodeValidation)
			{
				Node->PreCompileNodeInstanceValidation(MessageLog, CompilerLog);

				// Check for validation within any references.
				if (SMBlueprint->bEnableReferenceNodeValidation)
				{
					if (USMGraphNode_StateMachineStateNode* StateMachineStateNode = Cast<USMGraphNode_StateMachineStateNode>(Node))
					{
						if (StateMachineStateNode->IsStateMachineReference())
						{
							PreCompileValidateReferenceNodes(StateMachineStateNode, CompilerLog);
						}
					}
				}
			}
		}
	}

	ISMEditorConstructionManager::Get().CleanupEditorStateMachine(SMBlueprint);
}

void FSMKismetCompilerContext::PostCompile()
{
	// Display node counts.
	{
		const FString StateCountMessage = FString::Printf(TEXT("Number of states: %i"), NumberStates);
		MessageLog.Note(*StateCountMessage);

		const FString TransitionCountMessage = FString::Printf(TEXT("Number of transitions: %i"), NumberTransitions);
		MessageLog.Note(*TransitionCountMessage);
	}

	const USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetProjectEditorSettings();
	if (InputConsumingEvent && Settings->bDisplayInputEventNotes)
	{
		MessageLog.Note(TEXT("Input event(s) @@ will always consume input. Consider setting `Consume Input` to false."), InputConsumingEvent);
	}
	
	if (const USMGraph* Graph = FSMBlueprintEditorUtils::GetRootStateMachineGraph(Blueprint))
	{
		TArray<USMGraphK2Node_Base*> K2Nodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_Base>(Graph, K2Nodes);
		for (USMGraphK2Node_Base* Node : K2Nodes)
		{
			Node->PostCompileValidate(MessageLog);
		}
	}

	Super::PostCompile();
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OnStateMachinePostCompiled.Broadcast(*this);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	FSMCompilerDelegates::OnStateMachinePostCompiled.Broadcast(*this);
}

USMGraphK2Node_StateMachineNode* FSMKismetCompilerContext::GetRootStateMachineNode() const
{
	TArray<USMGraphK2Node_StateMachineSelectNode*> StateMachineSelectNodeList;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_StateMachineSelectNode>(ConsolidatedEventGraph, StateMachineSelectNodeList);

	// Should only happen on initial construction.
	if (!StateMachineSelectNodeList.Num())
	{
		return nullptr;
	}

	USMGraphK2Node_StateMachineSelectNode* SelectNode = StateMachineSelectNodeList[0];
	UEdGraphPin* InputPin = SelectNode->GetInputPin();

	if (InputPin->LinkedTo.Num())
	{
		if (USMGraphK2Node_StateMachineNode* StateMachineNode = Cast<USMGraphK2Node_StateMachineNode>(InputPin->LinkedTo[0]->GetOwningNode()))
		{
			return StateMachineNode;
		}
	}

	if (bBlueprintIsDerived)
	{
		MessageLog.Note(TEXT("State Machine Select Node @@ is not connected to any state machine. Parent State Machine will be used instead."), SelectNode);
	}
	else
	{
		MessageLog.Warning(TEXT("State Machine Select Node @@ is not connected to any state machine."), SelectNode);
	}

	return nullptr;
}

void FSMKismetCompilerContext::ValidateAllNodes(USMGraph* StateMachineGraph)
{
	TArray<USMGraphNode_Base*> Nodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_Base>(StateMachineGraph, Nodes);
	for (USMGraphNode_Base* Node : Nodes)
	{
		Node->ValidateNodeDuringCompilation(MessageLog);
	}
	
	TArray<USMGraphK2Node_Base*> K2Nodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_Base>(StateMachineGraph, K2Nodes);
	for (USMGraphK2Node_Base* Node : K2Nodes)
	{
		Node->PreConsolidatedEventGraphValidate(MessageLog);
		Node->ValidateNodeDuringCompilation(MessageLog);
	}
}

void FSMKismetCompilerContext::PreCompileValidateReferenceNodes(USMGraphNode_StateMachineStateNode* InStateMachineStateNode,
	USMCompilerLog* InCompilerLog)
{
	check(InStateMachineStateNode);
	if (const USMBlueprint* ReferencedBlueprint = InStateMachineStateNode->GetStateMachineReference())
	{
		if (ReferencedBlueprintsValidating.Contains(ReferencedBlueprint))
		{
			MessageLog.Warning(TEXT("Reference Node Validation - State Machine Reference @@ is duplicated in node @@. 'Enable Reference Node Validation' does not fully support duplicate references."), ReferencedBlueprint, InStateMachineStateNode);
		}
		else
		{
			ReferencedBlueprintsValidating.Add(ReferencedBlueprint);
		}

		TArray<USMGraphNode_Base*> ReferencedNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested(ReferencedBlueprint, ReferencedNodes);

		for (USMGraphNode_Base* ReferencedNode : ReferencedNodes)
		{
			ReferencedNode->PreCompileNodeInstanceValidation(MessageLog, InCompilerLog, InStateMachineStateNode);

			// Recursively check nested references.
			if (USMGraphNode_StateMachineStateNode* StateMachineStateNode = Cast<USMGraphNode_StateMachineStateNode>(ReferencedNode))
			{
				if (StateMachineStateNode->IsStateMachineReference())
				{
					PreCompileValidateReferenceNodes(StateMachineStateNode, InCompilerLog);
				}
			}
		}
	}
}

void FSMKismetCompilerContext::CalculatePathGuids(USMInstance* DefaultInstance)
{
	if (DefaultInstance == nullptr)
	{
		return;
	}

	DefaultInstance->SetRootPathGuidCache({});

	if (DefaultInstance->GetClass()->HasAnyClassFlags(CLASS_Abstract))
	{
		return;
	}

	const USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetProjectEditorSettings();
	if (!Settings->bCalculateGuidsOnCompile)
	{
		TMap<FGuid, FSMGuidMap> EmptyNodeGuidToPathGuid;
		DefaultInstance->SetRootPathGuidCache(EmptyNodeGuidToPathGuid);
		LDEDITOR_LOG_VERBOSE(TEXT("Skipping guid calculation during compile because project editor setting `bCalculateGuidsOnCompile` is disabled."))
		return;
	}

	struct Local
	{
		static void BuildPathGuidMap(const FSMStateMachine& InStateMachine, const FGuid& InPrimaryGuid, TMap<FGuid, FSMGuidMap>& InOutGuidMap)
		{
			ensure(InPrimaryGuid.IsValid() || !InOutGuidMap.Contains(InStateMachine.GetGuid()));

			auto AddNode = [&](const FSMNode_Base* Node)
			{
				// Each USMInstance owned state machine should have its own primary guid on the top level map. Each sub state machine
				// that isn't a reference should belong to the local guid map under the primary guid.

				FSMGuidMap& LocalGuidMap = InOutGuidMap.FindOrAdd(InPrimaryGuid.IsValid() ? InPrimaryGuid : InStateMachine.GetGuid());
				const FGuid& NodeGuid = Node->GetNodeGuid();
				const FGuid& PathGuid = Node->GetGuid();
				ensure(NodeGuid != PathGuid);
				ensure(!LocalGuidMap.NodeToPathGuids.Contains(NodeGuid));
				LocalGuidMap.NodeToPathGuids.Add(NodeGuid, PathGuid);
			};

			for (const FSMState_Base* State : InStateMachine.GetStates())
			{
				AddNode(State);

				if (State->IsStateMachine())
				{
					FSMStateMachine& NestedStateMachine = *((FSMStateMachine*)State);
					if (NestedStateMachine.GetInstanceReference())
					{
						FSMStateMachine& ReferenceRootSM = NestedStateMachine.GetInstanceReference()->GetRootStateMachine();
						BuildPathGuidMap(ReferenceRootSM, ReferenceRootSM.GetGuid(), InOutGuidMap);
					}
					else
					{
						BuildPathGuidMap(NestedStateMachine, InPrimaryGuid, InOutGuidMap);
					}
				}
			}

			for (const FSMTransition* Transition : InStateMachine.GetTransitions())
			{
				AddNode(Transition);
			}
		}
	};

	FGuid RootGuid;
	TSet<FStructProperty*> Properties;
	if (USMUtils::TryGetStateMachinePropertiesForClass(DefaultInstance->GetClass(), Properties, RootGuid))
	{
		FSMStateMachine TestStateMachine;
		TestStateMachine.SetNodeGuid(DefaultInstance->RootStateMachineGuid);
		if (USMUtils::GenerateStateMachine(DefaultInstance, TestStateMachine, Properties, true))
		{
			TMap<FString, int32> Paths;
			TestStateMachine.CalculatePathGuid(Paths, false);

			TMap<FGuid, FSMGuidMap> NodeGuidToPathGuid;
			Local::BuildPathGuidMap(TestStateMachine, TestStateMachine.GetGuid(), NodeGuidToPathGuid);

			DefaultInstance->SetRootPathGuidCache(MoveTemp(NodeGuidToPathGuid));
			TestStateMachine.ResetGeneratedValues();
		}
		else
		{
			MessageLog.Error(TEXT("Error caching guids for state machine @@."), Blueprint);

			const ISMSystemEditorModule& SMBlueprintEditorModule = FModuleManager::GetModuleChecked<ISMSystemEditorModule>(LOGICDRIVER_EDITOR_MODULE_NAME);
			if (SMBlueprintEditorModule.IsPlayingInEditor())
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("Blueprint"), FText::FromString(GetNameSafe(Blueprint)));

				FNotificationInfo Info(FText::Format(LOCTEXT("SMCompileValidationError", "Compile validation failed for State Machine: {Blueprint}. Please restart the editor play session."), Args));

				Info.bUseLargeFont = false;
				Info.ExpireDuration = 5.0f;

				const TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
				if (Notification.IsValid())
				{
					Notification->SetCompletionState(SNotificationItem::CS_Fail);
				}
			}
		}
	}
}

void FSMKismetCompilerContext::PreProcessStateMachineNodes(UEdGraph* Graph)
{
	TArray<USMGraphNode_StateMachineStateNode*> StateMachines;

	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_StateMachineStateNode>(Graph, StateMachines);
	for (USMGraphNode_StateMachineStateNode* StateMachine : StateMachines)
	{
		ProcessNestedStateMachineNode(StateMachine);
	}
}

void FSMKismetCompilerContext::PreProcessRuntimeReferences(UEdGraph* Graph)
{
	TArray<USMGraphK2Node_RuntimeNodeContainer*> Containers;
	TArray<USMGraphK2Node_RuntimeNodeReference*> References;

	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_RuntimeNodeContainer>(Graph, Containers);
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_RuntimeNodeReference>(Graph, References);
	for (USMGraphK2Node_RuntimeNodeContainer* Container : Containers)
	{
		Container->ContainerOwnerGuid = GenerateGuid(Container);

		if (USMGraphK2Node_RuntimeNodeContainer* SourceContainer = Cast<USMGraphK2Node_RuntimeNodeContainer>(MessageLog.FindSourceObject(Container)))
		{
			SourceContainerToDuplicatedContainer.Add(SourceContainer, Container);
		}
		else
		{
			// TODO: This should be an error, but this was added in late for input support only (currently) and is only a warning as a precaution.
			MessageLog.Warning(TEXT("Could not map runtime container @@ to source container."), Container);
		}
	}

	for (USMGraphK2Node_RuntimeNodeReference* Reference : References)
	{
		if (const USMGraphK2Node_RuntimeNodeContainer* Container = Reference->GetRuntimeContainer())
		{
			Reference->ContainerOwnerGuid = Container->ContainerOwnerGuid;
		}
	}
}

void FSMKismetCompilerContext::ExpandParentNodes(UEdGraph* Graph)
{
	TArray<USMGraphNode_StateMachineParentNode*> Parents;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_StateMachineParentNode>(Graph, Parents);

	// Fully expand all parents.
	for (USMGraphNode_StateMachineParentNode* GraphNode : Parents)
	{
		ProcessParentNode(GraphNode);
	}

	Parents.Empty();
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_StateMachineParentNode>(Graph, Parents);
	TMap<FGuid, TArray<UEdGraphNode*>> DupedRuntimeNodes;
	TSet<USMGraph*> ExpandedGraphs;

	// Collect all expanded parent graphs.
	for (USMGraphNode_StateMachineParentNode* ExpandedParent : Parents)
	{
		ExpandedGraphs.Append(ExpandedParent->GetAllNestedExpandedParents());
	}

	// Look for duplicates considering all nested parent graphs.
	for (USMGraph* ExpandedGraph : ExpandedGraphs)
	{
		FSMBlueprintEditorUtils::FindNodesWithDuplicateRuntimeGuids(ExpandedGraph, DupedRuntimeNodes);
	}

	/*
	 * Adjust the NodeGuid only for duplicate nodes. Even with PathGuids this is unavoidable in cases of a grand child calling a child multiple times which calls a parent.
	 * What we do is calculate a new NodeGuid based on the original NodeGuid combined with the times duplicated. This allows the NodeGuid to be unique, but calculated so hopefully
	 * on the next compile it won't change if there were no modifications done.
	 *
	 * These changes aren't done to the runtime nodes contained in the editor graph, only to a cloned graph of the parents.
	 */
	for (TTuple<FGuid, TArray<UEdGraphNode*>>& KeyVal : DupedRuntimeNodes)
	{
		for (int32 Idx = 1; Idx < KeyVal.Value.Num(); ++Idx)
		{
			FSMNode_Base* Node = FSMBlueprintEditorUtils::GetRuntimeNodeFromExactNodeChecked(KeyVal.Value[Idx]);
			Node->DuplicateId = Idx;

			FString AdjustedGuid(*(Node->GetNodeGuid().ToString() + TEXT("_") + FString::FromInt(Node->DuplicateId)));
			FGuid NewGuid;
			FGuid::Parse(FMD5::HashAnsiString(*AdjustedGuid), NewGuid);
			Node->SetNodeGuid(NewGuid);
		}
	}
}

void FSMKismetCompilerContext::ProcessStateMachineGraph(USMGraph* StateMachineGraph)
{
	// This state machine's Guid. Default to root Guid.
	FGuid ThisStateMachinesGuid = NewSMBlueprintClass->GetRootGuid();
	// Back out early if the state machine has no entry point.
	USMGraphNode_StateMachineEntryNode* StateMachineEntryNode = StateMachineGraph->GetEntryNode();
	if (!StateMachineEntryNode)
	{
		MessageLog.Warning(TEXT("State Machine @@ Entry Node not found."), StateMachineGraph);
		return;
	}
	{
		// If this is a nested node we need to create a runtime container for a state machine.
		if (USMGraphNode_StateMachineStateNode* OwningNode = StateMachineGraph->GetOwningStateMachineNodeWhenNested())
		{
			if (USMGraphK2Node_StateMachineEntryNode* NewEntryNode = Cast<USMGraphK2Node_StateMachineEntryNode>(ProcessNestedStateMachineNode(OwningNode)))
			{
				// All nodes being processed below are assigned to this state machine.
				ThisStateMachinesGuid = NewEntryNode->StateMachineNode.GetNodeGuid();
			}
		}

		// Look for an initial state node.
		TArray<USMGraphNode_StateNodeBase*> InitialStateNodes;
		StateMachineEntryNode->GetAllOutputNodesAs(InitialStateNodes);

		if (InitialStateNodes.Num() == 0)
		{
			return;
		}
		
		for (USMGraphNode_StateNodeBase* InitialStateNode : InitialStateNodes)
		{
			// Record the root node so the state machine can be easily constructed later.
			FSMNode_Base* RunTimeNode = nullptr;
			if (USMGraphNode_LinkStateNode* LinkState = Cast<USMGraphNode_LinkStateNode>(InitialStateNode))
			{
				if (LinkState->GetLinkedState())
				{
					RunTimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(LinkState->GetLinkedState()->GetBoundGraph());
				}
			}
			else
			{
				RunTimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(InitialStateNode->GetBoundGraph());
			}

			if (!RunTimeNode)
			{
				MessageLog.Error(TEXT("Runtime node missing for node @@."), InitialStateNode);
				return;
			}

			static_cast<FSMState_Base*>(RunTimeNode)->bIsRootNode = true;
		}
	}

	// First pass handle state machines.
	for (const TObjectPtr<UEdGraphNode>& GraphNode : StateMachineGraph->Nodes)
	{
		if (USMGraphNode_Base* BaseNode = Cast<USMGraphNode_Base>(GraphNode))
		{
			BaseNode->OnCompile(*this);

			// Grab any property graphs.
			for (const auto& KeyVal : BaseNode->GetAllPropertyGraphs())
			{
				FEdGraphUtilities::CloneAndMergeGraphIn(ConsolidatedEventGraph, KeyVal.Value, MessageLog, true, true);
			}
		}
		
		if (USMGraphNode_StateMachineStateNode* StateMachineState = Cast<USMGraphNode_StateMachineStateNode>(GraphNode))
		{
			// The state machine graph for this state machine.
			UEdGraph* SourceGraph = StateMachineState->GetBoundGraph();
			if (!SourceGraph)
			{
				// These errors could occur if a compile happens while a state is being deleted.
				MessageLog.Error(TEXT("State Machine State Machine Node @@ has no state graph."), StateMachineState);
				continue;
			}

			// Set runtime property information. This likely has to be looked up from a temporary node since the runtime container is created dynamically on compile.
			FSMStateMachine* RuntimeStateMachine = (FSMStateMachine*)FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(SourceGraph);

			StateMachineState->SetRuntimeDefaults(*RuntimeStateMachine);
			RuntimeStateMachine->SetOwnerNodeGuid(ThisStateMachinesGuid);

			if (USMGraphNode_StateMachineParentNode* ParentNode = Cast<USMGraphNode_StateMachineParentNode>(StateMachineState))
			{
				// The parent graph is either completely expanded already or empty.
				USMGraph* ParentGraph = ParentNode->ExpandedGraph ? ParentNode->ExpandedGraph.Get() : CastChecked<USMGraph>(ParentNode->GetBoundGraph());
				ProcessStateMachineGraph(ParentGraph);
			}
			else if (USMGraph* StateSourceGraph = Cast<USMGraph>(SourceGraph))
			{
				// A full state machine graph can be processed normally even if this is a reference without the reference graph.
				ProcessStateMachineGraph(StateSourceGraph);
			}
			else if (USMIntermediateGraph* StateIntermediateGraph = Cast<USMIntermediateGraph>(SourceGraph))
			{
				// This has a reference graph and needs to be processed directly.
				ProcessNestedStateMachineNode(StateMachineState);
			}
			else
			{
				MessageLog.Error(TEXT("State Machine State Machine Node @@ has an unrecognized bound graph."), StateMachineState);
			}
		}
	}

	// Second pass handle states.
	TArray<UEdGraphNode*> GraphNodes = StateMachineGraph->Nodes;
	for (UEdGraphNode* GraphNode : GraphNodes)
	{
		if (USMGraphNode_StateNode* StateNode = Cast<USMGraphNode_StateNode>(GraphNode))
		{
			// The logic graph for this state.
			USMStateGraph* StateSourceGraph = Cast<USMStateGraph>(StateNode->GetBoundGraph());

			if (!StateSourceGraph)
			{
				// These errors could occur if a compile happens while a state is being deleted.
				MessageLog.Error(TEXT("State Machine State Node @@ has no state graph."), StateNode);
				continue;
			}

			// Set runtime property information.
			StateNode->SetRuntimeDefaults(StateSourceGraph->EntryNode->StateNode);
			StateSourceGraph->EntryNode->StateNode.SetOwnerNodeGuid(ThisStateMachinesGuid);

			// Clone the state graph and any sub graphs to the consolidated graph.
			FEdGraphUtilities::CloneAndMergeGraphIn(ConsolidatedEventGraph, StateSourceGraph, MessageLog, true, true);
		}
		else if (USMGraphNode_StateMachineStateNode* StateMachineNode = Cast<USMGraphNode_StateMachineStateNode>(GraphNode))
		{
			// Only reference graph's need to be processed.
			if (USMIntermediateGraph* IntermediateGraph = Cast< USMIntermediateGraph>(StateMachineNode->GetBoundGraph()))
			{
				FEdGraphUtilities::CloneAndMergeGraphIn(ConsolidatedEventGraph, IntermediateGraph, MessageLog, true, true);
			}
		}
		else if (USMGraphNode_ConduitNode* ConduitNode = Cast<USMGraphNode_ConduitNode>(GraphNode))
		{
			USMConduitGraph* ConduitSourceGraph = Cast<USMConduitGraph>(ConduitNode->GetBoundGraph());

			if (!ConduitSourceGraph)
			{
				// These errors could occur if a compile happens while a state is being deleted.
				MessageLog.Error(TEXT("State Machine Conduit Node @@ has no transition graph."), ConduitNode);
				continue;
			}

			// Set runtime property information.
			ConduitNode->SetRuntimeDefaults(ConduitSourceGraph->ResultNode->ConduitNode);
			ConduitSourceGraph->ResultNode->ConduitNode.SetOwnerNodeGuid(ThisStateMachinesGuid);

			// Clone the conduit graph and any sub graphs to the consolidated graph.
			FEdGraphUtilities::CloneAndMergeGraphIn(ConsolidatedEventGraph, ConduitSourceGraph, MessageLog, true, true);
		}
		else if (USMGraphNode_AnyStateNode* AnyState = Cast<USMGraphNode_AnyStateNode>(GraphNode))
		{
			// Any State nodes will duplicate their transitions to all valid state nodes.
			for (int32 Idx = 0; Idx < AnyState->GetOutputPin()->LinkedTo.Num(); ++Idx)
			{
				if (USMGraphNode_TransitionEdge* Transition = AnyState->GetNextTransition(Idx))
				{
					USMGraphNode_StateNodeBase* TargetStateNode = Transition->GetToState();

					for (UEdGraphNode* OtherNode : GraphNodes)
					{
						if (USMGraphNode_StateNodeBase* FromStateNode = Cast<USMGraphNode_StateNodeBase>(OtherNode))
						{
							if (!FSMBlueprintEditorUtils::DoesAnyStateImpactOtherNode(AnyState, FromStateNode) ||
								(OtherNode == TargetStateNode && !AnyState->bAllowInitialReentry) ||
								FromStateNode->IsA<USMGraphNode_LinkStateNode>())
							{
								continue;
							}

							FGraphNodeCreator<USMGraphNode_TransitionEdge> TransitionNodeCreator(*StateMachineGraph);
							USMGraphNode_TransitionEdge* ClonedTransition = TransitionNodeCreator.CreateNode();
							ClonedTransition->bFromAnyState = true;
							TransitionNodeCreator.Finalize();

							ClonedTransition->CopyFrom(*Transition);

							StateMachineGraph->GetSchema()->TryCreateConnection(FromStateNode->GetOutputPin(), ClonedTransition->GetInputPin());
							StateMachineGraph->GetSchema()->TryCreateConnection(ClonedTransition->GetOutputPin(), TargetStateNode->GetInputPin());

							// Clone original transition graph logic to new graph.
							USMTransitionGraph* ClonedTransitionGraph = CastChecked<USMTransitionGraph>(FEdGraphUtilities::CloneGraph(Transition->GetBoundGraph(), ClonedTransition, &MessageLog, true));

							// Makes sure the graph is named correctly once connections are established, don't call UpdatePrimaryTransition
							// because that will incorrectly rename this to have a _1 after.
							ClonedTransitionGraph->Rename(*Transition->GetBoundGraph()->GetName(), nullptr, REN_DoNotDirty | REN_ForceNoResetLoaders | REN_DontCreateRedirectors);

							ClonedTransition->SetBoundGraph(ClonedTransitionGraph);

							// Setup container and references. Similar to PreProcessRuntimeReferences.
							{
								TArray<USMGraphK2Node_RuntimeNodeContainer*> Containers;
								FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_RuntimeNodeContainer>(ClonedTransitionGraph, Containers);

								// Assign container guids so they can be mapped by the references.
								// Properties will be created normally during container processing.
								for (USMGraphK2Node_RuntimeNodeContainer* Container : Containers)
								{
									Container->ContainerOwnerGuid = GenerateGuid(Container);
									ClonedTransitionGraph->ResultNode = CastChecked<USMGraphK2Node_TransitionResultNode>(Container);

									// The source node and destination node are the same.
									SourceContainerToDuplicatedContainer.Add(Container, Container);
								}

								TArray<USMGraphK2Node_RuntimeNodeReference*> References;
								FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_RuntimeNodeReference>(ClonedTransitionGraph, References);

								// Sync reference nodes with their containers.
								for (USMGraphK2Node_RuntimeNodeReference* Reference : References)
								{
									if (USMGraphK2Node_TransitionResultNode* Container = Cast<USMGraphK2Node_TransitionResultNode>(Reference->GetRuntimeContainer()))
									{
										Reference->ContainerOwnerGuid = Container->ContainerOwnerGuid;
										Reference->SyncWithContainer();
									}
									else
									{
										MessageLog.Error(TEXT("Could not locate TransitionResultNode container for RuntimeNodeReference @@."), Reference);
									}
								}
							}

							// Adjust the cloned any state guid so it is unique yet deterministic.

							FSMTransition* OriginalRuntimeNode = Transition->GetRuntimeNode();
							check(OriginalRuntimeNode);

							FSMTransition* ClonedRuntimeNode = ClonedTransition->GetRuntimeNode();
							check(ClonedRuntimeNode);

							if (ensure(OriginalRuntimeNode->GetNodeGuid() == ClonedRuntimeNode->GetNodeGuid()))
							{
								const FGuid ClonedGuid = GenerateGuid(ClonedTransition, FString::Printf(TEXT("AnyState_%i"), Idx));
								if (ensure(ClonedGuid != OriginalRuntimeNode->GetNodeGuid()))
								{
									ClonedRuntimeNode->SetNodeGuid(ClonedGuid);
								}

								if (USMGraphNode_Base* SourceGraphNode = Cast<USMGraphNode_Base>(MessageLog.FindSourceObject(Transition)))
								{
									SourceGraphNode->RecordDuplicatedNodeGuid(ClonedGuid);
								}
							}

							// Record cloned node templates for the compiler.
							// TODO: Refactor so this isn't duplicated from the OnCompile method of transitions.
							{
								// Node template.
								if (!ClonedTransition->IsUsingDefaultNodeClass())
								{
									if (USMNodeInstance* ClonedTemplate = ClonedTransition->GetNodeTemplate())
									{
										AddDefaultObjectTemplate(ClonedRuntimeNode->GetNodeGuid(), ClonedTemplate, FTemplateContainer::ETemplateType::NodeTemplate);
									}
								}

								// Transition stack templates.
								for (const FTransitionStackContainer& Template : ClonedTransition->TransitionStack)
								{
									if (Template.NodeStackInstanceTemplate && ClonedTransition->GetDefaultNodeClass() != Template.TransitionStackClass)
									{
										AddDefaultObjectTemplate(ClonedRuntimeNode->GetNodeGuid(), Template.NodeStackInstanceTemplate,
											FTemplateContainer::StackTemplate, Template.TemplateGuid);
									}
								}
							}

							// Map all nodes to the new graph. The correct graph may not be able to be found after this point otherwise.
							{
								TArray<UK2Node*> AllNodes;
								FSMBlueprintEditorUtils::GetAllNodesOfClassNested<UK2Node>(ClonedTransitionGraph, AllNodes);

								for (UK2Node* Node : AllNodes)
								{
									// Node name needs to be unique if there are multiple Any State transitions.
									const FString NewNodeName = CreateUniqueName(Node, TEXT("AnyState"));
									Node->Rename(*NewNodeName, Node->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
									NodeToGraph.Add(Node->GetFName(), ClonedTransitionGraph);
								}
							}

							// Check for duplicated events such as from manual binding.
							TArray<UK2Node_Event*> Events;
							FSMBlueprintEditorUtils::GetAllNodesOfClassNested<UK2Node_Event>(ClonedTransitionGraph, Events);

							TArray<UK2Node_CreateDelegate*> CreateDelegates;
							FSMBlueprintEditorUtils::GetAllNodesOfClassNested<UK2Node_CreateDelegate>(ClonedTransitionGraph, CreateDelegates);

							for (UK2Node_Event* Event : Events)
							{
								FName OriginalFunctionName = Event->CustomFunctionName;
								const FString NewName = CreateUniqueName(Event, Event->CustomFunctionName.ToString());
								UK2Node_CreateDelegate** MatchingDelegate = CreateDelegates.FindByPredicate([&](const UK2Node_CreateDelegate* Delegate) {
									return Delegate->GetFunctionName() == OriginalFunctionName;
								});

								if (MatchingDelegate)
								{
									(*MatchingDelegate)->SelectedFunctionName = *NewName;
								}

								Event->CustomFunctionName = *NewName;
							}
						}
					}
				}
			}
		}
		else if (USMGraphNode_LinkStateNode* LinkState = Cast<USMGraphNode_LinkStateNode>(GraphNode))
		{
			if (LinkState->IsLinkedStateValid())
			{
				// Link state nodes move their transitions to their referenced state.
				for (auto It = LinkState->GetInputPin()->LinkedTo.CreateIterator(); It;)
				{
					if (USMGraphNode_TransitionEdge* Transition = LinkState->GetPreviousTransition(It.GetIndex()))
					{
						Transition->GetOutputPin()->BreakLinkTo(LinkState->GetInputPin());

						if (USMGraphNode_StateNodeBase* TargetStateNode = LinkState->GetLinkedState())
						{
							Transition->GetOutputPin()->MakeLinkTo(TargetStateNode->GetInputPin());
							if (USMGraphNode_TransitionEdge* PrimaryTransition = Transition->GetPrimaryReroutedTransition())
							{
								PrimaryTransition->bFromLinkState = true;
							}
						}
					}
					else
					{
						ensure((*It)->GetOwningNode()->IsA<USMGraphNode_StateMachineEntryNode>());
						++It;
					}
				}
			}
		}

		if (Cast<USMGraphNode_StateNodeBase>(GraphNode) != nullptr && !GraphNode->IsA<USMGraphNode_AnyStateNode>())
		{
			NumberStates++;
		}
	}

	// Third pass link transitions.
	for (const TObjectPtr<UEdGraphNode>& GraphNode : StateMachineGraph->Nodes)
	{
		if (USMGraphNode_TransitionEdge* EdgeNode = Cast<USMGraphNode_TransitionEdge>(GraphNode))
		{
			if (EdgeNode->IsRerouted() && EdgeNode->GetPreviousRerouteNode() != nullptr)
			{
				// For reroutes just take the first transition in the chain.
				// Everything will be redirected to the primary transition and we can skip processing the rest of the chain.
				continue;
			}

			ensure(EdgeNode->IsPrimaryReroutedTransition());

			USMGraphNode_StateNodeBase* StartNode = EdgeNode->GetFromState();
			if (!StartNode)
			{
				// These errors could occur if a compile happens while a state is being deleted.
				MessageLog.Error(TEXT("State Machine Transition Node @@ has no state A connection."), EdgeNode);
				continue;
			}

			if (StartNode->IsA<USMGraphNode_AnyStateNode>())
			{
				// Already processed.
				continue;
			}

			USMGraphNode_StateNodeBase* EndNode = EdgeNode->GetToState();
			if (!EndNode)
			{
				// These errors could occur if a compile happens while a state is being deleted.
				MessageLog.Error(TEXT("State Machine Transition Node @@ has no state B connection."), EdgeNode);
				continue;
			}

			if (EndNode->IsA<USMGraphNode_AnyStateNode>())
			{
				MessageLog.Error(TEXT("State Machine Transition Node @@ attempting to link to Any State Node @@. This behavior is now allowed."), EdgeNode, EndNode);
				continue;
			}

			// The boolean logic for this graph.
			USMTransitionGraph* TransitionSourceGraph = CastChecked<USMTransitionGraph>(EdgeNode->GetBoundGraph());

			// Set runtime property information.
			EdgeNode->SetRuntimeDefaults(TransitionSourceGraph->ResultNode->TransitionNode);
			TransitionSourceGraph->ResultNode->TransitionNode.SetOwnerNodeGuid(ThisStateMachinesGuid);

			// Link the transition to source nodes by guid. They will be resolved to pointers later.
			{
				UEdGraph* SourceStateGraph = Cast<UEdGraph>(StartNode->GetBoundGraph());
				if (!SourceStateGraph)
				{
					// These errors could occur if a compile happens while a state is being deleted.
					MessageLog.Error(TEXT("State Machine Transition Node @@ has no graph for start node @@."), EdgeNode, StartNode);
					continue;
				}

				FSMNode_Base* SourceState = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(SourceStateGraph);
				if (!SourceState)
				{
					// These errors could occur if a compile happens while a state is being deleted.
					MessageLog.Error(TEXT("State Machine Transition Node @@ has an invalid runtime node for start node @@."), EdgeNode, StartNode);
					continue;
				}

				UEdGraph* TargetStateGraph = Cast<UEdGraph>(EndNode->GetBoundGraph());
				if (!TargetStateGraph)
				{
					if (!EndNode->IsA<USMGraphNode_LinkStateNode>())
					{
						// These errors could occur if a compile happens while a state is being deleted or if this is an invalid link node.
						MessageLog.Error(TEXT("State Machine Transition Node @@ has no graph for end node @@."), EdgeNode, EndNode);
					}
					continue;
				}

				FSMNode_Base* TargetState = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(TargetStateGraph);
				if (!TargetState)
				{
					// These errors could occur if a compile happens while a state is being deleted.
					MessageLog.Error(TEXT("State Machine Transition Node @@ has an invalid runtime node for end node @@."), EdgeNode, EndNode);
					continue;
				}

				TransitionSourceGraph->ResultNode->TransitionNode.FromGuid = SourceState->GetNodeGuid();
				if (!TransitionSourceGraph->ResultNode->TransitionNode.FromGuid.IsValid())
				{
					MessageLog.Error(TEXT("State Machine Transition Node @@ has an invalid guid for from state @@."), EdgeNode, StartNode);
					continue;
				}

				TransitionSourceGraph->ResultNode->TransitionNode.ToGuid = TargetState->GetNodeGuid();
				if (!TransitionSourceGraph->ResultNode->TransitionNode.ToGuid.IsValid())
				{
					MessageLog.Error(TEXT("State Machine Transition Node @@ has an invalid guid for target state @@."), EdgeNode, EndNode);
					continue;
				}
			}

			// Clone the transition graph and any sub graphs to the consolidated graph
			FEdGraphUtilities::CloneAndMergeGraphIn(ConsolidatedEventGraph, TransitionSourceGraph, MessageLog, true, true);
			NumberTransitions++;
		}
	}
}

void FSMKismetCompilerContext::ProcessRuntimeContainers()
{
	TArray<USMGraphK2Node_RuntimeNodeContainer*> RuntimeContainerNodeList;
	ConsolidatedEventGraph->GetNodesOfClass<USMGraphK2Node_RuntimeNodeContainer>(/*out*/ RuntimeContainerNodeList);

	for (USMGraphK2Node_RuntimeNodeContainer* RuntimeContainerNode : RuntimeContainerNodeList)
	{
		// Create the actual property for this node.
		FStructProperty* NewProperty = CreateRuntimeProperty(RuntimeContainerNode);
		if (!NewProperty)
		{
			continue;
		}

		FSMExposedFunctionContainer ExposedFunctionContainer;

		const FSMNode_Base* BaseNode = RuntimeContainerNode->GetRunTimeNodeChecked();

		FSMNode_FunctionHandlers* FunctionHandlers =
			NodeExposedFunctions.FindOrAdd(BaseNode->GetNodeGuid()).GetOrAddInitialElement(RuntimeContainerNode->GetRunTimeNodeType());
		check(FunctionHandlers);

		if (USMGraphK2Node_StateEntryNode* StateEntryNode = Cast<USMGraphK2Node_StateEntryNode>(RuntimeContainerNode))
		{
			SetupStateEntry(StateEntryNode, ExposedFunctionContainer.ExposedFunctionHandlers);
			static_cast<FSMState_FunctionHandlers*>(FunctionHandlers)->BeginStateGraphEvaluator = MoveTemp(ExposedFunctionContainer.ExposedFunctionHandlers);

		}
		else if (USMGraphK2Node_ConduitResultNode* ConduitResultNode = Cast<USMGraphK2Node_ConduitResultNode>(RuntimeContainerNode))
		{
			SetupTransitionEntry(ConduitResultNode, NewProperty, ExposedFunctionContainer.ExposedFunctionHandlers);
			static_cast<FSMConduit_FunctionHandlers*>(FunctionHandlers)->CanEnterConduitGraphEvaluator = MoveTemp(ExposedFunctionContainer.ExposedFunctionHandlers);
		}
		else if (USMGraphK2Node_TransitionResultNode* TransitionResultNode = Cast<USMGraphK2Node_TransitionResultNode>(RuntimeContainerNode))
		{
			SetupTransitionEntry(TransitionResultNode, NewProperty, ExposedFunctionContainer.ExposedFunctionHandlers);
			static_cast<FSMTransition_FunctionHandlers*>(FunctionHandlers)->CanEnterTransitionGraphEvaluator = MoveTemp(ExposedFunctionContainer.ExposedFunctionHandlers);
		}
		else if (USMGraphK2Node_IntermediateEntryNode* ReferenceNode = Cast<USMGraphK2Node_IntermediateEntryNode>(RuntimeContainerNode))
		{
			SetupStateEntry(ReferenceNode, ExposedFunctionContainer.ExposedFunctionHandlers);
			static_cast<FSMState_FunctionHandlers*>(FunctionHandlers)->BeginStateGraphEvaluator = MoveTemp(ExposedFunctionContainer.ExposedFunctionHandlers);
		}
	}
}

void FSMKismetCompilerContext::ProcessRuntimeReferences()
{
	// Process transition events first since they will expand additional runtime node references.
	TArray<USMGraphK2Node_FunctionNode_TransitionEvent*> TransitionEvents;
	ConsolidatedEventGraph->GetNodesOfClass<USMGraphK2Node_FunctionNode_TransitionEvent>(/*out*/ TransitionEvents);
	for (USMGraphK2Node_FunctionNode_TransitionEvent* TransitionEvent : TransitionEvents)
	{
		if (TransitionEvent->HandlesOwnExpansion())
		{
			TransitionEvent->CustomExpandNode(*this, MappedContainerNodes.FindRef(TransitionEvent->ContainerOwnerGuid), nullptr);
		}
	}

	// Process all other reference nodes.
	TArray<USMGraphK2Node_RuntimeNodeReference*> RuntimeNodeReferences;
	ConsolidatedEventGraph->GetNodesOfClass<USMGraphK2Node_RuntimeNodeReference>(/*out*/ RuntimeNodeReferences);

	TArray<USMGraphK2Node_RuntimeNodeReference*> RemainingNodesToProcess;
	for (USMGraphK2Node_RuntimeNodeReference* RuntimeReferenceNode : RuntimeNodeReferences)
	{
		if (RuntimeReferenceNode->IsA<USMGraphK2Node_FunctionNode_TransitionEvent>())
		{
			// Already handled.
			continue;
		}

		// Gather nodes that require an additional pass.
		{
			if (USMGraphK2Node_StateReadNode* ReadNode = Cast<USMGraphK2Node_StateReadNode>(RuntimeReferenceNode))
			{
				RemainingNodesToProcess.Add(ReadNode);
				continue;
			}

			if (USMGraphK2Node_StateWriteNode* WriteNode = Cast<USMGraphK2Node_StateWriteNode>(RuntimeReferenceNode))
			{
				RemainingNodesToProcess.Add(WriteNode);
				continue;
			}

			if (USMGraphK2Node_FunctionNode* FunctionNode = Cast<USMGraphK2Node_FunctionNode>(RuntimeReferenceNode))
			{
				RemainingNodesToProcess.Add(FunctionNode);
				continue;
			}
		}

		// The first logic node of this function.
		UEdGraphNode* StateStartLogicNode = RuntimeReferenceNode->GetOutputNode();
		if (!StateStartLogicNode)
		{
			continue;
		}

		// Locate the runtime node so we can store defaults.
		USMGraphK2Node_RuntimeNodeContainer* Container = MappedContainerNodes.FindRef(
			RuntimeReferenceNode->ContainerOwnerGuid);
		check(Container);
		
		const UScriptStruct* RuntimeType = Container->GetRunTimeNodeType();
		check(RuntimeType);
		
		const FSMNode_Base* RuntimeNode = Container->GetRunTimeNodeChecked();

		bool bCreatePins = false;

		//////////////////////////////////////////////////////////////////////////
		// Runtime Reference type variation

		FSMExposedFunctionHandler Handler;
		FSMExposedFunctionContainer ExposedFunctionContainer;

		FSMNode_FunctionHandlers* FunctionHandlers =
			NodeExposedFunctions.FindOrAdd(RuntimeNode->GetNodeGuid()).GetOrAddInitialElement(RuntimeType);
		check(FunctionHandlers);

		if (USMGraphK2Node_IntermediateStateMachineStartNode* IntermediateStateMachineStartNode = Cast<USMGraphK2Node_IntermediateStateMachineStartNode>(RuntimeReferenceNode))
		{
			ConfigureExposedFunctionHandler(IntermediateStateMachineStartNode, Container, Handler, ExposedFunctionContainer.ExposedFunctionHandlers);
			FunctionHandlers->OnRootStateMachineStartedGraphEvaluator = MoveTemp(ExposedFunctionContainer.ExposedFunctionHandlers);
		}
		else if (USMGraphK2Node_IntermediateStateMachineStopNode* IntermediateOwningStateMachineStartNode = Cast<USMGraphK2Node_IntermediateStateMachineStopNode>(RuntimeReferenceNode))
		{
			ConfigureExposedFunctionHandler(IntermediateOwningStateMachineStartNode, Container, Handler, ExposedFunctionContainer.ExposedFunctionHandlers);
			FunctionHandlers->OnRootStateMachineStoppedGraphEvaluator = MoveTemp(ExposedFunctionContainer.ExposedFunctionHandlers);
		}
		else if (USMGraphK2Node_StateUpdateNode* StateUpdateNode = Cast<USMGraphK2Node_StateUpdateNode>(RuntimeReferenceNode))
		{
			ConfigureExposedFunctionHandler(StateUpdateNode, Container, Handler, ExposedFunctionContainer.ExposedFunctionHandlers);
			static_cast<FSMState_FunctionHandlers*>(FunctionHandlers)->UpdateStateGraphEvaluator = MoveTemp(ExposedFunctionContainer.ExposedFunctionHandlers);
			bCreatePins = true;
		}
		else if (USMGraphK2Node_StateEndNode* StateEndNode = Cast<USMGraphK2Node_StateEndNode>(RuntimeReferenceNode))
		{
			ConfigureExposedFunctionHandler(StateEndNode, Container, Handler, ExposedFunctionContainer.ExposedFunctionHandlers);
			static_cast<FSMState_FunctionHandlers*>(FunctionHandlers)->EndStateGraphEvaluator = MoveTemp(ExposedFunctionContainer.ExposedFunctionHandlers);
		}
		else if (USMGraphK2Node_TransitionEnteredNode* TransitionEnteredNode = Cast<USMGraphK2Node_TransitionEnteredNode>(RuntimeReferenceNode))
		{
			ConfigureExposedFunctionHandler(TransitionEnteredNode, Container, Handler, ExposedFunctionContainer.ExposedFunctionHandlers);
			if (RuntimeType == FSMTransition::StaticStruct())
			{
				static_cast<FSMTransition_FunctionHandlers*>(FunctionHandlers)->TransitionEnteredGraphEvaluator = MoveTemp(ExposedFunctionContainer.ExposedFunctionHandlers);
			}
			else if (RuntimeType == FSMConduit::StaticStruct())
			{
				static_cast<FSMConduit_FunctionHandlers*>(FunctionHandlers)->ConduitEnteredGraphEvaluator = MoveTemp(ExposedFunctionContainer.ExposedFunctionHandlers);
			}
		}
		else if (USMGraphK2Node_TransitionInitializedNode* TransitionInitializedNode = Cast<USMGraphK2Node_TransitionInitializedNode>(RuntimeReferenceNode))
		{
			ConfigureExposedFunctionHandler(TransitionInitializedNode, Container, Handler, ExposedFunctionContainer.ExposedFunctionHandlers);
			FunctionHandlers->NodeInitializedGraphEvaluators.Append(MoveTemp(ExposedFunctionContainer.ExposedFunctionHandlers));
		}
		else if (USMGraphK2Node_TransitionShutdownNode* TransitionShutdownNode = Cast<USMGraphK2Node_TransitionShutdownNode>(RuntimeReferenceNode))
		{
			ConfigureExposedFunctionHandler(TransitionShutdownNode, Container, Handler, ExposedFunctionContainer.ExposedFunctionHandlers);
			FunctionHandlers->NodeShutdownGraphEvaluators.Append(MoveTemp(ExposedFunctionContainer.ExposedFunctionHandlers));
		}
		else if (USMGraphK2Node_TransitionPreEvaluateNode* TransitionPreEvaluateNode = Cast<USMGraphK2Node_TransitionPreEvaluateNode>(RuntimeReferenceNode))
		{
			ConfigureExposedFunctionHandler(TransitionPreEvaluateNode, Container, Handler, ExposedFunctionContainer.ExposedFunctionHandlers);
			static_cast<FSMTransition_FunctionHandlers*>(FunctionHandlers)->TransitionPreEvaluateGraphEvaluator = MoveTemp(ExposedFunctionContainer.ExposedFunctionHandlers);
		}
		else if (USMGraphK2Node_TransitionPostEvaluateNode* TransitionPostEvaluateNode = Cast<USMGraphK2Node_TransitionPostEvaluateNode>(RuntimeReferenceNode))
		{
			ConfigureExposedFunctionHandler(TransitionPostEvaluateNode, Container, Handler, ExposedFunctionContainer.ExposedFunctionHandlers);
			static_cast<FSMTransition_FunctionHandlers*>(FunctionHandlers)->TransitionPostEvaluateGraphEvaluator = MoveTemp(ExposedFunctionContainer.ExposedFunctionHandlers);
		}

		// End Runtime Reference type variation
		//////////////////////////////////////////////////////////////////////////

		// Create a custom event in the graph to replace the dummy entry node. This will also link all input pins.
		if (Handler.ExecutionType == ESMExposedFunctionExecutionType::SM_Graph)
		{
			check(Handler.BoundFunction != NAME_None)
			const UK2Node_CustomEvent* EntryEventNode = CreateEntryNode(RuntimeReferenceNode, Handler.BoundFunction, bCreatePins);

			// The exec (then) pin of the new event node.
			UEdGraphPin* EntryNodeOutPin = Schema->FindExecutionPin(*EntryEventNode, EGPD_Output);

			// The exec (entry) pin of the logic node.
			EntryNodeOutPin->CopyPersistentDataFromOldPin(*RuntimeReferenceNode->GetThenPin());
			MessageLog.NotifyIntermediatePinCreation(EntryNodeOutPin, RuntimeReferenceNode->GetThenPin());
		}
		
		// Disconnect the dummy node.
		RuntimeReferenceNode->BreakAllNodeLinks();
	}

	// These nodes need to be processed after the main function entry nodes.
	for (USMGraphK2Node_RuntimeNodeReference* RuntimeReferenceNode : RemainingNodesToProcess)
	{
		if (USMGraphK2Node_StateReadNode* ReadNode = Cast<USMGraphK2Node_StateReadNode>(RuntimeReferenceNode))
		{
			ProcessReadNode(ReadNode);
		}
		else if (USMGraphK2Node_StateWriteNode* WriteNode = Cast<USMGraphK2Node_StateWriteNode>(RuntimeReferenceNode))
		{
			ProcessWriteNode(WriteNode);
		}
		else if (USMGraphK2Node_FunctionNode* FunctionNode = Cast<USMGraphK2Node_FunctionNode>(RuntimeReferenceNode))
		{
			ProcessFunctionNode(FunctionNode);
		}
	}
}

void FSMKismetCompilerContext::ProcessPropertyNodes()
{
	TArray<USMGraphK2Node_PropertyNode_Base*> PropertyNodes;
	ConsolidatedEventGraph->GetNodesOfClass<USMGraphK2Node_PropertyNode_Base>(/*out*/ PropertyNodes);

	for (USMGraphK2Node_PropertyNode_Base* PropertyNode : PropertyNodes)
	{
		// Map the specific property by Guid and store under the template instance. This is needed so during CDO construction
		// the template property will map to the correct property on the CDO.

		USMNodeInstance* NodeTemplate = PropertyNode->GetOwningTemplate();
		if (!NodeTemplate)
		{
			MessageLog.Error(TEXT("Node template not found for node @@."), PropertyNode);
			return;
		}

		FSMGraphProperty_Base* GraphProperty = PropertyNode->GetPropertyNodeChecked();
		if (GraphProperty->IsVariableReadOnly() && !GraphProperty->ShouldCompileReadOnlyVariables())
		{
			// Don't compile read only properties into the graph. Unreal can handle natively.
			continue;
		}
		
		const FGuid OldGuid = GraphProperty->GetGuid();
		
		if (!GraphProperty->ShouldGenerateGuidFromVariable())
		{
			const FGuid NewGuid = GenerateGuid(PropertyNode);
			GraphProperty->SetGuid(NewGuid);

			// Make sure runtime property matches.
			PropertyNode->ConfigureRuntimePropertyNode();
		}

		// Create the actual property for this node.
		FStructProperty* NewProperty = CreateRuntimeProperty(PropertyNode);
		if (!NewProperty)
		{
			MessageLog.Error(TEXT("Could not create node property @@."), PropertyNode);
			continue;
		}
		
		SetupPropertyEntry(PropertyNode, NewProperty);

		TMap<FGuid, USMGraphK2Node_Base*>& GraphProperties = MappedTemplatesToNodeProperties.FindOrAdd(NodeTemplate);
		ensure(!GraphProperties.Contains(GraphProperty->GetGuid()));
		GraphProperties.Add(GraphProperty->GetGuid(), PropertyNode);

		TMap<FGuid, FGuid>& RemappedProperties = GraphPropertyRemap.FindOrAdd(NodeTemplate);
		RemappedProperties.Add(OldGuid, GraphProperty->GetGuid());
	}
}

void FSMKismetCompilerContext::ProcessInputNodes()
{
	TArray<UK2Node_InputKey*> InputKeyNodes;
	ConsolidatedEventGraph->GetNodesOfClass<UK2Node_InputKey>(/*out*/ InputKeyNodes);

	TArray<UK2Node_InputAction*> InputActionNodes;
	ConsolidatedEventGraph->GetNodesOfClass<UK2Node_InputAction>(/*out*/ InputActionNodes);

	TArray<UK2Node_InputAxisEvent*> InputAxisEventNodes;
	ConsolidatedEventGraph->GetNodesOfClass<UK2Node_InputAxisEvent>(/*out*/ InputAxisEventNodes);

	TArray<UK2Node_InputAxisKeyEvent*> InputAxisKeyEventNodes;
	ConsolidatedEventGraph->GetNodesOfClass<UK2Node_InputAxisKeyEvent>(/*out*/ InputAxisKeyEventNodes);

	for (UK2Node_InputKey* InputNode : InputKeyNodes)
	{
		if (ExpandInputNode(InputNode) && InputNode->bConsumeInput)
		{
			InputConsumingEvent = InputNode;
		}
	}

	for (UK2Node_InputAction* InputNode : InputActionNodes)
	{
		if (ExpandInputNode(InputNode) && InputNode->bConsumeInput)
		{
			InputConsumingEvent = InputNode;
		}
	}

	for (UK2Node_InputAxisEvent* InputNode : InputAxisEventNodes)
	{
		if (ExpandInputNode(InputNode) && InputNode->bConsumeInput)
		{
			InputConsumingEvent = InputNode;
		}
	}

	for (UK2Node_InputAxisKeyEvent* InputNode : InputAxisKeyEventNodes)
	{
		if (ExpandInputNode(InputNode) && InputNode->bConsumeInput)
		{
			InputConsumingEvent = InputNode;
		}
	}
}

void FSMKismetCompilerContext::ProcessEnhancedInputNodes()
{
	TArray<UK2Node_EnhancedInputAction*> EnhancedInputActionNodes;
	ConsolidatedEventGraph->GetNodesOfClass<UK2Node_EnhancedInputAction>(/*out*/ EnhancedInputActionNodes);

	for (UK2Node_EnhancedInputAction* InputNode : EnhancedInputActionNodes)
	{
		if (ExpandInputNode(InputNode) && InputNode->InputAction && InputNode->InputAction->bConsumeInput)
		{
			InputConsumingEvent = InputNode;
		}
	}
}

bool FSMKismetCompilerContext::ExpandInputNode(UK2Node* InputNode)
{
	auto ExpandInputNodePinBranch = [this](UEdGraphPin* FromPin, UEdGraph* SourceGraph, UK2Node* InputNode,
                                                        const TSubclassOf<UObject> TargetType) -> void
	{
		USMGraphK2Node_StateReadNode_GetNodeInstance* GetNodeInstance =
			SpawnIntermediateNode<USMGraphK2Node_StateReadNode_GetNodeInstance>(InputNode, ConsolidatedEventGraph);
		GetNodeInstance->AllocatePinsForType(TargetType);

		USMGraphK2Node_RuntimeNodeContainer* SourceContainer = FSMBlueprintEditorUtils::GetRuntimeContainerFromGraph(SourceGraph);
		if (!SourceContainer)
		{
			MessageLog.Error(TEXT("Could not find source container for input node @@ with source graph @@."), InputNode, SourceGraph);
			return;
		}
		
		USMGraphK2Node_RuntimeNodeContainer** DestinationContainer = SourceContainerToDuplicatedContainer.Find(SourceContainer);
		if (!DestinationContainer)
		{
			MessageLog.Error(TEXT("Couldn't process input node @@, mapped container not found."), SourceContainer);
			return;
		}
		
		GetNodeInstance->ContainerOwnerGuid = (*DestinationContainer)->ContainerOwnerGuid;
		GetNodeInstance->RuntimeNodeGuid = (*DestinationContainer)->GetRunTimeNodeChecked()->GetNodeGuid();
		
		/////////////////////////
		// Connect argument pins
		/////////////////////////
		UEdGraphPin* InstancePinOut = GetNodeInstance->GetInstancePinChecked();
		
		UFunction* Func_IsInitialized = USMNodeInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMNodeInstance, IsInitializedAndReadyForInputEvents));
		check(Func_IsInitialized);
		const UK2Node_CallFunction* CallFunc_IsInitialized = FSMBlueprintEditorUtils::CreateFunctionCall(ConsolidatedEventGraph, Func_IsInitialized);
		UEdGraphPin* SelfPinIn = CallFunc_IsInitialized->FindPinChecked(UEdGraphSchema_K2::PN_Self);
		UEdGraphPin* IsInitializedPinOut = CallFunc_IsInitialized->GetReturnValuePin();
		check(IsInitializedPinOut);

		UK2Node_IfThenElse* IfElseNode = SpawnIntermediateNode<UK2Node_IfThenElse>(InputNode, ConsolidatedEventGraph);
		IfElseNode->AllocateDefaultPins();

		// IfThen -> OriginalExecution
		IfElseNode->GetThenPin()->MovePersistentDataFromOldPin(*FromPin);

		// GetNodeInstance(Instance) -> IsInitialized(self)
		if (!Schema->TryCreateConnection(InstancePinOut, SelfPinIn))
		{
			MessageLog.Error(TEXT("Failed to wire input argument (IsInitialized) for @@."), InputNode);
		}
		
		// IsInitialized(bool) -> If(condition)
		if (!Schema->TryCreateConnection(IsInitializedPinOut, IfElseNode->GetConditionPin()))
		{
			MessageLog.Error(TEXT("Failed to wire input arguments (Branch Condition) for @@."), InputNode);
		}
		
		// InputPinThen -> IfExec
		if (!Schema->TryCreateConnection(FromPin, IfElseNode->GetExecPin()))
		{
			MessageLog.Error(TEXT("Failed to wire input execution (Branch Exec) for @@."), InputNode);
		}
	};

	UEdGraph* SourceGraph = FindSourceGraphFromNode(InputNode);
	if (!SourceGraph)
	{
		MessageLog.Error(TEXT("Could not find source graph for input node @@."), InputNode);
		return false;
	}

	const TSubclassOf<UObject> TargetType = FSMBlueprintEditorUtils::GetNodeTemplateClass(SourceGraph, true);
	if (!TargetType)
	{
		return false; // Hopefully this is the actual event graph!
	}
	
	for (UEdGraphPin* Pin : InputNode->GetAllPins())
	{
		if (Pin
			&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
			&& Pin->Direction == EGPD_Output
			&& Pin->LinkedTo.Num() > 0)
		{
			ExpandInputNodePinBranch(Pin, SourceGraph, InputNode, TargetType);
		}
	}
		
	return true;
}

void FSMKismetCompilerContext::ProcessReadNode(USMGraphK2Node_StateReadNode* ReadNode)
{
	// The node container this read node references.
	USMGraphK2Node_RuntimeNodeContainer* NodeContainer = MappedContainerNodes.FindRef(ReadNode->ContainerOwnerGuid);

	// The property for the container which should have been created already.
	FProperty* NodeProperty = nullptr;
	for (const auto& KeyVal : AllocatedNodePropertiesToNodes)
	{
		if (KeyVal.Value == NodeContainer)
		{
			NodeProperty = KeyVal.Key;
			break;
		}
	}

	if (ReadNode->HandlesOwnExpansion())
	{
		ReadNode->CustomExpandNode(*this, NodeContainer, NodeProperty);
		return;
	}

	check(NodeProperty);
	const FName PropertyName = NodeProperty->GetFName();

	// This is the original result node and boolean pin on the graph.
	UEdGraphPin* OriginalReadOutputPin = ReadNode->GetOutputPin();

	// Create a variable read node to get the property.
	UK2Node_StructMemberGet* VarGetNode = SpawnIntermediateNode<UK2Node_StructMemberGet>(ReadNode, ConsolidatedEventGraph);
	VarGetNode->VariableReference.SetSelfMember(PropertyName);
	VarGetNode->StructType = NodeContainer->GetRunTimeNodeType();
	VarGetNode->AllocateDefaultPins();

	// Find exact pin we're looking for.
	UEdGraphPin** NewPropertyPin = VarGetNode->Pins.FindByPredicate([&](const UEdGraphPin* Pin)
	{
		return Pin->GetFName() == OriginalReadOutputPin->GetFName();
	});
	check(NewPropertyPin);

	// Connect this new pin to the pin reading it. (Generally a result pin)
	(*NewPropertyPin)->CopyPersistentDataFromOldPin(*OriginalReadOutputPin);
	MessageLog.NotifyIntermediatePinCreation(*NewPropertyPin, OriginalReadOutputPin);

	// Disconnect old pin.
	ReadNode->BreakAllNodeLinks();
}

void FSMKismetCompilerContext::ProcessWriteNode(USMGraphK2Node_StateWriteNode* WriteNode)
{
	// The node container this write node references.
	USMGraphK2Node_RuntimeNodeContainer* NodeContainer = MappedContainerNodes.FindRef(WriteNode->ContainerOwnerGuid);

	// The property for the container which should have been created already.
	FProperty* NodeProperty = nullptr;
	for (auto& KeyVal : AllocatedNodePropertiesToNodes)
	{
		if (KeyVal.Value == NodeContainer)
		{
			NodeProperty = KeyVal.Key;
			break;
		}
	}

	if (WriteNode->HandlesOwnExpansion())
	{
		WriteNode->CustomExpandNode(*this, NodeContainer, NodeProperty);
		return;
	}

	check(NodeProperty);
	const FName PropertyName = NodeProperty->GetFName();

	CreateSetter(WriteNode, PropertyName, NodeContainer->GetRunTimeNodeType());
}

void FSMKismetCompilerContext::ProcessFunctionNode(USMGraphK2Node_FunctionNode* FunctionNode)
{
	// The node container this node references.
	USMGraphK2Node_RuntimeNodeContainer* NodeContainer = MappedContainerNodes.FindRef(FunctionNode->ContainerOwnerGuid);

	// The property for the container which should have been created already.
	FProperty* NodeProperty = nullptr;
	for (const auto& KeyVal : AllocatedNodePropertiesToNodes)
	{
		if (KeyVal.Value == NodeContainer)
		{
			NodeProperty = KeyVal.Key;
			break;
		}
	}

	if (FunctionNode->HandlesOwnExpansion())
	{
		FunctionNode->CustomExpandNode(*this, NodeContainer, NodeProperty);
	}
}

UK2Node_CustomEvent* FSMKismetCompilerContext::SetupStateEntry(USMGraphK2Node_RuntimeNodeContainer* ContainerNode,
	TArray<FSMExposedFunctionHandler>& InOutHandlerContainer)
{
	FSMExposedFunctionHandler FunctionHandler;
	const ESMExposedFunctionExecutionType ExecutionType = ConfigureExposedFunctionHandler(ContainerNode, ContainerNode, FunctionHandler, InOutHandlerContainer);

	FName FunctionName;
	if (ExecutionType != ESMExposedFunctionExecutionType::SM_Graph)
	{
		// Always create an entry point node so we can associate the runtime node with the graph node
		// to support visual debugging.
		const FSMNode_Base* RuntimeNode = ContainerNode->GetRunTimeNodeFromContainer(ContainerNode);
		FunctionName = CreateFunctionName(ContainerNode, RuntimeNode);
	}
	else
	{
		FunctionName = FunctionHandler.BoundFunction;
	}
	
	// Create a custom event in the graph to replace the dummy entry node.
	UK2Node_CustomEvent* EntryEventNode = CreateEntryNode(ContainerNode, FunctionName);
	if (ExecutionType != ESMExposedFunctionExecutionType::SM_Graph)
	{
		// This entry node isn't being used apart from visual debugging.
		return EntryEventNode;
	}
	
	// The exec (then) pin of the new event node.
	UEdGraphPin* EntryNodeOutPin = Schema->FindExecutionPin(*EntryEventNode, EGPD_Output);
	
	// The exec (entry) pin of the logic node.
	EntryNodeOutPin->CopyPersistentDataFromOldPin(*ContainerNode->GetThenPin());
	MessageLog.NotifyIntermediatePinCreation(EntryNodeOutPin, ContainerNode->GetThenPin());

	// Disconnect the dummy node.
	ContainerNode->BreakAllNodeLinks();

	return EntryEventNode;
}

UK2Node_CustomEvent* FSMKismetCompilerContext::SetupTransitionEntry(USMGraphK2Node_RuntimeNodeContainer* ContainerNode, FStructProperty* Property,
	TArray<FSMExposedFunctionHandler>& InOutHandlerContainer)
{
	FSMExposedFunctionHandler FunctionHandler;
	if (ConfigureExposedFunctionHandler(ContainerNode, ContainerNode, FunctionHandler, InOutHandlerContainer) != ESMExposedFunctionExecutionType::SM_Graph)
	{
		return nullptr;
	}

	// Create a custom event in the graph to start the evaluation.
	UK2Node_CustomEvent* EntryEventNode = CreateEntryNode(ContainerNode, FunctionHandler.BoundFunction);

	// The exec (then) pin of the new event node.
	UEdGraphPin* EntryNodeOutPin = Schema->FindExecutionPin(*EntryEventNode, EGPD_Output);

	// Create a variable assign node to record the result of the boolean operation.
	UK2Node_StructMemberSet* VarSetNode = CreateSetter(ContainerNode, Property->GetFName(), ContainerNode->GetRunTimeNodeType());

	// The exec (entry pin) of the new variable assign node.
	UEdGraphPin* ExecVariablesInPin = Schema->FindExecutionPin(*VarSetNode, EGPD_Input);
	EntryNodeOutPin->MakeLinkTo(ExecVariablesInPin);

	return EntryEventNode;
}

USMGraphK2Node_StateMachineEntryNode* FSMKismetCompilerContext::ProcessNestedStateMachineNode(USMGraphNode_StateMachineStateNode* StateMachineStateNode)
{
	check(StateMachineStateNode);
	
	// Find the owning state machine node.
	UEdGraph* Graph = StateMachineStateNode->GetBoundGraph();
	FSMStateMachine* StateMachineNode = (FSMStateMachine*)FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(Graph);
	if (StateMachineNode == nullptr)
	{
		ensure(StateMachineStateNode->IsStateMachineReference());
		MessageLog.Error(TEXT("Could not locate state machine runtime node for node @@. Check if this is a state machine reference and the reference is valid."), StateMachineStateNode);
		return nullptr;
	}
	StateMachineNode->SetClassReference(nullptr);
	StateMachineNode->SetReferencedTemplateName(NAME_None);

	// Check if we're a reference to another blueprint.
	if (USMBlueprint* ReferencedBlueprint = StateMachineStateNode->GetStateMachineReference())
	{
		StateMachineNode->SetClassReference(ReferencedBlueprint->GeneratedClass);
		if (USMInstance* Template = StateMachineStateNode->GetStateMachineReferenceTemplateDirect())
		{
			// Store a template if it exists. We will deep copy it to the CDO later.
			AddDefaultObjectTemplate(StateMachineNode->GetNodeGuid(), Template, FTemplateContainer::ETemplateType::ReferenceTemplate);
		}
	}

	USMGraphK2Node_StateMachineEntryNode* NewEntryNode = nullptr;

	// We will want to execute reference graphs during runtime.
	if (USMIntermediateGraph* IntermediateGraph = Cast<USMIntermediateGraph>(Graph))
	{
		NewEntryNode = IntermediateGraph->IntermediateEntryNode;
	}
	else if (USMGraph* StateMachineGraph = Cast<USMGraph>(StateMachineStateNode->GetBoundGraph()))
	{
		// Check if this has already been generated and return that node.
		const FGuid& ContainerGuid = GenerateGuid(StateMachineGraph, TEXT("StateMachineContainer"), true);
		if (USMGraphK2Node_StateMachineEntryNode* EntryNode = Cast<USMGraphK2Node_StateMachineEntryNode>(MappedContainerNodes.FindRef(ContainerGuid)))
		{
			return EntryNode;
		}
		
		// Create a container to store this state machine in the consolidated graph.
		FGraphNodeCreator<USMGraphK2Node_StateMachineEntryNode> NodeCreator(*ConsolidatedEventGraph);
		NewEntryNode = NodeCreator.CreateNode();
		NewEntryNode->StateMachineNode = *StateMachineNode;
		NewEntryNode->ContainerOwnerGuid = ContainerGuid;
		NodeCreator.Finalize();

		StateMachineGraph->GeneratedContainerNode = NewEntryNode;
		
		// Store the generated entry node so it can be retrieved easier since it exists in the consolidated graph.
		for (USMGraphK2Node_PropertyNode_Base* PropertyNode : StateMachineStateNode->GetAllPropertyGraphNodesAsArray())
		{
			PropertyNode->ContainerOwnerGuid = ContainerGuid;
			PropertyNode->RuntimeNodeGuid = StateMachineNode->GetNodeGuid();

			TArray<USMGraphK2Node_RuntimeNodeReference*> ReferenceNodes;
			FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_RuntimeNodeReference>(PropertyNode->GetPropertyGraph(), ReferenceNodes);

			for (USMGraphK2Node_RuntimeNodeReference* ReferenceNode : ReferenceNodes)
			{
				ReferenceNode->ContainerOwnerGuid = PropertyNode->ContainerOwnerGuid;
				ReferenceNode->RuntimeNodeGuid = PropertyNode->RuntimeNodeGuid;
			}
		}

		MappedContainerNodes.Add(ContainerGuid, NewEntryNode);
	}

	return NewEntryNode;
}

UK2Node_CustomEvent* FSMKismetCompilerContext::SetupPropertyEntry(USMGraphK2Node_PropertyNode_Base* PropertyNode,
	FStructProperty* Property)
{
	// Locate the runtime node so we can store defaults.
	FSMGraphProperty_Base* BaseNode = PropertyNode->GetPropertyNodeChecked();

	// Create a unique name to identify this function when it is called during run-time.
	const FName FunctionName = CreateFunctionName(PropertyNode, BaseNode);
	{
		FSMExposedFunctionHandler Handler;
		Handler.BoundFunction = FunctionName;
		// Always graph evaluate properties. Optimizations configured at node level.
		Handler.ExecutionType = ESMExposedFunctionExecutionType::SM_Graph;

		const USMGraphNode_Base* StateNode = CastChecked<USMGraphNode_Base>(PropertyNode->GetOwningGraphNode());
		const FSMNode_Base* RuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(StateNode->GetBoundGraph());
		check(RuntimeNode);
		FSMExposedNodeFunctions& NodeFunctions = NodeExposedFunctions.FindOrAdd(RuntimeNode->GetNodeGuid());
		ensure(!NodeFunctions.GraphPropertyFunctionHandlers.Contains(BaseNode->GetGuid()));
		NodeFunctions.GraphPropertyFunctionHandlers.Add(BaseNode->GetGuid(), Handler);
	}

	// Create a custom event in the graph to start the evaluation.
	UK2Node_CustomEvent* EntryEventNode = CreateEntryNode(PropertyNode, FunctionName);

	// The exec (then) pin of the new event node.
	UEdGraphPin* EntryNodeOutPin = Schema->FindExecutionPin(*EntryEventNode, EGPD_Output);

	UEdGraphNode* VarSetNode;
	
	// Create a variable assign node to record the result of the operation.
	if (BaseNode->ShouldAutoAssignVariable())
	{
		UEdGraphPin* VariableDataPin = PropertyNode->FindPin(BaseNode->VariableName);
		if (!VariableDataPin)
		{
			MessageLog.Error(TEXT("Could not locate variable pin on property node @@"), PropertyNode);
			return nullptr;
		}
		
		UEdGraphPin* SelfPin;
		if (BaseNode->bIsInArray)
		{
			UK2Node_VariableGet* VarGet = SpawnIntermediateNode<UK2Node_VariableGet>(PropertyNode, ConsolidatedEventGraph);
			VarGet->VariableReference = BaseNode->MemberReference;
			VarGet->AllocateDefaultPins();
			
			SelfPin = Schema->FindSelfPin(*VarGet, EGPD_Input);
			if (!SelfPin)
			{
				MessageLog.Error(TEXT("Could not locate a 'self' pin for node @@. Was the variable removed? Try recompiling the blueprint @@."),
					PropertyNode, PropertyNode->GetTemplateBlueprint());
				return nullptr;
			}
			
			UK2Node_CallArrayFunction* ArrayNode = SpawnIntermediateNode<UK2Node_CallArrayFunction>(PropertyNode, ConsolidatedEventGraph);
			ArrayNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Set), UKismetArrayLibrary::StaticClass());
			ArrayNode->AllocateDefaultPins();

			// Link the array variable to the add array node.
			UEdGraphPin* TargetArrayPin = ArrayNode->FindPinChecked(TEXT("TargetArray"));
			TargetArrayPin->MakeLinkTo(VarGet->GetValuePin());
			ArrayNode->PinConnectionListChanged(TargetArrayPin);

			// Set the array index.
			UEdGraphPin* TargetIndexPin = ArrayNode->FindPinChecked(TEXT("Index"));
			TargetIndexPin->DefaultValue = FString::FromInt(BaseNode->ArrayIndex);

			// Set that the array should resize itself.
			UEdGraphPin* SizeToFitPin = ArrayNode->FindPinChecked(TEXT("bSizeToFit"));
			SizeToFitPin->DefaultValue = "true";
			
			// Link the new item being added to the array.
			UEdGraphPin* NewItemPinIn = ArrayNode->FindPinChecked(TEXT("Item"));
			NewItemPinIn->CopyPersistentDataFromOldPin(*VariableDataPin);

			// We will wire to the execution node of the array pin below.
			VarSetNode = ArrayNode;
		}
		else
		{
			UK2Node_VariableSet* VarSet = SpawnIntermediateNode<UK2Node_VariableSet>(PropertyNode, ConsolidatedEventGraph);
			VarSet->VariableReference = BaseNode->MemberReference;
			VarSet->AllocateDefaultPins();
			SelfPin = Schema->FindSelfPin(*VarSet, EGPD_Input);
			if (!SelfPin)
			{
				MessageLog.Error(TEXT("Could not locate a 'self' pin for node @@. Was the variable removed? Try recompiling the blueprint @@."),
					PropertyNode, PropertyNode->GetTemplateBlueprint());
				return nullptr;
			}
			VarSetNode = VarSet;
			
			UEdGraphPin* VariableInputPin = VarSet->FindPin(BaseNode->VariableName, EGPD_Input);
			if (!VariableInputPin)
			{
				MessageLog.Error(TEXT("Could not locate variable pin on intermediate setter @@"), PropertyNode);
				return nullptr;
			}

			VariableInputPin->CopyPersistentDataFromOldPin(*VariableDataPin);
		}

		USMNodeInstance* OwningTemplate = PropertyNode->GetOwningTemplate();
		check(OwningTemplate);
		
		// TODO: Handle stack instances. Need to be able to look up by guid or index. Currently just "NodeInstance" is retrieved in a getter.
		USMGraphK2Node_StateReadNode_GetNodeInstance* GetNodeInstance = SpawnIntermediateNode<USMGraphK2Node_StateReadNode_GetNodeInstance>(PropertyNode, ConsolidatedEventGraph);
		GetNodeInstance->ContainerOwnerGuid = PropertyNode->ContainerOwnerGuid;
		GetNodeInstance->RuntimeNodeGuid = PropertyNode->RuntimeNodeGuid;
		GetNodeInstance->NodeInstanceGuid = PropertyNode->GetPropertyNodeConstChecked()->GetGuid();
		GetNodeInstance->bCanCreateNodeInstanceOnDemand = false; // Graph properties will always have an instance created for them.
		
		if (USMGraphNode_StateNode* StateNode = Cast<USMGraphNode_StateNode>(PropertyNode->GetOwningGraphNode()))
		{
			// This may be part of a state stack template. Store the index so it can be retrieved in GetNodeInstance.
			GetNodeInstance->NodeInstanceIndex = StateNode->GetIndexOfTemplate(OwningTemplate->GetTemplateGuid());
		}
		
		GetNodeInstance->AllocatePinsForType(OwningTemplate->GetClass());
		Schema->TryCreateConnection(GetNodeInstance->GetOutputPin(), SelfPin);
	}
	else
	{
		VarSetNode = CreateSetter(PropertyNode, Property->GetFName(), PropertyNode->GetRuntimePropertyNodeType());
	}
	
	// The exec (entry pin) of the new variable assign node.
	UEdGraphPin* ExecVariablesInPin = Schema->FindExecutionPin(*VarSetNode, EGPD_Input);
	EntryNodeOutPin->MakeLinkTo(ExecVariablesInPin);

	PropertyNode->BreakAllNodeLinks();
	
	return EntryEventNode;
}

USMGraph* FSMKismetCompilerContext::ProcessParentNode(USMGraphNode_StateMachineParentNode* ParentStateMachineNode)
{
	USMGraph* DefaultGraph = CastChecked<USMGraph>(ParentStateMachineNode->GetBoundGraph());

	if (!NewSMBlueprintClass->IsChildOf(ParentStateMachineNode->ParentClass) || NewSMBlueprintClass == ParentStateMachineNode->ParentClass.Get())
	{
		MessageLog.Error(TEXT("Invalid parent chosen for state machine node @@."), ParentStateMachineNode);
		// Default processing so basic nodes can be setup preventing check to fail during runtime generation from linked transition nodes.
		ProcessStateMachineGraph(DefaultGraph);
		return nullptr;
	}

	FSMStateMachine* StateMachineNode = (FSMStateMachine*)FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(DefaultGraph);
	check(StateMachineNode);

	USMBlueprintGeneratedClass* ParentClass = Cast<USMBlueprintGeneratedClass>(ParentStateMachineNode->ParentClass.Get());
	UBlueprint* ParentBlueprint = UBlueprint::GetBlueprintFromClass(ParentClass);
	if (!ParentBlueprint)
	{
		MessageLog.Error(TEXT("Parent state machine node @@ could not locate parent blueprint."), ParentStateMachineNode);
		// Default processing so basic nodes can be setup preventing check to fail during runtime generation from linked transition nodes.
		ProcessStateMachineGraph(DefaultGraph);
		return nullptr;
	}

	USMGraph* ParentStateMachineGraph = FSMBlueprintEditorUtils::GetRootStateMachineGraph(ParentBlueprint);
	if (!ParentStateMachineGraph)
	{
		MessageLog.Warning(TEXT("Parent state machine node @@ has no root state machine graph in parent blueprint @@."), ParentStateMachineNode, ParentBlueprint);
		// Default processing so basic nodes can be setup preventing check to fail during runtime generation from linked transition nodes.
		ProcessStateMachineGraph(DefaultGraph);
		return nullptr;
	}

	// Clone the entire parent graph and process as if it belongs directly to the child.
	USMGraph* ClonedParentGraph = CastChecked<USMGraph>(FEdGraphUtilities::CloneGraph(ParentStateMachineGraph, ParentStateMachineNode, &MessageLog, true));
	ValidateAllNodes(ClonedParentGraph);
	
	USMGraphNode_StateMachineEntryNode* EntryNode = ClonedParentGraph->GetEntryNode();
	EntryNode->StateMachineNode = *StateMachineNode;

	ParentStateMachineNode->ExpandedGraph = ClonedParentGraph;

	// Continue to expand all parents of parents.
	TArray<USMGraphNode_StateMachineParentNode*> ParentNodesInParent;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_StateMachineParentNode>(ClonedParentGraph, ParentNodesInParent);
	for (USMGraphNode_StateMachineParentNode* Node : ParentNodesInParent)
	{
		ProcessParentNode(Node);
	}

	// Establish runtime container-reference unique ids. If this parent graph is referenced more than once there will be duplicates otherwise!
	PreProcessStateMachineNodes(ClonedParentGraph);
	PreProcessRuntimeReferences(ClonedParentGraph);
	
	return ClonedParentGraph;
}

UK2Node_StructMemberSet* FSMKismetCompilerContext::CreateSetter(UK2Node* WriteNode, FName PropertyName,
	UScriptStruct* ScriptStruct, bool bCreateGetterForDefaults)
{
	// Create a variable write node to set the property.
	UK2Node_StructMemberSet* VarSetNode = SpawnIntermediateNode<UK2Node_StructMemberSet>(WriteNode, ConsolidatedEventGraph);
	VarSetNode->VariableReference.SetSelfMember(PropertyName);
	VarSetNode->StructType = ScriptStruct;
	VarSetNode->AllocateDefaultPins();

	UK2Node_StructMemberGet* VarGetNode = nullptr;

	for (UEdGraphPin* NewPin : VarSetNode->Pins)
	{
		// First attempt to find desired pin from the setter.
		UEdGraphPin** OriginalPin = WriteNode->Pins.FindByPredicate([&](const UEdGraphPin* Pin)
		{
			return Pin->GetFName() == NewPin->GetFName();
		});

		// This can be the execution pin, then pin, or value pin we are setting.
		if (OriginalPin != nullptr)
		{
			NewPin->CopyPersistentDataFromOldPin(**OriginalPin);
			(*OriginalPin)->BreakAllPinLinks();
		}
		// If this fails create a getter and find the matching pin so we can keep previous values.
		else
		{
			if (bCreateGetterForDefaults)
			{
				if (VarGetNode == nullptr)
				{
					VarGetNode = SpawnIntermediateNode<UK2Node_StructMemberGet>(WriteNode, ConsolidatedEventGraph);
					check(VarGetNode);
					VarGetNode->VariableReference.SetSelfMember(PropertyName);
					VarGetNode->StructType = ScriptStruct;
					VarGetNode->AllocateDefaultPins();
				}

				OriginalPin = VarGetNode->Pins.FindByPredicate([&](const UEdGraphPin* Pin)
				{
					return Pin->GetFName() == NewPin->GetFName();
				});
			}

			if (OriginalPin == nullptr)
			{
				// If we are connecting to a pure node we don't need to worry about execution or if this is a then pin from a write node which doesn't have a then.
				if ((Schema->IsExecPin(*NewPin) && WriteNode->IsNodePure()) || (USMGraphK2Schema::IsThenPin(NewPin) && WriteNode->FindPin(NewPin->GetFName(), EGPD_Output) == nullptr))
				{
					continue;
				}

				MessageLog.Error(TEXT("Could not wire set node @@ with pin @@"), WriteNode, NewPin);
				continue;
			}

			Schema->TryCreateConnection(*OriginalPin, NewPin);
		}

		MessageLog.NotifyIntermediatePinCreation(NewPin, *OriginalPin);
	}

	// Disconnect old pin.
	WriteNode->BreakAllNodeLinks();

	return VarSetNode;
}

UK2Node_CustomEvent* FSMKismetCompilerContext::CreateEntryNode(USMGraphK2Node_RootNode* RootNode, FName FunctionName, bool bCreateAndLinkParamPins)
{
	// Add a custom event in the graph that we can call by the function name.
	UK2Node_CustomEvent* EntryEventNode = SpawnIntermediateEventNode<UK2Node_CustomEvent>(RootNode, nullptr, ConsolidatedEventGraph);
	EntryEventNode->bInternalEvent = true;
	EntryEventNode->CustomFunctionName = FunctionName;
	EntryEventNode->AllocateDefaultPins();

	if (bCreateAndLinkParamPins)
	{
		// Find all of the connections of the original pin properties.
		for (UEdGraphPin* OriginalParamPinOut : RootNode->Pins)
		{
			if (OriginalParamPinOut->Direction != EGPD_Output || UEdGraphSchema_K2::IsExecPin(*OriginalParamPinOut))
			{
				continue;
			}

			// Create the new output pin. Must not use CreatePin or when the FunctionCall is created in KismetCompiler it will have no pins.
			UEdGraphPin* NewParamPinOut = EntryEventNode->CreateUserDefinedPin(OriginalParamPinOut->PinName, OriginalParamPinOut->PinType, OriginalParamPinOut->Direction);
			check(NewParamPinOut);

			// Wire param pin of the new entry node to the logic pin the old one was connected to.
			NewParamPinOut->CopyPersistentDataFromOldPin(*OriginalParamPinOut);
			MessageLog.NotifyIntermediatePinCreation(NewParamPinOut, OriginalParamPinOut);
		}

		EntryEventNode->ReconstructNode();
	}

	return EntryEventNode;
}

FStructProperty* FSMKismetCompilerContext::CreateRuntimeProperty(USMGraphK2Node_RuntimeNodeContainer* RuntimeContainerNode)
{
	// Any valid name will do, we will map to runtime node guids for lookup later.
	const FString NodeVariableName = CreateUniqueName(RuntimeContainerNode, TEXT("LD_Prop"));
	FEdGraphPinType NodeVariableType;
	NodeVariableType.PinCategory = USMGraphK2Schema::PC_Struct;
	NodeVariableType.PinSubCategoryObject = MakeWeakObjectPtr(RuntimeContainerNode->GetRunTimeNodeType());

	FStructProperty* NewProperty = CastField<FStructProperty>(CreateVariable(FName(*NodeVariableName), NodeVariableType));

	// This shouldn't ever happen unless maybe a custom node is being added incorrectly.
	if (!NewProperty)
	{
		MessageLog.Error(TEXT("Failed to create node property for @@"), RuntimeContainerNode);
		return NewProperty;
	}

	NewProperty->SetMetaData(TEXT("NoLogicDriverExport"), TEXT("true"));

	// Record the property so it can be referenced during DefaultObject setup.
	AllocatedNodePropertiesToNodes.Add(NewProperty, RuntimeContainerNode);

	// Record this node for quick access by container references.
	if (RuntimeContainerNode->ContainerOwnerGuid.IsValid())
	{
		MappedContainerNodes.Add(RuntimeContainerNode->ContainerOwnerGuid, RuntimeContainerNode);
	}
	
	return NewProperty;
}

FStructProperty* FSMKismetCompilerContext::CreateRuntimeProperty(USMGraphK2Node_PropertyNode_Base* PropertyNode)
{
	// Any valid name will do, we will map to runtime node guids for lookup later.
	const FString NodeVariableName = CreateUniqueName(PropertyNode, TEXT("LD_Prop"));
	FEdGraphPinType NodeVariableType;
	NodeVariableType.PinCategory = USMGraphK2Schema::PC_Struct;
	NodeVariableType.PinSubCategoryObject = MakeWeakObjectPtr(PropertyNode->GetRuntimePropertyNodeType());

	FStructProperty* NewProperty = CastField<FStructProperty>(CreateVariable(FName(*NodeVariableName), NodeVariableType));

	// This shouldn't ever happen unless maybe a custom node is being added incorrectly.
	if (!NewProperty)
	{
		MessageLog.Error(TEXT("Failed to create node property for @@"), PropertyNode);
		return NewProperty;
	}

	NewProperty->SetMetaData(TEXT("NoLogicDriverExport"), TEXT("true"));

	// Record the property so it can be referenced during DefaultObject setup.
	AllocatedNodePropertiesToNodes.Add(NewProperty, PropertyNode);

	return NewProperty;
}

void FSMKismetCompilerContext::AddDefaultObjectTemplate(const FGuid& RuntimeGuid, UObject* Template, FTemplateContainer::ETemplateType TemplateType, const FGuid& TemplateGuid)
{
	TArray<FTemplateContainer>& Templates = DefaultObjectTemplates.FindOrAdd(RuntimeGuid);
	Templates.AddUnique(FTemplateContainer(Template, TemplateType, TemplateGuid));
}

FName FSMKismetCompilerContext::CreateFunctionName(const USMGraphK2Node_RootNode* GraphNode, const FSMNode_Base* RuntimeNode)
{
	check(GraphNode);
	check(RuntimeNode);
	const FString Suffix = FString::Printf(TEXT("%s_%s"), *RuntimeNode->GetNodeName(), *RuntimeNode->GetNodeGuid().ToString());
	const FName NewName = *CreateUniqueName(GraphNode, Suffix);
	return NewName;
}

FName FSMKismetCompilerContext::CreateFunctionName(const USMGraphK2Node_RootNode* GraphNode, const FSMGraphProperty_Base* PropertyNode)
{
	check(GraphNode);
	check(PropertyNode);
	const FString Suffix = PropertyNode->GetGuid().ToString();
	const FName NewName = *CreateUniqueName(GraphNode, Suffix);
	return NewName;
}

FString FSMKismetCompilerContext::CreateUniqueName(const UObject* InObject, const FString& Suffix, bool bAllowReuse)
{
	// Localize the name to this specific blueprint. This can help if this is named the same as a parent blueprint and is
	// copied from the parent blueprint.
	const FString GeneratedSuffix = FString::Printf(TEXT("%s_%s"), *Blueprint->GetBlueprintGuid().ToString(), *Suffix);
	const FString UniqueName = bAllowReuse ? ClassScopeNetNameMap.MakeValidName(InObject, GeneratedSuffix) : SMClassNameMap.MakeValidName(InObject, GeneratedSuffix);
	if (!bAllowReuse)
	{
		NewSMBlueprintClass->GeneratedNames.Add(UniqueName);
	}
	return UniqueName;
}

FGuid FSMKismetCompilerContext::GenerateGuid(const UObject* InObject, const FString& Suffix, bool bAllowReuse)
{
	const FString UniqueName = CreateUniqueName(InObject, Suffix, bAllowReuse);

	FGuid OutGuid;
	FGuid::Parse(FMD5::HashAnsiString(*UniqueName), OutGuid);

	return MoveTemp(OutGuid);
}

USMBlueprint* FSMKismetCompilerContext::GetSMBlueprint() const
{
	return Cast<USMBlueprint>(Blueprint);
}

void FSMKismetCompilerContext::LogCompilerMessage(FCompilerResultsLog& InMessageLog, const FString& InMessage,
                                                  ESMCompilerLogType InSeverity, USMGraphNode_Base* InOwningNode, USMGraphNode_Base* InCallingNode)
{
#define LOG_COMPILE_MESSAGE(Type) \
	if (InOwningNode) \
	{ \
		InMessageLog.Type(*InMessage, InOwningNode, InCallingNode); \
	} \
	else if (InCallingNode) \
	{ \
		InMessageLog.Type(*InMessage, InCallingNode); \
	} \
	else \
	{ \
		InMessageLog.Type(*InMessage); \
	} \

	switch (InSeverity)
	{
	case ESMCompilerLogType::Note:
		LOG_COMPILE_MESSAGE(Note);
		break;
	case ESMCompilerLogType::Warning:
		LOG_COMPILE_MESSAGE(Warning);
		break;
	case ESMCompilerLogType::Error:
		LOG_COMPILE_MESSAGE(Error);
		break;
	}
}

void FSMKismetCompilerContext::RecompileChildren()
{
	/*
	 * Update -- UE 4.24.2 https://issues.unrealengine.com/issue/UE-86356 may have fixed this issue.
	 *
	 * Fixes #145 - On 4.24 modifying a parent only performs a skeleton recompile of children, but we need a full compile to expand updated parent nodes.
	 * This will mark the child blueprints dirty so they will be compiled on play. This is one part to the fix, the other was removing most
	 * BlueprintGeneratedDefaults meta calls as that would prevent reinstancing from copying over Guids.
	*/

	/*
	 * Update for 2.0 -- Fixes #151
	 * This is being repurposed to be called from CleanAndSanitizeClass. Fixes calls to parent graphs which reference another BP that has been modified.
	 * Only works correctly if the modified BP has been manually compiled. The children BPs will be marked dirty and compiled on play.
	 * If play is pressed the compile order isn't guaranteed and the child BP most likely won't be fully compiled until the next play session.
	 */

	if (PRIVATE_GIsRunningCommandlet)
	{
		return;
	}
	
	FSMBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	if (Blueprint->SkeletonGeneratedClass && !Blueprint->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
	{
		TArray<UClass*> ChildClasses;
		GetDerivedClasses(Blueprint->SkeletonGeneratedClass, ChildClasses);

		for (const UClass* ChildClass : ChildClasses)
		{
			if (USMBlueprint* ChildBlueprint = Cast<USMBlueprint>(UBlueprint::GetBlueprintFromClass(ChildClass)))
			{
				// Verify we're only on an SM generated class. It could be a macro library based off of an SM which will crash.
				if (USMBlueprintGeneratedClass* SMBPGC = Cast<USMBlueprintGeneratedClass>(ChildBlueprint->GeneratedClass))
				{
					if (ChildBlueprint->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad) || ChildBlueprint->bIsNewlyCreated)
					{
						continue;
					}
					
					UEdGraph* TopLevelStateMachineGraph = FSMBlueprintEditorUtils::GetTopLevelStateMachineGraph(ChildBlueprint);
					if (!TopLevelStateMachineGraph)
					{
						MessageLog.Error(TEXT("Recompile children error: Could not locate top level state machine graph for blueprint @@."), ChildBlueprint);
						continue;
					}

					TArray<USMGraphNode_StateMachineParentNode*> ParentCalls;
					FSMBlueprintEditorUtils::GetAllNodesOfClassNested(TopLevelStateMachineGraph, ParentCalls);

					// If there are no parent calls we can just use the normal skeleton recompile, otherwise the nodes need to be expanded in a full compile.
					if (ParentCalls.Num() > 0)
					{
						SMBPGC->bHasStaleParentData = true;
						
						FSMBlueprintEditorUtils::MarkBlueprintAsModified(ChildBlueprint);
						FSMBlueprintEditorUtils::EnsureCachedDependenciesUpToDate(ChildBlueprint);
						
						ISMSystemEditorModule& SMBlueprintEditorModule = FModuleManager::GetModuleChecked<ISMSystemEditorModule>(LOGICDRIVER_EDITOR_MODULE_NAME);
						const USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetProjectEditorSettings();
						if (SMBlueprintEditorModule.IsPlayingInEditor() && !ChildBlueprint->bBeingCompiled
							&& Settings->bWarnIfChildrenAreOutOfDate && FApp::CanEverRender())
						{
							FFormatNamedArguments Args;
							Args.Add(TEXT("Blueprint"), FText::FromString(GetNameSafe(ChildBlueprint)));

							FNotificationInfo Info(FText::Format(LOCTEXT("ChildrenValidationWarning", "The child State Machine: {Blueprint} may be out of date. You may need to restart the editor play session."), Args));

							Info.bUseLargeFont = false;
							Info.ExpireDuration = 5.0f;

							TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
							if (Notification.IsValid())
							{
								Notification->SetCompletionState(SNotificationItem::CS_Fail);
							}
						}
					}
				}
			}
		}
	}
}

UEdGraph* FSMKismetCompilerContext::FindSourceGraphFromNode(UK2Node* InNode) const
{
	check(InNode);
	
	if (UEdGraph* const* FoundGraph = NodeToGraph.Find(InNode->GetFName()))
	{
		return *FoundGraph;
	}
	
	if (const UK2Node* FoundNode = Cast<UK2Node>(MessageLog.FindSourceObject(InNode)))
	{
		return FoundNode->GetGraph();
	}

	return nullptr;
}

ESMExposedFunctionExecutionType FSMKismetCompilerContext::ConfigureExposedFunctionHandler(USMGraphK2Node_RuntimeNode_Base* InRuntimeNodeBase,
	USMGraphK2Node_RuntimeNodeContainer* InRuntimeNodeContainer, FSMExposedFunctionHandler& OutHandler, TArray<FSMExposedFunctionHandler>& InOutHandlerContainer)
{
	FSMExposedFunctionHandler Handler;
	Handler.ExecutionType = InRuntimeNodeBase->GetGraphExecutionType();
	if (Handler.ExecutionType != ESMExposedFunctionExecutionType::SM_None)
	{
		if (Handler.ExecutionType == ESMExposedFunctionExecutionType::SM_NodeInstance)
		{
			USMGraphK2Node_FunctionNode_NodeInstance* NodeInstanceNode = InRuntimeNodeBase->
				GetConnectedNodeInstanceFunctionIfValidForOptimization();
			check(NodeInstanceNode);
			// Use the predefined node instance function name.
			Handler.BoundFunction = NodeInstanceNode->GetInstanceRuntimeFunctionName();
		}
		else if (Handler.ExecutionType == ESMExposedFunctionExecutionType::SM_Graph)
		{
			const FSMNode_Base* RuntimeNode = InRuntimeNodeBase->GetRunTimeNodeFromContainer(InRuntimeNodeContainer);

			// Create a unique name to identify this function when it is called during run-time.
			Handler.BoundFunction = CreateFunctionName(InRuntimeNodeBase, RuntimeNode);
		}

		check(Handler.BoundFunction != NAME_None);
	}

	if (Handler.ExecutionType != ESMExposedFunctionExecutionType::SM_None)
	{
		OutHandler = InOutHandlerContainer.Add_GetRef(Handler);
	}
	else
	{
		OutHandler = Handler;
	}
	
	return Handler.ExecutionType;
}

bool FSMNodeKismetCompiler::CanCompile(const UBlueprint* Blueprint)
{
	return Blueprint->IsA<USMNodeBlueprint>();
}

void FSMNodeKismetCompiler::Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results)
{
	FSMNodeKismetCompilerContext Compiler(Blueprint, Results, CompileOptions);
	Compiler.Compile();
}

bool FSMNodeKismetCompiler::GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass,
	UClass*& OutBlueprintGeneratedClass) const
{
	if (ParentClass && ParentClass->IsChildOf<USMNodeInstance>())
	{
		OutBlueprintClass = USMNodeBlueprint::StaticClass();
		OutBlueprintGeneratedClass = USMNodeBlueprintGeneratedClass::StaticClass();
		return true;
	}
	
	return false;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FOnNodeCompiledSignature FSMNodeKismetCompilerContext::OnNodePreCompiled;
FOnNodeCompiledSignature FSMNodeKismetCompilerContext::OnNodePostCompiled;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

FSMNodeKismetCompilerContext::FSMNodeKismetCompilerContext(UBlueprint* InBlueprint, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions) :
FKismetCompilerContext(InBlueprint, InMessageLog, InCompilerOptions)
{
	
}

void FSMNodeKismetCompilerContext::CopyTermDefaultsToDefaultObject(UObject* DefaultObject)
{
	Super::CopyTermDefaultsToDefaultObject(DefaultObject);

	if (USMNodeInstance* NodeInstance = Cast<USMNodeInstance>(DefaultObject))
	{
		// Optimize editor-time construction scripts.
		{
			const bool bHasEditorConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(NodeInstance->GetClass(), ESMExecutionEnvironment::EditorExecution);
		
			FBoolProperty* HasEditorConstructionScriptsProperty = FindFProperty<FBoolProperty>(NodeInstance->GetClass(), TEXT("bHasEditorConstructionScripts"));
			check(HasEditorConstructionScriptsProperty);

			uint8* CDOContainer = HasEditorConstructionScriptsProperty->ContainerPtrToValuePtr<uint8>(DefaultObject);
			HasEditorConstructionScriptsProperty->SetPropertyValue(CDOContainer, bHasEditorConstructionScripts);
		}
		
		// Optimize run-time construction scripts which can speed up the initialize method.
		{
			const bool bHasGameConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(NodeInstance->GetClass(), ESMExecutionEnvironment::GameExecution);
		
			FBoolProperty* HasGameConstructionScriptsProperty = FindFProperty<FBoolProperty>(NodeInstance->GetClass(), TEXT("bHasGameConstructionScripts"));
			check(HasGameConstructionScriptsProperty);

			uint8* CDOContainer = HasGameConstructionScriptsProperty->ContainerPtrToValuePtr<uint8>(DefaultObject);
			HasGameConstructionScriptsProperty->SetPropertyValue(CDOContainer, bHasGameConstructionScripts);
		}

		// Check for known thread safety issues.
		if (NodeInstance->IsInitializationThreadSafe())
		{
			for (TFieldIterator<FProperty> It(DefaultObject->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				if (LD::PropertyUtils::GetGraphPropertyFromProperty(*It))
				{
					TArray<FSMGraphProperty_Base*> GraphProperties;
					USMUtils::BlueprintPropertyToNativeProperty(*It, NodeInstance, GraphProperties);
					for (const FSMGraphProperty_Base* RuntimePropertyNode : GraphProperties)
					{
						if (!RuntimePropertyNode->IsEditorThreadSafe())
						{
							NodeInstance->SetIsEditorThreadSafe(false);
							MessageLog.Note(TEXT("Setting 'Is Editor Thread Safe' to false because this node contains the graph property @@ which is not editor thread safe."), *It);
							break;
						}
					}
				}
			}
		}

		// Cleanup unused property overrides from removed variables.
		if (FSMBlueprintEditorUtils::GetProjectEditorSettings()->bEnableVariableCustomization)
		{
			for (auto VarIt = NodeInstance->ExposedPropertyOverrides.CreateIterator(); VarIt; ++VarIt)
			{
				if (!NodeInstance->GetClass()->FindPropertyByName(VarIt->VariableName))
				{
					VarIt.RemoveCurrent();
				}
			}
		}
	}
}

void FSMNodeKismetCompilerContext::PreCompile()
{
	if (const TObjectPtr<UEdGraph>* ConstructionScriptGraph = Blueprint->FunctionGraphs.FindByPredicate([](TObjectPtr<UEdGraph> Graph)
	{
		return Graph->GetFName() == USMNodeInstance::GetConstructionScriptFunctionName();
	}))
	{
		// TODO: UE 5.1 - only run this conversion on initial update, likely by incrementing node blueprint asset version.
		TArray<UK2Node_FunctionEntry*> EntryNodes;
		(*ConstructionScriptGraph)->GetNodesOfClass(EntryNodes);
		if (EntryNodes.Num() > 0)
		{
			// Primary function entry
			if (EntryNodes[0]->IsAutomaticallyPlacedGhostNode())
			{
				EntryNodes[0]->SetEnabledState(ENodeEnabledState::Enabled);
			}

			// Parent call
			if (const UEdGraphPin* ThenPin = EntryNodes[0]->GetThenPin())
			{
				if (ThenPin->LinkedTo.Num() == 1)
				{
					if (const UEdGraphPin* NextPin = ThenPin->LinkedTo[0])
					{
						if (UEdGraphNode* OwningNode = NextPin->GetOwningNodeUnchecked())
						{
							if (OwningNode->IsAutomaticallyPlacedGhostNode())
							{
								OwningNode->SetEnabledState(ENodeEnabledState::Enabled);
							}
						}
					}
				}
			}
		}
	}
	
	FKismetCompilerContext::PreCompile();
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OnNodePreCompiled.Broadcast(*this);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	FSMCompilerDelegates::OnNodePreCompiled.Broadcast(*this);

	// Check for invalid characters. UE allows special characters in blueprint variable names but this doesn't translate
	// well to the graphs and object names representing these variables since UE doesn't fully support those characters
	// in that capacity. Only warn so existing projects that use these characters aren't broken.
	if (Blueprint && FSMBlueprintEditorUtils::GetProjectEditorSettings()->bRestrictInvalidCharacters)
	{
		for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
		{
			FString VarName = Variable.VarName.ToString();
			if (VarName.StartsWith(TEXT(" ")) || VarName.EndsWith(TEXT(" ")))
			{
				const FString WarningMessage = FString::Printf(TEXT("Variable '%s' starts or ends with a space. This may cause problems in state machine graphs, such as when duplicating the owning node."
														"\nIt is recommended to name variables without any spaces or special characters."),
					*Variable.FriendlyName);
				MessageLog.Warning(*WarningMessage);
			}
			else
			{
				FText Reason;
				if (!Variable.VarName.IsValidXName(Reason, LD_INVALID_STATENAME_CHARACTERS))
				{
					const FString WarningMessage = FString::Printf(TEXT("Variable '%s' contains an invalid character. %s. This may cause problems in state machine graphs, such as when duplicating the owning node."
															"\nIt is recommended to name variables without any spaces or special characters."),
						*Variable.FriendlyName, *Reason.ToString());
					MessageLog.Warning(*WarningMessage);
				}
			}
		}
	}
}

void FSMNodeKismetCompilerContext::PostCompile()
{
	FKismetCompilerContext::PostCompile();
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OnNodePostCompiled.Broadcast(*this);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	FSMCompilerDelegates::OnNodePostCompiled.Broadcast(*this);
}

#undef LOCTEXT_NAMESPACE
