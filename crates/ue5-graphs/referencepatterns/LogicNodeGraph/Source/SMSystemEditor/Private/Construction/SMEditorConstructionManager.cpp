// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Construction/SMEditorConstructionManager.h"

#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "Construction/SMEditorInstance.h"
#include "Graph/Nodes/SMGraphNode_AnyStateNode.h"
#include "Graph/Nodes/SMGraphNode_Base.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/SMGraph.h"
#include "Graph/SMGraphK2.h"
#include "SMSystemEditorLog.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "SMConduit.h"
#include "SMStateMachineInstance.h"
#include "SMTransition.h"

#include "BlueprintCompilationManager.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectThreadContext.h"

#define LOCTEXT_NAMESPACE "SMEditorConstructionManager"

FSMEditorConstructionManager::~FSMEditorConstructionManager()
{
	CleanupAllEditorStateMachines();
	BlueprintsPendingConstruction.Reset();
}

FSMEditorConstructionManager* FSMEditorConstructionManager::GetInstance()
{
	static TSharedPtr<FSMEditorConstructionManager> ConstructionManager;
	if (!ConstructionManager.IsValid())
	{
		ConstructionManager = MakeShareable(new FSMEditorConstructionManager());
	}
	return ConstructionManager.Get();
}

void FSMEditorConstructionManager::Tick(float DeltaTime)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FSMEditorConstructionManager::Tick"), STAT_ConstructionManagerTick, STATGROUP_LogicDriverEditor);

	TMap<TWeakObjectPtr<USMBlueprint>, FSMConstructionConfiguration> BlueprintsToConstruct = MoveTemp(BlueprintsPendingConstruction);
	for (const TTuple<TWeakObjectPtr<USMBlueprint>, FSMConstructionConfiguration>& BlueprintConfigKeyVal : BlueprintsToConstruct)
	{
		if (BlueprintConfigKeyVal.Key.IsValid())
		{
			BlueprintsToConditionallyCompile.Remove(BlueprintConfigKeyVal.Key); // Conditional Compile Optimization
			RunAllConstructionScriptsForBlueprint_Internal(BlueprintConfigKeyVal.Key.Get(), BlueprintConfigKeyVal.Value);
		}
	}

	CleanupAllEditorStateMachines();
	BlueprintsPendingConstruction.Empty();

	TMap<TWeakObjectPtr<USMBlueprint>, FSMConditionalCompileConfiguration> BlueprintsToCompile = MoveTemp(BlueprintsToConditionallyCompile);
	for (const TTuple<TWeakObjectPtr<USMBlueprint>, FSMConditionalCompileConfiguration>& BlueprintConfigKeyVal : BlueprintsToCompile)
	{
		if (BlueprintConfigKeyVal.Key.IsValid())
		{
			ConditionalCompileBlueprint_Internal(BlueprintConfigKeyVal.Key.Get(), BlueprintConfigKeyVal.Value);
		}
	}

	BlueprintsToConditionallyCompile.Empty();
}

bool FSMEditorConstructionManager::IsTickable() const
{
	return HasPendingConstructionScripts() || BlueprintsToConditionallyCompile.Num() > 0;
}

TStatId FSMEditorConstructionManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FSMEditorConstructionManager, STATGROUP_Tickables);
}

void FSMEditorConstructionManager::SetEnableConstructionScripts(const bool bEnable)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bDisableConstructionScripts = !bEnable;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool FSMEditorConstructionManager::AreConstructionScriptsEnabled() const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return !bDisableConstructionScripts;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool FSMEditorConstructionManager::HasPendingConstructionScripts() const
{
	return BlueprintsPendingConstruction.Num() > 0;
}

bool FSMEditorConstructionManager::IsRunningConstructionScripts(USMBlueprint* ForBlueprint) const
{
	return ForBlueprint ? BlueprintsBeingConstructed.Contains(ForBlueprint) : BlueprintsBeingConstructed.Num() > 0;
}

