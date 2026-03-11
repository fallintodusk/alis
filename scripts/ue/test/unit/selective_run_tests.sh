#!/bin/bash
# Selectively run automation tests based on changed files.
# Determines relevant plugin/unit test filters from the working tree diff
# (optionally relative to a provided base ref) and executes run_tests.sh
# with those filters.

set -euo pipefail

usage() {
    cat <<EOF
Usage: $(basename "$0") [--base <ref>] [--help]

Options:
  --base <ref>   Compare changes against the specified git ref (default: HEAD)
  --help         Show this help message
EOF
}

BASE_REF="HEAD"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --base)
            if [[ -z "${2:-}" ]]; then
                echo "Error: --base requires a value" >&2
                usage
                exit 1
            fi
            BASE_REF="$2"
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
            echo "Unexpected positional argument: $1" >&2
            usage
            exit 1
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

cd "$PROJECT_ROOT"

# Verify the base reference exists (allow refs like origin/main)
if ! git rev-parse --verify "$BASE_REF" >/dev/null 2>&1; then
    echo "Error: base reference '$BASE_REF' is not a valid git ref" >&2
    exit 1
fi

echo "Analyzing changes relative to '${BASE_REF}'..."

# Collect changed files across commits and working tree
mapfile -t RANGE_DIFF < <(git diff --name-only "${BASE_REF}..HEAD")
mapfile -t WORKTREE_DIFF < <(git diff --name-only)
mapfile -t STAGED_DIFF < <(git diff --name-only --cached)
mapfile -t UNTRACKED_FILES < <(git ls-files --others --exclude-standard)

declare -A UNIQUE_FILES=()
for FILE in "${RANGE_DIFF[@]}" "${WORKTREE_DIFF[@]}" "${STAGED_DIFF[@]}" "${UNTRACKED_FILES[@]}"; do
    [[ -n "$FILE" ]] && UNIQUE_FILES["$FILE"]=1
done

if [[ ${#UNIQUE_FILES[@]} -eq 0 ]]; then
    echo "No local changes detected relative to '${BASE_REF}'. Skipping selective test run."
    exit 0
fi

# Map of path prefixes to automation filters
declare -A FILTER_MAP=(
    ["Plugins/Core/ProjectMenuUI"]="ProjectMenuUI"
    ["Plugins/Core/ProjectUI"]="ProjectUI"
    ["Plugins/Core/ProjectData"]="ProjectData"
    ["Plugins/Core/ProjectLoading"]="ProjectLoading"
    ["Plugins/Core/ProjectSession"]="ProjectSession"
    ["Plugins/Core/ProjectBoot"]="ProjectBoot"
    ["Plugins/Core/ProjectContentPacks"]="ProjectContentPacks"
    ["Plugins/Core/ProjectGameFeatures"]="ProjectGameFeatures"
    ["Plugins/Core/ProjectSave"]="ProjectSave"
    ["Plugins/GameFeatures/ProjectCombat"]="ProjectCombat"
    ["Plugins/GameFeatures/ProjectDialogue"]="ProjectDialogue"
    ["Plugins/GameFeatures/ProjectInventory"]="ProjectInventory"
    ["Plugins/UI/ProjectMenuUI"]="ProjectMenuUI"
)

DEFAULT_FILTERS=("ProjectMenuUI" "ProjectUI")
UNIT_FILTERS=(
    "ProjectCore" "ProjectBoot" "ProjectLoading" "ProjectSession"
    "ProjectData" "ProjectUI" "ProjectMenuUI" "ProjectContentPacks"
    "ProjectGameFeatures" "ProjectSave" "ProjectCombat"
    "ProjectDialogue" "ProjectInventory"
)

declare -A SELECTED_SET=()
SELECTED_FILTERS=()

add_filter() {
    local FILTER="$1"
    if [[ -n "$FILTER" && -z "${SELECTED_SET[$FILTER]+x}" ]]; then
        SELECTED_SET["$FILTER"]=1
        SELECTED_FILTERS+=("$FILTER")
    fi
}

for FILE in "${!UNIQUE_FILES[@]}"; do
    # If the testing harness or docs changed, run the full unit suite
    if [[ "$FILE" == utility/test/* || "$FILE" == Docs/testing.md ]]; then
        for FILTER in "${UNIT_FILTERS[@]}"; do
            add_filter "$FILTER"
        done
        continue
    fi

    local_matched=false
    for PREFIX in "${!FILTER_MAP[@]}"; do
        if [[ "$FILE" == "$PREFIX"* ]]; then
            add_filter "${FILTER_MAP[$PREFIX]}"
            local_matched=true
        fi
    done

    # If the change lives in generic UI config (Config/UI) ensure UI tests run
    if [[ "$local_matched" = false && "$FILE" == Config/UI/* ]]; then
        add_filter "ProjectMenuUI"
        add_filter "ProjectUI"
    fi
done

if [[ ${#SELECTED_FILTERS[@]} -eq 0 ]]; then
    echo "No plugin-specific mappings found for local changes."
    echo "Using default selective filters: ${DEFAULT_FILTERS[*]}"
    SELECTED_FILTERS=("${DEFAULT_FILTERS[@]}")
else
    echo "Selected automation filters: ${SELECTED_FILTERS[*]}"
fi

# Prepare plugins before test executes to keep pipeline aligned with make targets
"${SCRIPT_DIR}/prepare_tests.sh"

CMD=("${SCRIPT_DIR}/run_tests.sh")
for FILTER in "${SELECTED_FILTERS[@]}"; do
    CMD+=("--filter" "$FILTER")
done

echo "Executing: ${CMD[*]}"
"${CMD[@]}"
