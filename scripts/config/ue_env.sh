#!/bin/bash
# Central Unreal Engine environment configuration
# Auto-detects UE path from registry or uses manual config

# Normalize Windows-style paths to the current shell's preferred format
normalize_path() {
    local input_path="$1"
    if [ -z "$input_path" ]; then
        return 1
    fi

    # Already valid?
    if [ -d "$input_path" ]; then
        echo "$input_path"
        return 0
    fi

    # cygpath handles Git Bash / MSYS environments
    if command -v cygpath >/dev/null 2>&1; then
        local cygpath_out
        cygpath_out="$(cygpath -u "$input_path" 2>/dev/null)"
        if [ -n "$cygpath_out" ] && [ -d "$cygpath_out" ]; then
            echo "$cygpath_out"
            return 0
        fi
    fi

    # Manual conversion for Windows drive letters (useful for WSL)
    if [[ "$input_path" =~ ^[A-Za-z]: ]]; then
        local drive_letter
        drive_letter="$(printf "%s" "$input_path" | cut -c1 | tr 'A-Z' 'a-z')"
        local path_rest="${input_path:2}"
        path_rest="$(printf "%s" "$path_rest" | sed 's|\\|/|g')"

        local wsl_path="/mnt/${drive_letter}${path_rest}"
        if [ -d "$wsl_path" ]; then
            echo "$wsl_path"
            return 0
        fi

        local msys_path="/${drive_letter}${path_rest}"
        if [ -d "$msys_path" ]; then
            echo "$msys_path"
            return 0
        fi
    fi

    # If none of the conversions worked, return the original path
    echo "$input_path"
    return 1
}

# Convert Unix-style paths back to Windows style when needed
to_windows_path() {
    local input_path="$1"
    if [ -z "$input_path" ]; then
        return 1
    fi

    if [[ "$input_path" =~ ^/mnt/([A-Za-z])/(.*) ]]; then
        local drive_letter
        drive_letter="$(printf "%s" "${BASH_REMATCH[1]}" | tr 'a-z' 'A-Z')"
        local rest="${BASH_REMATCH[2]}"
        echo "${drive_letter}:/${rest}"
        return 0
    fi

    if [[ "$input_path" =~ ^/([A-Za-z])/(.*) ]]; then
        local drive_letter
        drive_letter="$(printf "%s" "${BASH_REMATCH[1]}" | tr 'a-z' 'A-Z')"
        local rest="${BASH_REMATCH[2]}"
        echo "${drive_letter}:/${rest}"
        return 0
    fi

    if command -v cygpath >/dev/null 2>&1; then
        local cygpath_out
        cygpath_out="$(cygpath -m "$input_path" 2>/dev/null)"
        if [ -n "$cygpath_out" ]; then
            echo "$cygpath_out"
            return 0
        fi
    fi

    if command -v wslpath >/dev/null 2>&1; then
        local wsl_out
        wsl_out="$(wslpath -m "$input_path" 2>/dev/null)"
        if [ -n "$wsl_out" ]; then
            echo "$wsl_out"
            return 0
        fi
    fi

    echo "$input_path"
    return 1
}

# Resolve script directory robustly (supports sourcing)
SCRIPT_SOURCE="${BASH_SOURCE[0]:-$0}"
SCRIPT_DIR="$(cd "$(dirname "$SCRIPT_SOURCE")" && pwd)"

# --------------------------------------------------------------------------
# AUTHORITY: conf (ue_path.local.conf > ue_path.conf, PER KEY) is
# authoritative. Pre-existing env UE_PATH is a derived cache; a mismatch
# with the resolved conf value is a HARD FAIL. Numeric-highest launcher
# fallback runs only when no conf declares UE_PATH.
# Grammar SOT: ue_path.conf header. Strict: dup/unknown/empty = error.
# --------------------------------------------------------------------------

CONFIG_FILE="${SCRIPT_DIR}/ue_path.conf"
LOCAL_CONFIG_FILE="${SCRIPT_DIR}/ue_path.local.conf"
UE_ENV_INCOMING_UE_PATH="${UE_PATH:-}"
UE_PATH=""
UE_SOURCE_PATH="${UE_SOURCE_PATH:-}"

# Canonical comparable form: windows-style, lowercase, no trailing slash
canon_engine_path() {
    local p="$1"
    [ -z "$p" ] && return 0
    p="$(to_windows_path "$p")"
    p="$(printf '%s' "$p" | sed 's|\\|/|g; s|/*$||' | tr 'A-Z' 'a-z')"
    printf '%s' "$p"
}

