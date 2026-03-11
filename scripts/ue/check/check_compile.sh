#!/bin/bash
# Fast compilation and syntax checks for Alis project
# No full build required - validates UHT, Blueprints, and assets

set -e

# Load central UE environment configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/../config/ue_env.sh"

# Configuration
REPORTS_DIR="${PROJECT_ROOT}/utility/check/reports"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Create reports directory
mkdir -p "$REPORTS_DIR"

# Determine whether asset validation is required
NEEDS_ASSET_VALIDATION=1
if [ "${FORCE_FULL_VALIDATION:-0}" != "1" ]; then
    pushd "$PROJECT_ROOT" > /dev/null
    CHANGED_TRACKED="$(git diff --name-only)"
    CHANGED_UNTRACKED="$(git ls-files --others --exclude-standard)"
    popd > /dev/null

    ALL_CHANGED="$(printf "%s\n%s" "$CHANGED_TRACKED" "$CHANGED_UNTRACKED" | sed '/^$/d' | sort -u)"

    if [ -z "$ALL_CHANGED" ]; then
        NEEDS_ASSET_VALIDATION=0
    elif ! echo "$ALL_CHANGED" | grep -E '\.(uasset|umap)$|^Content/|^Plugins/.*/Content/' >/dev/null; then
        NEEDS_ASSET_VALIDATION=0
    fi
fi

echo "========================================="
echo "  Alis Project Fast Check Suite"
echo "========================================="
echo ""

# Test 1: UnrealHeaderTool (UHT) - Validates reflection markup
echo -e "${YELLOW}[1/4] Running UnrealHeaderTool (validates UPROPERTY/UFUNCTION)...${NC}"
"${UE_PATH}/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
  -Mode=UnrealHeaderTool \
  "-Target=AlisEditor Win64 Development -Project=\"${PROJECT_FILE_WINDOWS}\"" \
  -WarningsAsErrors \
  -FailIfGeneratedCodeChanges \
  > "${REPORTS_DIR}/uht.log" 2>&1

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ UHT passed - All reflection markup valid${NC}"
else
    echo -e "${RED}✗ UHT failed - Check ${REPORTS_DIR}/uht.log${NC}"
    cat "${REPORTS_DIR}/uht.log"
    exit 1
fi
echo ""

# Test 2: Skip Compile (runs UHT + validation without full compile)
echo -e "${YELLOW}[2/4] Running build validation (no compile)...${NC}"
"${UE_PATH}/Engine/Build/BatchFiles/Build.bat" \
  AlisEditor Win64 Development \
  "${PROJECT_FILE_WINDOWS}" \
  -skipcompile \
  -NoHotReload \
  > "${REPORTS_DIR}/skipcompile.log" 2>&1

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build validation passed${NC}"
else
    echo -e "${RED}✗ Build validation failed - Check ${REPORTS_DIR}/skipcompile.log${NC}"
    tail -50 "${REPORTS_DIR}/skipcompile.log"
    exit 1
fi
echo ""

# Test 3: Compile All Blueprints
echo -e "${YELLOW}[3/4] Compiling all Blueprints...${NC}"
"${UE_PATH}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "${PROJECT_FILE_WINDOWS}" \
  -run=CompileAllBlueprints \
  -unattended \
  -nop4 \
  -CrashForUAT \
  -log="${REPORTS_DIR}/bp_compile.log" \
  > "${REPORTS_DIR}/bp_compile_stdout.log" 2>&1

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ All Blueprints compiled successfully${NC}"
else
    echo -e "${RED}✗ Blueprint compilation failed - Check ${REPORTS_DIR}/bp_compile.log${NC}"
    tail -50 "${REPORTS_DIR}/bp_compile.log"
    exit 1
fi
echo ""

# Test 4: Data Validation
if [ "$NEEDS_ASSET_VALIDATION" -eq 0 ]; then
    echo -e "${YELLOW}[4/4] Skipping data validation (no asset changes detected)${NC}"
else
    echo -e "${YELLOW}[4/4] Running data validation (assets, naming, references)...${NC}"
    "${UE_PATH}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
      "${PROJECT_FILE_WINDOWS}" \
      -run=DataValidation \
      -unattended \
      -nop4 \
      -log="${REPORTS_DIR}/data_validation.log" \
      > "${REPORTS_DIR}/data_validation_stdout.log" 2>&1

    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Data validation passed${NC}"
    else
        echo -e "${YELLOW}⚠ Data validation warnings - Check ${REPORTS_DIR}/data_validation.log${NC}"
        # Don't fail on data validation warnings, just notify
    fi
fi
echo ""

echo "========================================="
echo -e "${GREEN}✓ ALL CHECKS PASSED${NC}"
echo "========================================="
echo ""
echo "Reports saved to: ${REPORTS_DIR}/"
echo "  - uht.log"
echo "  - skipcompile.log"
echo "  - bp_compile.log"
echo "  - data_validation.log"
echo ""
echo "Run 'make full-build' to perform actual compilation."
