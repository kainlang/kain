// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Properties/SMTextGraphProperty.h"
#include "SMConduitInstance.h"
#include "SMNode_Info.h"
#include "SMStateInstance.h"
#include "SMStateMachineComponent.h"
#include "SMStateMachineInstance.h"
#include "SMTransitionInstance.h"

#include "SMTestContext.generated.h"

class USMInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTestDelegateSignature);

USTRUCT()
struct FSMTestData
{
	GENERATED_BODY()

	int32 Count = 0;
	double TimeStamp = 0.f;

	void Increase(bool bTakeTimeStamp = true)
	{
		Count++;
		if (bTakeTimeStamp)
		{
			TakeTimeStamp();
		}
	}

	void TakeTimeStamp()
	{
		TimeStamp = FPlatformTime::Seconds();
	}
};

UCLASS(NotBlueprintable, NotBlueprintable, Transient)
class ULambdaWrapper : public UObject
{
	GENERATED_BODY()

public:
	FSMTestData OnInitializeHit;
	FSMTestData OnStartHit;
	FSMTestData OnStoppedHit;
	FSMTestData OnShutdownHit;

	UFUNCTION()
	void OnInitialize(USMInstance* Instance)
	{
		OnInitializeHit.Increase();
	}

	UFUNCTION()
	void OnStart(USMInstance* Instance)
	{
		OnStartHit.Increase();
	}

	UFUNCTION()
	void OnStop(USMInstance* Instance)
	{
		OnStoppedHit.Increase();
	}

	UFUNCTION()
	void OnShutdown(USMInstance* Instance)
	{
		OnShutdownHit.Increase();
	}
};

UCLASS(Blueprintable)
class USMTestContext : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseEntryInt() { TestEntryInt++;}

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseUpdateInt(float Value);

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseEndInt() { TestEndInt++;}

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	int32 GetEntryInt() const { return TestEntryInt; }

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	int32 GetUpdateFromDeltaSecondsInt() const { return TestUpdateFromDeltaSecondsInt; }

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	int32 GetEndInt() const { return TestEndInt; }

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	bool CanTransition() const { return bCanTransition; }

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseTransitionInit();

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseTransitionShutdown();

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseTransitionPreEval() { TestTransitionPreEval.Increase(); }

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseTransitionPostEval() { TestTransitionPostEval.Increase(); }

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseTransitionTaken() { TestTransitionEntered.Increase(); }

	/** Quick test for feeding in float data. */
	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	bool FloatGreaterThan(float Input) const
	{
		return Input > GreaterThanTest;
	}

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void SetTestReference(USMInstance* Instance)
	{
		TestReference = Instance;
	}
	
	void Reset()
	{
		TestEntryInt = 0;
		TestTransitionsHit = 0;
		TestStatesHit = 0;
		bCanTransition = false;
	}

	UFUNCTION()
	void OnTransitionTaken(USMInstance* Instance, FSMTransitionInfo Transition)
	{
		TestTransitionsHit++;
	}

	UFUNCTION()
	void OnStateChanged(USMInstance* Instance, FSMStateInfo To, FSMStateInfo From)
	{
		TestStatesHit++;
	}

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 TestEntryInt = 0;

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 TestUpdateFromDeltaSecondsInt = 0;

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 TestEndInt = 0;

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 TestTransitionsHit = 0;

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 TestStatesHit = 0;
	
	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	bool bCanTransition = true;

	UPROPERTY()
	FSMTestData TestTransitionInit;

	UPROPERTY()
	FSMTestData TestTransitionShutdown;

	UPROPERTY()
	FSMTestData TestTransitionPreEval;

	UPROPERTY()
	FSMTestData TestTransitionPostEval;

	UPROPERTY()
	FSMTestData TestTransitionEntered;

	FSMTestData TimesUpdateHit;

	UPROPERTY()
	TObjectPtr<USMInstance> TestReference;

	UPROPERTY(BlueprintAssignable, Category = "State Machine Tests")
	FTestDelegateSignature TransitionEvent;
	
	static float GreaterThanTest;
};