void FSMEditorConstructionManager::CleanupAllEditorStateMachines()
{
	TArray<TWeakObjectPtr<USMBlueprint>> AllBlueprints;
	EditorStateMachines.GetKeys(AllBlueprints);

	for (const TWeakObjectPtr<USMBlueprint>& Blueprint : AllBlueprints)
	{
		CleanupEditorStateMachine(Blueprint.Get());
	}
}

void FSMEditorConstructionManager::CleanupEditorStateMachine(USMBlueprint* InBlueprint)
{
	if (FSMEditorStateMachine* EditorFSM = EditorStateMachines.Find(InBlueprint))
	{
		if (EditorFSM->StateMachineEditorInstance)
		{
			EditorFSM->StateMachineEditorInstance->Shutdown();
		}

		for (FSMNode_Base* Node : EditorFSM->EditorInstanceNodeStorage)
		{
			if (USMNodeInstance* Template = Node->GetNodeInstance())
			{
				Template->SetOwningNode(nullptr, false);
			}
			for (USMNodeInstance* StackNode : Node->StackNodeInstances)
			{
				StackNode->SetOwningNode(nullptr, false);
			}
			delete Node;
		}

		EditorFSM->EditorInstanceNodeStorage.Empty();
		EditorFSM->RuntimeNodeToGraphNode.Empty();

		if (EditorFSM->StateMachineEditorInstance)
		{
			EditorFSM->StateMachineEditorInstance->RemoveFromRoot();
		}
		EditorStateMachines.Remove(InBlueprint);
	}
}

void FSMEditorConstructionManager::RunAllConstructionScriptsForBlueprintImmediately(USMBlueprint* InBlueprint, bool bCleanupEditorStateMachine)
{
	FSMConstructionConfiguration Configuration;
	Configuration.bSkipOnCompile = false;
	Configuration.bFullRefreshNeeded = true;
	RunAllConstructionScriptsForBlueprint(InBlueprint, MoveTemp(Configuration));

	if (BlueprintsPendingConstruction.Contains(InBlueprint))
	{
		const FSMConstructionConfiguration& Data = BlueprintsPendingConstruction.FindChecked(InBlueprint);
		const bool bConstructionScriptsRan = RunAllConstructionScriptsForBlueprint_Internal(InBlueprint, Data);

		if (bCleanupEditorStateMachine || !bConstructionScriptsRan)
		{
			CleanupEditorStateMachine(InBlueprint);
		}
		
		BlueprintsPendingConstruction.Remove(InBlueprint);
	}
}

void FSMEditorConstructionManager::RunAllConstructionScriptsForBlueprint(UObject* InObject, const FSMConstructionConfiguration& InConfiguration)
{
	const ESMEditorConstructionScriptProjectSetting ConstructionProjectSetting = FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting;
	if (!AreConstructionScriptsEnabled() || ConstructionProjectSetting == ESMEditorConstructionScriptProjectSetting::SM_Legacy)
	{
		LDEDITOR_LOG_INFO
		(
			TEXT("Skipping FSMEditorConstructionManager::RunAllConstructionScriptsForBlueprint, bDisableConstructionScripts: %s, ConstructionProjectSetting %d"),
			!AreConstructionScriptsEnabled() ? TEXT("true") : TEXT("false"), ConstructionProjectSetting
		);
		return;
	}

	if (USMBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintFromObject(InObject))
	{
		const TWeakObjectPtr<USMBlueprint> BlueprintPtr = MakeWeakObjectPtr(Blueprint);
		if (!(InConfiguration.bSkipOnCompile && Blueprint->bBeingCompiled) && Blueprint->bAllowEditorConstructionScripts &&
			!BlueprintsBeingConstructed.Contains(BlueprintPtr) && !BlueprintsPendingConstruction.Contains(BlueprintPtr))
		{
			// Don't add pending if currently being constructed.
			// Running the construction script itself can trigger property changes triggering this.
			BlueprintsPendingConstruction.Add(BlueprintPtr, InConfiguration);
		}
	}
	else
	{
		LDEDITOR_LOG_WARNING
		(
			TEXT("Couldn't find SMBlueprint for FSMEditorConstructionManager::RunAllConstructionScriptsForBlueprint")
		);
	}
}

