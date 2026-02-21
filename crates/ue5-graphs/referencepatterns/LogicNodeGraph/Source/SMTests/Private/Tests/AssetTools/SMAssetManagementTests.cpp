// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMAssetTestInstance.h"
#include "SMTestHelpers.h"

#include "SMAssetTestInstance.h"
#include "SMTestHelpers.h"

#include "ISMAssetManager.h"
#include "ISMAssetToolsModule.h"
#include "ISMGraphGeneration.h"
#include "AssetExporter/SMAssetExportManager.h"
#include "AssetExporter/Types/SMAssetExporterJson.h"
#include "AssetImporter/SMAssetImportManager.h"

#include "SMUtils.h"
#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"

#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Utilities/SMTextUtils.h"
#include "Utilities/SMVersionUtils.h"

#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

void ValidateAsset(FAutomationTestBase* Test, USMBlueprint* InBlueprint,
                   const ISMAssetManager::FCreateStateMachineBlueprintArgs& InArgs)
{
	// Verify correct type created.
	Test->TestNotNull("New asset object should be USMBlueprint", InBlueprint);

	{
		USMBlueprintGeneratedClass* GeneratedClass = Cast<USMBlueprintGeneratedClass>(InBlueprint->GetGeneratedClass());
		Test->TestNotNull("Generated Class should match expected class", GeneratedClass);

		UClass* Parent = InArgs.ParentClass.Get() ? InArgs.ParentClass.Get() : USMInstance::StaticClass();
		Test->TestEqual("Generated Class should match expected class", GeneratedClass->GetSuperClass(), Parent);

		// Verify new version set correctly.
		Test->TestTrue("Instance version is correctly created", FSMVersionUtils::IsAssetUpToDate(InBlueprint));
	}

	TestHelpers::ValidateNewStateMachineBlueprint(Test, InBlueprint);
}

