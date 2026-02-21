// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTestHelpers.h"
#include "SMTestContext.h"

#include "Blueprints/SMBlueprintFactory.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_StateReadNodes.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_IntermediateRootStateMachineNodes.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateEndNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateEntryNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateMachineSelectNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateUpdateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionEnteredNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionInitializedNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionResultNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionShutdownNode.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMGraph.h"
#include "Graph/SMIntermediateGraph.h"
#include "Graph/SMPropertyGraph.h"
#include "Graph/SMStateGraph.h"
#include "Graph/SMTransitionGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMNodeInstanceUtils.h"
#include "Utilities/SMPropertyUtils.h"
#include "Utilities/SMVersionUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"

#include "SMUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "BlueprintEditorSettings.h"
#include "EdGraphUtilities.h"
#include "Editor/UnrealEdEngine.h"
#include "HAL/FileManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "UnrealEdGlobals.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

bool FAssetHandler::CreateAsset()
{
	const FString PackageName = GamePath + Name;

	UPackage* AssetPackage = CreatePackage(*PackageName);
	Package = AssetPackage;

	const EObjectFlags Flags = RF_Public | RF_Standalone;

	Object = Factory->FactoryCreateNew(Class, Package, FName(*Name), Flags, nullptr, GWarn);
	if (Object)
	{
		Object->AddToRoot();
		FAssetRegistryModule::AssetCreated(Object);

		Package->MarkPackageDirty();

		return true;
	}

	return false;
}

bool FAssetHandler::SaveAsset()
{
	if (Package && Object)
	{
		Package->SetDirtyFlag(true);
		const FString PackagePath = FString::Printf(TEXT("%s%s"), *GamePath, *Name);

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Standalone;
		Args.Error = GError;
		Args.bForceByteSwapping = false;
		Args.bWarnOfLongFilename = true;
		Args.SaveFlags = SAVE_NoError;
		
		return UPackage::SavePackage(Package,
			nullptr,
			*FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension()),
			Args);
	}

	return false;
}

bool FAssetHandler::LoadAsset()
{
	Package = nullptr;
	Object = nullptr;

	const FString PackagePath = FString::Printf(TEXT("%s%s"), *GamePath, *Name);

	Package = LoadPackage(nullptr, *FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension()), LOAD_None);
	if (!Package)
	{
		return false;
	}

	Object = LoadObject<UObject>(Package, *Name);

	return Object != nullptr;
}

bool FAssetHandler::DeleteAsset()
{
	bool bSuccessful = false;

	if (Object)
	{
		Object->RemoveFromRoot();
		bSuccessful = ObjectTools::DeleteSingleObject(Object, false);

		// Try force delete.
		if (!bSuccessful)
		{
			TArray<UObject*> Objects = TArray<UObject*>();
			Objects.Add(Object);
			bSuccessful = ObjectTools::ForceDeleteObjects(Objects, false) > 0;
		}

		// Delete the package.
		if (bSuccessful)
		{
			FString PackageFilename;
			if (FPackageName::DoesPackageExist(Package->GetName(), &PackageFilename))
			{
				TArray<UPackage*> PackagesToDelete;
				PackagesToDelete.Add(Package);

				ObjectTools::CleanupAfterSuccessfulDelete(PackagesToDelete, false);
			}
		}
	}
	return bSuccessful;
}

bool FAssetHandler::UnloadAsset()
{
	bool bResult = false;
	if (Object)
	{
		Object->ConditionalBeginDestroy();
	}
	
	if (Package)
	{
		TArray<UPackage*> Packages{ Package };
		bResult = UPackageTools::UnloadPackages(Packages);
	}
	
	Package = nullptr;
	Object = nullptr;

	GEngine->ForceGarbageCollection(true);

	return bResult;
}

bool FAssetHandler::ReloadAsset()
{
	check(Package);
	TArray<UPackage*> Packages;
	Packages.Add(Package);

	const FString PackageName = Package->GetName();

	if (!UPackageTools::ReloadPackages(Packages))
	{
		return false;
	}

	Package = FindPackage(nullptr, *PackageName);
	check(Package);

	Object = LoadObject<UObject>(Package, *Name);

	return true;
}

bool FAssetHandler::CreateAsset(FAutomationTestBase* Test)
{
	const bool bResult = CreateAsset();
	Test->TestTrue("Asset should have created", bResult);
	return bResult;
}

bool FAssetHandler::SaveAsset(FAutomationTestBase* Test)
{
	const bool bResult = SaveAsset();
	Test->TestTrue("Asset should have saved", bResult);
	return bResult;
}

bool FAssetHandler::LoadAsset(FAutomationTestBase* Test)
{
	const bool bResult = LoadAsset();
	Test->TestTrue("Asset should have loaded", bResult);
	return bResult;
}

bool FAssetHandler::DeleteAsset(FAutomationTestBase* Test)
{
	const bool bResult = DeleteAsset();
	Test->TestTrue("Asset should have been deleted", bResult);
	return bResult;
}

bool FAssetHandler::UnloadAsset(FAutomationTestBase* Test)
{
	const bool bResult = UnloadAsset();
	Test->TestTrue("Asset should have been unloaded", bResult);
	return bResult;
}

bool FAssetHandler::ReloadAsset(FAutomationTestBase* Test)
{
	const bool bResult = ReloadAsset();
	Test->TestTrue("Asset should have been reloaded", bResult);
	return bResult;
}

USMInstance* TestHelpers::CreateNewStateMachineInstanceFromBP(FAutomationTestBase* Test, USMBlueprint* Blueprint, USMTestContext* Context, bool bTestNodeMap)
{
	USMInstance* StateMachineInstance = USMBlueprintUtils::CreateStateMachineInstance(Blueprint->GetGeneratedClass(), Context, false);
	Test->TestNotNull("State Machine should not be null", StateMachineInstance);
	Test->TestFalse("State Machine should not be initialized", StateMachineInstance->IsInitialized());

	ULambdaWrapper* LambdaWrapper = NewObject<ULambdaWrapper>(GetTransientPackage());
	StateMachineInstance->OnStateMachineInitializedEvent.AddUniqueDynamic(LambdaWrapper, &ULambdaWrapper::OnInitialize);

	StateMachineInstance->Initialize(Context);

	Test->TestTrue("State Machine should be initialized", StateMachineInstance->IsInitialized());
	Test->TestEqual("Initialize event hit", LambdaWrapper->OnInitializeHit.Count, 1);

	// Verify names.
	TArray<USMInstance*> AllInstances = StateMachineInstance->GetAllReferencedInstances(true);
	AllInstances.Add(StateMachineInstance);

	for (USMInstance* Instance : AllInstances)
	{
		USMStateMachineInstance* StateMachineClassInstance = CastChecked<USMStateMachineInstance>(Instance->GetRootStateMachine().GetOrCreateNodeInstance());
		const FString& NodeName = StateMachineClassInstance->GetNodeName();

		USMInstance* ReferenceOwner = Instance->GetReferenceOwner();
		if (ReferenceOwner == nullptr)
		{
			Test->TestTrue("The correct name is assigned to the root instance", NodeName == USMInstance::GetRootNodeNameDefault());
		}
		else
		{
			check(Instance->GetRootStateMachine().GetReferencedByStateMachine());
			const FString& RealName = Instance->GetRootStateMachine().GetReferencedByStateMachine()->GetNodeName();
			Test->TestEqual("The correct name is assigned to the reference", NodeName, RealName);
		}
	}
	
	TArray<USMStateInstance_Base*> StateInstances;
	StateMachineInstance->GetAllStateInstances(StateInstances);

	int32 ConstructionScriptsCount = 0;
	int32 TotalConstructionScriptStates = 0;
	for (USMStateInstance_Base* StateInstance : StateInstances)
	{
		if (USMStateTestInstance* TestStateInstance = Cast<USMStateTestInstance>(StateInstance))
		{
			TotalConstructionScriptStates++;
			ConstructionScriptsCount += TestStateInstance->ConstructionScriptHit.Count;

			// Test instance always matches CDO.
			{
				FBoolProperty* HasGameConstructionScriptsProperty = FindFProperty<FBoolProperty>(TestStateInstance->GetClass(), TEXT("bHasGameConstructionScripts"));
				check(HasGameConstructionScriptsProperty);

				uint8* InstanceContainer = HasGameConstructionScriptsProperty->ContainerPtrToValuePtr<uint8>(TestStateInstance);
				bool InstanceValue = HasGameConstructionScriptsProperty->GetPropertyValue(InstanceContainer);

				uint8* CDOContainer = HasGameConstructionScriptsProperty->ContainerPtrToValuePtr<uint8>(TestStateInstance->GetClass()->GetDefaultObject());
				bool CDOValue = HasGameConstructionScriptsProperty->GetPropertyValue(CDOContainer);

				Test->TestEqual("bHasGameConstructionScripts instance matches CDO", InstanceValue, CDOValue);
			}
		}
	}

	Test->TestEqual("Construction scripts hit correct amount of times", ConstructionScriptsCount, TotalConstructionScriptStates);
	
	if (bTestNodeMap)
	{
		// Nodes mapped via generated guid.
		const TMap<FGuid, FSMNode_Base*>& NodeMap = StateMachineInstance->GetNodeMap();

		// Search for actual nodes.
		FSMStateMachine::FGetNodeArgs Args;
		Args.bIncludeNested = true;
		const TArray<FSMNode_Base*> Nodes = StateMachineInstance->GetRootStateMachine().GetAllNodes(MoveTemp(Args));
		
		TMap<USMInstance*, int32> ReferenceInstanceCount;
		TArray<FSMStateMachine*> ReferencedStateMachines = StateMachineInstance->GetStateMachinesWithReferences(true);
		TArray<USMInstance*> TotalReferences;

		// Only valid if reference is actually reused.
		bool bHasReusedInstances = false;
		// Total amount reused not including base instance.
		int32 TotalReused = 0;
		for (FSMStateMachine* SM : ReferencedStateMachines)
		{
			TotalReferences.Add(SM->GetInstanceReference());

			int32& Count = ReferenceInstanceCount.FindOrAdd(SM->GetInstanceReference());
			if (++Count > 1)
			{
				bHasReusedInstances = true;
				TotalReused++;
			}
		}
		const TSet<USMInstance*> UniqueReferences(TotalReferences);

		Test->TestFalse("References can no longer be reused", bHasReusedInstances);

		Test->TestEqual("References are not reused and unique", UniqueReferences.Num(), TotalReferences.Num());

		TSet<FSMNode_Base*> UniqueNodes(Nodes);
		Test->TestEqual("All nodes unique", UniqueNodes.Num(), Nodes.Num());
		
		// They should be the same less 1 because the root state machine used to initiate the search isn't counted.
		Test->TestEqual("Calculated node hashes matches node count", NodeMap.Num(), Nodes.Num() + 1);
	}
	
	return StateMachineInstance;
}

