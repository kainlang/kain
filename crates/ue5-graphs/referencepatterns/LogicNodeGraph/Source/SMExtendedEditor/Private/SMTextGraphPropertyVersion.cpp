// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTextGraphPropertyVersion.h"
#include "Serialization/CustomVersion.h"

const FGuid FSMTextGraphPropertyCustomVersion::GUID(0xBDE90488, 0xd3F03965, 0x36AB7227, 0x0FC49660);
FCustomVersionRegistration GRegisterTextGraphPropertyCustomVersion(FSMTextGraphPropertyCustomVersion::GUID, FSMTextGraphPropertyCustomVersion::LatestVersion, TEXT("TextGraphProperty"));