void FSMEditorConstructionManager::QueueBlueprintForConditionalCompile(USMBlueprint* InBlueprint,
	const FSMConditionalCompileConfiguration& InConfiguration)
{
	if (CanConditionallyCompileBlueprint(InBlueprint))
	{
		if (InConfiguration.bCompileNow)
		{
			ConditionalCompileBlueprint_Internal(InBlueprint, InConfiguration);
			BlueprintsToConditionallyCompile.Remove(InBlueprint);
		}
		else
		{
			BlueprintsToConditionallyCompile.Add(InBlueprint, InConfiguration);
		}
	}
}

FSMEditorStateMachine& FSMEditorConstructionManager::CreateEditorStateMachine(USMBlueprint* InBlueprint)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FSMEditorConstructionManager::CreateEditorStateMachine"), STAT_CreateEditorStateMachine, STATGROUP_LogicDriverEditor);
	
	FSMEditorStateMachine& EditorFSM = EditorStateMachines.FindOrAdd(InBlueprint);
	if (!EditorFSM.StateMachineEditorInstance)
	{
		EditorFSM.StateMachineEditorInstance = NewObject<USMEditorInstance>();
		EditorFSM.StateMachineEditorInstance->GetRootStateMachine().SetNodeName(USMInstance::GetRootNodeNameDefault());

		EditorFSM.StateMachineEditorInstance->AddToRoot();
	}
	else
	{
		if (!InBlueprint->IsPossiblyDirty())
		{
			// Don't bother rebuilding if we haven't changed.
			return EditorFSM;
		}
		
		CleanupEditorStateMachine(InBlueprint);
	}

	TArray<USMGraphNode_Base*> Nodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested(InBlueprint, Nodes);

	// Make sure all nodes are converted to latest version since this will fire before Pre-Compile version updates will.
	for (USMGraphNode_Base* Node : Nodes)
	{
		Node->ConvertToCurrentVersion();
	}

	FSMStateMachine& RootStateMachine = EditorFSM.StateMachineEditorInstance->GetRootStateMachine();

	// Setup the root node instance.
	SetupRootStateMachine(RootStateMachine, InBlueprint);

	ConstructEditorStateMachine
	(
		FSMBlueprintEditorUtils::GetRootStateMachineGraph(InBlueprint),
		RootStateMachine, EditorFSM
	);

	EditorFSM.StateMachineEditorInstance->Initialize(NewObject<USMEditorContext>());
	return EditorFSM;
}

bool FSMEditorConstructionManager::TryGetEditorStateMachine(USMBlueprint* InBlueprint,
	FSMEditorStateMachine& OutEditorStateMachine)
{
	if (const FSMEditorStateMachine* EditorStateMachine = EditorStateMachines.Find(InBlueprint))
	{
		OutEditorStateMachine = *EditorStateMachine;
		return true;
	}

	return false;
}

void FSMEditorConstructionManager::SetAllowConstructionScriptsOnLoad(bool bAllow)
{
	bAllowConstructionScriptsOnLoad = bAllow;
}

bool FSMEditorConstructionManager::AreConstructionScriptsAllowedOnLoad() const
{
	return FSMBlueprintEditorUtils::GetProjectEditorSettings()->bRunConstructionScriptsOnLoad && bAllowConstructionScriptsOnLoad;
}

void FSMEditorConstructionManager::SetAllowConstructionScriptsOnLoadForBlueprint(const FString& InPath, bool bValue)
{
	if (bValue)
	{
		BlueprintsToSkipConstructionScriptsOnLoad.Remove(InPath);
	}
	else
	{
		BlueprintsToSkipConstructionScriptsOnLoad.Add(InPath);
	}
}