USMInstance* TestHelpers::CompileAndCreateStateMachineInstanceFromBP(USMBlueprint* Blueprint, bool bInitialize)
{
	check(Blueprint);
	
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	// Create a context we will run the state machine for.
	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* StateMachineInstance = USMBlueprintUtils::CreateStateMachineInstance(Blueprint->GetGeneratedClass(), Context, bInitialize);
	
	return StateMachineInstance;
}

FAssetHandler TestHelpers::ConstructNewStateMachineAsset(TSubclassOf<USMInstance> InParentClass)
{
	const FString AssetName(FGuid::NewGuid().ToString());

	USMBlueprintFactory* Factory = NewObject<USMBlueprintFactory>();
	if (InParentClass)
	{
		Factory->SetParentClass(InParentClass);
	}
	return FAssetHandler(AssetName, USMBlueprint::StaticClass(), Factory);
}

FAssetHandler TestHelpers::CreateAssetFromBlueprint(UBlueprint* InBlueprint)
{
	FString ReferencedPath = InBlueprint->GetPathName();
	FAssetHandler ReferencedAsset(InBlueprint->GetName(), USMBlueprint::StaticClass(), NewObject<USMBlueprintFactory>(), &ReferencedPath);
	ReferencedAsset.Object = InBlueprint;

	UPackage* Package = FAssetData(InBlueprint).GetPackage();
	ReferencedAsset.Package = Package;
	return MoveTemp(ReferencedAsset);
}

bool TestHelpers::TryCreateNewStateMachineAsset(FAutomationTestBase* Test, FAssetHandler& NewAsset, bool Save, TSubclassOf<USMInstance> ParentClass)
{
	NewAsset = ConstructNewStateMachineAsset(ParentClass);

	if (!NewAsset.CreateAsset(Test))
	{
		return false;
	}

	if (!Save)
	{
		return true;
	}

	if (!NewAsset.SaveAsset(Test))
	{
		return false;
	}

	return true;
}

void TestHelpers::ValidateNewStateMachineBlueprint(FAutomationTestBase* Test, USMBlueprint* NewBP)
{
	// Verify event graph.
	{
		TObjectPtr<UEdGraph>* FoundGraph = NewBP->UbergraphPages.FindByPredicate([&](UEdGraph* Graph)
		{
			return Graph->GetFName() == USMGraphK2Schema::GN_EventGraph;
		});
		Test->TestNotNull("Event Graph should exist", FoundGraph);
	}

	// Verify state machine graph.
	{
		USMGraphK2* TopLevelGraph = FSMBlueprintEditorUtils::GetTopLevelStateMachineGraph(NewBP);
		Test->TestNotNull("State Machine Graph should exist", TopLevelGraph);

		TArray<USMGraphK2Node_StateMachineSelectNode*> SelectNodes;
		TopLevelGraph->GetNodesOfClass<USMGraphK2Node_StateMachineSelectNode>(SelectNodes);

		Test->TestTrue("One state machine select node should exist", SelectNodes.Num() == 1);

		USMGraphK2Node_StateMachineSelectNode* SelectNode = SelectNodes[0];
		Test->TestTrue("Select node should have a single default wire", SelectNode->GetInputPin()->LinkedTo.Num() == 1);
		Test->TestTrue("SelectNode should be wired to a state machine definition node", SelectNode->GetInputPin()->LinkedTo[0]->GetOwningNode()->IsA<USMGraphK2Node_StateMachineNode>());
	}
}

FAssetHandler TestHelpers::ConstructNewNodeAsset(UClass* NodeClass)
{
	const FString AssetName(FGuid::NewGuid().ToString());

	USMNodeBlueprintFactory* Factory = NewObject<USMNodeBlueprintFactory>();
	Factory->SetParentClass(NodeClass);
	return FAssetHandler(AssetName, USMNodeBlueprint::StaticClass(), Factory);
}

bool TestHelpers::TryCreateNewNodeAsset(FAutomationTestBase* Test, FAssetHandler& NewAsset, UClass* NodeClass, bool Save)
{
	NewAsset = ConstructNewNodeAsset(NodeClass);

	if (!NewAsset.CreateAsset(Test))
	{
		return false;
	}

	USMNodeBlueprint* NodeBlueprint = NewAsset.GetObjectAs<USMNodeBlueprint>();
	FSMVersionUtils::IsAssetUpToDate(NodeBlueprint);
	
	// Test construction graph.
	Test->TestTrue("Construction graph created", NodeBlueprint->FunctionGraphs.Num() >= 1);
	
	TArray<UK2Node*> ConstructionNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<UK2Node>(NodeBlueprint->FunctionGraphs[0], ConstructionNodes);

	Test->TestEqual("Construction nodes created", ConstructionNodes.Num(), 3); // Entry, parent call, execution environment.

	int32 NodesChecked = 0;
	for (UK2Node* Node : ConstructionNodes)
	{
		if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
		{
			NodesChecked++;
			Test->TestEqual("Construction entry exists", EntryNode->FunctionReference.GetMemberName(), USMNodeInstance::GetConstructionScriptFunctionName());
		}
		else if (UK2Node_CallParentFunction* Parent = Cast<UK2Node_CallParentFunction>(Node))
		{
			NodesChecked++;
		}
		else if (UK2Node_CallFunction* FunctionNode = Cast<UK2Node_CallFunction>(Node))
		{
			NodesChecked++;
			Test->TestEqual("Execution environment exists", FunctionNode->FunctionReference.GetMemberName(), GET_FUNCTION_NAME_CHECKED(USMNodeInstance, WithExecutionEnvironment));
		}
	}
	Test->TestEqual("All construction nodes present", NodesChecked, ConstructionNodes.Num());

	// Test event graph.
	if (NodeBlueprint->ParentClass->IsChildOf<USMStateInstance_Base>())
	{
		Test->TestEqual("Event graph created", NodeBlueprint->UbergraphPages.Num(), 1);

		TArray<UK2Node_Event*> StateNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<UK2Node_Event>(NodeBlueprint->UbergraphPages[0], StateNodes);

		Test->TestEqual("Event nodes created", StateNodes.Num(), 3);

		Test->TestEqual("OnStateBegin exists", StateNodes[0]->EventReference.GetMemberName(), GET_FUNCTION_NAME_CHECKED(USMStateInstance_Base, OnStateBegin));
		Test->TestEqual("OnStateUpdate exists", StateNodes[1]->EventReference.GetMemberName(), GET_FUNCTION_NAME_CHECKED(USMStateInstance_Base, OnStateUpdate));
		Test->TestEqual("OnStateEnd exists", StateNodes[2]->EventReference.GetMemberName(), GET_FUNCTION_NAME_CHECKED(USMStateInstance_Base, OnStateEnd));
	}
	else if (NodeBlueprint->ParentClass->IsChildOf<USMTransitionInstance>())
	{
		TArray<UK2Node*> TransitionNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<UK2Node>(NodeBlueprint->FunctionGraphs[1], TransitionNodes);

		// TODO: Test more (Entry, Return, Parent call)
		Test->TestEqual("Transition nodes created", TransitionNodes.Num(), 3);
	}
	
	if (!Save)
	{
		return true;
	}

	if (!NewAsset.SaveAsset(Test))
	{
		return false;
	}

	return true;
}

