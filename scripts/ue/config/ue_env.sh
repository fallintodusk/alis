#!/bin/bash
# Unreal Engine Environment Configuration
# Single source of truth for UE paths and settings

# Load UE path from config (single source of truth)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
CONFIG_FILE="${PROJECT_ROOT}/scripts/config/ue_path.conf"

UE_PATH=""
if [ -f "$CONFIG_FILE" ]; then
    while IFS= read -r line; do
        case "$line" in
            \#*|"") continue ;;
        esac
        if [[ "$line" =~ ^[[:space:]]*UE_PATH[[:space:]]*(:=|=)[[:space:]]*(.+)$ ]]; then
            UE_PATH="${BASH_REMATCH[2]}"
            UE_PATH="${UE_PATH%\"}"
            UE_PATH="${UE_PATH#\"}"
            UE_PATH="${UE_PATH%\'}"
            UE_PATH="${UE_PATH#\'}"
            break
        fi
    done < "$CONFIG_FILE"
fi

if [ -z "$UE_PATH" ]; then
    echo "ERROR: UE_PATH not set. Configure scripts/config/ue_path.conf"
    exit 1
fi

export UE_PATH
export UE_TYPE="config"

# Project paths (Unix-style for Git Bash)
export PROJECT_ROOT="${PROJECT_ROOT}"
export PROJECT_FILE="${PROJECT_ROOT}/Alis.uproject"

# Windows-style paths for UE tools (convert Unix paths)
export UE_PATH_WINDOWS="$(cygpath -w "$UE_PATH" 2>/dev/null || echo "$UE_PATH")"
export PROJECT_FILE_WINDOWS="$(cygpath -w "$PROJECT_FILE" 2>/dev/null || echo "$PROJECT_FILE")"

echo "UE Environment:"
echo "  UE Path: $UE_PATH ($UE_TYPE)"
echo "  Project: $PROJECT_FILE"
