#!/bin/bash
# Conformance runner for the sh resolver (scripts/config/ue_env.sh).
#
# ue_env.sh validates that the resolved engine dir EXISTS, so this runner
# generates grammar-equivalent cases pointing at real temp directories
# (same case IDs as the shared corpus in fixtures/). Native runner on
# purpose: Pester must not require bash.
#
# Run: bash scripts/config/test/conformance_sh.sh
# Exit code: 0 = all pass, 1 = failures.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UE_ENV="${HERE}/../ue_env.sh"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

FAILURES=0

check() {
    local name="$1" ok="$2" detail="${3:-}"
    if [ "$ok" = "1" ]; then
        echo "[OK] $name"
    else
        echo "[FAIL] $name $detail"
        FAILURES=$((FAILURES + 1))
    fi
}

# Real engine-shaped temp dirs (ue_env.sh checks existence)
ENGINE_A="$WORK/EngineA"; mkdir -p "$ENGINE_A/Engine"
ENGINE_B="$WORK/EngineB"; mkdir -p "$ENGINE_B/Engine"
WIN_A="$(cd "$ENGINE_A" && pwd -W 2>/dev/null || pwd)"
WIN_B="$(cd "$ENGINE_B" && pwd -W 2>/dev/null || pwd)"

# run_case <name> <expect: ok|err> [env_ue_path]
# Uses conf files staged into $WORK/case/<name>; ue_env.sh reads confs
# from ITS OWN dir -> copy it into the case dir alongside the confs.
run_case() {
    local name="$1" expect="$2" env_val="${3:-}"
    local dir="$WORK/case/$name"
    mkdir -p "$dir"
    cp "$UE_ENV" "$dir/ue_env.sh"
    local out rc
    if [ -n "$env_val" ]; then
        out="$(cd "$dir" && UE_PATH="$env_val" bash ./ue_env.sh 2>&1)"
    else
        out="$(cd "$dir" && env -u UE_PATH bash ./ue_env.sh 2>&1)"
    fi
    rc=$?
    if [ "$expect" = "ok" ]; then
        check "$name" "$([ $rc -eq 0 ] && echo 1 || echo 0)" "rc=$rc out=$out"
    else
        check "$name" "$([ $rc -ne 0 ] && echo 1 || echo 0)" "expected failure, rc=0 out=$out"
    fi
}

stage() { mkdir -p "$WORK/case/$1"; }

# 01 basic
stage 01_basic
printf 'UE_PATH=%s\n' "$WIN_A" > "$WORK/case/01_basic/ue_path.conf"
run_case 01_basic ok

# 02 local override wins
stage 02_local_override
printf 'UE_PATH=%s\n' "$WIN_A" > "$WORK/case/02_local_override/ue_path.conf"
printf 'UE_PATH=%s\n' "$WIN_B" > "$WORK/case/02_local_override/ue_path.local.conf"
run_case 02_local_override ok

# 03 subset merge (local adds UE_SOURCE_PATH only)
stage 03_subset_merge
printf 'UE_PATH=%s\n' "$WIN_A" > "$WORK/case/03_subset_merge/ue_path.conf"
printf 'UE_SOURCE_PATH=%s\n' "$WIN_B" > "$WORK/case/03_subset_merge/ue_path.local.conf"
run_case 03_subset_merge ok

# 04 duplicate key
stage 04_duplicate_key
printf 'UE_PATH=%s\nUE_PATH=%s\n' "$WIN_A" "$WIN_B" > "$WORK/case/04_duplicate_key/ue_path.conf"
run_case 04_duplicate_key err

# 05 unknown key
stage 05_unknown_key
printf 'UE_PATH=%s\nFOO_KEY=bar\n' "$WIN_A" > "$WORK/case/05_unknown_key/ue_path.conf"
run_case 05_unknown_key err

# 06 empty value
stage 06_empty_value
printf 'UE_PATH=\n' > "$WORK/case/06_empty_value/ue_path.conf"
run_case 06_empty_value err

# 07 spaces around '='
stage 07_spaces_around_eq
printf 'UE_PATH = %s\n' "$WIN_A" > "$WORK/case/07_spaces_around_eq/ue_path.conf"
run_case 07_spaces_around_eq err

# 08 inline comment
stage 08_inline_comment
printf 'UE_PATH=%s # inline\n' "$WIN_A" > "$WORK/case/08_inline_comment/ue_path.conf"
run_case 08_inline_comment err

# 09 dollar
stage 09_dollar
printf 'UE_PATH=%s/$X\n' "$WIN_A" > "$WORK/case/09_dollar/ue_path.conf"
run_case 09_dollar err

# 10 CRLF valid
stage 10_crlf
printf 'UE_PATH=%s\r\n' "$WIN_A" > "$WORK/case/10_crlf/ue_path.conf"
run_case 10_crlf ok

# 12 make syntax rejected
stage 12_make_syntax
printf 'UE_PATH := %s\n' "$WIN_A" > "$WORK/case/12_make_syntax/ue_path.conf"
run_case 12_make_syntax err

# stale env: mismatching cache must hard-fail
stage 20_stale_env
printf 'UE_PATH=%s\n' "$WIN_A" > "$WORK/case/20_stale_env/ue_path.conf"
run_case 20_stale_env err "$WIN_B"

# matching env (different form: trailing slash) must pass
stage 21_env_match
printf 'UE_PATH=%s\n' "$WIN_A" > "$WORK/case/21_env_match/ue_path.conf"
run_case 21_env_match ok "$WIN_A/"

echo "---"
if [ "$FAILURES" -gt 0 ]; then
    echo "FAILED: $FAILURES case(s)"
    exit 1
fi
echo "ALL PASS"