/**
 * Create an asset programatically.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsCreateAssetTest, "LogicDriver.AssetTools.CreateAsset",
                                 EAutomationTestFlags::ApplicationContextMask |
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
                                 EAutomationTestFlags::EngineFilter)

bool FAssetToolsCreateAssetTest::RunTest(const FString& Parameters)
{
	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
		LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	const FName AssetName = *FGuid::NewGuid().ToString();

	ISMAssetManager::FCreateStateMachineBlueprintArgs Args;
	Args.Name = AssetName;
	Args.Path = FAssetHandler::DefaultGamePath();
	USMBlueprint* NewBP = AssetToolsModule.GetAssetManagerInterface()->CreateStateMachineBlueprint(Args);

	ValidateAsset(this, NewBP, Args);
	TestEqual("Asset name is exact", NewBP->GetFName(), Args.Name);

	Args.ParentClass = NewBP->GeneratedClass;
	USMBlueprint* NewBPChild = AssetToolsModule.GetAssetManagerInterface()->CreateStateMachineBlueprint(Args);
	ValidateAsset(this, NewBPChild, Args);
	TestNotEqual("Asset name has been changed due to a collision", NewBPChild->GetFName(), Args.Name);

	return true;
}

/**
 * Programatically set the class defaults of a newly created asset.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsPopulateClassDefaultsTest, "LogicDriver.AssetTools.PopulateClassDefaults",
                                 EAutomationTestFlags::ApplicationContextMask |
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
                                 EAutomationTestFlags::EngineFilter)

bool FAssetToolsPopulateClassDefaultsTest::RunTest(const FString& Parameters)
{
	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
		LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	const FName AssetName = *FGuid::NewGuid().ToString();

	ISMAssetManager::FCreateStateMachineBlueprintArgs Args;
	Args.Name = AssetName;
	Args.Path = FAssetHandler::DefaultGamePath();
	Args.ParentClass = USMAssetTestInstance::StaticClass();

	// Create a new asset.
	USMBlueprint* NewBP = AssetToolsModule.GetAssetManagerInterface()->CreateStateMachineBlueprint(Args);
	{
		TestEqual("Asset name is exact", NewBP->GetFName(), Args.Name);
		ValidateAsset(this, NewBP, Args);
	}

	const USMAssetTestInstance* CDO = CastChecked<USMAssetTestInstance>(NewBP->GeneratedClass->ClassDefaultObject);
	TestEqual("CDO is default", CDO->OurTestInt, 0);

	// Update asset CDO.
	USMAssetTestInstance* NewAssetData = NewObject<USMAssetTestInstance>();
	{
		const int32 UpdatedValue = 1;
		NewAssetData->OurTestInt = UpdatedValue;

		AssetToolsModule.GetAssetManagerInterface()->PopulateClassDefaults(NewBP, NewAssetData);

		CDO = CastChecked<USMAssetTestInstance>(NewBP->GeneratedClass->ClassDefaultObject);
		TestEqual("CDO has updated", CDO->OurTestInt, UpdatedValue);

		FKismetEditorUtilities::CompileBlueprint(NewBP);

		CDO = CastChecked<USMAssetTestInstance>(NewBP->GeneratedClass->ClassDefaultObject);
		TestEqual("CDO is still updated", CDO->OurTestInt, UpdatedValue);
	}

	return true;
}

/**
 * Generate a state machine graph programatically.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsGenerateGraphTest, "LogicDriver.AssetTools.GenerateGraph",
                                 EAutomationTestFlags::ApplicationContextMask |
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
                                 EAutomationTestFlags::EngineFilter)

bool FAssetToolsGenerateGraphTest::RunTest(const FString& Parameters)
{
	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
		LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	const FName AssetName = *FGuid::NewGuid().ToString();

	ISMAssetManager::FCreateStateMachineBlueprintArgs Args;
	Args.Name = AssetName;
	Args.Path = FAssetHandler::DefaultGamePath();

	// Create a new asset.
	USMBlueprint* NewBP = AssetToolsModule.GetAssetManagerInterface()->CreateStateMachineBlueprint(Args);
	ValidateAsset(this, NewBP, Args);

	USMGraphNode_StateNode* InitialRootState = nullptr;
	{
		ISMGraphGeneration::FCreateStateNodeArgs CreateStateNodeArgs;
		CreateStateNodeArgs.StateName = "Created by Automation";
		CreateStateNodeArgs.StateInstanceClass = USMAssetTestBasicStateInstance::StaticClass();
		CreateStateNodeArgs.bIsEntryState = true;
		InitialRootState = Cast<USMGraphNode_StateNode>(
			AssetToolsModule.GetGraphGenerationInterface()->CreateStateNode(NewBP, CreateStateNodeArgs));
		TestNotNull("State node created", InitialRootState);

		// Add state stack
		{
			ISMGraphGeneration::FCreateStateStackArgs CreateStateStackArgs;
			CreateStateStackArgs.StateStackInstanceClass = USMAssetTestPropertyStateInstance::StaticClass();
			{
				USMStateInstance* StackInstance =
					AssetToolsModule.GetGraphGenerationInterface()->CreateStateStackInstance(
						InitialRootState, CreateStateStackArgs);
				TestNotNull("Stack instance created", StackInstance);
				TestEqual("Stack instance added", StackInstance,
				          Cast<USMStateInstance>(InitialRootState->GetTemplateFromIndex(0)));
			}

			// Should end up first.
			CreateStateStackArgs.StateStackInstanceClass = USMAssetTestPropertyStateTextGraphInstance::StaticClass();
			CreateStateStackArgs.StateStackIndex = 0;
			{
				USMStateInstance* StackInstance =
					AssetToolsModule.GetGraphGenerationInterface()->CreateStateStackInstance(
						InitialRootState, CreateStateStackArgs);
				TestNotNull("Stack instance created", StackInstance);
				TestEqual("Stack instance added", StackInstance,
				          Cast<USMStateInstance>(InitialRootState->GetTemplateFromIndex(0)));
			}
		}
	}

	USMGraphNode_StateNodeBase* SecondStateConduit = nullptr;
	{
		ISMGraphGeneration::FCreateStateNodeArgs CreateStateNodeArgs;
		CreateStateNodeArgs.StateInstanceClass = USMAssetTestConduitInstance::StaticClass();
		CreateStateNodeArgs.NodePosition.X += 250.f;
		SecondStateConduit = AssetToolsModule.GetGraphGenerationInterface()->
		                                      CreateStateNode(NewBP, CreateStateNodeArgs);
		TestNotNull("State node created", SecondStateConduit);
	}

	// Transition
	{
		{
			ISMGraphGeneration::FCreateTransitionEdgeArgs CreateTransitionEdgeArgs;
			CreateTransitionEdgeArgs.FromStateNode = InitialRootState;
			CreateTransitionEdgeArgs.ToStateNode = SecondStateConduit;
			CreateTransitionEdgeArgs.bDefaultToTrue = true;

			USMGraphNode_TransitionEdge* TransitionEdge = AssetToolsModule.GetGraphGenerationInterface()->
			                                                               CreateTransitionEdge(
				                                                               NewBP, CreateTransitionEdgeArgs);
			TestNotNull("Transition created", TransitionEdge);
		}
	}

	USMGraphNode_StateMachineStateNode* ThirdStateStateMachine = nullptr;
	{
		ISMGraphGeneration::FCreateStateNodeArgs CreateStateNodeArgs;
		CreateStateNodeArgs.StateInstanceClass = USMAssetTestStateMachineInstance::StaticClass();
		CreateStateNodeArgs.NodePosition.X += 500.f;
		ThirdStateStateMachine = AssetToolsModule.GetGraphGenerationInterface()->CreateStateNode<
			USMGraphNode_StateMachineStateNode>(NewBP, CreateStateNodeArgs);
		TestNotNull("State node created", ThirdStateStateMachine);
	}

	// Transition
	{
		{
			ISMGraphGeneration::FCreateTransitionEdgeArgs CreateTransitionEdgeArgs;
			CreateTransitionEdgeArgs.FromStateNode = SecondStateConduit;
			CreateTransitionEdgeArgs.ToStateNode = ThirdStateStateMachine;
			CreateTransitionEdgeArgs.bDefaultToTrue = true;

			USMGraphNode_TransitionEdge* TransitionEdge = AssetToolsModule.GetGraphGenerationInterface()->
			                                                               CreateTransitionEdge(
				                                                               NewBP, CreateTransitionEdgeArgs);
			TestNotNull("Transition created", TransitionEdge);
		}
	}

	USMGraphNode_StateNodeBase* NestedRootState = nullptr;
	{
		ISMGraphGeneration::FCreateStateNodeArgs CreateStateNodeArgs;
		CreateStateNodeArgs.StateName = "Created by Automation";
		CreateStateNodeArgs.StateInstanceClass = USMAssetTestBasicStateInstance::StaticClass();
		CreateStateNodeArgs.bIsEntryState = true;
		CreateStateNodeArgs.GraphOwner = ThirdStateStateMachine->GetBoundStateMachineGraph();
		NestedRootState = AssetToolsModule.GetGraphGenerationInterface()->CreateStateNode<USMGraphNode_StateNodeBase>(
			NewBP, CreateStateNodeArgs);
		TestNotNull("State node created", NestedRootState);
	}

	// Create state machine reference
	USMGraphNode_StateMachineStateNode* NestedFSM = nullptr;
	USMBlueprint* ReferenceBP = nullptr;
	{
		// Create the nested FSM node which will link to the reference
		{
			ISMGraphGeneration::FCreateStateNodeArgs CreateStateNodeArgs;
			CreateStateNodeArgs.StateName = "State Machine Reference";
			CreateStateNodeArgs.StateInstanceClass = USMStateMachineInstance::StaticClass();

			NestedFSM = AssetToolsModule.GetGraphGenerationInterface()->CreateStateNode<USMGraphNode_StateMachineStateNode>(
				NewBP, CreateStateNodeArgs);
			TestNotNull("State node created", NestedFSM);
		}

		// Transition to nested FSM node
		{
			ISMGraphGeneration::FCreateTransitionEdgeArgs CreateTransitionEdgeArgs;
			CreateTransitionEdgeArgs.FromStateNode = ThirdStateStateMachine;
			CreateTransitionEdgeArgs.ToStateNode = NestedFSM;
			CreateTransitionEdgeArgs.bDefaultToTrue = true;

			USMGraphNode_TransitionEdge* TransitionEdge = AssetToolsModule.GetGraphGenerationInterface()->
																		   CreateTransitionEdge(
																			   NewBP, CreateTransitionEdgeArgs);
			TestNotNull("Transition created", TransitionEdge);
		}

		// Create reference BP
		Args.Name = *FGuid::NewGuid().ToString();
		ReferenceBP = AssetToolsModule.GetAssetManagerInterface()->CreateStateMachineBlueprint(Args);
		ValidateAsset(this, ReferenceBP, Args);

		// Add state to reference
		{
			ISMGraphGeneration::FCreateStateNodeArgs CreateStateNodeArgs;
			CreateStateNodeArgs.StateName = "Reference state";
			CreateStateNodeArgs.StateInstanceClass = USMAssetTestBasicStateInstance::StaticClass();
			CreateStateNodeArgs.bIsEntryState = true;
			InitialRootState = Cast<USMGraphNode_StateNode>(
				AssetToolsModule.GetGraphGenerationInterface()->CreateStateNode(ReferenceBP, CreateStateNodeArgs));
			TestNotNull("State node created", InitialRootState);
		}

		FKismetEditorUtilities::CompileBlueprint(ReferenceBP); // Compile before linking

		TestFalse("Is reference", NestedFSM->IsStateMachineReference());
		NestedFSM->ReferenceStateMachine(ReferenceBP);
		TestTrue("Is reference", NestedFSM->IsStateMachineReference());
	}

	int32 A, B, C;
	TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C);

	return true;
}

USMBlueprint* GenerateStateMachine(FAutomationTestBase* Test)
{
	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
	LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	const FName AssetName = *FGuid::NewGuid().ToString();

	ISMAssetManager::FCreateStateMachineBlueprintArgs Args;
	Args.Name = AssetName;
	Args.Path = FAssetHandler::DefaultGamePath();

	// Create a new asset.
	USMBlueprint* NewBP = AssetToolsModule.GetAssetManagerInterface()->CreateStateMachineBlueprint(Args);
	ValidateAsset(Test, NewBP, Args);

	USMGraphNode_StateNode* StateNode = nullptr;
	{
		ISMGraphGeneration::FCreateStateNodeArgs CreateStateNodeArgs;
		CreateStateNodeArgs.StateName = "Created by Automation";
		CreateStateNodeArgs.StateInstanceClass = USMAssetTestPropertyStateInstance::StaticClass();
		CreateStateNodeArgs.bIsEntryState = true;
		StateNode = Cast<USMGraphNode_StateNode>(
			AssetToolsModule.GetGraphGenerationInterface()->CreateStateNode(NewBP, CreateStateNodeArgs));
		Test->TestNotNull("State node created", StateNode);

		// Add state stack
		{
			ISMGraphGeneration::FCreateStateStackArgs CreateStateStackArgs;
			CreateStateStackArgs.StateStackInstanceClass = USMAssetTestPropertyStateInstance::StaticClass();
			USMStateInstance* StackInstance =
				AssetToolsModule.GetGraphGenerationInterface()->CreateStateStackInstance(
					StateNode, CreateStateStackArgs);
			Test->TestNotNull("Stack instance created", StackInstance);
			Test->TestEqual("Stack instance added", StackInstance,
			          Cast<USMStateInstance>(StateNode->GetTemplateFromIndex(0)));

			// Stack string
			{
				ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
				PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, ExposedString);
				PropertyArgs.PropertyDefaultValue = "State stack string value";
				PropertyArgs.NodeInstance = StackInstance;
				bool bResult = AssetToolsModule.GetGraphGenerationInterface()->
												SetNodePropertyValue(StateNode, PropertyArgs);
				Test->TestTrue("Property set", bResult);

				Test->TestEqual("Property value set",
						  CastChecked<USMAssetTestPropertyStateInstance>(StateNode->GetTemplateFromIndex(0))->ExposedString,
						  PropertyArgs.PropertyDefaultValue);
			}

			// Stack Text graph
			{
				ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
				PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, TextGraph);
				PropertyArgs.PropertyDefaultValue = "State stack new text graph value";
				PropertyArgs.NodeInstance = StackInstance;
				AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
				Test->TestEqual("Property value set",
						  CastChecked<USMAssetTestPropertyStateInstance>(StateNode->GetTemplateFromIndex(0))->TextGraph.Result.ToString(),
						  PropertyArgs.PropertyDefaultValue);
			}
		}
	}

	USMGraphNode_StateNodeBase* SecondStateNode = nullptr;
	{
		ISMGraphGeneration::FCreateStateNodeArgs CreateStateNodeArgs;
		CreateStateNodeArgs.StateName = "Created by Automation";
		CreateStateNodeArgs.StateInstanceClass = USMAssetTestPropertyStateInstance::StaticClass();
		CreateStateNodeArgs.NodePosition = FVector2D(500.f, 0.f);
		SecondStateNode = AssetToolsModule.GetGraphGenerationInterface()->CreateStateNode(NewBP, CreateStateNodeArgs);
		Test->TestNotNull("State node created", SecondStateNode);
	}

	// Transition
	{
		USMGraphNode_TransitionEdge* TransitionEdge = nullptr;
		ISMGraphGeneration::FCreateTransitionEdgeArgs CreateTransitionEdgeArgs;
		CreateTransitionEdgeArgs.TransitionInstanceClass = USMAssetTestTransitionInstance::StaticClass();
		CreateTransitionEdgeArgs.FromStateNode = StateNode;
		CreateTransitionEdgeArgs.ToStateNode = SecondStateNode;

		TransitionEdge = AssetToolsModule.GetGraphGenerationInterface()->CreateTransitionEdge(
			NewBP, CreateTransitionEdgeArgs);
		Test->TestNotNull("Transition created", TransitionEdge);

		// String
		{
			ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
			PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestTransitionInstance, StringValue);
			PropertyArgs.PropertyDefaultValue = "String value";
			AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(TransitionEdge, PropertyArgs);
			Test->TestEqual("Property value set",
			          TransitionEdge->GetNodeTemplateAs<USMAssetTestTransitionInstance>()->StringValue,
			          PropertyArgs.PropertyDefaultValue);
		}

		// Array
		{
			// Add elements

			ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
			PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestTransitionInstance, IntArray);
			PropertyArgs.PropertyDefaultValue = "1";
			PropertyArgs.PropertyIndex = 0;
			AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(TransitionEdge, PropertyArgs);
			Test->TestEqual("Property value set",
			          FString::FromInt(
				          TransitionEdge->GetNodeTemplateAs<USMAssetTestTransitionInstance>()->IntArray[0]),
			          PropertyArgs.PropertyDefaultValue);

			PropertyArgs.PropertyDefaultValue = "2";
			PropertyArgs.PropertyIndex = 1;
			AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(TransitionEdge, PropertyArgs);
			Test->TestEqual("Property value set",
			          FString::FromInt(
				          TransitionEdge->GetNodeTemplateAs<USMAssetTestTransitionInstance>()->IntArray[1]),
			          PropertyArgs.PropertyDefaultValue);

			PropertyArgs.PropertyDefaultValue = "3";
			PropertyArgs.PropertyIndex = 2;
			AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(TransitionEdge, PropertyArgs);
			Test->TestEqual("Property value set",
					  FString::FromInt(
						  TransitionEdge->GetNodeTemplateAs<USMAssetTestTransitionInstance>()->IntArray[2]),
					  PropertyArgs.PropertyDefaultValue);

			// Remove element
			{
				PropertyArgs.PropertyDefaultValue = "";
				PropertyArgs.PropertyIndex = 1;
				PropertyArgs.ArrayChangeType = ISMGraphGeneration::EArrayChangeType::RemoveElement;
				AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(TransitionEdge, PropertyArgs);

				// Test correct element removed.
				{
					Test->TestEqual("Property value set",
							 TransitionEdge->GetNodeTemplateAs<USMAssetTestTransitionInstance>()->IntArray.Num(),
							 2);

					Test->TestEqual("Property value set",
							  FString::FromInt(
								  TransitionEdge->GetNodeTemplateAs<USMAssetTestTransitionInstance>()->IntArray[0]),
							  "1");

					Test->TestEqual("Property value set",
							  FString::FromInt(
								  TransitionEdge->GetNodeTemplateAs<USMAssetTestTransitionInstance>()->IntArray[1]),
							  "3");
				}

				// Clear elements
				{
					PropertyArgs.PropertyDefaultValue = "";
					PropertyArgs.PropertyIndex = 0;
					PropertyArgs.ArrayChangeType = ISMGraphGeneration::EArrayChangeType::Clear;
					AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(TransitionEdge, PropertyArgs);

					// Test correct element removed.
					{
						Test->TestEqual("Property value set",
								 TransitionEdge->GetNodeTemplateAs<USMAssetTestTransitionInstance>()->IntArray.Num(),
								 0);
					}
				}
			}
		}
	}

	// String
	{
		ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
		PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, ExposedString);
		PropertyArgs.PropertyDefaultValue = "A new string value";
		AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
		Test->TestEqual("Property value set",
		          StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->ExposedString,
		          PropertyArgs.PropertyDefaultValue);
	}

	// Text
	{
		FString LiteralString = "A new text value";
		FText TextValue = NSLOCTEXT("LDTestNS", "LDTestKey", "A new text value");
		ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
		PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, ExposedText);
		PropertyArgs.PropertyDefaultValue = LD::TextUtils::TextToStringBuffer(TextValue);
		AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
		Test->TestEqual("Property literal value set",
		          StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->ExposedText.ToString(),
		          LiteralString);

		FString StringBuffer = LD::TextUtils::TextToStringBuffer(StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->ExposedText);

		Test->TestEqual("Property localization value set",
				  StringBuffer,
				  PropertyArgs.PropertyDefaultValue);
	}

	// Bool
	{
		ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
		PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, ExposedBool);
		PropertyArgs.PropertyDefaultValue = "true";
		AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
		Test->TestEqual("Property value set", StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->ExposedBool,
		          PropertyArgs.PropertyDefaultValue.ToBool());
	}

	// Int
	{
		ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
		PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, ExposedInt);
		PropertyArgs.PropertyDefaultValue = "2";
		AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
		Test->TestEqual("Property value set",
		          FString::FromInt(StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->ExposedInt),
		          PropertyArgs.PropertyDefaultValue);
	}

	// Enum value
	{
		ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
		PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, ExposedEnum);
		PropertyArgs.PropertyDefaultValue = "ValueOne";
		AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
		Test->TestEqual("Property value set", StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->ExposedEnum,
		          static_cast<EAssetTestEnum>(1));
	}

	// String array
	{
		ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
		PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, ExposedStringArray);
		PropertyArgs.PropertyDefaultValue = "Index 1 inserted before index 0";
		PropertyArgs.PropertyIndex = 1;
		AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
		Test->TestEqual("Property value set",
		          StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->ExposedStringArray[1],
		          PropertyArgs.PropertyDefaultValue);

		PropertyArgs.PropertyDefaultValue = "Index 0 set after index 1";
		PropertyArgs.PropertyIndex = 0;
		AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
		Test->TestEqual("Property value set",
		          StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->ExposedStringArray[0],
		          PropertyArgs.PropertyDefaultValue);
	}

	// Soft object value
	/* // Needs asset created for this.
	{
		ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
		PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestStateInstance, ExposedSoftObject);
		PropertyArgs.PropertyDefaultValue = "/Game/SoftObjectTest.SoftObjectTest";
		AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(FromState, PropertyArgs);
		TestEqual("Property value set", FromState->GetNodeTemplateAs<USMAssetTestStateInstance>()->ExposedSoftObject->GetPathName(), PropertyArgs.PropertyDefaultValue);
	}*/

	// Text graph
	{
		FString LiteralString = "A new text graph value";
		FText TextValue = NSLOCTEXT("LDTestNS", "LDTestKey2", "A new text graph value");
		ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
		PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, TextGraph);
		PropertyArgs.PropertyDefaultValue = LD::TextUtils::TextToStringBuffer(TextValue);
		AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
		Test->TestEqual("Property literal value set",
		          StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->TextGraph.Result.ToString(),
		          LiteralString);

		FString StringBuffer = LD::TextUtils::TextToStringBuffer(StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->TextGraph.Result);
		Test->TestEqual("Property localization value set",
				  StringBuffer,
				  PropertyArgs.PropertyDefaultValue);
	}

	// Text graph array
	{
		ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
		PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, TextGraphArray);
		PropertyArgs.PropertyDefaultValue = "Index 1 inserted before index 0";
		PropertyArgs.PropertyIndex = 1;
		AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
		Test->TestEqual("Property value set",
		          StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->TextGraphArray[1].Result.
		          ToString(), PropertyArgs.PropertyDefaultValue);

		PropertyArgs.PropertyDefaultValue = "Index 0 set after index 1";
		PropertyArgs.PropertyIndex = 0;
		AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
		Test->TestEqual("Property value set",
		          StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->TextGraphArray[0].Result.
		          ToString(), PropertyArgs.PropertyDefaultValue);
	}

	// Non-exposed properties
	{
		// String
		{
			ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
			PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, NonExposedString);
			PropertyArgs.PropertyDefaultValue = "Not exposed string";
			AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
			Test->TestEqual("Property value set",
			          StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->NonExposedString,
			          PropertyArgs.PropertyDefaultValue);
		}

		// Text
		{
			ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
			PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, NonExposedText);
			PropertyArgs.PropertyDefaultValue = "Not exposed text";
			AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
			Test->TestEqual("Property value set",
			          StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->NonExposedText.ToString(),
			          PropertyArgs.PropertyDefaultValue);
		}

		// String array
		{
			ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
			PropertyArgs.PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance,
			                                                    NonExposedStringArray);
			PropertyArgs.PropertyDefaultValue = "A new string value";
			PropertyArgs.PropertyIndex = 0;
			AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
			Test->TestEqual("Property value set",
			          StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->NonExposedStringArray[0],
			          PropertyArgs.PropertyDefaultValue);

			PropertyArgs.PropertyDefaultValue = "Another new string value";
			PropertyArgs.PropertyIndex = 1;
			AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(StateNode, PropertyArgs);
			Test->TestEqual("Property value set",
			          StateNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>()->NonExposedStringArray[1],
			          PropertyArgs.PropertyDefaultValue);
		}
	}

	return NewBP;
}