UK2Node_CallFunction* TestHelpers::CreateContextGetter(FAutomationTestBase* Test, UEdGraph* Graph, UEdGraphPin** ContextOutPin)
{
	UK2Node_CallFunction* GetContextNode = NewObject<UK2Node_CallFunction>(Graph);
	UFunction* MakeNodeFunction = USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, GetContext));
	GetContextNode->CreateNewGuid();
	GetContextNode->PostPlacedNewNode();
	GetContextNode->SetFromFunction(MakeNodeFunction);
	GetContextNode->SetFlags(RF_Transactional);
	GetContextNode->AllocateDefaultPins();
	UEdGraphSchema_K2::SetNodeMetaData(GetContextNode, FNodeMetadata::DefaultGraphNode);
	Graph->AddNode(GetContextNode);

	UEdGraphPin** FoundPin = GetContextNode->Pins.FindByPredicate([&](UEdGraphPin* Pin)
	{
		return Pin->Direction == EGPD_Output;
	});

	Test->TestNotNull("Expected to find ContextOutPin", FoundPin);

	if (FoundPin)
	{
		*ContextOutPin = *FoundPin;
	}

	return GetContextNode;
}

UK2Node_CallFunction* TestHelpers::CreateFunctionCall(UEdGraph* Graph, UFunction* Function)
{
	return FSMBlueprintEditorUtils::CreateFunctionCall(Graph, Function);
}

UK2Node_DynamicCast* TestHelpers::CreateAndLinkPureCastNode(FAutomationTestBase* Test, UEdGraph* Graph, UEdGraphPin* ObjectOutPin, UEdGraphPin* ObjectInPin)
{
	// These pins won't connect, attempt to auto create a cast node.
	UBlueprintEditorSettings* BlueprintSettings = GetMutableDefault<UBlueprintEditorSettings>();
	const bool bOriginalAutoCast = BlueprintSettings->bAutoCastObjectConnections;
	BlueprintSettings->bAutoCastObjectConnections = true;
	Test->TestTrue("Editor Settings - Auto Cast Settings needs to be true or the test can't place Cast Nodes", BlueprintSettings->bAutoCastObjectConnections);

	const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());

	TOptional<UEdGraphSchema_K2::FFindSpecializedConversionNodeResults> Result = GraphSchema->FindSpecializedConversionNode(ObjectOutPin->PinType, *ObjectInPin, true);
	check(Result.IsSet());
	
	UK2Node* CreatedCastNode = Result->TargetNode;
	check(CreatedCastNode != nullptr); // If this fails you need to enable bAutoCastObjectConnections under Editor Settings.

	BlueprintSettings->bAutoCastObjectConnections = bOriginalAutoCast;
	
	CreatedCastNode->Rename(nullptr, Graph, RF_Transactional);
	CreatedCastNode->CreateNewGuid();
	//CreatedCastNode->PostPlacedNewNode(); // This will reset bIsPureCast unless project is set to use bFavorPureCast.
	CreatedCastNode->AllocateDefaultPins();
	Graph->AddNode(CreatedCastNode);

	UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(CreatedCastNode);
	Test->TestNotNull("Conversion should be DynamicCastNode", CastNode);
	CastNode->SetPurity(true);
	
	// The original object input pin to the cast.
	UEdGraphPin* CastedObjectSourcePin = GetCastSourcePin(CastNode);
	Test->TestNotNull("CastSourcePin should exist", CastedObjectSourcePin);

	// Connect the object to be casted.
	Test->TestTrue("Tried to make connection from original object to casted object source", GraphSchema->TryCreateConnection(ObjectOutPin, CastedObjectSourcePin));

	// The casted object type pin.
	UEdGraphPin* CastedObjectOutPin = GetCastResultPin(CastNode);
	Test->TestNotNull("CastResultPin should exist", CastedObjectOutPin);

	// Wire the casted result pin to the object in pin.
	Test->TestTrue("Tried to make connection from casted object result to logic node", GraphSchema->TryCreateConnection(CastedObjectOutPin, ObjectInPin));

	return CastNode;
}

void TestHelpers::TestNodeHasGuid(FAutomationTestBase* Test, FSMNode_Base* RuntimeNode)
{
	check(RuntimeNode);
	Test->TestTrue("GUID generated for runtime node", RuntimeNode->GetNodeGuid().IsValid());
}

bool TestHelpers::CanTransitionExist(const UEdGraphNode* InGraphNode)
{
	return InGraphNode && InGraphNode->IsA<USMGraphNode_StateNodeBase>() && !InGraphNode->IsA<USMGraphNode_StateMachineEntryNode>();
}

void TestHelpers::BuildLinearStateMachine(FAutomationTestBase* Test, USMGraph* StateMachineGraph, int32 NumStates, UEdGraphPin** FromPinInOut, UClass* StateClass, UClass* TransitionClass, bool bForceTransitionsToTrue)
{
	TestNodeHasGuid(Test, StateMachineGraph->GetRuntimeNode());
	
	// Choose start pin.
	UEdGraphPin* FromPin = FromPinInOut && *FromPinInOut ? *FromPinInOut : StateMachineGraph->GetEntryNode()->GetOutputPin();

	for (int32 i = 0; i < NumStates; ++i)
	{
		// Add a new node.
		USMGraphNode_StateNode* StateNode = TestHelpers::CreateNewNode<USMGraphNode_StateNode>(Test, StateMachineGraph, FromPin);
		TestNodeHasGuid(Test, StateNode->FindRuntimeNode());

		SetNodeClass(Test, StateNode, StateClass);
		
		// Create logic we can execute on a context.
		AddStateEntryLogic(Test, StateNode);
		AddStateUpdateLogic(Test, StateNode);
		AddStateEndLogic(Test, StateNode);

		// Make sure a transition can exist.
		if (CanTransitionExist(FromPin->GetOwningNode()))
		{
			// Make the transition able to be taken.
			USMGraphNode_TransitionEdge* Transition = CastChecked<USMGraphNode_TransitionEdge>(StateNode->GetInputPin()->LinkedTo[0]->GetOwningNode());
			TestNodeHasGuid(Test, Transition->FindRuntimeNode());

			SetNodeClass(Test, Transition, TransitionClass);

			if (bForceTransitionsToTrue)
			{
				AddTransitionResultLogic(Test, Transition);
			}
		}

		// For wiring the next state.
		FromPin = StateNode->GetOutputPin();
	}

	// Most recent outgoing pin.
	if (FromPinInOut)
	{
		*FromPinInOut = FromPin;
	}
}

