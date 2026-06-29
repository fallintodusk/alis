#!/bin/bash
# Compatibility entry point. Keep integration launch logic in integration.sh.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec bash "${SCRIPT_DIR}/integration.sh" "$@"