UCLASS(Blueprintable)
class USMStateTestInstance : public USMStateInstance
{
public:
	GENERATED_BODY()
		
	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 ExposedInt;

	UPROPERTY()
	FSMTestData StateBeginHit;

	UPROPERTY()
	FSMTestData StateUpdateHit;

	UPROPERTY()
	FSMTestData StateEndHit;
	
	UPROPERTY()
	FSMTestData StateMachineStartHit;

	UPROPERTY()
	FSMTestData StateMachineStopHit;

	UPROPERTY()
	FSMTestData StateBeginEventHit;

	UPROPERTY()
	FSMTestData StateUpdateEventHit;

	UPROPERTY()
	FSMTestData StateEndEventHit;

	UPROPERTY()
	FSMTestData StateInitializedEventHit;

	UPROPERTY()
	FSMTestData StateShutdownEventHit;

	UPROPERTY()
	FSMTestData ConstructionScriptHit;
	
	UFUNCTION()
	void OnStateBeginEventFunc(USMStateInstance_Base* Instance);

	UFUNCTION()
	void OnStateUpdateEventFunc(USMStateInstance_Base* Instance, float DeltaSeconds);

	UFUNCTION()
	void OnStateEndEventFunc(USMStateInstance_Base* Instance);
	
protected:
	virtual void OnStateBegin_Implementation() override;
	virtual void OnStateUpdate_Implementation(float DeltaSeconds) override;
	virtual void OnStateEnd_Implementation() override;
	virtual void OnRootStateMachineStart_Implementation() override;
	virtual void OnRootStateMachineStop_Implementation() override;
	virtual void OnStateInitialized_Implementation() override;
	virtual void OnStateShutdown_Implementation() override;
	virtual void ConstructionScript_Implementation() override;

	virtual void NativeInitialize() override { check(!bNativeInitialized); bNativeInitialized = true; Super::NativeInitialize(); }
	virtual void NativeShutdown() override { check(bNativeInitialized); bNativeInitialized = false; Super::NativeShutdown(); }

	bool bNativeInitialized = false;

public:
	UPROPERTY(BlueprintAssignable, Category = "State Machine Tests")
	FTestDelegateSignature StateEvent;

	UPROPERTY()
	FGuid GuidSetFromConstruction;

	static bool bTestEditorGuids;
};

UCLASS(Blueprintable)
class USMStateArrayTestInstance : public USMStateTestInstance
{
public:
	GENERATED_BODY()

	static int32 ExposedIntArrDefaultValue1;
	static int32 ExposedIntArrDefaultValue2;

	USMStateArrayTestInstance();
	
	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	TArray<int32> ExposedIntArray;

};

UCLASS(Blueprintable)
class USMStateConstructionTestInstance : public USMStateTestInstance
{
public:
	GENERATED_BODY()

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	
	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	FString SetByConstructionScript;

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 CanReadNextStates;

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 CanReadPreviousStates;

	UPROPERTY()
	FSMTestData PostEditChangeHit;

	UPROPERTY()
	FString NameSetByCreator;
protected:
	virtual void ConstructionScript_Implementation() override;

};

/**
 * For adding to the state stack during construction.
 */
UCLASS(Blueprintable)
class USMStateStackConstructionTestInstance : public USMStateTestInstance
{
public:
	GENERATED_BODY()

	static const FString StackName1;
	static const FString StackName2;
	static const FString StackName3;

	// -3 none, -2 add, -1 all, 0+ specific. Resets to -3 each time since construction scripts can run twice per pass.
	int32 RemoveIndex = -3;

protected:
	virtual void ConstructionScript_Implementation() override;

};

UCLASS(Blueprintable)
class USMStatePropertyResetTestInstance : public USMStateInstance
{
	GENERATED_BODY()

public:
	USMStatePropertyResetTestInstance()
	{
		StringVarArrayExposed = {"A_Exposed", "B_Exposed", "C_Exposed"};
	}
	
	UPROPERTY()
	int32 IntVar;

	UPROPERTY(BlueprintReadWrite, Category=Test)
	FString StringVar;

