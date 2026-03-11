@echo off
REM ============================================================================
REM Test Configuration - Central config for all test scripts (SOLID: DIP)
REM ============================================================================
REM This file defines all paths and settings used by test scripts.
REM Modify this file once to affect all tests (Open/Closed Principle).
REM ============================================================================

REM Unreal Engine Paths
set "UE_ENGINE_ROOT=<ue-path>"
set "UE_EDITOR=%UE_ENGINE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe"
set "UE_EDITOR_CMD=%UE_ENGINE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

REM Project Paths
set "PROJECT_ROOT=<project-root>"
set "PROJECT_FILE=%PROJECT_ROOT%\Alis.uproject"
set "LOG_DIR=%PROJECT_ROOT%\Saved\Logs"
set "LOG_FILE=%LOG_DIR%\Alis.log"

REM Test Settings
set "EDITOR_TIMEOUT_SECONDS=60"
set "LOG_CHECK_INTERVAL_SECONDS=2"

REM GameFeature Plugin Names (comma-separated for batch processing)
set "GAMEFEATURE_PLUGINS=ProjectCombat,ProjectDialogue,ProjectInventory,ProjectMenuCoreGF,ProjectMenuExperienceGF"

REM Exit with success to indicate config loaded
exit /b 0