/**
 * Set a wide range of node properties programatically.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsSetNodePropertiesTest, "LogicDriver.AssetTools.SetNodeProperties",
                                 EAutomationTestFlags::ApplicationContextMask |
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
                                 EAutomationTestFlags::EngineFilter)

bool FAssetToolsSetNodePropertiesTest::RunTest(const FString& Parameters)
{
	GenerateStateMachine(this);
	return true;
}

/**
 * Export a state machine then import it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsExportImportAssetTest, "LogicDriver.AssetTools.External.ExportAndImportAsset",
								 EAutomationTestFlags::ApplicationContextMask |
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
								 EAutomationTestFlags::EngineFilter)

bool FAssetToolsExportImportAssetTest::RunTest(const FString& Parameters)
{
	USMBlueprint* NewBP = GenerateStateMachine(this);
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
	LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	const FString Format = "json";
	const FString FileName = NewBP->GetName() + TEXT(".") + Format;
	const FString FilePath = FAssetHandler::GetFullGamePath() / "Export" / FileName;

	bool bExportDelegateHit = false;
	FDelegateHandle ExportDelegateHandle = AssetToolsModule.GetAssetExporter()->OnAssetExported().AddLambda(
		[&](const USMAssetExporter::FExportResult& InResult)
		{
			bExportDelegateHit = true;
			TestEqual("BP in response", InResult.ExportedBlueprint.Get(), NewBP);
			TestEqual("Result success", InResult.ExportStatus, USMAssetExporter::EExportStatus::Success);
		});

	USMAssetExporter::FExportArgs ExportArgs;
	ExportArgs.Blueprint = NewBP;
	ExportArgs.ExportFullFilePath = FilePath;
	ExportArgs.ExportType = Format;
	const USMAssetExporter::FExportResult Result = AssetToolsModule.GetAssetExporter()->ExportAsset(ExportArgs);
	TestEqual("Result success", Result.ExportStatus, USMAssetExporter::EExportStatus::Success);
	TestTrue("Export delegate hit", bExportDelegateHit);

	TestTrue("Export file created", IFileManager::Get().FileExists(*FilePath));

	USMAssetImporter::FImportArgs ImportArgs;
	ImportArgs.ImportType = Format;
	ImportArgs.ImportFullFilePath = FilePath;
	ImportArgs.SaveToContentPath = FAssetHandler::DefaultGamePath() / "Import";

	bool bImportDelegateHit = false;

	FDelegateHandle ImportDelegateHandle = AssetToolsModule.GetAssetImporter()->OnAssetImported().AddLambda(
		[&](const USMAssetImporter::FImportResult& ImportResult)
		{
			bImportDelegateHit = true;
			TestEqual("Import success", ImportResult.ResultStatus, USMAssetImporter::EImportStatus::Success);
			check(TestNotNull("Blueprint created for import", ImportResult.Blueprint.Get()));
			TestTrue("Asset importer valid", ImportResult.AssetImporter.IsValid());
		});

	const USMAssetImporter::FImportResult ImportResult = AssetToolsModule.GetAssetImporter()->ImportAsset(ImportArgs);
	TestTrue("Import delegate hit", bImportDelegateHit);

	TestEqual("Import success", ImportResult.ResultStatus, USMAssetImporter::EImportStatus::Success);
	check(TestNotNull("Blueprint created for import", ImportResult.Blueprint.Get()));

	FKismetEditorUtilities::CompileBlueprint(ImportResult.Blueprint.Get());

	TArray<USMGraphNode_Base*> OriginalGraphNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested(NewBP, OriginalGraphNodes);

	TArray<USMGraphNode_Base*> ImportedGraphNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested(ImportResult.Blueprint.Get(), ImportedGraphNodes);

	TestEqual("Same number of nodes imported", OriginalGraphNodes.Num(), ImportedGraphNodes.Num());

	USMInstance* OriginalCDO = CastChecked<USMInstance>(NewBP->GeneratedClass->ClassDefaultObject);
	USMInstance* ImportedCDO = CastChecked<USMInstance>(ImportResult.Blueprint->GeneratedClass->ClassDefaultObject);

	// Validate node guids.
	{
		TestEqual("CDO guids match", OriginalCDO->GetRootStateMachine().GetNodeGuid(), ImportedCDO->GetRootStateMachine().GetNodeGuid());

		for (const USMGraphNode_Base* OriginalNode : OriginalGraphNodes)
		{
			if (const FSMNode_Base* OriginalRuntimeNode = OriginalNode->FindRuntimeNode())
			{
				USMGraphNode_Base** MatchingImportNode = ImportedGraphNodes.FindByPredicate(
					[OriginalRuntimeNode](const USMGraphNode_Base* ImportedNode)
					{
						if (const FSMNode_Base* ImportedRuntimeNode = ImportedNode->FindRuntimeNode())
						{
							return ImportedRuntimeNode->GetNodeGuid() == OriginalRuntimeNode->GetNodeGuid();
						}

						return false;
					});

				TestNotNull("Matching guid imported", MatchingImportNode);
			}
			else
			{
				TestTrue("Runtime node not found because graph node doesn't have one", OriginalNode->IsA<USMGraphNode_StateMachineEntryNode>());
			}
		}
	}

	// Validate path guids.
	{
		USMInstance* OriginalInstance = USMBlueprintUtils::CreateStateMachineInstance(OriginalCDO->GetClass(), NewObject<USMTestContext>());
		USMInstance* ImportedInstance = USMBlueprintUtils::CreateStateMachineInstance(ImportedCDO->GetClass(), NewObject<USMTestContext>());

		TestEqual("Root state machine node guid equal", OriginalInstance->GetRootStateMachine().GetNodeGuid(), ImportedInstance->GetRootStateMachine().GetNodeGuid());
		TestEqual("Root state machine path guid equal", OriginalInstance->GetRootStateMachine().GetGuid(), ImportedInstance->GetRootStateMachine().GetGuid());

		for (const auto& OriginalNodeKeyVal : OriginalInstance->GetNodeMap())
		{
			FSMNode_Base* const* ImportedRuntimeNode = ImportedInstance->GetNodeMap().Find(OriginalNodeKeyVal.Key);
			TestNotNull("Imported runtime node found by original path guid", ImportedRuntimeNode);
		}
	}

	AssetToolsModule.GetAssetExporter()->OnAssetExported().Remove(ExportDelegateHandle);
	AssetToolsModule.GetAssetImporter()->OnAssetImported().Remove(ImportDelegateHandle);

	return true;
}

/**
 * Export a state machine to memory then import it as raw data.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsExportAssetToMemoryTest, "LogicDriver.AssetTools.External.ExportAssetToMemoryAndImportRaw",
								 EAutomationTestFlags::ApplicationContextMask |
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
								 EAutomationTestFlags::EngineFilter)

bool FAssetToolsExportAssetToMemoryTest::RunTest(const FString& Parameters)
{
	USMBlueprint* NewBP = GenerateStateMachine(this);

	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
	LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	const FString Format = "json";
	const FString FileName = NewBP->GetName() + TEXT(".") + Format;
	const FString FilePath = FAssetHandler::GetFullGamePath() / "Export" / FileName;

	bool bExportDelegateHit = false;
	FDelegateHandle ExportDelegateHandle = AssetToolsModule.GetAssetExporter()->OnAssetExported().AddLambda(
		[&](const USMAssetExporter::FExportResult& InResult)
		{
			bExportDelegateHit = true;
			TestEqual("BP in response", InResult.ExportedBlueprint.Get(), NewBP);
			TestEqual("Result success", InResult.ExportStatus, USMAssetExporter::EExportStatus::Success);

			TestTrue("Asset exporter valid", InResult.AssetExporter.IsValid());
			const USMAssetExporterJson* ExporterJson = CastChecked<USMAssetExporterJson>(InResult.AssetExporter.Get());
			const TSharedPtr<FJsonObject> JsonObject = ExporterJson->GetExportedJsonObject();
			TestTrue("Json valid", JsonObject.IsValid());
		});

	USMAssetExporter::FExportArgs ExportArgs;
	ExportArgs.Blueprint = NewBP;
	ExportArgs.ExportFullFilePath = FilePath;
	ExportArgs.ExportType = Format;
	ExportArgs.bMemoryOnly = true;
	{
		const USMAssetExporter::FExportResult Result = AssetToolsModule.GetAssetExporter()->ExportAsset(ExportArgs);
		TestEqual("Result success", Result.ExportStatus, USMAssetExporter::EExportStatus::Success);
		TestTrue("Export delegate hit", bExportDelegateHit);

		TestFalse("Export file not created", IFileManager::Get().FileExists(*FilePath));
	}

	// Test export with no file path preset. Should work since in memory is true.
	{
		bExportDelegateHit = false;
		ExportArgs.ExportFullFilePath.Empty();
		const USMAssetExporter::FExportResult Result = AssetToolsModule.GetAssetExporter()->ExportAsset(ExportArgs);
		TestEqual("Result success", Result.ExportStatus, USMAssetExporter::EExportStatus::Success);
		TestTrue("Export delegate hit", bExportDelegateHit);

		TestTrue("Asset exporter valid", Result.AssetExporter.IsValid());
		const USMAssetExporterJson* ExporterJson = CastChecked<USMAssetExporterJson>(Result.AssetExporter.Get());
		const TSharedPtr<FJsonObject> JsonObject = ExporterJson->GetExportedJsonObject();
		TestTrue("Json valid", JsonObject.IsValid());

		// Test importing the raw data.
		{
			USMAssetImporter::FImportArgs ImportArgs;
			ImportArgs.ImportType = "json";
			ImportArgs.SaveToContentPath = FAssetHandler::DefaultGamePath() / "Import";

			ImportArgs.ImportData = static_cast<void*>(JsonObject.Get());
			const USMAssetImporter::FImportResult ImportResult = AssetToolsModule.GetAssetImporter()->ImportAsset(ImportArgs);

			// Validate
			TestEqual("Import success", ImportResult.ResultStatus, USMAssetImporter::EImportStatus::Success);
			check(TestNotNull("Blueprint created for import", ImportResult.Blueprint.Get()));

			TArray<USMGraphNode_Base*> OriginalGraphNodes;
			FSMBlueprintEditorUtils::GetAllNodesOfClassNested(NewBP, OriginalGraphNodes);

			TArray<USMGraphNode_Base*> ImportedGraphNodes;
			FSMBlueprintEditorUtils::GetAllNodesOfClassNested(ImportResult.Blueprint.Get(), ImportedGraphNodes);

			TestEqual("Same number of nodes imported", OriginalGraphNodes.Num(), ImportedGraphNodes.Num());
		}
	}

	AssetToolsModule.GetAssetExporter()->OnAssetExported().Remove(ExportDelegateHandle);
	
	return true;
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS
