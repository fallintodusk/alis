#!/bin/bash
# Compatibility entry point. Keep selection logic in selective.sh.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec bash "${SCRIPT_DIR}/selective.sh" "$@"
