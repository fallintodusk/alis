#!/usr/bin/env bash
# Autonomous build script for Build Service (Git Bash on Windows)
# Ensures CMake and NASM are in PATH before running cargo

set -e

# Add CMake to PATH (standard Windows installation location)
export PATH="/c/Program Files/CMake/bin:$PATH"

# Add NASM to PATH if installed via Chocolatey
if [ -d "/c/Program Files/NASM" ]; then
    export PATH="/c/Program Files/NASM:$PATH"
fi

# Verify CMake is available
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake not found. Please install CMake:"
    echo "  choco install cmake"
    echo "Or download from: https://cmake.org/download/"
    exit 1
fi

echo "Using CMake: $(cmake --version | head -1)"

# Set AWS_LC_SYS_PREBUILT_NASM to use prebuilt NASM objects
export AWS_LC_SYS_PREBUILT_NASM=1

# Run cargo command (pass all arguments)
echo "Running: cargo $@"
cargo "$@"
