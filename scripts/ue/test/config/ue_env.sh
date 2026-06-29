#!/bin/bash
# Compatibility bridge for legacy test wrappers.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck source=../../config/ue_env.sh
source "${SCRIPT_DIR}/../../config/ue_env.sh"
