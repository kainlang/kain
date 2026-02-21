// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLogicDriverExtended, Log, All);

#define LD_TEXTGRAPH_LOG_INFO(FMT, ...) UE_LOG(LogLogicDriverExtended, Log, (FMT), ##__VA_ARGS__)
#define LD_TEXTGRAPH_LOG_WARNING(FMT, ...) UE_LOG(LogLogicDriverExtended, Warning, (FMT), ##__VA_ARGS__)
#define LD_TEXTGRAPH_LOG_ERROR(FMT, ...) UE_LOG(LogLogicDriverExtended, Error, (FMT), ##__VA_ARGS__)