void TestHelpers::BuildBranchingStateMachine(FAutomationTestBase* Test, USMGraph* StateMachineGraph, int32 Rows,
	int32 Branches, bool bRunParallel, TArray<UEdGraphPin*>* FromPinsInOut, bool bLeaveActive, bool bReEnterStates, bool bEvalIfNextStateActive, UClass* StateClass, UClass* TransitionClass)
{
	TestNodeHasGuid(Test, StateMachineGraph->GetRuntimeNode());

	// Choose start pin.
	TArray<UEdGraphPin*> FromPins = FromPinsInOut && FromPinsInOut->Num() ? *FromPinsInOut : TArray<UEdGraphPin*>{ StateMachineGraph->GetEntryNode()->GetOutputPin() };

	for (int32 r = 0; r < Rows; ++r)
	{
		TArray<UEdGraphPin*> NextFromPins;
		for (UEdGraphPin* FromPin : FromPins) 
		{
			// Each pin needs to branch.
			for (int32 b = 0; b < Branches; ++b)
			{
				// Add a new node.
				USMGraphNode_StateNode* StateNode = TestHelpers::CreateNewNode<USMGraphNode_StateNode>(Test, StateMachineGraph, FromPin);
				TestNodeHasGuid(Test, StateNode->FindRuntimeNode());

				SetNodeClass(Test, StateNode, StateClass);

				// Create logic we can execute on a context.
				AddStateEntryLogic(Test, StateNode);
				AddStateUpdateLogic(Test, StateNode);
				AddStateEndLogic(Test, StateNode);

				CastChecked<USMStateInstance_Base>(StateNode->GetNodeTemplate())->SetStayActiveOnStateChange(bLeaveActive);
				CastChecked<USMStateInstance_Base>(StateNode->GetNodeTemplate())->SetAllowParallelReentry(bReEnterStates);
				// Make sure a transition can exist.
				if (TestHelpers::CanTransitionExist(FromPin->GetOwningNode()))
				{
					// Make the transition able to be taken.
					USMGraphNode_TransitionEdge* Transition = CastChecked<USMGraphNode_TransitionEdge>(StateNode->GetInputPin()->LinkedTo[0]->GetOwningNode());
					TestNodeHasGuid(Test, Transition->FindRuntimeNode());

					SetNodeClass(Test, Transition, TransitionClass);

					AddTransitionResultLogic(Test, Transition);

					TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionInitializedNode>(Test, Transition,
						USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionInit)));

					TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionShutdownNode>(Test, Transition,
						USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionShutdown)));

					Transition->GetNodeTemplateAs<USMTransitionInstance>()->SetRunParallel(bRunParallel);
					Transition->GetNodeTemplateAs<USMTransitionInstance>()->SetEvalIfNextStateActive(bEvalIfNextStateActive);
				}
				else if (USMGraphNode_StateMachineEntryNode* EntryNode = Cast<USMGraphNode_StateMachineEntryNode>(FromPin->GetOwningNode()))
				{
					EntryNode->bAllowParallelEntryStates = bRunParallel;
				}

				// For wiring the next state.
				NextFromPins.Add(StateNode->GetOutputPin());
			}
		}

		FromPins = NextFromPins;
	}

	// Most recent outgoing pin.
	if (FromPinsInOut)
	{
		*FromPinsInOut = FromPins;
	}
}

USMGraphNode_StateMachineStateNode* TestHelpers::BuildNestedStateMachine(FAutomationTestBase* Test,
                                                                         USMGraph* StateMachineGraph, int32 NumStates, UEdGraphPin** FromPinInOut, UEdGraphPin** NestedPinOut)
{
	UEdGraphPin* FromPin = FromPinInOut && *FromPinInOut ? *FromPinInOut : StateMachineGraph->GetEntryNode()->GetOutputPin();
	
	USMGraphNode_StateMachineStateNode* NestedStateMachineNode = CreateNewNode<USMGraphNode_StateMachineStateNode>(Test, StateMachineGraph, FromPin);

	UEdGraphPin* LastNestedPin = nullptr;
	BuildLinearStateMachine(Test, Cast<USMGraph>(NestedStateMachineNode->GetBoundGraph()), 1, &LastNestedPin);

	// Add logic to the state machine transition.
	if (USMGraphNode_TransitionEdge* TransitionToNestedStateMachine = Cast<USMGraphNode_TransitionEdge>(NestedStateMachineNode->GetInputPin()->LinkedTo[0]->GetOwningNode()))
	{
		AddTransitionResultLogic(Test, TransitionToNestedStateMachine);
	}
	
	if (NumStates > 1)
	{
		// Add to the nested state machine
		BuildLinearStateMachine(Test, Cast<USMGraph>(NestedStateMachineNode->GetBoundGraph()), NumStates - 1, &LastNestedPin);
	}

	// Most recent outgoing pin.
	if (FromPinInOut)
	{
		*FromPinInOut = FromPin;
	}

	if (NestedPinOut)
	{
		*NestedPinOut = LastNestedPin;
	}
	
	return NestedStateMachineNode;
}

int32 TestHelpers::BuildStateMachineWithReferences(FAutomationTestBase* Test, USMGraph* StateMachineGraph, int32 NumStatesBeforeReferences, int32 NumStatesAfterReferences, int32 NumReferences, int32 NumNestedStates,
	TArray<FAssetHandler>& OutCreatedReferenceAssets, TArray<USMGraphNode_StateMachineStateNode*>& OutNestedStateMachineNodes)
{
	OutCreatedReferenceAssets.Reserve(NumReferences);
	OutNestedStateMachineNodes.Reserve(NumReferences);
	
	// Total states to test.
	int32 TotalStates = 0;
	int32 TotalTopLevelStates = 0;
	UEdGraphPin* LastStatePin = nullptr;

	// Build top level state machine.
	{
		const int32 CurrentStates = NumStatesBeforeReferences;
		BuildLinearStateMachine(Test, StateMachineGraph, CurrentStates, &LastStatePin);

		TotalStates += CurrentStates;
		TotalTopLevelStates += CurrentStates;
	}

	// Build a nested state machine.
	for (int32 Idx = 0; Idx < NumReferences; ++Idx)
	{
		UEdGraphPin* EntryPointForNestedStateMachine = LastStatePin;
		USMGraphNode_StateMachineStateNode* NestedStateMachineNode = TestHelpers::CreateNewNode<USMGraphNode_StateMachineStateNode>(Test, StateMachineGraph, EntryPointForNestedStateMachine);

		UEdGraphPin* LastNestedPin = nullptr;
		{
			const int32 CurrentStates = NumNestedStates;
			BuildLinearStateMachine(Test, Cast<USMGraph>(NestedStateMachineNode->GetBoundGraph()), CurrentStates, &LastNestedPin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());
			const FString ReferenceName = "Nested_State_Machine_For_Reference_" + FGuid::NewGuid().ToString();
			NestedStateMachineNode->GetBoundGraph()->Rename(*ReferenceName);
			LastStatePin = NestedStateMachineNode->GetOutputPin();

			TotalStates += CurrentStates;
			TotalTopLevelStates += 1;
		}

		// Add logic to the state machine transition.
		USMGraphNode_TransitionEdge* TransitionToNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(NestedStateMachineNode->GetInputPin()->LinkedTo[0]->GetOwningNode());
		AddTransitionResultLogic(Test, TransitionToNestedStateMachine);

		Test->TestTrue("Nested state machine has correct node count", NestedStateMachineNode->GetBoundGraph()->Nodes.Num() > 1);

		// Now convert the state machine to a reference.
		USMBlueprint* NewReferencedBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(NestedStateMachineNode, false, nullptr, nullptr);
		Test->TestNotNull("New referenced blueprint created", NewReferencedBlueprint);
		TestStateMachineConvertedToReference(Test, NestedStateMachineNode);

		FKismetEditorUtilities::CompileBlueprint(NewReferencedBlueprint);

		// Store handler information so we can delete the object.
		FAssetHandler ReferencedAsset = CreateAssetFromBlueprint(NewReferencedBlueprint);

		OutNestedStateMachineNodes.Add(NestedStateMachineNode);
		OutCreatedReferenceAssets.Add(MoveTemp(ReferencedAsset));
	}
	
	// Add more top level.
	{
		const int32 CurrentStates = NumStatesAfterReferences;
		BuildLinearStateMachine(Test, StateMachineGraph, CurrentStates, &LastStatePin);

		TotalStates += CurrentStates;
		TotalTopLevelStates += CurrentStates;
	}
	
	return TotalStates;
}

