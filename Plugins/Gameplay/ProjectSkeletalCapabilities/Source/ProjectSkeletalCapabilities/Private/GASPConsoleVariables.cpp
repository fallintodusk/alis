// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Animation/AnimTypes.h"
#include "HAL/IConsoleManager.h"

// AnimationWarping registers this name only while ENABLE_ANIM_DEBUG is set.
// Shipping and Test compile its copy out, where a Blueprint lookup of a missing
// name reads 0; registering the engine's default of 1 keeps them like Development.
#if !ENABLE_ANIM_DEBUG
static TAutoConsoleVariable<int32> CVarOffsetRootBoneEnable(
	TEXT("a.animnode.offsetrootbone.enable"),
	1,
	TEXT("Enable OffsetRootBone animation node. 0 = off, 1 = on."),
	ECVF_Default);
#endif