	UPROPERTY()
	TObjectPtr<UObject> ObjectValue;

	UPROPERTY()
	TArray<FString> StringVarArray;

	UPROPERTY(BlueprintReadWrite, Category=Test)
	TArray<FString> StringVarArrayExposed;

	UPROPERTY()
	TSet<FString> StringVarSet;

	UPROPERTY()
	TMap<FString, FString> StringVarMap;
};

UCLASS(Blueprintable)
class USMStateAuxiliaryPropertyResetTestInstance : public USMStateInstance
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bBoolVar = false;

	UPROPERTY(BlueprintReadWrite, Category=Test)
	FText TextVar;

	UPROPERTY()
	TArray<FString> StringVarArray;
};

UCLASS()
class USMStateEditorPropertyResetTestInstance : public USMStateInstance
{
	GENERATED_BODY()

public:
	USMStateEditorPropertyResetTestInstance();

	static int32 DefaultIntValue;

	UPROPERTY(BlueprintReadWrite, Category=Test)
	int32 IntVar;
};

UCLASS(Blueprintable)
class USMTransitionConstructionTestInstance : public USMTransitionInstance
{
public:
	GENERATED_BODY()

	UPROPERTY()
	FSMTestData ConstructionScriptHit;
	
protected:
	virtual void ConstructionScript_Implementation() override;
	virtual void OnPreCompileValidate_Implementation(USMCompilerLog* CompilerLog) const override;
	virtual void NativeInitialize() override { check(!bNativeInitialized); bNativeInitialized = true; Super::NativeInitialize(); }
	virtual void NativeShutdown() override { check(bNativeInitialized); bNativeInitialized = false; Super::NativeShutdown(); }

	virtual bool CanEnterTransition_Implementation() const override;

	bool bNativeInitialized = false;
};

UCLASS(Blueprintable)
class USMStateTestInstance2 : public USMStateTestInstance
{
public:
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 AnotherExposedInt;
};

UCLASS(Blueprintable)
class USMStateReadOnlyTestInstance : public USMStateInstance
{
public:
	GENERATED_BODY()

	static int32 DefaultReadOnlyValue;
	
	USMStateReadOnlyTestInstance();
	
	UPROPERTY(BlueprintReadOnly, Category = "State Machine Tests")
	int32 ReadOnlyInt;
};

UCLASS(Blueprintable)
class USMStateManualTransitionTestInstance : public USMStateTestInstance
{
public:
	GENERATED_BODY()

protected:
	virtual void OnStateUpdate_Implementation(float DeltaSeconds) override;
};

UCLASS(Blueprintable)
class USMStateEvaluateFromManuallyBoundEventTestInstance : public USMStateManualTransitionTestInstance
{
public:
	GENERATED_BODY()

protected:
	virtual void OnStateUpdate_Implementation(float DeltaSeconds) override;
};

UCLASS(Blueprintable)
class USMStateMachineTestInstance : public USMStateMachineInstance
{
public:
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 ExposedInt;

	UPROPERTY()
	FSMTestData StateBeginHit;

	UPROPERTY()
	FSMTestData StateUpdateHit;

	UPROPERTY()
	FSMTestData StateEndHit;

	UPROPERTY()
	FSMTestData RootSMStartHit;

	UPROPERTY()
	FSMTestData RootSMStopHit;

	UPROPERTY()
	FSMTestData InitializeHit;

	UPROPERTY()
	FSMTestData ShutdownHit;

	UPROPERTY()
	FSMTestData EndStateReachedHit;

	UPROPERTY()
	FSMTestData StateMachineCompletedHit;
protected:
	virtual void OnStateBegin_Implementation() override;
	virtual void OnStateUpdate_Implementation(float DeltaSeconds) override;
	virtual void OnStateEnd_Implementation() override;
	virtual void OnRootStateMachineStart_Implementation() override;
	virtual void OnRootStateMachineStop_Implementation() override;
	virtual void OnStateInitialized_Implementation() override;
	virtual void OnStateShutdown_Implementation() override;

	virtual void OnEndStateReached_Implementation() override;
	virtual void OnStateMachineCompleted_Implementation() override;

