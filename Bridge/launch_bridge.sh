#!/bin/bash
# NodeToCode MCP Bridge Launcher (macOS/Linux)
# Automatically finds Python - prefers UE's bundled Python, falls back to system Python

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BRIDGE_SCRIPT="$SCRIPT_DIR/nodetocode_bridge.py"

# Check if bridge script exists
if [ ! -f "$BRIDGE_SCRIPT" ]; then
    echo "[NodeToCode-Bridge] ERROR: Bridge script not found at $BRIDGE_SCRIPT" >&2
    exit 1
fi

PYTHON_EXE=""

# === Try to find UE's bundled Python ===

# Common UE installation paths on macOS
UE_PATHS=(
    "/Users/Shared/Epic Games"
    "$HOME/Epic Games"
    "/Applications/Epic Games"
)

# UE versions to check (newest first)
UE_VERSIONS=("UE_5.7" "UE_5.6" "UE_5.5" "UE_5.4" "UE_5.3" "UE_5.2" "UE_5.1" "UE_5.0")

for ue_path in "${UE_PATHS[@]}"; do
    for ue_version in "${UE_VERSIONS[@]}"; do
        TEST_PATH="$ue_path/$ue_version/Engine/Binaries/ThirdParty/Python3/Mac/bin/python3"
        if [ -f "$TEST_PATH" ]; then
            PYTHON_EXE="$TEST_PATH"
            echo "[NodeToCode-Bridge] Found UE Python: $PYTHON_EXE" >&2
            break 2
        fi
    done
done

# === Fall back to system Python ===

if [ -z "$PYTHON_EXE" ]; then
    if command -v python3 &> /dev/null; then
        PYTHON_EXE="python3"
        echo "[NodeToCode-Bridge] Using system Python3" >&2
    elif command -v python &> /dev/null; then
        PYTHON_EXE="python"
        echo "[NodeToCode-Bridge] Using system Python" >&2
    else
        echo "[NodeToCode-Bridge] ERROR: Python not found. Please install Python or specify UE Python path." >&2
        exit 1
    fi
fi

# Run the bridge with all passed arguments
exec "$PYTHON_EXE" "$BRIDGE_SCRIPT" "$@"