USMInstance* TestHelpers::TestLinearStateMachine(FAutomationTestBase* Test, USMBlueprint* Blueprint, int32 NumStates, bool bShutdownStateMachine)
{
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	// Create a context we will run the state machine for.
	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* StateMachineInstance = CreateNewStateMachineInstanceFromBP(Test, Blueprint, Context);

	// Verify machine matches expected states.
	Test->TestEqual("Testing states of instance match expected", StateMachineInstance->GetRootStateMachine().GetStates().Num(), NumStates);
	Test->TestEqual("Testing transitions of instance match expected", StateMachineInstance->GetRootStateMachine().GetTransitions().Num(), NumStates - 1);

	// Start the machine.
	int32 CurrentInt = 0;
	FSMState_Base* CurrentState = StateMachineInstance->GetRootStateMachine().GetSingleActiveState();
	Test->TestNull("Current state shouldn't be set", CurrentState);

	FGuid CurrentGuid;
	Test->TestEqual("Context int should be unchanged", Context->GetEntryInt(), CurrentInt);

	FDateTime CurrentTime = FDateTime::UtcNow();
	
	StateMachineInstance->Start();
	
	for (int32 i = 0; i < NumStates; ++i)
	{
		const float DeltaTime = 1.f;
		const bool bIsEndState = i == NumStates - 1;
		
		// State should have changed.
		Test->TestNotEqual("Test current state", StateMachineInstance->GetRootStateMachine().GetSingleActiveState()->GetGuid(), CurrentGuid);
		Test->TestEqual("Context int should have incremented", Context->GetEntryInt(), ++CurrentInt);
		CurrentState = StateMachineInstance->GetRootStateMachine().GetSingleActiveState();
		CurrentGuid = CurrentState->GetGuid();

		USMNodeInstance* NodeInstance = CurrentState->GetNodeInstance();
		Test->TestNotNull("Node instance created", NodeInstance);

		// Instance checks.
		if (USMStateTestInstance* StateInstance = Cast<USMStateTestInstance>(NodeInstance))
		{
			Test->TestEqual("State Machine Start hit", StateInstance->StateMachineStartHit.Count, 1);
			Test->TestEqual("State begin hit", StateInstance->StateBeginHit.Count, 1);
			Test->TestEqual("State update not hit", StateInstance->StateUpdateHit.Count, 0);
			Test->TestEqual("State end not hit", StateInstance->StateEndHit.Count, 0);
			Test->TestTrue("Exposed variable hit", StateInstance->ExposedInt >= 1); // May be a different base value depending on tests.

			Test->TestEqual("State begin event hit", StateInstance->StateBeginEventHit.Count, 1);
			Test->TestEqual("State update event not hit", StateInstance->StateUpdateEventHit.Count, 0);
			Test->TestEqual("State end event not hit", StateInstance->StateEndEventHit.Count, 0);

			Test->TestEqual("State initialize event hit", StateInstance->StateInitializedEventHit.Count, 1);
			Test->TestEqual("State shutdown event not hit", StateInstance->StateShutdownEventHit.Count, 0);

			Test->TestTrue("Start time recorded", StateInstance->GetStartTime() >= CurrentTime);
			
			if (USMTransitionTestInstance* TransitionInstance = Cast<USMTransitionTestInstance>(StateInstance->GetTransitionByIndex(0)))
			{
				Test->TestEqual("Transition root sm start hit", TransitionInstance->TransitionRootSMStartHit.Count, 1);

				if (StateMachineInstance->IsActive())
				{
					Test->TestEqual("Transition root sm stop not hit", TransitionInstance->TransitionRootSMStopHit.Count, 0);
				}
				
				Test->TestEqual("Transition initialized hit", TransitionInstance->TransitionInitializedHit.Count, 1);
				Test->TestTrue("State initialize fired before transition initialized", StateInstance->StateInitializedEventHit.TimeStamp > 0.f &&
					StateInstance->StateInitializedEventHit.TimeStamp < TransitionInstance->TransitionInitializedHit.TimeStamp);
			}

			if (i < NumStates - 1)
			{
				USMStateInstance_Base* NextStateInstance = StateInstance->GetNextStateByTransitionIndex(0);
				Test->TestNotNull("Next instance found", NextStateInstance);
				Test->TestTrue("Start time of next state not recorded", NextStateInstance->GetStartTime() == FDateTime(0));
			}
		}
		
		// Test that the state won't change if it can't transition.
		Context->bCanTransition = false;
		Context->TestUpdateFromDeltaSecondsInt = 0;

		CurrentTime = FDateTime::UtcNow();
		StateMachineInstance->Update(DeltaTime);

		Test->TestEqual("Test current state", StateMachineInstance->GetRootStateMachine().GetSingleActiveState()->GetGuid(), CurrentGuid);
		Test->TestEqual("Context int should be unchanged", Context->GetEntryInt(), CurrentInt);
		Test->TestEqual("Context update int should have increased", Context->GetUpdateFromDeltaSecondsInt(), (int32)DeltaTime);

		// Instance checks.
		if (USMStateTestInstance* StateInstance = Cast<USMStateTestInstance>(NodeInstance))
		{
			Test->TestEqual("State update hit", StateInstance->StateUpdateHit.Count, 1);
			Test->TestEqual("State end not hit", StateInstance->StateEndHit.Count, 0);

			Test->TestEqual("State initialize event unchanged", StateInstance->StateInitializedEventHit.Count, 1);
			Test->TestEqual("State shutdown event unchanged", StateInstance->StateShutdownEventHit.Count, 0);
		}
		
		// Is end state.
		if (bIsEndState)
		{
			const bool bIsConsideredEndState = CurrentState->IsEndState();
			const bool bOwningSmIsInEndState = StateMachineInstance->GetRootStateMachine().IsStateMachineInEndState();
			Test->TestTrue("State should be end state", CurrentState->bCanBeEndState ? bIsConsideredEndState : !bIsConsideredEndState);
			Test->TestTrue("State Machine should be in end state", CurrentState->bCanBeEndState ? bOwningSmIsInEndState : !bOwningSmIsInEndState);
			break;
		}

		// Test taking the transition.
		Context->bCanTransition = true;

		Context->TestEndInt = 0;
		StateMachineInstance->Update(DeltaTime);
		Test->TestEqual("Context end int should have changed", Context->GetEndInt(), 1);

		// Instance checks.
		if (USMStateTestInstance* StateInstance = Cast<USMStateTestInstance>(NodeInstance))
		{
			Test->TestEqual("State update hit", StateInstance->StateUpdateHit.Count, 1);
			Test->TestEqual("State end not hit", StateInstance->StateEndHit.Count, 1);

			Test->TestEqual("State initialize event unchanged", StateInstance->StateInitializedEventHit.Count, 1);
			Test->TestEqual("State shutdown event hit", StateInstance->StateShutdownEventHit.Count, 1);

			if (USMTransitionTestInstance* TransitionInstance = Cast<USMTransitionTestInstance>(StateInstance->GetTransitionByIndex(0)))
			{
				Test->TestEqual("Transition shutdown hit", TransitionInstance->TransitionShutdownHit.Count, 1);
				Test->TestTrue("State shutdown fired after transition shutdown", StateInstance->StateShutdownEventHit.TimeStamp > 0.f &&
					StateInstance->StateShutdownEventHit.TimeStamp > TransitionInstance->TransitionShutdownHit.TimeStamp);
			}
		}
	}

	if (bShutdownStateMachine)
	{
		if (StateMachineInstance->IsActive())
		{
			StateMachineInstance->Stop();
		}
		
		if (NumStates == StateMachineInstance->GetStateMap().Num() - 1)
		{
			// Don't test unless all states hit.
			
			for (const auto& State : StateMachineInstance->GetStateMap())
			{
				if (USMStateTestInstance* StateInstance = Cast<USMStateTestInstance>(State.Value->GetNodeInstance()))
				{
					Test->TestEqual("State Machine Stop hit", StateInstance->StateMachineStopHit.Count, 1);

					Test->TestEqual("State begin event hit", StateInstance->StateBeginEventHit.Count, 1);
					Test->TestEqual("State update event hit", StateInstance->StateUpdateEventHit.Count, 1);
					Test->TestEqual("State end event hit", StateInstance->StateEndEventHit.Count, 1);
				}
			}

			for (const auto& Transition : StateMachineInstance->GetTransitionMap())
			{
				if (USMTransitionTestInstance* TransitionInstance = Cast<USMTransitionTestInstance>(Transition.Value->GetNodeInstance()))
				{
					Test->TestEqual("Transition initialized hit", TransitionInstance->TransitionInitializedHit.Count, 1);
					Test->TestEqual("Transition shutdown hit", TransitionInstance->TransitionShutdownHit.Count, 1);

					Test->TestEqual("Transition taken event hit", TransitionInstance->TransitionEnteredEventHit.Count, 1);

					Test->TestEqual("Transition root sm start hit", TransitionInstance->TransitionRootSMStartHit.Count, 1);
					Test->TestEqual("Transition root sm stop hit", TransitionInstance->TransitionRootSMStopHit.Count, 1);
				}
			}
		}
		
		StateMachineInstance->Shutdown();
		Test->TestFalse("State Machine should have stopped", StateMachineInstance->IsActive());
	}

	return StateMachineInstance;
}

