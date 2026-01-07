"""
UE Python Finder

Finds Unreal Engine's bundled Python installation using the same methods
as UnrealVersionSelector:
1. Windows Registry (HKEY_CURRENT_USER\SOFTWARE\Epic Games\Unreal Engine\Builds)
2. Epic Games Launcher's LauncherInstalled.dat file
3. Common installation paths as fallback

This module uses only Python standard library.
"""

import os
import sys
import json
import re
from typing import Optional, List, Tuple

# Platform-specific imports
if sys.platform == 'win32':
    import winreg


def log_debug(message: str, debug: bool = False) -> None:
    """Log debug message to stderr."""
    if debug:
        print(f"[UE-Python-Finder] {message}", file=sys.stderr, flush=True)


def get_ue_installs_from_registry(debug: bool = False) -> List[Tuple[str, str]]:
    """
    Get UE installations from Windows Registry.
    Returns list of (version_name, install_path) tuples.
    """
    installations = []

    if sys.platform != 'win32':
        return installations

    # Check HKEY_CURRENT_USER\SOFTWARE\Epic Games\Unreal Engine\Builds
    # This is where custom/source builds are registered
    try:
        key_path = r"SOFTWARE\Epic Games\Unreal Engine\Builds"
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, key_path) as key:
            i = 0
            while True:
                try:
                    name, value, _ = winreg.EnumValue(key, i)
                    if os.path.isdir(value):
                        log_debug(f"Registry build found: {name} -> {value}", debug)
                        installations.append((name, value))
                    i += 1
                except OSError:
                    break
    except FileNotFoundError:
        log_debug("Registry key for custom builds not found", debug)
    except Exception as e:
        log_debug(f"Error reading registry builds: {e}", debug)

    # Check HKEY_LOCAL_MACHINE\SOFTWARE\EpicGames\Unreal Engine
    # This is where launcher-installed versions are registered
    try:
        key_path = r"SOFTWARE\EpicGames\Unreal Engine"
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, key_path) as key:
            i = 0
            while True:
                try:
                    subkey_name = winreg.EnumKey(key, i)
                    try:
                        with winreg.OpenKey(key, subkey_name) as subkey:
                            install_dir, _ = winreg.QueryValueEx(subkey, "InstalledDirectory")
                            if os.path.isdir(install_dir):
                                log_debug(f"Registry installed found: {subkey_name} -> {install_dir}", debug)
                                installations.append((subkey_name, install_dir))
                    except (FileNotFoundError, OSError):
                        pass
                    i += 1
                except OSError:
                    break
    except FileNotFoundError:
        log_debug("Registry key for installed versions not found", debug)
    except Exception as e:
        log_debug(f"Error reading registry installations: {e}", debug)

    return installations


def get_ue_installs_from_launcher(debug: bool = False) -> List[Tuple[str, str]]:
    """
    Get UE installations from Epic Games Launcher's LauncherInstalled.dat file.
    Returns list of (version_name, install_path) tuples.
    """
    installations = []

    # LauncherInstalled.dat location
    if sys.platform == 'win32':
        launcher_file = os.path.join(
            os.environ.get('PROGRAMDATA', 'C:\\ProgramData'),
            'Epic', 'UnrealEngineLauncher', 'LauncherInstalled.dat'
        )
    elif sys.platform == 'darwin':
        launcher_file = os.path.expanduser(
            '~/Library/Application Support/Epic/UnrealEngineLauncher/LauncherInstalled.dat'
        )
    else:
        return installations

    if not os.path.exists(launcher_file):
        log_debug(f"Launcher file not found: {launcher_file}", debug)
        return installations

    try:
        with open(launcher_file, 'r', encoding='utf-8') as f:
            data = json.load(f)

        for item in data.get('InstallationList', []):
            install_path = item.get('InstallLocation', '')
            app_name = item.get('AppName', '')

            # UE installations have AppName like "UE_5.4"
            if install_path and os.path.isdir(install_path):
                # Try to determine version from path or app name
                version = app_name if app_name else os.path.basename(install_path)
                log_debug(f"Launcher found: {version} -> {install_path}", debug)
                installations.append((version, install_path))

    except json.JSONDecodeError as e:
        log_debug(f"Error parsing launcher file: {e}", debug)
    except Exception as e:
        log_debug(f"Error reading launcher file: {e}", debug)

    return installations