	virtual void NativeInitialize() override { check(!bNativeInitialized); bNativeInitialized = true; Super::NativeInitialize(); }
	virtual void NativeShutdown() override { check(bNativeInitialized); bNativeInitialized = false; Super::NativeShutdown(); }

	bool bNativeInitialized = false;
};

UCLASS(Blueprintable)
class USMStateMachineReferenceTestInstance : public USMStateMachineTestInstance
{
public:
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	FString SetByConstructionScript;

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 CanReadNextStates;

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 CanReadPreviousStates;

protected:
	virtual void ConstructionScript_Implementation() override;
	virtual void OnStateBegin_Implementation() override;

};

UCLASS(Blueprintable)
class USMConduitTestInstance : public USMConduitInstance
{
public:
	GENERATED_BODY()
	
	UPROPERTY()
	int32 IntValue;

	UPROPERTY()
	bool bCanTransition = false;

	UPROPERTY()
	FSMTestData ConduitEnteredEventHit;

	UPROPERTY()
	FSMTestData ConduitInitializedHit;

	UPROPERTY()
	FSMTestData ConduitShutdownHit;

protected:
	virtual bool CanEnterTransition_Implementation() const override { return bCanTransition; }

	virtual void OnConduitEntered_Implementation() override { ConduitEnteredEventHit.Increase(); }
	virtual void OnConduitInitialized_Implementation() override { ConduitInitializedHit.Increase(); }
	virtual void OnConduitShutdown_Implementation() override { ConduitShutdownHit.Increase(); }

	virtual void NativeInitialize() override { check(!bNativeInitialized); bNativeInitialized = true; Super::NativeInitialize(); }
	virtual void NativeShutdown() override { check(bNativeInitialized); bNativeInitialized = false; Super::NativeShutdown(); }

	bool bNativeInitialized = false;
};

UCLASS(Blueprintable)
class USMTransitionTestInstance : public USMTransitionInstance
{
public:
	GENERATED_BODY()

	UPROPERTY()
	int32 IntValue;

	UPROPERTY()
	FSMTestData TransitionEnteredEventHit;

	UPROPERTY()
	FSMTestData TransitionInitializedHit;

	UPROPERTY()
	FSMTestData TransitionShutdownHit;
	
	UPROPERTY()
	FSMTestData TransitionRootSMStartHit;
	
	UPROPERTY()
	FSMTestData TransitionRootSMStopHit;
	
	UFUNCTION()
	void OnTransitionEnteredEventFunc(USMTransitionInstance* TransitionInstance);

	UPROPERTY()
	bool bCanTransition = false;
protected:
	virtual void OnTransitionInitialized_Implementation() override;
	virtual void OnTransitionShutdown_Implementation() override;
	virtual bool CanEnterTransition_Implementation() const override { return bCanTransition; }

	virtual void OnRootStateMachineStart_Implementation() override;
	virtual void OnRootStateMachineStop_Implementation() override;

	virtual void NativeInitialize() override { check(!bNativeInitialized); bNativeInitialized = true; Super::NativeInitialize(); }
	virtual void NativeShutdown() override { check(bNativeInitialized); bNativeInitialized = false; Super::NativeShutdown(); }

	bool bNativeInitialized = false;
};

UCLASS(Blueprintable)
class USMTransitionStackTestInstance : public USMTransitionInstance
{
public:
	GENERATED_BODY()
};

#define TEXTGRAPH_NAMESPACE "TextGraphNamespace"

UCLASS(Blueprintable)
class USMTextGraphState : public USMStateInstance
{
public:
	GENERATED_BODY()

	/** TextGraph default text from ctor. */
	static FText DefaultText;
	
	USMTextGraphState();
	
	UPROPERTY(EditDefaultsOnly, Category = "Test")
	FSMTextGraphProperty TextGraph;

	UPROPERTY(BlueprintReadWrite, Category = "Test", meta = (HideOnNode))
	FText EvaluatedText;

protected:
	virtual void OnStateBegin_Implementation() override;
};

