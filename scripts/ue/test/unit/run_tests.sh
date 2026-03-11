#!/bin/bash
# Run automated tests with Unreal Engine automation framework
# Supports filtering by test tier (unit vs integration) and generates reports

set -e

# Load central UE environment configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/../config/ue_env.sh"

# Configuration
REPORTS_DIR="${PROJECT_ROOT}/Saved/Automation/Reports"
TEST_LOG="${REPORTS_DIR}/tests.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse arguments
usage() {
    cat <<EOF
Usage: $(basename "$0") [mode] [--filter <name> ...]

Modes:
  --unit            Run Tier 1 plugin unit tests (default when no filters provided)
  --integration     Run Tier 2 integration tests
  --quick           Run smoke tests
  --all             Run all tests (Unit + Integration)

Filters:
  --filter <name>   Add a specific automation filter (can be repeated)
  --filters <a,b>   Comma-separated list of automation filters

Any bare argument that is not a recognised flag will be treated as a filter.
EOF
}

TEST_MODE=""
CUSTOM_FILTERS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --unit|--integration|--quick|--all)
            TEST_MODE="$1"
            shift
            ;;
        --filter|-f)
            if [[ -z "${2:-}" ]]; then
                echo "Error: --filter requires a value" >&2
                usage
                exit 1
            fi
            CUSTOM_FILTERS+=("$2")
            shift 2
            ;;
        --filters)
            if [[ -z "${2:-}" ]]; then
                echo "Error: --filters requires a comma-separated list" >&2
                usage
                exit 1
            fi
            IFS=',' read -ra EXTRA_FILTERS <<< "$2"
            for FILTER in "${EXTRA_FILTERS[@]}"; do
                CUSTOM_FILTERS+=("$FILTER")
            done
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        --*)
            echo "Unknown option: $1" >&2
            usage
            exit 1
            ;;
        *)
            CUSTOM_FILTERS+=("$1")
            shift
            ;;
    esac
done

if [[ -z "$TEST_MODE" && ${#CUSTOM_FILTERS[@]} -eq 0 ]]; then
    # Default to unit tests unless explicitly overridden to keep runs focused
    TEST_MODE="--unit"
fi

# Create reports directory
mkdir -p "$REPORTS_DIR"

echo "========================================="
echo "  Automated Test Execution"
echo "========================================="
echo ""

# Determine which tests to run based on mode/filter selection
TEST_FILTERS=()
declare -A FILTER_SET=()

add_filter() {
    local FILTER="$1"
    if [[ -n "$FILTER" && -z "${FILTER_SET[$FILTER]+x}" ]]; then
        FILTER_SET["$FILTER"]=1
        TEST_FILTERS+=("$FILTER")
    fi
}

if [[ ${#CUSTOM_FILTERS[@]} -gt 0 ]]; then
    for FILTER in "${CUSTOM_FILTERS[@]}"; do
        add_filter "$FILTER"
    done
else
    case "$TEST_MODE" in
        --unit)
            echo -e "${BLUE}Running Tier 1: Plugin-Internal Unit Tests${NC}"
            DEFAULT_UNIT_FILTERS=(
                "ProjectCore"
                "ProjectBoot"
                "ProjectLoading"
                "ProjectSession"
                "ProjectData"
                "ProjectUI"
                "ProjectMenuUI"
                "ProjectContentPacks"
                "ProjectGameFeatures"
                "ProjectSave"
                "ProjectCombat"
                "ProjectDialogue"
                "ProjectInventory"
            )
            for FILTER in "${DEFAULT_UNIT_FILTERS[@]}"; do
                add_filter "$FILTER"
            done
            ;;
        --integration)
            echo -e "${BLUE}Running Tier 2: Cross-Plugin Integration Tests${NC}"
            add_filter "ProjectIntegrationTests"
            ;;
        --quick)
            echo -e "${BLUE}Running Quick Smoke Tests${NC}"
            add_filter "Smoke"
            ;;
        --all)
            echo -e "${BLUE}Running All Tests (Unit + Integration)${NC}"
            add_filter "Project"
            ;;
        *)
            echo "Unknown test mode: ${TEST_MODE}" >&2
            usage
            exit 1
            ;;
    esac
fi

if [[ ${#TEST_FILTERS[@]} -eq 0 ]]; then
    echo "No automation filters selected. Nothing to run." >&2
    exit 0
fi

if [[ ${#CUSTOM_FILTERS[@]} -gt 0 ]]; then
    echo -e "${BLUE}Running custom automation filters${NC}"
fi

# Build the automation command string (`;` separates console commands)
AUTOMATION_COMMANDS=""
for FILTER in "${TEST_FILTERS[@]}"; do
    if [ -n "$AUTOMATION_COMMANDS" ]; then
        AUTOMATION_COMMANDS+=";"
    fi
    AUTOMATION_COMMANDS+="Automation RunTests ${FILTER}"
done

echo "Test Filters: ${TEST_FILTERS[*]}"
echo ""

# Run tests using Unreal Engine automation framework
echo -e "${YELLOW}Starting Unreal Editor test runner...${NC}"
echo ""

"${UE_PATH}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
    "${PROJECT_FILE_WINDOWS}" \
    -ExecCmds="${AUTOMATION_COMMANDS}" \
    -unattended \
    -nopause \
    -nosplash \
    -NoSound \
    -NoLogWindow \
    -NoLoadingScreen \
    -game \
    -NullRHI \
    -NoLiveCoding \
    -Messaging \
    -testexit="Automation Test Queue Empty" \
    -log="${TEST_LOG}" \
    -ReportOutputPath="${REPORTS_DIR}"

TEST_EXIT_CODE=$?

echo ""
echo "========================================="

if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✓ ALL TESTS PASSED${NC}"
    echo "========================================="
    echo ""
    echo "Test reports saved to:"
    echo "  ${REPORTS_DIR}/"
    echo ""
    exit 0
else
    echo -e "${RED}✗ TESTS FAILED${NC}"
    echo "========================================="
    echo ""
    echo "Check test log for details:"
    echo "  ${TEST_LOG}"
    echo ""
    echo "Test reports:"
    echo "  ${REPORTS_DIR}/"
    echo ""

    # Show last 50 lines of log for quick diagnosis
    if [ -f "$TEST_LOG" ]; then
        echo ""
        echo "Last 50 lines of test log:"
        echo "---"
        tail -50 "$TEST_LOG"
    fi

    exit 1
fi
