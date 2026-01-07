"""
Update AGENTS.md with Unreal Engine Source Path

This script detects the UE installation and updates the AGENTS.md file
with the correct Engine/Source path.

Uses ue_python_finder.py for UE detection.
"""

import os
import sys
import re

# Add mcp_bridge to path to import ue_python_finder
script_dir = os.path.dirname(os.path.abspath(__file__))
plugin_root = os.path.dirname(os.path.dirname(script_dir))
mcp_bridge_path = os.path.join(plugin_root, 'Content', 'Python', 'mcp_bridge')
sys.path.insert(0, mcp_bridge_path)

from ue_python_finder import find_ue_source


def update_agents_md(source_path: str, agents_md_path: str) -> bool:
    """
    Update the UE_SOURCE_PATH line in AGENTS.md.

    Args:
        source_path: The UE Engine/Source path to set
        agents_md_path: Path to AGENTS.md file

    Returns:
        True if successful, False otherwise
    """
    if not os.path.exists(agents_md_path):
        print(f"Error: AGENTS.md not found at {agents_md_path}", file=sys.stderr)
        return False

    try:
        with open(agents_md_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # Pattern to match the UE_SOURCE_PATH line
        # Matches: UE_SOURCE_PATH: <anything until end of line>
        pattern = r'^UE_SOURCE_PATH:.*$'
        # Escape backslashes in path for regex replacement
        escaped_path = source_path.replace('\\', '\\\\')
        replacement = f'UE_SOURCE_PATH: {escaped_path}'

        new_content, count = re.subn(pattern, replacement, content, flags=re.MULTILINE)

        if count == 0:
            print("Warning: UE_SOURCE_PATH line not found in AGENTS.md", file=sys.stderr)
            return False

        with open(agents_md_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

        print(f"Successfully updated AGENTS.md with UE source path: {source_path}")
        return True

    except Exception as e:
        print(f"Error updating AGENTS.md: {e}", file=sys.stderr)
        return False


def main():
    import argparse

    parser = argparse.ArgumentParser(description='Update AGENTS.md with UE source path')
    parser.add_argument('--debug', action='store_true', help='Enable debug output')
    parser.add_argument('--agents-md', type=str, help='Path to AGENTS.md (auto-detected if not specified)')
    args = parser.parse_args()

    # Auto-detect AGENTS.md path
    if args.agents_md:
        agents_md_path = args.agents_md
    else:
        # Default to plugin root AGENTS.md
        agents_md_path = os.path.join(plugin_root, 'AGENTS.md')

    if args.debug:
        print(f"Looking for AGENTS.md at: {agents_md_path}", file=sys.stderr)

    # Find UE source path
    source_path = find_ue_source(prefer_newest=True, debug=args.debug)

    if not source_path:
        print("Error: Could not find Unreal Engine installation", file=sys.stderr)
        print("Please install Unreal Engine or manually set UE_SOURCE_PATH in AGENTS.md", file=sys.stderr)
        sys.exit(1)

    # Update AGENTS.md
    if update_agents_md(source_path, agents_md_path):
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == '__main__':
    main()