void FSMEditorConstructionManager::ConstructEditorStateMachine(USMGraph* InGraph, FSMStateMachine& StateMachineOut,
                                                               FSMEditorStateMachine& EditorStateMachineInOut)
{
	if (!InGraph)
	{
		return;
	}
	
	USMGraphNode_StateMachineEntryNode* EntryNode = InGraph->GetEntryNode();
	if (!ensure(EntryNode))
	{
		// TODO: check instead.
		return;
	}
			
	TArray<USMGraphNode_StateNodeBase*> InitialStateNodes;
	EntryNode->GetAllOutputNodesAs(InitialStateNodes);

	TSet<USMGraphNode_StateNodeBase*> InitialStatesSet(InitialStateNodes);
	
	UBlueprint* ThisBlueprint = FSMBlueprintEditorUtils::FindBlueprintForGraph(InGraph);
	TMap<FGuid, FSMTransition*> AllTransitions;
	
	TSet<UEdGraphNode*> GraphNodes;
	for (const TObjectPtr<UEdGraphNode>& GraphNode : InGraph->Nodes)
	{
		USMGraphNode_StateNodeBase* GraphStateNodeBaseSelected = nullptr;
		FSMState_Base* RuntimeStateSelected = nullptr;
		if (USMGraphNode_StateMachineStateNode* StateMachineNode = Cast<USMGraphNode_StateMachineStateNode>(GraphNode))
		{
			if (StateMachineNode->IsStateMachineReference())
			{
				if (USMBlueprint* ReferenceBlueprint = StateMachineNode->GetStateMachineReference())
				{
					if (ThisBlueprint == ReferenceBlueprint)
					{
						// Circular reference?
						continue;
					}
					
					GraphStateNodeBaseSelected = StateMachineNode;

					// Container in this SM pointing to the referenced instance.
					FSMStateMachine* ContainerStateMachine = new FSMStateMachine();
					RuntimeStateSelected = ContainerStateMachine;

					USMEditorInstance* ReferenceInstance = NewObject<USMEditorInstance>();
					FSMStateMachine* ReferenceRootStateMachine = &ReferenceInstance->GetRootStateMachine();

					ContainerStateMachine->SetInstanceReference(ReferenceInstance);

					// The node guid will either be the state machine reference root node or
					// adjusted in the case of a duplicate reference. Reference paths are always */Container/Root/*
					bool bIsRuntimeGuid = false;
					FGuid ContainerGuid = StateMachineNode->GetCorrectNodeGuid(&bIsRuntimeGuid);
					if (ensure(bIsRuntimeGuid))
					{
						ContainerStateMachine->SetNodeGuid(ContainerGuid);
					}

					GraphStateNodeBaseSelected->SetRuntimeDefaults(*RuntimeStateSelected);
					SetupRootStateMachine(*ReferenceRootStateMachine, ReferenceBlueprint);
					ConstructEditorStateMachine
					(
						FSMBlueprintEditorUtils::GetRootStateMachineGraph(ReferenceBlueprint),
						*ReferenceRootStateMachine, EditorStateMachineInOut
					);
				}
			}
			else if (USMGraph* NestedFSMGraph = Cast<USMGraph>(StateMachineNode->GetBoundGraph()))
			{
				if (USMGraphNode_StateMachineEntryNode* NestedEntryNode = NestedFSMGraph->GetEntryNode())
				{
					GraphStateNodeBaseSelected = StateMachineNode;
					GraphStateNodeBaseSelected->SetRuntimeDefaults(NestedEntryNode->StateMachineNode);
					RuntimeStateSelected = new FSMStateMachine(NestedEntryNode->StateMachineNode);
					ConstructEditorStateMachine(NestedFSMGraph, *static_cast<FSMStateMachine*>(RuntimeStateSelected), EditorStateMachineInOut);
				}
			}
		}
		else if (USMGraphNode_StateNode* StateNode = Cast<USMGraphNode_StateNode>(GraphNode))
		{
			if (USMGraphK2* StateGraph = Cast<USMGraphK2>(StateNode->GetBoundGraph()))
			{
				if (FSMNode_Base* Node = StateGraph->GetRuntimeNode())
				{
					GraphStateNodeBaseSelected = StateNode;
					StateNode->SetRuntimeDefaults(*static_cast<FSMState*>(Node));
					RuntimeStateSelected = new FSMState(*static_cast<FSMState*>(Node));
				}
			}
		}
		else if (USMGraphNode_ConduitNode* ConduitNode = Cast<USMGraphNode_ConduitNode>(GraphNode))
		{
			if (USMGraphK2* StateGraph = Cast<USMGraphK2>(ConduitNode->GetBoundGraph()))
			{
				if (FSMNode_Base* Node = StateGraph->GetRuntimeNode())
				{
					GraphStateNodeBaseSelected = ConduitNode;
					ConduitNode->SetRuntimeDefaults(*static_cast<FSMState_Base*>(Node));
					RuntimeStateSelected = new FSMConduit(*static_cast<FSMConduit*>(Node));
				}
			}
		}
		else if (USMGraphNode_AnyStateNode* AnyStateNode = Cast<USMGraphNode_AnyStateNode>(GraphNode))
		{
			GraphStateNodeBaseSelected = AnyStateNode;
		}
		
		auto GetOrCopyTransition = [&](FSMTransition* InTransition, USMGraphNode_TransitionEdge* TransitionEdge) -> FSMTransition*
		{
			if (FSMTransition* Transition = AllTransitions.FindRef(InTransition->GetNodeGuid()))
			{
				return Transition;
			}

			TransitionEdge->SetRuntimeDefaults(*InTransition);
			FSMTransition* NewTransition = new FSMTransition(*InTransition);
			AllTransitions.Add(NewTransition->GetNodeGuid(), NewTransition);
			
			NewTransition->NodeInstance = TransitionEdge->GetNodeTemplate();
			if (NewTransition->NodeInstance)
			{
				NewTransition->NodeInstance->SetOwningNode(NewTransition, true);
			}
			StateMachineOut.AddTransition(NewTransition);
			EditorStateMachineInOut.EditorInstanceNodeStorage.Add(NewTransition);
			EditorStateMachineInOut.RuntimeNodeToGraphNode.Add(NewTransition, TransitionEdge);
			return NewTransition;
		};

		if (GraphStateNodeBaseSelected)
		{
			if (RuntimeStateSelected)
			{
				RuntimeStateSelected->NodeInstance = GraphStateNodeBaseSelected->GetNodeTemplate();
				if (RuntimeStateSelected->NodeInstance)
				{
					RuntimeStateSelected->NodeInstance->SetOwningNode(RuntimeStateSelected, true);
				}

				if (USMGraphNode_StateNode* GraphStateNode = Cast<USMGraphNode_StateNode>(GraphStateNodeBaseSelected))
				{
					// State stack.
					for (const FStateStackContainer& StackTemplate : GraphStateNode->StateStack)
					{
						if (StackTemplate.NodeStackInstanceTemplate)
						{
							StackTemplate.NodeStackInstanceTemplate->SetOwningNode(RuntimeStateSelected, true);
							RuntimeStateSelected->StackNodeInstances.Add(StackTemplate.NodeStackInstanceTemplate);
						}
					}
				}
			
				StateMachineOut.AddState(RuntimeStateSelected);
				EditorStateMachineInOut.EditorInstanceNodeStorage.Add(RuntimeStateSelected);
				EditorStateMachineInOut.RuntimeNodeToGraphNode.Add(RuntimeStateSelected, GraphStateNodeBaseSelected);

				if (InitialStatesSet.Contains(GraphStateNodeBaseSelected))
				{
					RuntimeStateSelected->bIsRootNode = true;
					StateMachineOut.AddInitialState(RuntimeStateSelected);
				}
			}

			// Input Transitions
			if (RuntimeStateSelected)
			{
				TArray<USMGraphNode_TransitionEdge*> Transitions;
				GraphStateNodeBaseSelected->GetInputTransitions(Transitions);

				for (USMGraphNode_TransitionEdge* Transition : Transitions)
				{
					if (USMGraphNode_TransitionEdge* PrimaryTransition = Transition->GetPrimaryReroutedTransition())
					{
						if (USMGraphK2* TransitionGraph = Cast<USMGraphK2>(PrimaryTransition->GetBoundGraph()))
						{
							if (FSMNode_Base* Node = TransitionGraph->GetRuntimeNode())
							{
								FSMTransition* RuntimeTransition = GetOrCopyTransition(static_cast<FSMTransition*>(Node), PrimaryTransition);
								RuntimeTransition->SetToState(RuntimeStateSelected);
							}
						}
					}
				}
			}

			// Output transitions -- These need to be processed even without a runtime state, such as from an AnyState.
			{
				TArray<USMGraphNode_TransitionEdge*> Transitions;
				GraphStateNodeBaseSelected->GetOutputTransitions(Transitions);

				for (USMGraphNode_TransitionEdge* Transition : Transitions)
				{
					if (USMGraphNode_TransitionEdge* PrimaryTransition = Transition->GetPrimaryReroutedTransition())
					{
						if (USMGraphK2* TransitionGraph = Cast<USMGraphK2>(PrimaryTransition->GetBoundGraph()))
						{
							if (FSMNode_Base* Node = TransitionGraph->GetRuntimeNode())
							{
								FSMTransition* RuntimeTransition = GetOrCopyTransition(static_cast<FSMTransition*>(Node), PrimaryTransition);
								if (RuntimeStateSelected)
								{
									RuntimeTransition->SetFromState(RuntimeStateSelected);
								}
								
								// Transition stack.
								for (const FTransitionStackContainer& StackTemplate : PrimaryTransition->TransitionStack)
								{
									if (StackTemplate.NodeStackInstanceTemplate)
									{
										StackTemplate.NodeStackInstanceTemplate->SetOwningNode(RuntimeTransition, true);
										RuntimeTransition->StackNodeInstances.Add(StackTemplate.NodeStackInstanceTemplate);
									}
								}
							}
						}
					}
				}
			}

			if (RuntimeStateSelected)
			{
				RuntimeStateSelected->SortTransitions();
			}
		}
	}
}