UCLASS(Blueprintable)
class USMTextGraphArrayState : public USMStateInstance
{
public:
	GENERATED_BODY()

	static FText DefaultText_1;
	static FText DefaultText_2;
	
	USMTextGraphArrayState();
	
	UPROPERTY(EditDefaultsOnly, Category = "Test")
	TArray<FSMTextGraphProperty> TextGraphArray;

	TArray<FText> EvaluatedTextArray;
	
protected:
	virtual void OnStateBegin_Implementation() override;
};

UCLASS(Blueprintable)
class USMTextGraphStateExtra : public USMTextGraphState
{
public:
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Test", meta = (DisplayOrder = 1))
	FString StringVar;
};

UCLASS()
class USMTestObject : public UObject
{
public:
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Test")
	FText CustomToText() const { return FText::FromString("Test serializer"); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Test")
	FText GlobalCustomToText() const { return FText::FromString("Test serializer from global settings"); }
};

UCLASS()
class USMStateMachineTestComponent : public USMStateMachineComponent
{
	GENERATED_UCLASS_BODY()

public:
	void SetStateMachineClass(UClass* NewClass);
	void ClearTemplateInstance();
	
	void SetAllowTick(bool bAllowOverride, bool bCanEverTick);
	void SetTickInterval(bool bAllowOverride, float TickInterval);

	void ImportDeprecatedProperties_Public();
};

static void RecordTime(double& InOutVariable)
{
#if !PLATFORM_WINDOWS
	FPlatformProcess::Sleep(0.001f);
#endif
	InOutVariable = FPlatformTime::Cycles64();
}

UCLASS(Blueprintable)
class USMOrderState : public USMStateTestInstance
{
public:
	GENERATED_BODY()
	
	USMOrderState() {}

	double Time_Start = 0.f;
	double Time_Update = 0.f;
	double Time_End = 0.f;

	double Time_Initialize = 0.f;
	double Time_Shutdown = 0.f;

	double Time_RootStart = 0.f;
	double Time_RootStop = 0.f;
	
protected:
	virtual void OnStateBegin_Implementation() override
	{
		RecordTime(Time_Start);
		Super::OnStateBegin_Implementation();
	}

	virtual void OnStateUpdate_Implementation(float DeltaSeconds) override
	{
		RecordTime(Time_Update);
		Super::OnStateUpdate_Implementation(DeltaSeconds);
	}

	virtual void OnStateEnd_Implementation() override
	{
		RecordTime(Time_End);
		Super::OnStateEnd_Implementation();
	}

	virtual void OnStateInitialized_Implementation() override
	{
		RecordTime(Time_Initialize);
		Super::OnStateInitialized_Implementation();
	}
	
	virtual void OnStateShutdown_Implementation() override
	{
		RecordTime(Time_Shutdown);
		Super::OnStateShutdown_Implementation();
	}

	virtual void OnRootStateMachineStart_Implementation() override
	{
		RecordTime(Time_RootStart);
		Super::OnRootStateMachineStart_Implementation();
	}

	virtual void OnRootStateMachineStop_Implementation() override
	{
		RecordTime(Time_RootStop);
		Super::OnRootStateMachineStop_Implementation();
	}
};

UCLASS(Blueprintable)
class USMOrderTransition : public USMTransitionTestInstance
{
public:
	GENERATED_BODY()
	
	USMOrderTransition() {}

	double Time_Entered = 0.f;
	
	double Time_Initialize = 0.f;
	double Time_Shutdown = 0.f;

	double Time_RootStart = 0.f;
	double Time_RootStop = 0.f;
	
protected:
	virtual void OnTransitionEntered_Implementation() override
	{
		RecordTime(Time_Entered);
		Super::OnTransitionEntered_Implementation();
	}
	
	virtual void OnTransitionInitialized_Implementation() override
	{
		RecordTime(Time_Initialize);
		Super::OnTransitionInitialized_Implementation();
	}
	
	virtual void OnTransitionShutdown_Implementation() override
	{
		RecordTime(Time_Shutdown);
		Super::OnTransitionShutdown_Implementation();
	}

