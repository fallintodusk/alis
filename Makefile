# Makefile for Alis Project
# Quick commands for building, testing, and validating the project

# Load central UE configuration
include scripts/config/ue_env.mk

# Configuration
PROJECT_FILE := $(CURDIR)/Alis.uproject
REPORTS_DIR := $(CURDIR)/Saved/Validation/Reports

# Detect WSL to handle Windows tool invocation and path conversion
# Environment detection
UNAME_S := $(shell uname -s 2>/dev/null)
UNAME_R := $(shell uname -r 2>/dev/null)

IS_WSL := 0
ifeq ($(UNAME_S),Linux)
  ifneq (,$(findstring microsoft,$(shell cat /proc/version 2>/dev/null | tr '[:upper:]' '[:lower:]')))
    IS_WSL := 1
  endif
endif

# Allow manual override: FORCE_WSL=0/1
ifneq ($(FORCE_WSL),)
  IS_WSL := $(FORCE_WSL)
endif

.PHONY: help check check-uht check-syntax check-blueprints check-assets full-build clean generate open test test-all test-unit test-integration test-quick prepare-tests merge-ai mirror build-module build-editor build-game build-server structurizr-start structurizr-stop structurizr-open

# Default target
help:
	@echo "Alis Project - Available Commands:"
	@echo ""
	@echo "  Fast Checks (no full build):"
	@echo "    make check             - Run all fast checks (UHT + BP + assets)"
	@echo "    make check-uht         - Validate C++ reflection markup only"
	@echo "    make check-syntax      - Validate build without compiling"
	@echo "    make check-blueprints  - Compile all Blueprints"
	@echo "    make check-assets      - Run data validation on assets"
	@echo ""
	@echo "  Testing Commands:"
	@echo "    make test              - Run all tests (unit + integration)"
	@echo "    make test-unit         - Run unit tests only (Tier 1: plugin-internal)"
	@echo "    make test-integration  - Run integration tests only (Tier 2: cross-plugin)"
	@echo "    make test-integration-batch - Run ProjectIntegrationTests (Windows headless)"
	@echo "    make test-quick        - Run quick smoke tests"
	@echo "    make prepare-tests     - Prepare plugins and generate code before tests"
	@echo ""
	@echo "  Build Commands:"
	@echo "    make generate          - Generate Visual Studio project files"
	@echo "    make full-build        - Full project build (Editor + Game)"
	@echo "    make build-editor      - Build editor only"
	@echo "    make build-module MODULE=<name> - Build specific module (fast iteration)"
	@echo "    make build-game        - Build game (standalone)"
	@echo "    make build-server      - Build dedicated server"
	@echo ""
	@echo "  Utilities:"
	@echo "    make open              - Open project in Unreal Editor"
	@echo "    make clean             - Clean build artifacts"
	@echo ""
	@echo "  Documentation:"
	@echo "    make structurizr-start - Start Structurizr Lite (C4 diagrams)"
	@echo "    make structurizr-stop  - Stop Structurizr Lite"
	@echo "    make structurizr-open  - Open Structurizr in browser"
	@echo ""
	@echo "  Git Workflow:"
	@echo "    make merge-ai BRANCH=<branch> - Squash merge AI work branch (clean history)"
	@echo "    make mirror MIRROR_REMOTE_URL=<url> - Publish sanitized public mirror snapshot"
	@echo "    make mirror MIRROR_DRY_RUN=1 MIRROR_REMOTE_URL=<url> - Preview against mirror baseline"
	@echo "    make mirror MIRROR_DRY_RUN=1 MIRROR_EPHEMERAL_PREVIEW=1 - One-off local preview without remote"
	@echo ""

# Fast check suite (recommended before commits)
check:
	@echo "Running fast check suite..."
	@$(MAKE) check-uht
	@$(MAKE) check-blueprints
	@$(MAKE) check-assets

# Individual fast checks
check-uht:
	@echo "Validating UnrealHeaderTool (reflection markup)..."