void FSMEditorConstructionManager::SetupRootStateMachine(FSMStateMachine& StateMachineInOut, const USMBlueprint* InBlueprint) const
{
	StateMachineInOut.NodeInstance = nullptr;

	// Try not to use skeleton class, it probably won't have the updated node class value.
	if (const USMBlueprintGeneratedClass* BPGC = Cast<USMBlueprintGeneratedClass>(InBlueprint->GeneratedClass ?
		InBlueprint->GeneratedClass :
		InBlueprint->SkeletonGeneratedClass))
	{
		// Locate the proper root Guid.
		if (const USMGraph* RootSMGraph = FSMBlueprintEditorUtils::GetRootStateMachineGraph(const_cast<USMBlueprint*>(InBlueprint)))
		{
			if (const FSMNode_Base* RootStateMachineNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(RootSMGraph))
			{
				StateMachineInOut.SetNodeGuid(RootStateMachineNode->GetNodeGuid());
			}
		}

		const UClass* StateMachineNodeClass = nullptr;
		if (const USMInstance* CDO = Cast<USMInstance>(BPGC->ClassDefaultObject))
		{
			StateMachineNodeClass = CDO->GetRootStateMachineNodeClass();
		}

		if (!StateMachineNodeClass)
		{
			// This could be during a compile where the CDO is cleared out. The compilation manager keeps track of
			// old CDOs and can recover default property values.

			FProperty* Property = FindFProperty<FProperty>(BPGC, USMInstance::GetStateMachineClassPropertyName());
			check(Property);
			Property = FBlueprintEditorUtils::GetMostUpToDateProperty(Property);

			FString DefaultValue;
			if (FBlueprintCompilationManager::GetDefaultValue(BPGC, Property, DefaultValue))
			{
				if (!DefaultValue.IsEmpty() && DefaultValue != TEXT("None"))
				{
					const TSoftClassPtr<USMStateMachineInstance> StateMachineNodeClassSoftPtr(DefaultValue);
					StateMachineNodeClass = StateMachineNodeClassSoftPtr.LoadSynchronous();
				}
			}
		}

		// If the class layout is changing it's not safe to instantiate an object. This could happen during load.
		// Testing has shown that even if a pre-compile validation script checks the node class it still works as
		// expected.
		const bool bUseCustomNodeClass = StateMachineNodeClass && !StateMachineNodeClass->bLayoutChanging;

		StateMachineInOut.NodeInstance = NewObject<USMStateMachineInstance>(GetTransientPackage(),
			bUseCustomNodeClass ? StateMachineNodeClass : USMStateMachineInstance::StaticClass());
		StateMachineInOut.NodeInstance->SetOwningNode(&StateMachineInOut, true);
	}
}

