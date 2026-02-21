// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogLogicDriverContentEditor, Log, All);

#define LDCONTENTEDITOR_LOG_INFO(FMT, ...) UE_LOG(LogLogicDriverContentEditor, Log, (FMT), ##__VA_ARGS__)
#define LDCONTENTEDITOR_LOG_WARNING(FMT, ...) UE_LOG(LogLogicDriverContentEditor, Warning, (FMT), ##__VA_ARGS__)
#define LDCONTENTEDITOR_LOG_ERROR(FMT, ...) UE_LOG(LogLogicDriverContentEditor, Error, (FMT), ##__VA_ARGS__)
