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
  --list-filters Print selected automation filters without running tests
  --help         Show this help message
EOF
}

BASE_REF="HEAD"
LIST_FILTERS_ONLY=0

CHANGE_PATHS=(
    "Alis.uproject"
    "Makefile"
    "Source/**"
    "scripts/**"
    "docs/**"
    "Config/**"
    "Plugins/**/Source/**"
    "Plugins/**/Data/**"
    "Plugins/**/Config/**"
    "Plugins/**/*.uplugin"
)

DEFAULT_FILTERS=("ProjectCore" "ProjectUI")

PLUGIN_ROOTS=()
PLUGIN_FILTERS=()

declare -A PLUGIN_TEST_FILTER_MAP=(
    ["Orchestrator"]="Orchestrator"
    ["ProjectCombat"]="ProjectCombat"
    ["ProjectCore"]="ProjectCore"
    ["ProjectDialogue"]="ProjectDialogue"
    # ProjectIntegrationTests is owned by test-integration-batch.
    ["ProjectInventory"]="ProjectInventory"
    ["ProjectLoading"]="ProjectLoading"
    ["ProjectSave"]="ProjectSave"
    ["ProjectUI"]="ProjectUI"
)

# Hooks are disabled by pointing at a UNIQUE, empty, per-invocation directory -
# never at /dev/null. On Windows a native git resolves "/dev/null" RELATIVE to the
# repository (it has no drive letter), so git-lfs creates a literal
# <repo>/dev/null/ directory and installs post-checkout, post-commit, post-merge
# and pre-push into it. Git Bash happens to rewrite the argument to "nul" today,
# which only hides the hazard. See git-lfs issue 6297.
# The directory must be per-invocation: a fixed shared path persists between runs,
# so anything that ever drops a hook there would be executed by every later
# "safe" call - the opposite of the guarantee this makes.
PROJECT_EMPTY_HOOKS="$(mktemp -d "${TMPDIR:-/tmp}/project-empty-hooks.XXXXXX")"
trap 'rm -rf "$PROJECT_EMPTY_HOOKS" 2>/dev/null || true' EXIT

git_safe() {
    GIT_LFS_SKIP_SMUDGE=1 git \
        -c core.fsmonitor=false \
        -c core.hooksPath="$PROJECT_EMPTY_HOOKS" \
        -c core.safecrlf=false \
        -c filter.lfs.process= \
        -c filter.lfs.required=false \
        -c filter.lfs.smudge= \
        -c filter.lfs.clean= \
        "$@"
}

load_plugin_roots() {
    local plugin_file
    local plugin_root
    local plugin_name

    while IFS= read -r plugin_file; do
        [[ -n "$plugin_file" ]] || continue
        plugin_root="${plugin_file%/*}"
        plugin_name="${plugin_file##*/}"
        plugin_name="${plugin_name%.uplugin}"
        PLUGIN_ROOTS+=("$plugin_root")
        PLUGIN_FILTERS+=("$plugin_name")
    done < <(
        git_safe ls-files "Plugins/**/*.uplugin" |
            awk '{ print length($0) "\t" $0 }' |
            sort -rn |
            cut -f2-
    )
}

plugin_filter_for_file() {
    local file="$1"
    local index
    local root

    for index in "${!PLUGIN_ROOTS[@]}"; do
        root="${PLUGIN_ROOTS[$index]}"
        if [[ "$file" == "$root" || "$file" == "$root/"* ]]; then
            printf '%s\n' "${PLUGIN_FILTERS[$index]}"
            return 0
        fi
    done

    return 1
}

test_filter_for_plugin() {
    local plugin_name="$1"

    if [[ -n "${PLUGIN_TEST_FILTER_MAP[$plugin_name]+x}" ]]; then
        printf '%s\n' "${PLUGIN_TEST_FILTER_MAP[$plugin_name]}"
        return 0
    fi

    return 1
}

collect_range_changes() {
    git_safe diff --no-ext-diff --name-only "${BASE_REF}..HEAD" -- "${CHANGE_PATHS[@]}"
}

collect_local_changes() {
    local line
    local path

    git_safe status --porcelain=v1 --untracked-files=all -- "${CHANGE_PATHS[@]}" |
        while IFS= read -r line; do
            [[ -n "$line" ]] || continue
            path="${line:3}"
            if [[ "$path" == *" -> "* ]]; then
                printf '%s\n' "${path%% -> *}"
                printf '%s\n' "${path##* -> }"
            else
                printf '%s\n' "$path"
            fi
        done
}

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
        --list-filters)
            LIST_FILTERS_ONLY=1
            shift
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
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"

