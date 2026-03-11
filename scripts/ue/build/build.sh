#!/bin/bash
# Unreal Engine Build Wrapper
# Unified interface for building, cleaning, and packaging Alis project

set -e

# Load central UE environment configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/../config/ue_env.sh"

# Build configurations
BUILD_CONFIG=${2:-"Development"}  # Default to Development if not specified
BUILD_TARGET=${1:-"AlisEditor"}   # Default to Editor if not specified
BUILD_PLATFORM="Win64"

# Build command paths
BUILD_BAT="${UE_PATH}/Engine/Build/BatchFiles/Build.bat"
CLEAN_BAT="${UE_PATH}/Engine/Build/BatchFiles/Clean.bat"
REBUILD_BAT="${UE_PATH}/Engine/Build/BatchFiles/Rebuild.bat"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# Parse command
COMMAND=${3:-"build"}

# Extra arguments (e.g., -Module=ProjectUI)
EXTRA_ARGS="${@:4}"

case "$COMMAND" in
    build)
        echo -e "${YELLOW}Building ${BUILD_TARGET} (${BUILD_CONFIG})...${NC}"
        "${BUILD_BAT}" ${BUILD_TARGET} ${BUILD_PLATFORM} ${BUILD_CONFIG} "${PROJECT_FILE_WINDOWS}" ${EXTRA_ARGS}
        ;;

    clean)
        echo -e "${YELLOW}Cleaning ${BUILD_TARGET}...${NC}"
        "${CLEAN_BAT}" ${BUILD_TARGET} ${BUILD_PLATFORM} ${BUILD_CONFIG} "${PROJECT_FILE_WINDOWS}" ${EXTRA_ARGS}
        ;;

    rebuild)
        echo -e "${YELLOW}Rebuilding ${BUILD_TARGET}...${NC}"
        "${REBUILD_BAT}" ${BUILD_TARGET} ${BUILD_PLATFORM} ${BUILD_CONFIG} "${PROJECT_FILE_WINDOWS}" ${EXTRA_ARGS}
        ;;

    *)
        cat <<EOF
Usage: $(basename "$0") [target] [config] [command] [extra_args...]

Targets:
  Alis              - Game target (client + server)
  AlisEditor        - Editor target (default)
  AlisClient        - Client-only target
  AlisServer        - Dedicated server target

Configs:
  Debug, DebugGame, Development (default), Test, Shipping

Commands:
  build    - Build the project (default)
  clean    - Clean build files
  rebuild  - Clean and build

Extra Args:
  -Module=<name>    - Build specific module only
  -WaitMutex        - Wait for mutex before building
  -clean            - Force clean before build

Examples:
  $(basename "$0")                                    # Build AlisEditor Development
  $(basename "$0") Alis Shipping build                # Build game in Shipping
  $(basename "$0") AlisEditor Development build -Module=ProjectBoot
EOF
        exit 1
        ;;
esac

# Check exit code
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ ${COMMAND} completed successfully${NC}"
else
    echo -e "${RED}✗ ${COMMAND} failed with exit code $?${NC}"
    exit $?
fi
