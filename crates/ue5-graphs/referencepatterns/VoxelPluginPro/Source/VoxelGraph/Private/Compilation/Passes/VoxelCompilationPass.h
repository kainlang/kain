// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FVoxelGraphCompiler;

#define VOXEL_PASS_BODY(Class) static const TCHAR* GetName() { sizeof(Class); return TEXT(#Class); }

