// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLogicDriverEditor, Log, All);

#define LDEDITOR_LOG_VERBOSE(FMT, ...) UE_LOG(LogLogicDriverEditor, Verbose, (FMT), ##__VA_ARGS__)
#define LDEDITOR_LOG_INFO(FMT, ...) UE_LOG(LogLogicDriverEditor, Log, (FMT), ##__VA_ARGS__)
#define LDEDITOR_LOG_WARNING(FMT, ...) UE_LOG(LogLogicDriverEditor, Warning, (FMT), ##__VA_ARGS__)
#define LDEDITOR_LOG_ERROR(FMT, ...) UE_LOG(LogLogicDriverEditor, Error, (FMT), ##__VA_ARGS__)

DECLARE_STATS_GROUP(TEXT("LogicDriverEditor"), STATGROUP_LogicDriverEditor, STATCAT_Advanced)