ifeq ($(IS_WSL),1)
	@mkdir -p $(REPORTS_DIR)
	@# WSL: invoke Windows UBT via cmd.exe and convert paths
	@PROJ_WIN=$$(wslpath -w "$(PROJECT_FILE)" ); \
	LOG_WIN=$$(wslpath -w "$(REPORTS_DIR)/uht.log"); \
	UE_WIN=$$(wslpath -w "$(UE_PATH)" ); \
	CMD="\"$${UE_WIN}\\Engine\\Binaries\\DotNET\\UnrealBuildTool\\UnrealBuildTool.exe\" -Mode=UnrealHeaderTool \"-Target=AlisEditor Win64 Development -Project=$${PROJ_WIN}\" -WarningsAsErrors -FailIfGeneratedCodeChanges > $${LOG_WIN} 2>&1"; \
	echo Running: cmd.exe /C $$CMD; \
	cmd.exe /C $$CMD; \
	if grep -Eqi "(error|exception|fail)" "$(REPORTS_DIR)/uht.log"; then \
		echo "UHT validation reported errors"; \
		exit 1; \
	else \
		echo "✓ UHT validation passed"; \
	fi
else
	@powershell -NoLogo -NoProfile -Command "if (-not (Test-Path \"$(REPORTS_DIR)\")) { New-Item -ItemType Directory -Path \"$(REPORTS_DIR)\" | Out-Null }"
	@"$(UE_PATH)/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
		-Mode=UnrealHeaderTool \
		"-Target=AlisEditor Win64 Development -Project=\"$(PROJECT_FILE)\"" \
		-WarningsAsErrors \
		-FailIfGeneratedCodeChanges \
		> "$(REPORTS_DIR)/uht.log" 2>&1
	@echo "✓ UHT validation passed"
endif

check-syntax:
	@echo "Validating build syntax (no compile)..."
ifeq ($(IS_WSL),1)
	@mkdir -p $(REPORTS_DIR)
	@PROJ_WIN=$$(wslpath -w "$(PROJECT_FILE)"); \
	LOG_WIN=$$(wslpath -w "$(REPORTS_DIR)/skipcompile.log"); \
	UE_WIN=$$(wslpath -w "$(UE_PATH)"); \
	CMD="\"$${UE_WIN}\\Engine\\Build\\BatchFiles\\Build.bat\" AlisEditor Win64 Development \"$${PROJ_WIN}\" -skipcompile -NoHotReload > $${LOG_WIN} 2>&1"; \
	echo Running: cmd.exe /C $$CMD; \
	cmd.exe /C $$CMD; \
	echo "✓ Syntax validation passed"
else
	@powershell -NoLogo -NoProfile -Command "if (-not (Test-Path \"$(REPORTS_DIR)\")) { New-Item -ItemType Directory -Path \"$(REPORTS_DIR)\" | Out-Null }"
	@"$(UE_PATH)/Engine/Build/BatchFiles/Build.bat" \
		AlisEditor Win64 Development \
		"$(PROJECT_FILE)" \
		-skipcompile \
		-NoHotReload \
		> "$(REPORTS_DIR)/skipcompile.log" 2>&1
	@echo "✓ Syntax validation passed"
endif

check-blueprints:
	@echo "Compiling all Blueprints..."
ifeq ($(IS_WSL),1)
	@mkdir -p $(REPORTS_DIR)
	@echo "WSL detected: skipping direct blueprint compile; run from PowerShell:" \
		&& echo "  \"$(UE_PATH)\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe\" \"$$(wslpath -w "$(PROJECT_FILE)")\" -run=CompileAllBlueprints -unattended -nop4 -CrashForUAT -log=\"$$(wslpath -w "$(REPORTS_DIR)/bp_compile.log")\"" \
		&& echo "(Log will be saved to $(REPORTS_DIR)/bp_compile.log)" \
		&& echo "✓ Skipped in WSL"