bool FSMEditorConstructionManager::CanConditionallyCompileBlueprint(USMBlueprint* InBlueprint) const
{
	return InBlueprint && !InBlueprint->bBeingCompiled && !InBlueprint->bQueuedForCompilation &&
		!IsRunningConstructionScripts(InBlueprint) && !InBlueprint->bPreventConditionalCompile;
}

bool FSMEditorConstructionManager::RunAllConstructionScriptsForBlueprint_Internal(USMBlueprint* InBlueprint, const FSMConstructionConfiguration& InConfigurationData)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FSMEditorConstructionManager::RunAllConstructionScriptsForBlueprint"), STAT_RunAllBlueprintConstructionScripts, STATGROUP_LogicDriverEditor);

	check(InBlueprint);

	if ((!AreConstructionScriptsAllowedOnLoad() && (InConfigurationData.bFromLoad || InBlueprint->bIsRegeneratingOnLoad)) ||
		BlueprintsToSkipConstructionScriptsOnLoad.Contains(FSoftObjectPath(InBlueprint).GetAssetPathString()))
	{
		return false;
	}

	const ESMEditorConstructionScriptProjectSetting ConstructionProjectSetting = FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting;
	if (!AreConstructionScriptsEnabled() || ConstructionProjectSetting == ESMEditorConstructionScriptProjectSetting::SM_Legacy)
	{
		LDEDITOR_LOG_INFO
		(
			TEXT("Skipping FSMEditorConstructionManager::RunAllConstructionScriptsForBlueprint_Internal, bDisableConstructionScripts: %s, ConstructionProjectSetting %d"),
			!AreConstructionScriptsEnabled() ? TEXT("true") : TEXT("false"), ConstructionProjectSetting
		);
		return false;
	}

	// Group construction into a transaction that doesn't actually transact in history. This allows them to run during
	// undo/redo without interfering with history.
	FScopedTransaction Transaction(LOCTEXT("RunConstructionScripts", "Run Construction Scripts"), false);

	const TWeakObjectPtr<USMBlueprint> BlueprintWeakPtr = InBlueprint;
	BlueprintsBeingConstructed.Add(BlueprintWeakPtr);

	bool bSetEditorLoadPackage = false;
	if (InConfigurationData.bFromLoad && InConfigurationData.bDoNotDirty && !GIsEditorLoadingPackage)
	{
		// At this point we are deferred from the initial load where GIsEditorLoadingPackage would have been true.
		// Force set this so the engine won't prompt to checkout packages which won't be dirtied. ActorDeferredScriptManager
		// ends up doing something similar to prevent the level package from being dirtied from actor construction scripts.
		GIsEditorLoadingPackage = true;
		bSetEditorLoadPackage = true;
	}

	const bool bWasDirty = InBlueprint->GetPackage()->IsDirty();

	TArray<USMGraphNode_Base*> GraphNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested(InBlueprint, GraphNodes);

	// Make sure all pin values have been recorded, otherwise a value just entered by the user may not be reflected
	// if another node attempts to read its values.
	for (USMGraphNode_Base* GraphNode : GraphNodes)
	{
		GraphNode->SetGraphPropertyDefaultsFromPins();
	}
	
	const FSMEditorStateMachine& EditorStateMachine = CreateEditorStateMachine(InBlueprint);

	// Run the construction script for our root node.
	if (USMNodeInstance* NodeInstance = EditorStateMachine.StateMachineEditorInstance->GetRootStateMachine().GetNodeInstance())
	{
		if (NodeInstance->GetClass() != USMStateMachineInstance::StaticClass() &&
			!FUObjectThreadContext::Get().IsRoutingPostLoad)
		{
			NodeInstance->RunConstructionScript();
		}
	}
	
	// Perform any extra passes if configured to do so. The main pass runs after and requests a slate refresh for
	// each node.
	for (uint8 Pass = 1; Pass < InBlueprint->NumConstructionScriptPasses; ++Pass)
	{
		for (USMGraphNode_Base* GraphNode : GraphNodes)
		{
			if (GraphNode->CanRunConstructionScripts())
			{
				GraphNode->RunAllConstructionScripts();
			}
		}
	}

	// Perform main/extra pass -- There was a bug that caused construction scripts to be fired a second time.
	// This is fixed, but the extra pass allows additional behavior on standard cs, such as the changing a nested FSM node property and having the owning
	// FSM construction script read it. Without a second pass it wouldn't update until a manual compile was initiated.

	// This is being changed so that only 1 pass occurs by default. The performance gain outweighs the case of needing to traverse
	// sub state machine nodes reading values that were modified by other construction scripts.
	
	for (USMGraphNode_Base* GraphNode : GraphNodes)
	{
		if (GraphNode->CanRunConstructionScripts())
		{
			GraphNode->RunAllConstructionScripts();
			GraphNode->RequestSlateRefresh(InConfigurationData.bFullRefreshNeeded);
		}
	}

	if (bSetEditorLoadPackage)
	{
		GIsEditorLoadingPackage = false;
	}

	// Necessary for listeners like SBlueprintDiff since construction scripts may invalidate pins that are in use.
	// Limit to onload otherwise the entire graph gets refreshed on any change, even interactive ones.
	if (!InBlueprint->bBeingCompiled && InConfigurationData.bFromLoad)
	{
		InBlueprint->BroadcastChanged();
	}

	if (InConfigurationData.bDoNotDirty && !bWasDirty && !InBlueprint->IsPossiblyDirty())
	{
		// Compile status is clean so the asset shouldn't actually be marked dirty.
		InBlueprint->GetPackage()->ClearDirtyFlag();
	}

	BlueprintsBeingConstructed.Remove(BlueprintWeakPtr);
	return true;
}

