// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "ISMStateMachineInterface.generated.h"

struct FSMTransitionTransaction;

UENUM(BlueprintType)
enum ESMNetworkConfigurationType
{
	SM_Client			UMETA(DisplayName = "Client"),
	SM_Server			UMETA(DisplayName = "Server"),
	SM_ClientAndServer	UMETA(DisplayName = "ClientAndServer")
};

UINTERFACE(BlueprintType)
class SMSYSTEM_API USMInstanceInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class SMSYSTEM_API ISMInstanceInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	virtual UObject* GetContext() const;
};

UINTERFACE(BlueprintType)
class SMSYSTEM_API USMStateMachineInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class SMSYSTEM_API ISMStateMachineInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	/** Initialize bound functions and load in the context. */
	virtual void Initialize(UObject* Context = nullptr);

	/** Start the root state machine. */
	virtual void Start();

	/** Manual way of updating the root state machine if tick is disabled. */
	virtual void Update(float DeltaSeconds);

	/** This will complete the state machine's current state and force the machine to end regardless of if the state is an end state. */
	virtual void Stop();

	/** Forcibly restart the state machine and place it back into an entry state. */
	virtual void Restart();
	
	/** Shutdown this instance. Calls Stop.*/
	virtual void Shutdown();
};

UINTERFACE(BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class SMSYSTEM_API USMStateMachineNetworkedInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class SMSYSTEM_API ISMStateMachineNetworkedInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	virtual void ServerInitialize(UObject* Context) {}
	virtual void ServerStart() {}
	virtual void ServerStop() {}
	virtual void ServerShutdown() {}
	virtual void ServerTakeTransition(const FSMTransitionTransaction& TransitionTransactions) {}
	virtual void ServerActivateState(const FGuid& StateGuid, bool bActive, bool bSetAllParents, bool bActivateNowLocally) {}
	virtual void ServerFullSync() {}

	virtual bool HandleNewChannelOpen(class UActorChannel* Channel, struct FReplicationFlags* RepFlags) { return false; }
	virtual void HandleChannelClosed(class UActorChannel* Channel) {}
	virtual bool CanExecuteTransitionEnteredLogic() const { return false; }
	virtual bool HasAuthorityToChangeStates() const  { return false; }
	virtual bool HasAuthorityToChangeStatesLocally() const { return false; }
	virtual bool HasAuthorityToExecuteLogic() const { return false; }
	virtual bool HasAuthorityToTick() const { return false; }

	/** Signals ticking should be possible on the network providing it has authority. */
	virtual void SetCanEverNetworkTick(bool bNewValue) {}
	
	/** Checks if this interface is networked and replicated. */
	UFUNCTION(BlueprintCallable, Category = "Network")
	virtual bool IsConfiguredForNetworking() const  { return false; }
	
	/**
	 * If the interface is considered to have authority for Logic Driver. (Such as an instance running on a server)
	 * This is not necessarily the same as UE's native HasAuthority.
	 */
	UFUNCTION(BlueprintCallable, Category = "Network")
	virtual bool HasAuthority() const { return false; }

	/** If this interface is only a simulated proxy. */
	UFUNCTION(BlueprintCallable, Category = "Network")
	virtual bool IsSimulatedProxy() const { return false; }
};