// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLogicDriverSearch, Log, All);

#define LDSEARCH_LOG_INFO(FMT, ...) UE_LOG(LogLogicDriverSearch, Log, (FMT), ##__VA_ARGS__)
#define LDSEARCH_LOG_WARNING(FMT, ...) UE_LOG(LogLogicDriverSearch, Warning, (FMT), ##__VA_ARGS__)
#define LDSEARCH_LOG_ERROR(FMT, ...) UE_LOG(LogLogicDriverSearch, Error, (FMT), ##__VA_ARGS__)

DECLARE_STATS_GROUP(TEXT("LogicDriverSearch"), STATGROUP_LogicDriverSearch, STATCAT_Advanced)