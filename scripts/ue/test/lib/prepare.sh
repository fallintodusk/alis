#!/bin/bash
# Prepare plugins for testing - ensure all compiled and intermediate files generated
# This runs BEFORE Unreal Editor starts to ensure plugins are ready

set -e

# Load central UE environment configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/../config/ue_env.sh"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "========================================="
echo "  Test Preparation - Plugin Validation"
echo "========================================="
echo ""

# Step 1: Generate project files if needed
if [ ! -f "${PROJECT_ROOT}/Alis.sln" ]; then
    echo -e "${YELLOW}[1/4] Generating Visual Studio project files...${NC}"
    "${UE_PATH}/Engine/Build/BatchFiles/Build.bat" \
        -projectfiles \
        -project="${PROJECT_FILE_WINDOWS}" \
        -game \
        -engine
    echo -e "${GREEN}✓ Project files generated${NC}"
else
    echo -e "${GREEN}[1/4] Project files exist${NC}"
fi
echo ""

# Step 2: Run UnrealHeaderTool to generate reflection code
echo -e "${YELLOW}[2/4] Running UnrealHeaderTool (generate reflection code)...${NC}"
"${UE_PATH}/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
    -Mode=UnrealHeaderTool \
    "-Target=AlisEditor Win64 Development -Project=\"${PROJECT_FILE_WINDOWS}\"" \
    > /dev/null 2>&1

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ UHT completed - All .generated.h files created${NC}"
else
    echo -e "${RED}✗ UHT failed - Check reflection markup${NC}"
    exit 1
fi
echo ""

# Step 3: Do minimal compilation verification (tests will compile at runtime)
# Note: Full individual module compilation is complex; Editor will handle at launch
echo -e "${YELLOW}[3/4] Verifying test module sources available...${NC}"

# Dynamic discovery: Find test modules recursively in Plugins/Test
TEST_MODULES=()
while IFS= read -r -d '' MODULE_PATH; do
    MODULE_NAME=$(basename "$MODULE_PATH" .Build.cs)
    # Use the plugin directory name as the module name (should match)
    PLUGIN_DIR="$(dirname "$(dirname "$(dirname "$MODULE_PATH")")")"  # Go up 3 levels from .Build.cs file
    PLUGIN_NAME="$(basename "$PLUGIN_DIR")"
    MODULE_NAME="$PLUGIN_NAME"  # Plugin name = Module name for test plugins
    TEST_MODULES+=("$MODULE_NAME")
done < <(find "${PROJECT_ROOT}/Plugins/Test" -name "*.Build.cs" -type f -print0 2>/dev/null)

MODULE_COUNT=${#TEST_MODULES[@]}
if [ $MODULE_COUNT -gt 0 ]; then
    echo -e "${GREEN}✓ Found ${MODULE_COUNT} test modules with source: ${TEST_MODULES[*]}${NC}"

    # Quick validation: check if modules have valid .h/.cpp files
    MISSING_CPP=0
    for MODULE in "${TEST_MODULES[@]}"; do
        MODULE_DIR="$(find "${PROJECT_ROOT}/Plugins/Test" -name "${MODULE}" -type d 2>/dev/null | head -1)"
        if [ -n "$MODULE_DIR" ]; then
            CPP_COUNT=$(find "$MODULE_DIR/Source" -name "*.cpp" 2>/dev/null | wc -l 2>/dev/null || echo 0)
            H_COUNT=$(find "$MODULE_DIR/Source" -name "*.h" 2>/dev/null | wc -l 2>/dev/null || echo 0)
            echo -e "  ${BLUE}${MODULE}: ${CPP_COUNT} .cpp files, ${H_COUNT} .h files${NC}"
            if [ "$CPP_COUNT" -eq 0 ]; then
                MISSING_CPP=$((MISSING_CPP + 1))
            fi
        fi
    done

    if [ $MISSING_CPP -gt 0 ]; then
        echo -e "${YELLOW}⚠ ${MISSING_CPP} modules have no .cpp files${NC}"
    fi
else
    echo -e "${YELLOW}⊘ No test modules found${NC}"
fi
echo ""

# Step 4: Verify intermediate files for successfully compiled modules
echo -e "${YELLOW}[4/4] Verifying intermediate files...${NC}"

MISSING_INTERMEDIATE=0
VERIFY_COUNT=0

# Check each compiled module for proper intermediate files
for MODULE in "${TEST_MODULES[@]}"; do
    # Find the module's directory in Plugins
    MODULE_DIR=$(find "${PROJECT_ROOT}/Plugins" -name "${MODULE}" -type d 2>/dev/null | head -1)

    if [ -n "$MODULE_DIR" ] && [ -d "${MODULE_DIR}/Source" ]; then
        # Check for the expected intermediate directory
        INTERMEDIATE_DIR="${MODULE_DIR}/Intermediate/Build/Win64/UnrealEditor/Inc"

        VERIFY_COUNT=$((VERIFY_COUNT + 1))

        if [ -d "$INTERMEDIATE_DIR" ]; then
            # Check if there are generated files
            GENERATED_COUNT=$(find "$INTERMEDIATE_DIR" -name "*.generated.h" 2>/dev/null | wc -l 2>/dev/null || echo 0)
            if [ "$GENERATED_COUNT" -gt 0 ]; then
                echo -e "  ${GREEN}✓ ${MODULE} intermediate files verified (${GENERATED_COUNT} .generated.h files)${NC}"
                continue
            fi
        fi

        # Intermediate files missing
        echo -e "  ${RED}✗ Missing intermediate files for ${MODULE}${NC}"
        echo -e "    ${YELLOW}Expected: ${INTERMEDIATE_DIR}${NC}"
        MISSING_INTERMEDIATE=$((MISSING_INTERMEDIATE + 1))
    fi
done

if [ $VERIFY_COUNT -eq 0 ]; then
    echo -e "${YELLOW}⊘ No modules to verify${NC}"
elif [ $MISSING_INTERMEDIATE -eq 0 ]; then
    echo -e "${GREEN}✓ All ${VERIFY_COUNT} modules have intermediate files${NC}"
else
    echo -e "${YELLOW}✗ ${MISSING_INTERMEDIATE}/${VERIFY_COUNT} modules missing intermediate files${NC}"
    echo -e "${YELLOW}Tip: Run 'make clean && make generate' to regenerate${NC}"
fi
echo ""

echo "========================================="
echo -e "${GREEN}✓ TEST PREPARATION COMPLETE${NC}"
echo "========================================="
echo ""
echo "Plugins are ready for testing."
echo "You can now run:"
echo "  - make test           (all tests)"
echo "  - make test-unit      (Tier 1: plugin-internal)"
echo "  - make test-integration (Tier 2: cross-plugin)"
echo ""
