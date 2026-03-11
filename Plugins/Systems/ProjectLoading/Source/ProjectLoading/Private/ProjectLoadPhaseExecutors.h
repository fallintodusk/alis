// Copyright ALIS. All Rights Reserved.

#pragma once

/**
 * Aggregate header for all phase executors.
 *
 * Provides backward compatibility by including all individual executor headers.
 * Each executor is now in its own file following the Single Responsibility Principle (SOLID).
 *
 * Individual executors:
 * - Executors/ResolveAssetsPhaseExecutor.h
 * - Executors/MountContentPhaseExecutor.h
 * - Executors/PreloadCriticalAssetsPhaseExecutor.h
 * - Executors/ActivateFeaturesPhaseExecutor.h
 * - Executors/TravelPhaseExecutor.h
 * - Executors/WarmupPhaseExecutor.h
 */

#include "Executors/ResolveAssetsPhaseExecutor.h"
#include "Executors/MountContentPhaseExecutor.h"
#include "Executors/PreloadCriticalAssetsPhaseExecutor.h"
#include "Executors/ActivateFeaturesPhaseExecutor.h"
#include "Executors/TravelPhaseExecutor.h"
#include "Executors/WarmupPhaseExecutor.h"