USMInstance* TestHelpers::RunStateMachineToCompletion(FAutomationTestBase* Test, USMBlueprint* Blueprint,
	int32& LogicEntryValueOut, int32& LogicUpdateValueOut, int32& LogicEndValueOut, int32 MaxIterations,
	bool bShutdownStateMachine, bool bTestCompletion, bool bCompile, int32* IterationsRan, USMInstance* UseInstance)
{
	if (bCompile)
	{
		// Compile the BP twice, checking to make sure all generated names are deterministic.

		check(UseInstance == nullptr);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		const USMBlueprintGeneratedClass* GeneratedClass = Blueprint->GetGeneratedClass();
		TArray<FString> OldNames = GeneratedClass->GeneratedNames;

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		GeneratedClass = Blueprint->GetGeneratedClass();
		TArray<FString> NewNames = GeneratedClass->GeneratedNames;

		if (Test->TestEqual("Array sizes match", NewNames.Num(), OldNames.Num()))
		{
			for (int32 Idx = 0; Idx < OldNames.Num(); ++Idx)
			{
				Test->TestEqual("Generated names match", NewNames[Idx], OldNames[Idx]);
			}
		}
	}

	ULambdaWrapper* LambdaWrapper = nullptr;

	// Create a context we will run the state machine for.
	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* StateMachineInstance = UseInstance ? UseInstance : CreateNewStateMachineInstanceFromBP(Test, Blueprint, Context);
	if (UseInstance)
	{
		StateMachineInstance->Initialize(Context);
	}
	else
	{
		LambdaWrapper = NewObject<ULambdaWrapper>(GetTransientPackage());
		StateMachineInstance->OnStateMachineInitializedEvent.AddUniqueDynamic(LambdaWrapper, &ULambdaWrapper::OnInitialize);
		StateMachineInstance->OnStateMachineStartedEvent.AddUniqueDynamic(LambdaWrapper, &ULambdaWrapper::OnStart);
		if (bShutdownStateMachine)
		{
			StateMachineInstance->OnStateMachineStoppedEvent.AddUniqueDynamic(LambdaWrapper, &ULambdaWrapper::OnStop);
			StateMachineInstance->OnStateMachineShutdownEvent.AddUniqueDynamic(LambdaWrapper, &ULambdaWrapper::OnShutdown);
		}
	}

	StateMachineInstance->SetCanEverTick(false);
	StateMachineInstance->SetAutoManageTime(false);
	StateMachineInstance->Start();
	Test->TestTrue("State Machine should have started", StateMachineInstance->IsActive());

	// Run until an end state is reached or the max iterations is hit.
	const float DeltaTime = 1.f;
	int32 CurrentIterations = 0;
	while (!StateMachineInstance->GetRootStateMachine().IsStateMachineInEndState() && CurrentIterations < MaxIterations)
	{
		TArray<FSMState_Base*> ActiveStates = StateMachineInstance->GetRootStateMachine().GetActiveStates();

		Test->TestTrue("Active states exist when active", ActiveStates.Num() > 0);

		// Parallel test.
		if (ActiveStates.Num() > 1)
		{
			for (int32 Idx = 0; Idx < ActiveStates.Num() - 1; ++Idx)
			{
				FSMState_Base* ActiveState = ActiveStates[Idx];
				FSMState_Base* NextActiveState = ActiveStates[Idx + 1];

				Test->TestTrue("States executed in correct order.", ActiveState->GetStartCycle() < NextActiveState->GetStartCycle());
			}
		}
		
		StateMachineInstance->Update(DeltaTime);

		if (CurrentIterations++ > MaxIterations)
		{
			break;
		}
	}

	if (bTestCompletion)
	{
		Test->TestTrue("State Machine reached end state", StateMachineInstance->GetRootStateMachine().IsStateMachineInEndState());
	}

	if (LambdaWrapper)
	{
		Test->TestEqual("Start event hit", LambdaWrapper->OnStartHit.Count, 1);
	}

	if (bShutdownStateMachine)
	{
		// Will trigger last end state call.
		StateMachineInstance->Shutdown();
		Test->TestFalse("State Machine should have stopped", StateMachineInstance->IsActive());

		if (LambdaWrapper)
		{
			Test->TestEqual("Stopped event hit", LambdaWrapper->OnStoppedHit.Count, 1);
			Test->TestEqual("Shutdown event hit", LambdaWrapper->OnShutdownHit.Count, 1);
		}
	}

	LogicEntryValueOut = Context->GetEntryInt();
	LogicUpdateValueOut = Context->GetUpdateFromDeltaSecondsInt();
	LogicEndValueOut = Context->GetEndInt();

	if (IterationsRan)
	{
		*IterationsRan = CurrentIterations;
	}

	return StateMachineInstance;
}

bool ShouldAbortStateMachineRun(int32 AbortAfterStatesHit, int32 StatesHit)
{
	return AbortAfterStatesHit >= 0 && StatesHit >= AbortAfterStatesHit;
}

int32 TestHelpers::RunAllStateMachinesToCompletion(FAutomationTestBase* Test, USMInstance* Instance,
	FSMStateMachine* StateMachine, int32 AbortAfterStatesHit, int32 CheckStatesHit, bool bBindEvents)
{
	const float DeltaTime = 1.f;
	int32 StatesHit = 0;
	StateMachine = StateMachine ? StateMachine : &Instance->GetRootStateMachine();
	TArray<FSMState_Base*> StatesRemaining = StateMachine->GetStates();

	if (bBindEvents)
	{
		Instance->OnStateMachineTransitionTakenEvent.AddUniqueDynamic(Cast<USMTestContext>(Instance->GetContext()), &USMTestContext::OnTransitionTaken);
		Instance->OnStateMachineStateChangedEvent.AddUniqueDynamic(Cast<USMTestContext>(Instance->GetContext()), &USMTestContext::OnStateChanged);
	}
	
	if (!Instance->HasStarted())
	{
		Instance->Start();
		Test->TestTrue("State Machine should have started", Instance->IsActive());
	}
	
	while (!StateMachine->IsStateMachineInEndState())
	{
		if (ShouldAbortStateMachineRun(AbortAfterStatesHit, StatesHit))
		{
			return StatesHit;
		}
		
		StatesHit++;
		
		FSMState_Base* ActiveState = StateMachine->GetSingleActiveState();
		FSMState_Base* NestedActiveState = Instance->GetSingleNestedActiveState();

		// Test retrieving basic state information.
		FSMStateInfo Info;
		bool bSuccess;
		Instance->TryGetStateInfo(NestedActiveState->GetGuid(), Info, bSuccess);
		Test->TestTrue("", bSuccess);
		Test->TestEqual("", Info.Guid, NestedActiveState->GetGuid());

		// Test retrieving basic transition information.
		for (FSMTransitionInfo& TransitionInfo : Info.OutgoingTransitions)
		{
			FSMTransitionInfo FoundTransition;
			Instance->TryGetTransitionInfo(TransitionInfo.Guid, FoundTransition, bSuccess);
			Test->TestTrue("", bSuccess);
			Test->TestEqual("", TransitionInfo.Guid, FoundTransition.Guid);
		}
		
		Instance->TryGetNestedActiveState(Info, bSuccess);
		Test->TestTrue("", bSuccess);
		Test->TestEqual("", Info.Guid, NestedActiveState->GetGuid());
		
		StatesRemaining.Remove(ActiveState);
		if (ActiveState->IsStateMachine())
		{
			FSMStateMachine* NestedStateMachine = (FSMStateMachine*)ActiveState;
			StatesHit += RunAllStateMachinesToCompletion(Test, Instance, NestedStateMachine, AbortAfterStatesHit < 0 ? AbortAfterStatesHit : (AbortAfterStatesHit - StatesHit), CheckStatesHit, false);

			USMStateMachineInstance* NestedNodeInstance = CastChecked<USMStateMachineInstance>(NestedStateMachine->GetOrCreateNodeInstance());
			USMInstance* ReferencedInstance = NestedNodeInstance->GetStateMachineReference();
			
			Test->TestEqual("Reference assigned correctly", ReferencedInstance, NestedStateMachine->GetInstanceReference());
		}
		else
		{
			// Validate retrieval of exact active state from instance level. Only valid to check here. State machines may have multiple nested.
			
			Test->TestEqual("Nested active state should match active state", ActiveState, NestedActiveState);
		}
		
		Instance->Update(DeltaTime);

		// Won't get hit otherwise.
		if (StateMachine->IsStateMachineInEndState())
		{
			StatesRemaining.Remove(StateMachine->GetSingleActiveState());
		}
	}

	if (CheckStatesHit >= 0)
	{
		Test->TestEqual("All states should have been hit.", StatesRemaining.Num(), CheckStatesHit);
	}

	if (bBindEvents)
	{
		USMTestContext* Context = Cast<USMTestContext>(Instance->GetContext());
		Test->TestTrue("Transition events hit", Context->TestTransitionsHit > 0);
		Test->TestTrue("State change events hit", Context->TestStatesHit > 0);
		
		Instance->OnStateMachineTransitionTakenEvent.RemoveDynamic(Cast<USMTestContext>(Instance->GetContext()), &USMTestContext::OnTransitionTaken);
		Instance->OnStateMachineStateChangedEvent.RemoveDynamic(Cast<USMTestContext>(Instance->GetContext()), &USMTestContext::OnStateChanged);
	}

	for (const TTuple<FGuid, FSMState_Base*>& Node : Instance->GetStateMap())
	{
		if (Node.Value->IsInitializedForRun())
		{
			Test->TestTrue("Node has not shutdown", Node.Value->IsActive());
		}
		else
		{
			Test->TestFalse("Nodes has shutdown", Node.Value->IsActive());
		}
	}

	for (const TTuple<FGuid, FSMTransition*>& Node : Instance->GetTransitionMap())
	{
		if (Node.Value->GetFromState()->IsActive())
		{
			Test->TestTrue("Transition has not shutdown", Node.Value->IsInitializedForRun());
		}
		else
		{
			Test->TestFalse("Transition has shutdown", Node.Value->IsInitializedForRun());
		}
	}
	
	return StatesHit;
}

