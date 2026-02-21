// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "ISMStateMachineInterface.h"

#define LOCTEXT_NAMESPACE "ISMStateMachineInstance"

USMInstanceInterface::USMInstanceInterface(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UObject* ISMInstanceInterface::GetContext() const
{
	return nullptr;
}

USMStateMachineInterface::USMStateMachineInterface(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void ISMStateMachineInterface::Initialize(UObject* Context)
{
}

void ISMStateMachineInterface::Start()
{
}

void ISMStateMachineInterface::Update(float DeltaSeconds)
{
}

void ISMStateMachineInterface::Stop()
{
}

void ISMStateMachineInterface::Restart()
{
}

void ISMStateMachineInterface::Shutdown()
{
}

USMStateMachineNetworkedInterface::USMStateMachineNetworkedInterface(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

#undef LOCTEXT_NAMESPACE