	virtual void OnRootStateMachineStart_Implementation() override
	{
		RecordTime(Time_RootStart);
		Super::OnRootStateMachineStart_Implementation();
	}

	virtual void OnRootStateMachineStop_Implementation() override
	{
		RecordTime(Time_RootStop);
		Super::OnRootStateMachineStop_Implementation();
	}

	virtual bool CanEnterTransition_Implementation() const override
	{
		return true;
	}
};

UCLASS(Blueprintable)
class USMOrderConduit : public USMConduitTestInstance
{
public:
	GENERATED_BODY()
	
	USMOrderConduit() {}

	double Time_Start = 0.f;
	double Time_Update = 0.f;
	double Time_End = 0.f;

	double Time_Initialize = 0.f;
	double Time_Shutdown = 0.f;

	double Time_RootStart = 0.f;
	double Time_RootStop = 0.f;

	double Time_Entered = 0.f;
	
protected:
	virtual void OnStateBegin_Implementation() override
	{
		RecordTime(Time_Start);
		Super::OnStateBegin_Implementation();
	}

	virtual void OnStateUpdate_Implementation(float DeltaSeconds) override
	{
		RecordTime(Time_Update);
		Super::OnStateUpdate_Implementation(DeltaSeconds);
	}

	virtual void OnStateEnd_Implementation() override
	{
		RecordTime(Time_End);
		Super::OnStateEnd_Implementation();
	}

	virtual void OnRootStateMachineStart_Implementation() override
	{
		RecordTime(Time_RootStart);
		Super::OnRootStateMachineStart_Implementation();
	}

	virtual void OnRootStateMachineStop_Implementation() override
	{
		RecordTime(Time_RootStop);
		Super::OnRootStateMachineStop_Implementation();
	}

	virtual void OnConduitInitialized_Implementation() override
	{
		RecordTime(Time_Initialize);
		Super::OnConduitInitialized_Implementation();
	}

	virtual void OnConduitShutdown_Implementation() override
	{
		RecordTime(Time_Shutdown);
		Super::OnConduitShutdown_Implementation();
	}

	virtual void OnConduitEntered_Implementation() override
	{
		RecordTime(Time_Entered);
		Super::OnConduitEntered_Implementation();
	}

	virtual bool CanEnterTransition_Implementation() const override
	{
		return true;
	}
};

UCLASS(Blueprintable)
class USMMetaDataState : public USMStateInstance
{
public:
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category=Test, meta = (Categories = "Quests"))
	FGameplayTag Tag;
};

UCLASS(Blueprintable)
class USMOrderStateMachine : public USMStateMachineInstance
{
public:
	GENERATED_BODY()
	
	USMOrderStateMachine() {}

	double Time_Start = 0.f;
	double Time_Update = 0.f;
	double Time_End = 0.f;

	double Time_Initialize = 0.f;
	double Time_Shutdown = 0.f;

	double Time_RootStart = 0.f;
	double Time_RootStop = 0.f;
	
	double Time_EndState = 0.f;
	double Time_OnCompleted = 0.f;
	
protected:
	virtual void OnStateBegin_Implementation() override
	{
		RecordTime(Time_Start);
	}

	virtual void OnStateUpdate_Implementation(float DeltaSeconds) override
	{
		RecordTime(Time_Update);
	}

	virtual void OnStateEnd_Implementation() override
	{
		RecordTime(Time_End);
	}

	virtual void OnStateInitialized_Implementation() override
	{
		RecordTime(Time_Initialize);
	}
	
	virtual void OnStateShutdown_Implementation() override
	{
		RecordTime(Time_Shutdown);
	}

	virtual void OnRootStateMachineStart_Implementation() override
	{
		RecordTime(Time_RootStart);
	}

	virtual void OnRootStateMachineStop_Implementation() override
	{
		RecordTime(Time_RootStop);
	}

	virtual void OnEndStateReached_Implementation() override
	{
		RecordTime(Time_End);
	}

	virtual void OnStateMachineCompleted_Implementation() override
	{
		RecordTime(Time_OnCompleted);
	}
};

