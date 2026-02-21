// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

UENUM()
namespace ESMStateMachineInput
{
	enum Type
	{
		Disabled,
		/** Use the controller assigned to the context if one is available. */
		UseContextController,
		Player0,
		Player1,
		Player2,
		Player3,
		Player4,
		Player5,
		Player6,
		Player7,
	};
}

UENUM()
namespace ESMNodeInput
{
	enum Type
	{
		Disabled,
		/** All input values are determined by the owning state machine. */
		UseOwningStateMachine,
		/** Use the controller assigned to the context if one is available. */
		UseContextController,
		Player0,
		Player1,
		Player2,
		Player3,
		Player4,
		Player5,
		Player6,
		Player7,
	};
}