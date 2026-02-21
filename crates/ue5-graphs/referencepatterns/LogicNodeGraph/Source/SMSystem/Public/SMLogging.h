// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLogicDriver, Log, All);

#define LD_LOG_VERBOSE(FMT, ...) UE_LOG(LogLogicDriver, Verbose, (FMT), ##__VA_ARGS__)
#define LD_LOG_INFO(FMT, ...) UE_LOG(LogLogicDriver, Log, (FMT), ##__VA_ARGS__)
#define LD_LOG_WARNING(FMT, ...) UE_LOG(LogLogicDriver, Warning, (FMT), ##__VA_ARGS__)
#define LD_LOG_ERROR(FMT, ...) UE_LOG(LogLogicDriver, Error, (FMT), ##__VA_ARGS__)

DECLARE_STATS_GROUP(TEXT("LogicDriver"), STATGROUP_LogicDriver, STATCAT_Advanced)