else
	@powershell -NoLogo -NoProfile -Command "if (-not (Test-Path \"$(REPORTS_DIR)\")) { New-Item -ItemType Directory -Path \"$(REPORTS_DIR)\" | Out-Null }"
	@"$(UE_PATH)/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
		"$(PROJECT_FILE)" \
		-run=CompileAllBlueprints \
		-unattended \
		-nop4 \
		-CrashForUAT \
		-log="$(REPORTS_DIR)/bp_compile.log"
	@echo "✓ All Blueprints compiled"
endif

check-assets:
	@echo "Running data validation..."
ifeq ($(IS_WSL),1)
	@mkdir -p $(REPORTS_DIR)
	@echo "WSL detected: skipping direct data validation; run from PowerShell:" \
		&& echo "  \"$(UE_PATH)\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe\" \"$$(wslpath -w "$(PROJECT_FILE)")\" -run=DataValidation -unattended -nop4 -log=\"$$(wslpath -w "$(REPORTS_DIR)/data_validation.log")\"" \
		&& echo "(Log will be saved to $(REPORTS_DIR)/data_validation.log)" \
		&& echo "✓ Skipped in WSL"
else
	@cmd /C "if not exist \"$(REPORTS_DIR_WIN)\" mkdir \"$(REPORTS_DIR_WIN)\""
	@"$(UE_PATH)/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
		"$(PROJECT_FILE)" \
		-run=DataValidation \
		-unattended \
		-nop4 \
		-log="$(REPORTS_DIR)/data_validation.log"
	@echo "✓ Data validation complete"
endif

# Generate project files
generate:
	@echo "Generating Visual Studio project files..."
ifeq ($(IS_WSL),1)
	@PROJ_WIN=$$(wslpath -w "$(PROJECT_FILE)"); \
	UE_WIN=$$(wslpath -w "$(UE_PATH)"); \
	CMD="\"$${UE_WIN}\\Engine\\Build\\BatchFiles\\Build.bat\" -projectfiles -project=\"$${PROJ_WIN}\" -game -engine"; \
	echo Running: cmd.exe /C $$CMD; \
	cmd.exe /C $$CMD; \
	echo "✓ Project files generated"
else
	@"$(UE_PATH)/Engine/Build/BatchFiles/Build.bat" \
		-projectfiles \
		-project="$(PROJECT_FILE)" \
		-game \
		-engine
	@echo "✓ Project files generated"
endif

# Full builds
full-build: build-editor build-game

build-editor:
	@echo "Building Alis Editor..."
ifeq ($(IS_WSL),1)
	@PROJ_WIN=$$(wslpath -w "$(PROJECT_FILE)"); \
	UE_WIN=$$(wslpath -w "$(UE_PATH)"); \
	CMD="\"$${UE_WIN}\\Engine\\Build\\BatchFiles\\Build.bat\" AlisEditor Win64 Development \"$${PROJ_WIN}\""; \
	echo Running: cmd.exe /C $$CMD; \
	cmd.exe /C $$CMD; \
	echo "✓ Editor build complete"
else
	@"$(UE_PATH)/Engine/Build/BatchFiles/Build.bat" \
		AlisEditor Win64 Development \
		"$(PROJECT_FILE)"
	@echo "✓ Editor build complete"
endif

# NOTE: WSL users should call scripts/ue/build/ directly or use cmd.exe wrapper
# Build specific module (fast iteration)
# Usage: make build-module MODULE=ProjectContentPacks
build-module:
ifndef MODULE
	@echo "ERROR: MODULE parameter required"
	@echo "Usage: make build-module MODULE=<ModuleName>"
	@echo "Example: make build-module MODULE=ProjectContentPacks"
	@exit 1
endif
	@echo "Building module: $(MODULE)..."
ifeq ($(IS_WSL),1)
	@PROJ_WIN=$$(wslpath -w "$(PROJECT_FILE)"); \
	UE_WIN=$$(wslpath -w "$(UE_PATH)"); \
	CMD="\"$${UE_WIN}\\Engine\\Build\\BatchFiles\\Build.bat\" AlisEditor Win64 Development \"$${PROJ_WIN}\" -WaitMutex -Module=$(MODULE)"; \
	echo Running: cmd.exe /C $$CMD; \
	cmd.exe /C $$CMD; \
	echo "✓ Module $(MODULE) build complete"
