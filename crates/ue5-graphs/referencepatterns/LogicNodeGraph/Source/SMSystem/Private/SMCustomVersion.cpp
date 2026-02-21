// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMCustomVersion.h"

#include "Serialization/CustomVersion.h"

const FGuid FSMNodeInstanceCustomVersion::GUID(0xCB1E2B23, 0x36C43AF, 0x9814D8FB, 0xD9F181F);
FCustomVersionRegistration GRegisterNodeInstanceCustomVersion(FSMNodeInstanceCustomVersion::GUID, FSMNodeInstanceCustomVersion::LatestVersion, TEXT("NodeInstanceVer"));
