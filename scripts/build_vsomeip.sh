#!/bin/bash
# Script to build vsomeip from repo root, without changing current dir

set -e

# Get repo root (directory of this script, then go up one level)
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
VSOMEIP_DIR="$REPO_ROOT/vsomeip"

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Check if git-lfs is initialized
if ! git lfs ls-files &>/dev/null; then
    echo "Error: git-lfs is not initialized. Please run 'git lfs install' and 'git lfs pull'." >&2
    exit 1
fi

# Configure CMake for vsomeip
cmake -S "$VSOMEIP_DIR" -B "$BUILD_DIR" -DICEORYX2_INSTALL_DIR=$REPO_ROOT/rootfs

# Build the project
make -C "$BUILD_DIR" -j$(nproc)