else
	@"$(UE_PATH)/Engine/Build/BatchFiles/Build.bat" \
		AlisEditor Win64 Development \
		"$(PROJECT_FILE)" \
		-WaitMutex \
		-Module=$(MODULE)
	@echo "✓ Module $(MODULE) build complete"
endif

build-game:
	@echo "Building Alis Game (standalone)..."
ifeq ($(IS_WSL),1)
	@PROJ_WIN=$$(wslpath -w "$(PROJECT_FILE)"); \
	UE_WIN=$$(wslpath -w "$(UE_PATH)"); \
	CMD="\"$${UE_WIN}\\Engine\\Build\\BatchFiles\\Build.bat\" Alis Win64 Development \"$${PROJ_WIN}\""; \
	echo Running: cmd.exe /C $$CMD; \
	cmd.exe /C $$CMD; \
	echo "✓ Game build complete"
else
	@"$(UE_PATH)/Engine/Build/BatchFiles/Build.bat" \
		Alis Win64 Development \
		"$(PROJECT_FILE)"
	@echo "✓ Game build complete"
endif

build-server:
	@echo "Building Alis Dedicated Server..."
ifeq ($(IS_WSL),1)
	@PROJ_WIN=$$(wslpath -w "$(PROJECT_FILE)"); \
	UE_WIN=$$(wslpath -w "$(UE_PATH)"); \
	CMD="\"$${UE_WIN}\\Engine\\Build\\BatchFiles\\Build.bat\" AlisServer Win64 Development \"$${PROJ_WIN}\""; \
	echo Running: cmd.exe /C $$CMD; \
	cmd.exe /C $$CMD; \
	echo "✓ Server build complete"
else
	@"$(UE_PATH)/Engine/Build/BatchFiles/Build.bat" \
		AlisServer Win64 Development \
		"$(PROJECT_FILE)"
	@echo "✓ Server build complete"
endif

# Open editor
open:
	@echo "Opening Unreal Editor..."
ifeq ($(IS_WSL),1)
	@PROJ_WIN=$$(wslpath -w "$(PROJECT_FILE)"); \
	UE_WIN=$$(wslpath -w "$(UE_PATH)"); \
	CMD="\"$${UE_WIN}\\Engine\\Binaries\\Win64\\UnrealEditor.exe\" \"$${PROJ_WIN}\""; \
	echo Running: cmd.exe /C $$CMD; \
	cmd.exe /C $$CMD
else
	@"$(UE_PATH)/Engine/Binaries/Win64/UnrealEditor.exe" "$(PROJECT_FILE)"
endif

# Clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf Binaries/ Intermediate/ Saved/
	@rm -rf Plugins/*/Binaries/ Plugins/*/Intermediate/
	@rm -rf Saved/Validation/Reports/
	@echo "✓ Clean complete"

# Testing targets
prepare-tests:
	@echo "Preparing plugins for testing..."
	@bash scripts/test/prepare.sh

test: prepare-tests
	@echo "Running all tests..."
	@bash scripts/test/run.sh --all

test-unit: prepare-tests
	@echo "Running unit tests (Tier 1: plugin-internal)..."
	@bash scripts/test/run.sh --unit

test-integration: prepare-tests
	@echo "Running integration tests (Tier 2: cross-plugin)..."
	@bash scripts/test/run.sh --integration

test-quick: prepare-tests
	@echo "Running quick smoke tests..."
	@bash scripts/test/run.sh --quick

test-unit-smart:
	@echo "Running selective unit tests (changed plugins only)..."
	@bash scripts/test/selective.sh

# Windows-specific integration test target (uses shell script)
test-integration-batch:
	@echo "Running ProjectIntegrationTests (headless script)..."
	@bash scripts/test/integration.sh

# Git workflow automation
merge-ai:
	@bash scripts/git/squash_merge.sh $(BRANCH)

