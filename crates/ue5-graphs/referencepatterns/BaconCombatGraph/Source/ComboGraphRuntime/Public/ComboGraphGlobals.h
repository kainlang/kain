// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

//Current Class Name + Function Name where this is called!
#define JOYSTR_CUR_CLASS_FUNC (FString(__FUNCTION__))

//Current Class where this is called!
#define JOYSTR_CUR_CLASS (FString(__FUNCTION__).Left(FString(__FUNCTION__).Find(TEXT(":"))) )

//Current Function Name where this is called!
#define JOYSTR_CUR_FUNC (FString(__FUNCTION__).Right(FString(__FUNCTION__).Len() - FString(__FUNCTION__).Find(TEXT("::")) - 2))

//Current Line Number in the code where this is called!
#define JOYSTR_CUR_LINE  (FString::FromInt(__LINE__))

//Current Class and Line Number where this is called!
#define JOYSTR_CUR_CLASS_LINE (JOYSTR_CUR_CLASS + "(" + JOYSTR_CUR_LINE + ")")

//Current Function Signature where this is called!
#define JOYSTR_CUR_FUNCSIG (FString(__FUNCSIG__))

#define MISC_LOG(Format, ...) UE_LOG(LogTemp, Log, TEXT("[%s]: %s"), *JOYSTR_CUR_FUNC, *FString::Printf(TEXT(Format), ##__VA_ARGS__ ))
#define ERR_LOG(Format, ...) UE_LOG(LogTemp, Error, TEXT("[%s]: %s"), *JOYSTR_CUR_FUNC, *FString::Printf(TEXT(Format), ##__VA_ARGS__ ))