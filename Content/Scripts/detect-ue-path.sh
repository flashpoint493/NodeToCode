#!/bin/bash
# NodeToCode UE Path Detector (macOS/Linux)
# Finds Unreal Engine installation and updates AGENTS.md
# Uses same Python detection methods as launch_bridge.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_SCRIPT="$SCRIPT_DIR/update_agents_ue_path.py"

# Check if Python script exists
if [ ! -f "$PYTHON_SCRIPT" ]; then
    echo "[detect-ue-path] ERROR: Python script not found at $PYTHON_SCRIPT"
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
            echo "[detect-ue-path] Found UE Python: $PYTHON_EXE"
            break 2
        fi
    done
done

# === Fall back to system Python ===

if [ -z "$PYTHON_EXE" ]; then
    if command -v python3 &> /dev/null; then
        PYTHON_EXE="python3"
        echo "[detect-ue-path] Using system Python3"
    elif command -v python &> /dev/null; then
        PYTHON_EXE="python"
        echo "[detect-ue-path] Using system Python"
    else
        echo "[detect-ue-path] ERROR: Python not found. Please install Python or Unreal Engine."
        exit 1
    fi
fi

# Run the Python script to update AGENTS.md
echo "[detect-ue-path] Running UE path detection..."
exec "$PYTHON_EXE" "$PYTHON_SCRIPT" "$@"