# Strict parse of one conf file; sets CONF_<KEY> vars. Exits on violation.
parse_conf_file() {
    local file="$1" lineno=0 line key value
    while IFS= read -r raw_line || [ -n "$raw_line" ]; do
        lineno=$((lineno + 1))
        line="$(printf '%s' "$raw_line" | tr -d '\r')"
        case "$line" in
            ''|\#*) continue ;;
        esac
        if [[ ! "$line" =~ ^([A-Z_][A-Z0-9_]*)=(.*)$ ]]; then
            echo "ERROR: ${file}:${lineno}: malformed line (grammar: KEY=VALUE, no spaces around '='): ${line}" >&2
            exit 1
        fi
        key="${BASH_REMATCH[1]}"
        value="${BASH_REMATCH[2]}"
        case "$key" in
            UE_PATH|UE_SOURCE_PATH|BUILD_TARGET|BUILD_CONFIG|BUILD_PLATFORM) ;;
            *)
                echo "ERROR: ${file}:${lineno}: unknown key '${key}'" >&2
                exit 1 ;;
        esac
        if eval "[ -n \"\${SEEN_${key}:-}\" ]"; then
            echo "ERROR: ${file}:${lineno}: duplicate key '${key}'" >&2
            exit 1
        fi
        if [ -z "$value" ]; then
            echo "ERROR: ${file}:${lineno}: empty value for '${key}'" >&2
            exit 1
        fi
        case "$value" in
            *'#'*|*'$'*|*'"'*|*"'"*)
                echo "ERROR: ${file}:${lineno}: forbidden character in value for '${key}'" >&2
                exit 1 ;;
        esac
        eval "SEEN_${key}=1"
        eval "CONF_${key}=\"\$value\""
    done < "$file"
    unset SEEN_UE_PATH SEEN_UE_SOURCE_PATH SEEN_BUILD_TARGET SEEN_BUILD_CONFIG SEEN_BUILD_PLATFORM
}

# Key-level merge: tracked first, local overrides
[ -f "$CONFIG_FILE" ] && parse_conf_file "$CONFIG_FILE"
[ -f "$LOCAL_CONFIG_FILE" ] && parse_conf_file "$LOCAL_CONFIG_FILE"

UE_PATH_RAW="${CONF_UE_PATH:-}"
UE_SOURCE_PATH="${CONF_UE_SOURCE_PATH:-$UE_SOURCE_PATH}"

# Numeric-highest valid launcher fallback (only when no conf declares it)
detect_ue_fallback() {
    local root dir best="" best_ver=""
    for root in "/c/UnrealEngine" "/c/Program Files/Epic Games"; do
        [ -d "$root" ] || continue
        for dir in "$root"/UE_*; do
            [ -d "$dir" ] || continue
            [[ "$(basename "$dir")" =~ ^UE_([0-9]+\.[0-9]+)$ ]] || continue
            [ -f "$dir/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ] || continue
            local ver="${BASH_REMATCH[1]}"
            if [ -z "$best" ] || [ "$(printf '%s\n%s\n' "$best_ver" "$ver" | sort -V | tail -1)" = "$ver" ]; then
                best="$dir"
                best_ver="$ver"
            fi
        done
    done
    [ -n "$best" ] && printf '%s' "$best"
}

if [ -z "$UE_PATH_RAW" ]; then
    UE_PATH_RAW="$(detect_ue_fallback)"
    if [ -n "$UE_PATH_RAW" ]; then
        echo "[ue_env] No conf UE_PATH; using highest valid install: $UE_PATH_RAW"
    fi
fi

if [ -z "$UE_PATH_RAW" ]; then
    echo "ERROR: UE_PATH not resolved. Set it in scripts/config/ue_path.conf (or ue_path.local.conf)." >&2
    exit 1
fi

# Stale-cache hard fail: derived env must match the resolved conf value
if [ -n "$UE_ENV_INCOMING_UE_PATH" ]; then
    if [ "$(canon_engine_path "$UE_ENV_INCOMING_UE_PATH")" != "$(canon_engine_path "$UE_PATH_RAW")" ]; then
        echo "ERROR: stale UE_PATH env ($UE_ENV_INCOMING_UE_PATH) does not match resolved conf value ($UE_PATH_RAW) - rerun scripts/setup/setup_ue_env.ps1" >&2
        exit 1
    fi
fi

UE_PATH="$(normalize_path "$UE_PATH_RAW")"
if [ -z "$UE_PATH" ] || [ ! -d "$UE_PATH" ]; then
    echo "ERROR: resolved UE_PATH is not a directory: $UE_PATH_RAW" >&2
    exit 1
fi
echo "Using UE_PATH: $UE_PATH"
export UE_PATH
export UE_ENGINE_DIR="${UE_PATH}/Engine"
[ -n "$UE_SOURCE_PATH" ] && export UE_SOURCE_PATH

# Verify engine directory exists
if [ ! -d "$UE_ENGINE_DIR" ]; then
    echo "ERROR: UE_PATH set but Engine directory not found!"
    echo "UE_PATH: $UE_PATH"
    echo "Expected: $UE_ENGINE_DIR"
    exit 1
fi

# Export for use in other scripts
export UE_PATH
export UE_ENGINE_DIR
export PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
export PROJECT_FILE="${PROJECT_ROOT}/Alis.uproject"
export PROJECT_FILE_WINDOWS="$(to_windows_path "$PROJECT_FILE")"
export UE_PATH_WINDOWS="$(to_windows_path "$UE_PATH")"

echo "[OK] Unreal Engine environment configured:"
echo "  UE_PATH: $UE_PATH"
echo "  PROJECT_ROOT: $PROJECT_ROOT"