UK2Node_CallFunction* TestHelpers::AddGenericContextLogicToExecutionEntry(FAutomationTestBase* Test,
	USMGraphK2Node_RuntimeNode_Base* ExecutionEntry, const FName& ContextFunctionName)
{
	UEdGraph* Graph = ExecutionEntry->GetGraph();
	const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());
	
	// Add a get context node.
	UEdGraphPin* ContextOutPin = nullptr;
	UK2Node_CallFunction* GetContextNode = CreateContextGetter(Test, Graph, &ContextOutPin);

	// Add a call to execute logic on the context.
	UK2Node_CallFunction* ExecuteNode = CreateFunctionCall(Graph, USMTestContext::StaticClass()->FindFunctionByName(ContextFunctionName));

	// The logic self pin (make this function a method).
	UEdGraphPin* LogicSelfPin = ExecuteNode->FindPin(TEXT("self"), EGPD_Input);
	Test->TestNotNull("Expected to find ExecuteTargetPin", LogicSelfPin);

	// Convert the context 'object' type out to our context type.
	UK2Node_DynamicCast* CastNode = CreateAndLinkPureCastNode(Test, Graph, ContextOutPin, LogicSelfPin);

	// Now connect entry exec out pin to the logic exec in pin.
	Test->TestTrue("Tried to make connection from entry node to logic execute node", GraphSchema->TryCreateConnection(ExecutionEntry->GetOutputPin(), ExecuteNode->GetExecPin()));

	return ExecuteNode;
}

void TestHelpers::AddStateEntryLogic(FAutomationTestBase* Test, USMGraphNode_StateNode* StateNode)
{
	UEdGraph* Graph = StateNode->GetBoundGraph();
	const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());
	USMGraphK2Node_StateEntryNode* Entry = CastChecked<USMStateGraph>(Graph)->EntryNode;

	// If this is already connected to a node, such as an instance node.
	UEdGraphPin* OldEntryToPin = nullptr;
	if (Entry->Pins.Num() > 0 && Entry->Pins[0]->LinkedTo.Num() > 0)
	{
		OldEntryToPin = Entry->Pins[0]->LinkedTo[0];
	}

	UK2Node_CallFunction* ExecuteNode = AddGenericContextLogicToExecutionEntry(Test, Entry, GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseEntryInt));
	
	// Wire any old logic after our function pin.
	if (OldEntryToPin)
	{
		OldEntryToPin->BreakAllPinLinks();
		Test->TestTrue("Tried to make connection from execute node to existing logic node", GraphSchema->TryCreateConnection(ExecuteNode->GetThenPin(), OldEntryToPin));
	}
}

void TestHelpers::AddStateUpdateLogic(FAutomationTestBase* Test, USMGraphNode_StateNode* StateNode)
{
	UEdGraph* Graph = StateNode->GetBoundGraph();
	const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());

	TArray<USMGraphK2Node_StateUpdateNode*> UpdateNodes;
	Graph->GetNodesOfClass<USMGraphK2Node_StateUpdateNode>(UpdateNodes);

	check(UpdateNodes.Num() == 1);

	USMGraphK2Node_StateUpdateNode* UpdateNode = UpdateNodes[0];

	// If this is already connected to a node, such as an instance node.
	UEdGraphPin* OldEntryToPin = nullptr;
	if (UpdateNode->Pins.Num() > 0 && UpdateNode->Pins[0]->LinkedTo.Num() > 0)
	{
		OldEntryToPin = UpdateNode->Pins[0]->LinkedTo[0];
	}
	
	// Find the update node's float output.
	UEdGraphPin** FloatOutputPin = UpdateNode->Pins.FindByPredicate([&](UEdGraphPin* Pin)
	{
		return Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Real;
	});
	check(FloatOutputPin);

	// Add a get context node.
	UEdGraphPin* ContextOutPin = nullptr;
	UK2Node_CallFunction* GetContextNode = CreateContextGetter(Test, Graph, &ContextOutPin);

	// Add a call to execute logic on the context.
	UK2Node_CallFunction* ExecuteNode = CreateFunctionCall(Graph, USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseUpdateInt)));

	// Find the float input pin of the execute node.
	UEdGraphPin** FloatInputPin = ExecuteNode->Pins.FindByPredicate([&](UEdGraphPin* Pin)
	{
		return Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Real;
	});
	check(FloatInputPin);

	Test->TestTrue("Tried to make float connection from update node to logic execute node", GraphSchema->TryCreateConnection(*FloatOutputPin, *FloatInputPin));

	// The logic self pin (make this function a method).
	UEdGraphPin* LogicSelfPin = ExecuteNode->FindPin(TEXT("self"), EGPD_Input);
	Test->TestNotNull("Expected to find ExecuteTargetPin", LogicSelfPin);

	// Convert the context 'object' type out to our context type.
	UK2Node_DynamicCast* CastNode = CreateAndLinkPureCastNode(Test, Graph, ContextOutPin, LogicSelfPin);

	// Now connect update exec out pin to the logic exec in pin.
	Test->TestTrue("Tried to make connection from update node to logic execute node", GraphSchema->TryCreateConnection(UpdateNode->GetOutputPin(), ExecuteNode->GetExecPin()));

	// Wire any old logic after our function pin.
	if (OldEntryToPin)
	{
		OldEntryToPin->BreakAllPinLinks();
		Test->TestTrue("Tried to make connection from execute node to existing logic node", GraphSchema->TryCreateConnection(ExecuteNode->GetThenPin(), OldEntryToPin));
	}
}

void TestHelpers::AddStateEndLogic(FAutomationTestBase* Test, USMGraphNode_StateNode* StateNode)
{
	UEdGraph* Graph = StateNode->GetBoundGraph();
	const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());

	TArray<USMGraphK2Node_StateEndNode*> EndNodes;
	Graph->GetNodesOfClass<USMGraphK2Node_StateEndNode>(EndNodes);

	check(EndNodes.Num() == 1);

	USMGraphK2Node_StateEndNode* EndNode = EndNodes[0];

	// If this is already connected to a node, such as an instance node.
	UEdGraphPin* OldEntryToPin = nullptr;
	if (EndNode->Pins.Num() > 0 && EndNode->Pins[0]->LinkedTo.Num() > 0)
	{
		OldEntryToPin = EndNode->Pins[0]->LinkedTo[0];
	}
	
	// Add a get context node.
	UEdGraphPin* ContextOutPin = nullptr;
	UK2Node_CallFunction* GetContextNode = CreateContextGetter(Test, Graph, &ContextOutPin);

	// Add a call to execute logic on the context.
	UK2Node_CallFunction* ExecuteNode = CreateFunctionCall(Graph, USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseEndInt)));

	// The logic self pin (make this function a method).
	UEdGraphPin* LogicSelfPin = ExecuteNode->FindPin(TEXT("self"), EGPD_Input);
	Test->TestNotNull("Expected to find ExecuteTargetPin", LogicSelfPin);

	// Convert the context 'object' type out to our context type.
	UK2Node_DynamicCast* CastNode = CreateAndLinkPureCastNode(Test, Graph, ContextOutPin, LogicSelfPin);

	// Now connect end exec out pin to the logic exec in pin.
	Test->TestTrue("Tried to make connection from end node to logic execute node", GraphSchema->TryCreateConnection(EndNode->GetOutputPin(), ExecuteNode->GetExecPin()));

	// Wire any old logic after our function pin.
	if (OldEntryToPin)
	{
		OldEntryToPin->BreakAllPinLinks();
		Test->TestTrue("Tried to make connection from execute node to existing logic node", GraphSchema->TryCreateConnection(ExecuteNode->GetThenPin(), OldEntryToPin));
	}
}

void TestHelpers::AddTransitionResultLogic(FAutomationTestBase* Test, USMGraphNode_TransitionEdge* TransitionEdge)
{
	UEdGraph* Graph = TransitionEdge->GetBoundGraph();
	const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());
	USMGraphK2Node_TransitionResultNode* Result = CastChecked<USMTransitionGraph>(Graph)->ResultNode;

	// Could be connected to an instance node.
	Result->BreakAllNodeLinks();

	// Verify can't transition.
	Test->TestFalse("Transition should read as not possible to transition without any result logic", TransitionEdge->PossibleToTransition());

	// Add a get context node.
	UEdGraphPin* ContextOutPin = nullptr;
	UK2Node_CallFunction* GetContextNode = CreateContextGetter(Test, Graph, &ContextOutPin);

	// Add a call to read from the context.
	UK2Node_CallFunction* CanTransitionGetter = CreateFunctionCall(Graph, USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, CanTransition)));

	// The logic self pin (make this function a method).
	UEdGraphPin* GetterPin = CanTransitionGetter->FindPin(TEXT("self"), EGPD_Input);
	Test->TestNotNull("Expected to find GetterPin", GetterPin);

	// Convert the context 'object' type out to our context type and wire it to the getter.
	UK2Node_DynamicCast* CastNode = CreateAndLinkPureCastNode(Test, Graph, ContextOutPin, GetterPin);

	UEdGraphPin** FoundPin = CanTransitionGetter->Pins.FindByPredicate([&](UEdGraphPin* Pin)
	{
		return Pin->Direction == EGPD_Output;
	});
	Test->TestNotNull("Expected to find Getter out pin", FoundPin);

	// Now connect the getter out pin to the result in pin.
	Test->TestTrue("Tried to make connection from getter node to result node", GraphSchema->TryCreateConnection(*FoundPin, Result->GetInputPin()));

	// Now verify it is possible to transition.
	Test->TestTrue("Transition should read as possible to transition", TransitionEdge->PossibleToTransition());
}

