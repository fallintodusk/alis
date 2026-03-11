// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * ProjectLoadingTests Module
 *
 * Unit tests for ProjectLoading plugin following Tier 1 test architecture.
 * Tests verify loading subsystem, phase executors, retry logic, and progress tracking.
 *
 * Test Coverage:
 * - Subsystem lifecycle (initialization, StartLoad, cancellation)
 * - Phase state tracking (6 phases + state transitions)
 * - Retry logic (exponential backoff, 3 attempts)
 * - Cancellation (graceful vs force modes)
 * - Progress tracking (0.0-1.0 normalization, overall calculation)
 * - Error handling (error codes 100-999, error messages)
 * - Telemetry (timing, progress snapshots)
 *
 * Running Tests:
 * - Via Makefile: make test-unit (filters ProjectLoading tests)
 * - Via Session Frontend: Window → Developer Tools → Session Frontend → Automation tab
 * - Filter: "ProjectLoading" to see all tests for this module
 */
class FProjectLoadingTestsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
