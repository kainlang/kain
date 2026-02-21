// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLogicDriverExtendedEditor, Log, All);

#define LDEDITOR_TEXTGRAPH_LOG_INFO(FMT, ...) UE_LOG(LogLogicDriverExtendedEditor, Log, (FMT), ##__VA_ARGS__)
#define LDEDITOR_TEXTGRAPH_LOG_WARNING(FMT, ...) UE_LOG(LogLogicDriverExtendedEditor, Warning, (FMT), ##__VA_ARGS__)
#define LDEDITOR_TEXTGRAPH_LOG_ERROR(FMT, ...) UE_LOG(LogLogicDriverExtendedEditor, Error, (FMT), ##__VA_ARGS__)