mirror:
ifeq ($(IS_WSL),1)
	@set -eu; \
	CMD="./scripts/git/mirror/mirror_to_github.sh"; \
	set --; \
	if [ -n "$(MIRROR_REMOTE_URL)" ]; then set -- "$$@" --remote-url "$(MIRROR_REMOTE_URL)"; fi; \
	if [ -n "$(MIRROR_BRANCH)" ]; then set -- "$$@" --branch "$(MIRROR_BRANCH)"; fi; \
	if [ -n "$(MIRROR_EXCLUDE_FILE)" ]; then set -- "$$@" --exclude-file "$(MIRROR_EXCLUDE_FILE)"; fi; \
	if [ -n "$(MIRROR_FORBIDDEN_PATTERNS_FILE)" ]; then set -- "$$@" --forbidden-patterns-file "$(MIRROR_FORBIDDEN_PATTERNS_FILE)"; fi; \
	if [ "$(MIRROR_DRY_RUN)" = "1" ]; then \
		set -- "$$@" --dry-run; \
	else \
		set -- "$$@" --push; \
	fi; \
	if [ "$(MIRROR_EPHEMERAL_PREVIEW)" = "1" ]; then set -- "$$@" --ephemeral-preview; fi; \
	if [ "$(MIRROR_FORCE)" = "1" ]; then set -- "$$@" --force; fi; \
	if [ -n "$(MIRROR_ARGS)" ]; then set -- "$$@" $(MIRROR_ARGS); fi; \
	echo "[INFO] mirror args: $$*"; \
	if [ -n "$(MIRROR_GIT_SSH_COMMAND)" ]; then \
		MIRROR_GIT_SSH_COMMAND="$(MIRROR_GIT_SSH_COMMAND)" bash "$$CMD" "$$@"; \
	else \
		bash "$$CMD" "$$@"; \
	fi
else
	@set -eu; \
	set --; \
	if [ -n "$(MIRROR_REMOTE_URL)" ]; then set -- "$$@" --remote-url "$(MIRROR_REMOTE_URL)"; fi; \
	if [ -n "$(MIRROR_BRANCH)" ]; then set -- "$$@" --branch "$(MIRROR_BRANCH)"; fi; \
	if [ -n "$(MIRROR_EXCLUDE_FILE)" ]; then set -- "$$@" --exclude-file "$(MIRROR_EXCLUDE_FILE)"; fi; \
	if [ -n "$(MIRROR_FORBIDDEN_PATTERNS_FILE)" ]; then set -- "$$@" --forbidden-patterns-file "$(MIRROR_FORBIDDEN_PATTERNS_FILE)"; fi; \
	if [ "$(MIRROR_DRY_RUN)" = "1" ]; then \
		set -- "$$@" --dry-run; \
	else \
		set -- "$$@" --push; \
	fi; \
	if [ "$(MIRROR_EPHEMERAL_PREVIEW)" = "1" ]; then set -- "$$@" --ephemeral-preview; fi; \
	if [ "$(MIRROR_FORCE)" = "1" ]; then set -- "$$@" --force; fi; \
	if [ -n "$(MIRROR_ARGS)" ]; then set -- "$$@" $(MIRROR_ARGS); fi; \
	echo "[INFO] mirror args: $$*"; \
	if [ -n "$(MIRROR_GIT_SSH_COMMAND)" ]; then \
		MIRROR_GIT_SSH_COMMAND="$(MIRROR_GIT_SSH_COMMAND)" powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File ".\\scripts\\git\\mirror\\mirror_to_github.ps1" "$$@"; \
	else \
		powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File ".\\scripts\\git\\mirror\\mirror_to_github.ps1" "$$@"; \
	fi
endif

# Documentation: Structurizr Lite (C4 Architecture Diagrams)
structurizr-start:
	@cmd.exe /C scripts\\utils\\structurizr\\start_structurizr.bat

structurizr-stop:
	@cmd.exe /C scripts\\utils\\structurizr\\stop_structurizr.bat

structurizr-open:
	@cmd.exe /C "start http://localhost:8080"
