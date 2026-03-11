// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * ProjectUITests Module
 *
 * Unit tests for ProjectUI plugin following Tier 1 test architecture.
 * Tests verify MVVM base classes, theme system, and widget infrastructure.
 *
 * Test Coverage:
 * - ViewModel base class (property binding, notifications, lifecycle)
 * - Theme system (loading, switching, data assets)
 * - Widget base classes (initialization, theme integration)
 * - Animation system (transitions, effects)
 *
 * Running Tests:
 * - Via Makefile: make test-unit (filters ProjectUI tests)
 * - Via Session Frontend: Window → Developer Tools → Session Frontend → Automation tab
 * - Filter: "ProjectUI" to see all tests for this module
 */
class FProjectUITestsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
