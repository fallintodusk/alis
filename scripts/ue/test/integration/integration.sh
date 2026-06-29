#!/bin/bash
# Windows shell script to run ProjectIntegrationTests in headless mode.
# Executes integration tests for cross-plugin validation via Unreal Automation.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/../config/ue_env.sh"

PROJECT_FILE="${PROJECT_FILE_WINDOWS}"
REPORTS_DIR="${PROJECT_ROOT}/Saved/Automation/Reports"
TEST_LOG="${REPORTS_DIR}/tests.log"
REPORTS_DIR_WINDOWS="$(to_windows_path "$REPORTS_DIR")"
TEST_LOG_WINDOWS="$(to_windows_path "$TEST_LOG")"
INTEGRATION_FILTERS=(
    "ProjectIntegrationTests.Correctness"
    "ProjectIntegrationTests.Gameplay"
    "ProjectIntegrationTests.Integration"
    "ProjectIntegrationTests.Interaction"
    "ProjectIntegrationTests.Inventory"
    "ProjectIntegrationTests.InventoryLootPlaces"
    "ProjectIntegrationTests.ObjectParentGeneralization"
    "ProjectIntegrationTests.UI"
    "ProjectIntegrationTests.Unit"
)
AUTOMATION_FILTER_EXPR="$(IFS=+; echo "${INTEGRATION_FILTERS[*]}")"

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=========================================${NC}"
echo -e "${BLUE}  ProjectIntegrationTests Launch Script${NC}"
echo -e "${BLUE}=========================================${NC}"
echo "Starting ProjectIntegrationTests (headless)..."
echo "Automation Filter Expression: ${AUTOMATION_FILTER_EXPR}"
echo "Character parity/clip-matrix tests are owned by scripts/ue/test/character/capture_parity.ps1."

set +e
"${UE_PATH}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
    "${PROJECT_FILE}" \
    -unattended \
    -nop4 \
    -NoSound \
    -stdout \
    -FullStdOutLogOutput \
    -NullRHI \
    -testexit="Automation Test Queue Empty" \
    -ABSLOG="${TEST_LOG_WINDOWS}" \
    -ReportExportPath="${REPORTS_DIR_WINDOWS}" \
    -ExecCmds="Automation RunTests ${AUTOMATION_FILTER_EXPR};Quit"
TEST_EXIT_CODE=$?
set -e

echo ""
echo "========================================="

if [ "$TEST_EXIT_CODE" -eq 0 ]; then
    echo -e "${GREEN}[OK] ProjectIntegrationTests COMPLETED SUCCESSFULLY${NC}"
    echo "========================================="
    echo ""
    echo "Test reports saved to:"
    echo "  ${REPORTS_DIR}/"
    echo ""
    exit 0
fi

echo -e "${RED}[FAIL] ProjectIntegrationTests FAILED${NC}"
echo "========================================="
echo ""
echo "Check test log for details:"
echo "  ${TEST_LOG}"
echo ""
echo "Test reports:"
echo "  ${REPORTS_DIR}/"
echo ""

if [ -f "${TEST_LOG}" ]; then
    echo ""
    echo "Last 50 lines of test log:"
    echo "---"
    tail -50 "${TEST_LOG}"
fi

exit "$TEST_EXIT_CODE"