void TestHelpers::TestSetTemplate(FAutomationTestBase* Test, USMInstance* Template, const FString& DefaultStringValue, const FString& NewStringValue)
{
	bool bStringDefaultValueVerified = false;
	for (TFieldIterator<FStrProperty> It(Template->GetClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FString* DestinationPtr = (*It)->ContainerPtrToValuePtr<FString>(Template);
		Test->TestEqual("Default string value matches template", *DestinationPtr, DefaultStringValue);

		DestinationPtr->Empty();
		DestinationPtr->Append(NewStringValue);
		bStringDefaultValueVerified = true;
	}
	Test->TestTrue("Template has string property created.", bStringDefaultValueVerified);
}

TSet<UEdGraphNode*> TestHelpers::DuplicateNodes(const TArray<UEdGraphNode*>& InNodes)
{
	UEdGraph* GraphToUse = nullptr;
	TSet<UObject*> NodesToCopy;
	NodesToCopy.Reserve(InNodes.Num());
	for (UObject* Object : InNodes)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
		{
			if (Node->CanDuplicateNode())
			{
				if (GraphToUse)
				{
					check(GraphToUse == Node->GetGraph())
				}
				GraphToUse = Node->GetGraph();
				Node->PrepareForCopying();

				NodesToCopy.Add(Node);
			}
		}
	}

	check(GraphToUse);
	
	FString ExportedText;
	FEdGraphUtilities::ExportNodesToText(NodesToCopy, /*out*/ ExportedText);

	TSet<UEdGraphNode*> OutNodes;
	FEdGraphUtilities::ImportNodesFromText(GraphToUse, ExportedText, OutNodes);
	return OutNodes;
}

void TestHelpers::TestStateMachineConvertedToReference(FAutomationTestBase* Test,
                                                       const USMGraphNode_StateMachineStateNode* StateMachineStateNode)
{
	check(StateMachineStateNode);
	Test->TestEqual("Nested state machine graph is now an intermediate reference",
		StateMachineStateNode->GetBoundGraph()->GetClass(), USMIntermediateGraph::StaticClass());
	Test->TestEqual("Nested state machine has had all nodes removed.", StateMachineStateNode->GetBoundGraph()->Nodes.Num(), 7); // Default nodes in intermediate graph.

	for (const UEdGraph* SubGraph : StateMachineStateNode->GetBoundGraph()->SubGraphs)
	{
		Test->TestEqual("Subgraph is a property graph", SubGraph->GetClass(), USMPropertyGraph::StaticClass());
	}
}

void TestHelpers::SetNodeClass(FAutomationTestBase* Test, USMGraphNode_Base* Node, TSubclassOf<USMNodeInstance> Class)
{
	Node->SetNodeClass(Class);

	if (Class == nullptr)
	{
		return;
	}

	TArray<FProperty*> ExposedProperties;
	for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		// Text graph properties won't be considered exposed by default.
		{
			if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(*It))
			{
				if (const FStructProperty* StructProperty = CastField<FStructProperty>(ArrayProperty->Inner))
				{
					if (StructProperty->Struct == FSMTextGraphProperty::StaticStruct())
					{
						ExposedProperties.Add(*It);
						continue;
					}
				}
			}
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(*It))
			{
				if (StructProperty->Struct == FSMTextGraphProperty::StaticStruct())
				{
					ExposedProperties.Add(*It);
					continue;
				}
			}
		}

		// Normal variable properties.
		if (LD::PropertyUtils::IsPropertyExposedToGraphNode(*It))
		{
			ExposedProperties.Add(*It);
		}
	}

	UEdGraph* BoundGraph = Node->GetBoundGraph();

	if (Class == USMStateInstance::StaticClass())
	{
		TestNodeNotInGraph<USMGraphK2Node_IntermediateStateMachineStartNode>(Test, BoundGraph);
		TestNodeNotInGraph<USMGraphK2Node_IntermediateStateMachineStopNode>(Test, BoundGraph);
	}
	else if (Class == USMTransitionInstance::StaticClass())
	{
		TestNodeNotInGraph<USMGraphK2Node_TransitionEnteredNode>(Test, BoundGraph);
		TestNodeNotInGraph<USMGraphK2Node_TransitionInitializedNode>(Test, BoundGraph);
		TestNodeNotInGraph<USMGraphK2Node_TransitionShutdownNode>(Test, BoundGraph);
	}
	else if (Class->IsChildOf<USMStateInstance>())
	{
		VerifyNodeWiredFromPin<USMGraphK2Node_StateEntryNode>(Test, BoundGraph, USMGraphK2Schema::PN_Then);
		VerifyNodeWiredFromPin<USMGraphK2Node_StateUpdateNode>(Test, BoundGraph, USMGraphK2Schema::PN_Then);
		VerifyNodeWiredFromPin<USMGraphK2Node_StateEndNode>(Test, BoundGraph, USMGraphK2Schema::PN_Then);
		VerifyNodeWiredFromPin<USMGraphK2Node_IntermediateStateMachineStartNode>(Test, BoundGraph, USMGraphK2Schema::PN_Then);
		VerifyNodeWiredFromPin<USMGraphK2Node_IntermediateStateMachineStopNode>(Test, BoundGraph, USMGraphK2Schema::PN_Then);
	}
	else if (Class->IsChildOf<USMTransitionInstance>())
	{
		VerifyNodeWiredFromPin<USMGraphK2Node_TransitionResultNode>(Test, BoundGraph, FName("bCanEnterTransition"));
		VerifyNodeWiredFromPin<USMGraphK2Node_TransitionEnteredNode>(Test, BoundGraph, USMGraphK2Schema::PN_Then);
		VerifyNodeWiredFromPin<USMGraphK2Node_TransitionInitializedNode>(Test, BoundGraph, USMGraphK2Schema::PN_Then);
		VerifyNodeWiredFromPin<USMGraphK2Node_TransitionShutdownNode>(Test, BoundGraph, USMGraphK2Schema::PN_Then);
	}
	
	const TArray<USMGraphK2Node_PropertyNode_Base*> GraphProperties = Node->GetAllPropertyGraphNodesAsArray();

	// Gather total count of all graph properties. Array elements can increase number of graphs but aren't actually new properties.
	TSet<FProperty*> TotalProperties;
	for (USMGraphK2Node_PropertyNode_Base* GraphProperty : GraphProperties)
	{
		if (FProperty* Property = GraphProperty->GetPropertyNodeChecked()->MemberReference.ResolveMember<FProperty>(Class))
		{
			TotalProperties.Add(Property);
		}
	}
	
	Test->TestEqual("Graph properties created", TotalProperties.Num(), ExposedProperties.Num());
}

UEdGraphPin* TestHelpers::GetValidCastPin(UK2Node_DynamicCast * CastNode)
{
	UEdGraphPin* Pin = CastNode->FindPin(UEdGraphSchema_K2::PN_CastSucceeded);
	check(Pin == nullptr || (Pin->Direction == EGPD_Output));
	return Pin;
}

UEdGraphPin* TestHelpers::GetCastResultPin(UK2Node_DynamicCast* CastNode)
{
	for (int32 PinIdx = 0; PinIdx < CastNode->Pins.Num(); PinIdx++)
	{
		if (CastNode->Pins[PinIdx]->Direction == EGPD_Output
			&& CastNode->Pins[PinIdx]->PinName.ToString().StartsWith(UEdGraphSchema_K2::PN_CastedValuePrefix))
		{
			return CastNode->Pins[PinIdx];
		}
	}

	return nullptr;
}

UEdGraphPin* TestHelpers::GetCastSourcePin(UK2Node_DynamicCast* CastNode)
{
	UEdGraphPin* Pin = CastNode->FindPin(UEdGraphSchema_K2::PN_ObjectToCast);
	check(Pin);
	check(Pin->Direction == EGPD_Input);
	return Pin;
}