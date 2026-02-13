// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerFactory.h"
#include "KClonerData.h"

UKClonerFactory::UKClonerFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UKClonerData::StaticClass();
}

UObject* UKClonerFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UKClonerData>(InParent, InClass, InName, Flags);
}
