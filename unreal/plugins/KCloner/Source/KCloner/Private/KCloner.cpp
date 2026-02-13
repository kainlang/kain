// Copyright 2026 K-Studio. All Rights Reserved.

#include "KCloner.h"

#define LOCTEXT_NAMESPACE "FKClonerModule"

void FKClonerModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("K-Cloner initialized"));
}

void FKClonerModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FKClonerModule, KCloner)