void FSMEditorConstructionManager::ConditionalCompileBlueprint_Internal(USMBlueprint* InBlueprint,
	const FSMConditionalCompileConfiguration& InConfiguration)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FSMEditorConstructionManager::ConditionalCompileBlueprint"), STAT_ConditionalCompileBlueprint, STATGROUP_LogicDriverEditor);

	if (CanConditionallyCompileBlueprint(InBlueprint))
	{
		FSMBlueprintEditorUtils::OnBlueprintPreConditionallyCompiledEvent.Broadcast(InBlueprint, InConfiguration.bUpdateDependencies, InConfiguration.bRecreateGraphProperties);

		if (InConfiguration.bRecreateGraphProperties)
		{
			TArray<USMGraphNode_Base*> GraphNodes;
			FBlueprintEditorUtils::GetAllNodesOfClass<USMGraphNode_Base>(InBlueprint, GraphNodes);

			for (USMGraphNode_Base* GraphNode : GraphNodes)
			{
				GraphNode->ForceRecreateProperties();
			}
		}

		// Conditional compile could have been called from a node class, when the BP is structurally modified below
		// UE will attempt a bytecode cleanup of children functions, which will no longer be valid in the case the
		// child calls the parent graph.
		FSMBlueprintEditorUtils::CleanupInvalidParentFunctionsInChildren(InBlueprint);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);

		if (InConfiguration.bUpdateDependencies)
		{
			FBlueprintEditorUtils::EnsureCachedDependenciesUpToDate(InBlueprint);
		}

		FSMBlueprintEditorUtils::OnBlueprintPostConditionallyCompiledEvent.Broadcast(InBlueprint, InConfiguration.bUpdateDependencies, InConfiguration.bRecreateGraphProperties);
	}
}

#undef LOCTEXT_NAMESPACE
