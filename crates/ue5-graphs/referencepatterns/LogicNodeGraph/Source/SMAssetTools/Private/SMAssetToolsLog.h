// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLogicDriverAssetTools, Log, All);

#define LDASSETTOOLS_LOG_INFO(FMT, ...) UE_LOG(LogLogicDriverAssetTools, Log, (FMT), ##__VA_ARGS__)
#define LDASSETTOOLS_LOG_WARNING(FMT, ...) UE_LOG(LogLogicDriverAssetTools, Warning, (FMT), ##__VA_ARGS__)
#define LDASSETTOOLS_LOG_ERROR(FMT, ...) UE_LOG(LogLogicDriverAssetTools, Error, (FMT), ##__VA_ARGS__)

DECLARE_STATS_GROUP(TEXT("LogicDriverAssetTools"), STATGROUP_LogicDriverAssetTools, STATCAT_Advanced)