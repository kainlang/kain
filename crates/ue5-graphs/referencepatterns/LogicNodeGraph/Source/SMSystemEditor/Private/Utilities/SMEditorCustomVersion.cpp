// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMEditorCustomVersion.h"

#include "Serialization/CustomVersion.h"

const FGuid FSMGraphNodeCustomVersion::GUID(0x1D76B187, 0xDB6A02E4, 0xA8BD1333, 0x944A7DAE);
FCustomVersionRegistration GRegisterGraphNodeGraphCustomVersion(FSMGraphNodeCustomVersion::GUID, FSMGraphNodeCustomVersion::LatestVersion, TEXT("SMGraphNode"));
