#!/bin/bash
# Windows shell script to run ProjectIntegrationTests in headless mode
# Executes integration tests for cross-plugin validation via Unreal Automation Framework

# Load central UE environment configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/../config/ue_env.sh"

# Configuration
PROJECT_FILE="${PROJECT_FILE_WINDOWS}"
REPORTS_DIR="${PROJECT_ROOT}/Saved/Automation/Reports"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=========================================${NC}"
echo -e "${BLUE}  ProjectIntegrationTests Launch Script${NC}"
echo -e "${BLUE}=========================================${NC}"

echo "Starting ProjectIntegrationTests (headless)..."

# Run tests using Unreal Engine automation framework
# Ensures clean execution and exit
"${UE_PATH}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
    "${PROJECT_FILE}" \
    -unattended \
    -nop4 \
    -NoSound \
    -stdout \
    -FullStdOutLogOutput \
    -game \
    -NullRHI \
    -testexit="Automation Test Queue Empty" \
    -ReportOutputPath="${REPORTS_DIR}" \
    -ExecCmds="Automation RunTests ProjectIntegrationTests.*;Quit"

TEST_EXIT_CODE=$?

echo ""
echo "========================================="

if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✓ ProjectIntegrationTests COMPLETED SUCCESSFULLY${NC}"
    echo "========================================="
    echo ""
    echo "Test reports saved to:"
    echo "  ${REPORTS_DIR}/"
    echo ""
    exit 0
else
    echo -e "${RED}✗ ProjectIntegrationTests FAILED${NC}"
    echo "========================================="
    echo ""
    echo "Check test log for details:"
    echo "  ${REPORTS_DIR}/tests.log"
    echo ""
    echo "Test reports:"
    echo "  ${REPORTS_DIR}/"
    echo ""

    # Show last 50 lines of log for quick diagnosis
    if [ -f "${REPORTS_DIR}/tests.log" ]; then
        echo ""
        echo "Last 50 lines of test log:"
        echo "---"
        tail -50 "${REPORTS_DIR}/tests.log"
    fi

    exit 1
fi
