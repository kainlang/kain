// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Styling/AppStyle.h"

/**
 * Typedef for primary Unreal Editor style to help usage across engine versions.
 * For minimal merge conflicts always access the instance with ::Get() before calling any methods.
 *
 * EditorStyle (UE4) / AppStyle (UE5)
 */
typedef FAppStyle FSMUnrealAppStyle;