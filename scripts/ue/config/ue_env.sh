#!/bin/bash
# Unreal Engine Environment Configuration
# Single source of truth for UE paths and settings

# Delegate to central config loader (SOT: scripts/config/ue_env.sh)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

# shellcheck source=../../config/ue_env.sh
source "${PROJECT_ROOT}/scripts/config/ue_env.sh"

# Project paths (Unix-style for Git Bash)
export PROJECT_ROOT="${PROJECT_ROOT}"
export PROJECT_FILE="${PROJECT_ROOT}/Alis.uproject"

# Windows-style paths for UE tools (convert Unix paths)
export UE_PATH_WINDOWS="$(cygpath -w "$UE_PATH" 2>/dev/null || echo "$UE_PATH")"
export PROJECT_FILE_WINDOWS="$(cygpath -w "$PROJECT_FILE" 2>/dev/null || echo "$PROJECT_FILE")"

echo "UE Environment:"
echo "  UE Path: $UE_PATH ($UE_TYPE)"
echo "  Project: $PROJECT_FILE"