UCLASS(Blueprintable)
class USMTestPreCompileState : public USMStateInstance
{
	GENERATED_BODY()

public:
	FString LogMessage = TEXT("An error message!");
	ESMCompilerLogType LogType = ESMCompilerLogType::Error;

	bool bLogProperty = false;
	bool bLogPropertySilent = false;

	UPROPERTY(BlueprintReadWrite, Category=Test)
	int32 TestProperty;

protected:
	virtual void OnPreCompileValidate_Implementation(USMCompilerLog* CompilerLog) const override
	{
		if (bLogProperty)
		{
			CompilerLog->LogProperty(GET_MEMBER_NAME_CHECKED(USMTestPreCompileState, TestProperty), this, LogMessage, LogType, false, bLogPropertySilent);
		}
		else
		{
			CompilerLog->Log(LogType, LogMessage);
		}
	}
};

UCLASS()
class UTestNestedInstanceSubObject : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class UTestInstanceSubObject : public UObject
{
	GENERATED_BODY()

	UTestInstanceSubObject()
	{
		NestedObject = CreateDefaultSubobject<UTestNestedInstanceSubObject>("NestedObject");
	}
public:

	UPROPERTY(Instanced)
	TObjectPtr<UTestNestedInstanceSubObject> NestedObject;
};

UCLASS()
class USMTestInstancedObjectState : public USMStateInstance
{
	GENERATED_BODY()

public:

	UPROPERTY(Instanced)
	TObjectPtr<UTestInstanceSubObject> InstanceObject;

	UPROPERTY(Instanced)
	TArray<TObjectPtr<UTestInstanceSubObject>> InstanceObjectArray;
};

UCLASS(Blueprintable)
class USMStateIsStoppingTestInstance : public USMStateTestInstance
{
public:
	GENERATED_BODY()

	// If true, an ensure is called OnStateEnd that the owning SMInstance IsStopping is true. Otherwise, the ensure
	// checks it is false.
	bool bEnsureIsStoppingOnStateEnd = false;

	USMStateIsStoppingTestInstance();

	virtual void OnStateEnd_Implementation() override;
};

UCLASS()
class USMTestDialogueInstance : public USMInstance
{
	GENERATED_BODY()

public:
	USMTestDialogueInstance();
	
	UFUNCTION(BlueprintCallable, Category=Test)
	FText GetTextFunction() const;

	UPROPERTY(BlueprintReadWrite, Category=Test)
	FText TextVar;
};

UCLASS()
class USMTestNotifyInstance : public USMInstance
{
	GENERATED_BODY()

public:

	virtual void OnStateMachineStart_Implementation() override;
	virtual void OnStateMachineStop_Implementation() override;
	virtual void OnStateMachineStateChanged_Implementation(const FSMStateInfo& ToState, const FSMStateInfo& FromState) override;
	virtual void OnStateMachineStateStarted_Implementation(const FSMStateInfo& State) override;
	virtual void OnStateMachineTransitionTaken_Implementation(const FSMTransitionInfo& Transition) override;

	bool bTestLastStateStarted =  true;
	
	int32 TimesStateChangedHit = 0;
	int32 TimesStateStartedHit = 0;
	int32 TimesTransitionTakenHit = 0;
	int32 TimesStateMachineStarted = 0;
	int32 TimesStateMachineStopped = 0;
	
	FGuid LastFromStateGuid;
	FGuid LastToStateGuid;

	FGuid LastStateStartedGuid;

	FGuid LastTransitionGuid;
};

UCLASS(Blueprintable)
class USMTestRenameState : public USMStateInstance
{
	GENERATED_BODY()

	virtual void ConstructionScript_Implementation() override;

public:
	UPROPERTY()
	FString NewNodeName = "NewStateName";

	UPROPERTY()
	bool bShouldResetName = false;
};

UCLASS(Blueprintable)
class USMTestRenameTransition : public USMTransitionInstance
{
	GENERATED_BODY()

	virtual void ConstructionScript_Implementation() override;

public:
	UPROPERTY()
	FString NewNodeName = "NewTransitionName";

	UPROPERTY()
	bool bShouldResetName = false;
};