cd "$PROJECT_ROOT"

# Verify the base reference exists (allow refs like origin/main)
if ! git_safe rev-parse --verify "$BASE_REF" >/dev/null 2>&1; then
    echo "Error: base reference '$BASE_REF' is not a valid git ref" >&2
    exit 1
fi

echo "Analyzing changes relative to '${BASE_REF}'..."
load_plugin_roots

# Collect changed files across commits and local working tree. Keep this
# path-scoped so asset-heavy branches do not make test selection crawl.
mapfile -t RANGE_DIFF < <(collect_range_changes)
mapfile -t LOCAL_DIFF < <(collect_local_changes)

declare -A UNIQUE_FILES=()
for FILE in "${RANGE_DIFF[@]}" "${LOCAL_DIFF[@]}"; do
    [[ -n "$FILE" ]] && UNIQUE_FILES["$FILE"]=1
done

if [[ ${#UNIQUE_FILES[@]} -eq 0 ]]; then
    echo "No local changes detected relative to '${BASE_REF}'. Skipping selective test run."
    exit 0
fi

declare -A SELECTED_SET=()
SELECTED_FILTERS=()

add_filter() {
    local FILTER="$1"
    if [[ -n "$FILTER" && -z "${SELECTED_SET[$FILTER]+x}" ]]; then
        SELECTED_SET["$FILTER"]=1
        SELECTED_FILTERS+=("$FILTER")
    fi
}

add_default_filters() {
    local FILTER
    for FILTER in "${DEFAULT_FILTERS[@]}"; do
        add_filter "$FILTER"
    done
}

# Safe superset for changes we cannot map to a single plugin (an unmapped
# plugin, or global config). Runs every known unit filter so a change can
# never report green while skipping the surface that could break. Sourced
# from PLUGIN_TEST_FILTER_MAP so there is no second list to keep in sync.
add_all_known_unit_filters() {
    local KEY
    for KEY in "${!PLUGIN_TEST_FILTER_MAP[@]}"; do
        add_filter "${PLUGIN_TEST_FILTER_MAP[$KEY]}"
    done
}

for FILE in "${!UNIQUE_FILES[@]}"; do
    # test-unit-smart stays a unit gate; integration has a separate make target.
    if [[ "$FILE" == scripts/ue/test/* || "$FILE" == scripts/test/* || "$FILE" == docs/testing/* ]]; then
        add_default_filters
        continue
    fi

    if [[ "$FILE" == Source/* ]]; then
        add_default_filters
        continue
    fi

    # Top-level engine/game config is global; it can affect any subsystem, so
    # run the full known unit set rather than only Core/UI. Plugin-scoped config
    # (Plugins/X/Config/...) is handled by the Plugins/* branch below, which
    # routes it to that plugin's filter.
    if [[ "$FILE" == Config/* ]]; then
        add_all_known_unit_filters
        continue
    fi

    if [[ "$FILE" == Plugins/* ]]; then
        if FILTER="$(plugin_filter_for_file "$FILE")"; then
            if TEST_FILTER="$(test_filter_for_plugin "$FILTER")"; then
                add_filter "$TEST_FILTER"
            else
                # Unmapped plugin: no single test filter is known for it, so
                # fall back to the full known set rather than only Core/UI.
                add_all_known_unit_filters
            fi
        fi
        continue
    fi

    if [[ "$FILE" == Makefile || "$FILE" == Alis.uproject || "$FILE" == scripts/* || "$FILE" == docs/build/* ]]; then
        add_default_filters
    fi
done

if [[ ${#SELECTED_FILTERS[@]} -eq 0 ]]; then
    echo "No plugin-specific mappings found for local changes."
    echo "Using default selective filters: ${DEFAULT_FILTERS[*]}"
    SELECTED_FILTERS=("${DEFAULT_FILTERS[@]}")
else
    echo "Selected automation filters: ${SELECTED_FILTERS[*]}"
fi

if [[ "$LIST_FILTERS_ONLY" -eq 1 ]]; then
    echo "Selection complete. Test execution skipped by --list-filters."
    exit 0
fi

# Prepare plugins before test executes to keep pipeline aligned with make targets
bash "${PROJECT_ROOT}/scripts/ue/test/lib/prepare_tests.sh"

CMD=(bash "${SCRIPT_DIR}/run_tests.sh")
for FILTER in "${SELECTED_FILTERS[@]}"; do
    CMD+=("--filter" "$FILTER")
done

echo "Executing: ${CMD[*]}"
"${CMD[@]}"