def get_python_path_for_ue_install(install_path: str, debug: bool = False) -> Optional[str]:
    """
    Get the Python executable path for a given UE installation.
    """
    if sys.platform == 'win32':
        python_path = os.path.join(
            install_path, 'Engine', 'Binaries', 'ThirdParty', 'Python3', 'Win64', 'python.exe'
        )
    elif sys.platform == 'darwin':
        python_path = os.path.join(
            install_path, 'Engine', 'Binaries', 'ThirdParty', 'Python3', 'Mac', 'bin', 'python3'
        )
    else:
        python_path = os.path.join(
            install_path, 'Engine', 'Binaries', 'ThirdParty', 'Python3', 'Linux', 'bin', 'python3'
        )

    if os.path.exists(python_path):
        log_debug(f"Found Python at: {python_path}", debug)
        return python_path

    log_debug(f"Python not found at: {python_path}", debug)
    return None


def parse_ue_version(version_str: str) -> Tuple[int, int, int]:
    """
    Parse a UE version string into a tuple for sorting.
    Examples: "UE_5.4" -> (5, 4, 0), "5.3.2" -> (5, 3, 2)
    """
    # Extract version numbers
    match = re.search(r'(\d+)\.(\d+)(?:\.(\d+))?', version_str)
    if match:
        major = int(match.group(1))
        minor = int(match.group(2))
        patch = int(match.group(3)) if match.group(3) else 0
        return (major, minor, patch)
    return (0, 0, 0)


def find_ue_python(prefer_newest: bool = True, debug: bool = False) -> Optional[str]:
    """
    Find UE's bundled Python installation.

    Args:
        prefer_newest: If True, prefer the newest UE version. If False, prefer oldest.
        debug: Enable debug logging.

    Returns:
        Path to Python executable, or None if not found.
    """
    all_installations = []

    # Gather installations from all sources
    all_installations.extend(get_ue_installs_from_registry(debug))
    all_installations.extend(get_ue_installs_from_launcher(debug))

    # Remove duplicates (by install path)
    seen_paths = set()
    unique_installations = []
    for version, path in all_installations:
        normalized_path = os.path.normpath(path).lower()
        if normalized_path not in seen_paths:
            seen_paths.add(normalized_path)
            unique_installations.append((version, path))

    if not unique_installations:
        log_debug("No UE installations found via registry or launcher", debug)
        return None

    # Sort by version
    unique_installations.sort(
        key=lambda x: parse_ue_version(x[0]),
        reverse=prefer_newest
    )

    log_debug(f"Found {len(unique_installations)} UE installation(s)", debug)

    # Find first installation with valid Python
    for version, install_path in unique_installations:
        python_path = get_python_path_for_ue_install(install_path, debug)
        if python_path:
            log_debug(f"Selected: {version} ({install_path})", debug)
            return python_path

    log_debug("No UE installation has valid Python", debug)
    return None


def find_system_python(debug: bool = False) -> Optional[str]:
    """Find system Python installation."""
    import shutil

    for cmd in ['python3', 'python']:
        path = shutil.which(cmd)
        if path:
            log_debug(f"Found system Python: {path}", debug)
            return path

    log_debug("System Python not found", debug)
    return None


def find_python(prefer_ue: bool = True, debug: bool = False) -> Optional[str]:
    """
    Find Python installation, preferring UE's bundled Python by default.

    Args:
        prefer_ue: If True, try UE Python first. If False, try system Python first.
        debug: Enable debug logging.

    Returns:
        Path to Python executable, or None if not found.
    """
    if prefer_ue:
        python_path = find_ue_python(debug=debug)
        if python_path:
            return python_path
        return find_system_python(debug)
    else:
        python_path = find_system_python(debug)
        if python_path:
            return python_path
        return find_ue_python(debug=debug)


if __name__ == '__main__':
    # When run directly, print the found Python path
    import argparse

    parser = argparse.ArgumentParser(description='Find UE Python installation')
    parser.add_argument('--debug', action='store_true', help='Enable debug output')
    parser.add_argument('--system-first', action='store_true', help='Prefer system Python')
    args = parser.parse_args()

    python_path = find_python(prefer_ue=not args.system_first, debug=args.debug)

    if python_path:
        print(python_path)
        sys.exit(0)
    else:
        print("Python not found", file=sys.stderr)
        sys.exit(1)
