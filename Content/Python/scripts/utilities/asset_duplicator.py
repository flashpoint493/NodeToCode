"""
Asset duplication utilities for Unreal Engine.

Clone assets with optional customization and batch operations.
Supports single asset duplication, batch duplication, and directory copying.

Usage:
    from scripts.utilities.asset_duplicator import duplicate_asset, batch_duplicate

    # Simple duplication
    result = duplicate_asset('/Game/BP_Enemy', '/Game/Enemies/BP_BossEnemy')

    # Batch duplicate with naming pattern
    batch_duplicate('/Game/BP_Item', '/Game/Items', count=5, name_pattern='{name}_{n}')

Functions return standardized dicts: {success: bool, data: {...}, error: str|None}
"""

import unreal
from typing import List, Dict, Any, Optional


def duplicate_asset(
    source_path: str,
    destination_path: str,
    save_after: bool = True
) -> Dict[str, Any]:
    """
    Duplicate an asset to a new location.

    Args:
        source_path: Source asset path (e.g., '/Game/BP_Enemy')
        destination_path: Destination path including new name (e.g., '/Game/Enemies/BP_BossEnemy')
        save_after: Save the duplicated asset after creation

    Returns:
        {success, data: {source_path, destination_path, asset_class}, error}
    """
    if not source_path:
        return _make_error("Source path cannot be empty")

    if not destination_path:
        return _make_error("Destination path cannot be empty")

    try:
        # Check source exists
        if not unreal.EditorAssetLibrary.does_asset_exist(source_path):
            return _make_error(f"Source asset does not exist: {source_path}")

        # Check destination doesn't exist
        if unreal.EditorAssetLibrary.does_asset_exist(destination_path):
            return _make_error(f"Destination already exists: {destination_path}")

        # Ensure destination directory exists
        dest_dir = '/'.join(destination_path.split('/')[:-1])
        if dest_dir and not unreal.EditorAssetLibrary.does_directory_exist(dest_dir):
            unreal.EditorAssetLibrary.make_directory(dest_dir)

        # Duplicate the asset
        duplicated = unreal.EditorAssetLibrary.duplicate_asset(source_path, destination_path)

        if duplicated is None:
            return _make_error(f"Failed to duplicate asset from {source_path} to {destination_path}")

        # Save if requested
        if save_after:
            unreal.EditorAssetLibrary.save_asset(destination_path, only_if_is_dirty=False)

        return _make_success({
            "source_path": source_path,
            "destination_path": destination_path,
            "asset_class": duplicated.get_class().get_name() if duplicated else "Unknown",
            "saved": save_after
        })

    except Exception as e:
        return _make_error(f"Error duplicating asset: {e}")


def duplicate_loaded_asset(
    source_asset: unreal.Object,
    destination_path: str,
    save_after: bool = True
) -> Dict[str, Any]:
    """
    Duplicate an already-loaded asset.

    Args:
        source_asset: Loaded UObject to duplicate
        destination_path: Destination path for the duplicate

    Returns:
        {success, data: {destination_path, asset_class}, error}
    """
    if source_asset is None:
        return _make_error("Source asset cannot be None")

    if not destination_path:
        return _make_error("Destination path cannot be empty")

    try:
        # Check destination doesn't exist
        if unreal.EditorAssetLibrary.does_asset_exist(destination_path):
            return _make_error(f"Destination already exists: {destination_path}")

        # Ensure destination directory exists
        dest_dir = '/'.join(destination_path.split('/')[:-1])
        if dest_dir and not unreal.EditorAssetLibrary.does_directory_exist(dest_dir):
            unreal.EditorAssetLibrary.make_directory(dest_dir)

        # Duplicate the loaded asset
        duplicated = unreal.EditorAssetLibrary.duplicate_loaded_asset(source_asset, destination_path)

        if duplicated is None:
            return _make_error(f"Failed to duplicate loaded asset to {destination_path}")

        # Save if requested
        if save_after:
            unreal.EditorAssetLibrary.save_asset(destination_path, only_if_is_dirty=False)

        return _make_success({
            "destination_path": destination_path,
            "asset_class": duplicated.get_class().get_name() if duplicated else "Unknown",
            "saved": save_after
        })

    except Exception as e:
        return _make_error(f"Error duplicating loaded asset: {e}")


def batch_duplicate(
    source_path: str,
    destination_folder: str,
    count: int,
    name_pattern: str = '{name}_{n}',
    start_index: int = 1,
    save_after: bool = True
) -> Dict[str, Any]:
    """
    Create multiple duplicates of an asset.

    Args:
        source_path: Source asset path
        destination_folder: Folder for duplicates
        count: Number of duplicates to create
        name_pattern: Naming pattern using placeholders:
            - {name}: Original asset name
            - {n}: Index number (uses start_index)
            - {n:02d}: Index with zero-padding (e.g., 01, 02)
        start_index: Starting index for {n}
        save_after: Save each duplicate after creation

    Returns:
        {success, data: {created_assets, failed_assets, count}, error}

    Example:
        batch_duplicate('/Game/BP_Item', '/Game/Items', 5, 'BP_Item_{n:02d}')
        # Creates: BP_Item_01, BP_Item_02, BP_Item_03, BP_Item_04, BP_Item_05
    """
    if not source_path:
        return _make_error("Source path cannot be empty")

    if not destination_folder:
        return _make_error("Destination folder cannot be empty")

    if count < 1:
        return _make_error("Count must be at least 1")

    try:
        # Check source exists
        if not unreal.EditorAssetLibrary.does_asset_exist(source_path):
            return _make_error(f"Source asset does not exist: {source_path}")

        # Get source asset name
        source_name = source_path.split('/')[-1]

        # Ensure destination folder exists
        if not unreal.EditorAssetLibrary.does_directory_exist(destination_folder):
            unreal.EditorAssetLibrary.make_directory(destination_folder)

        created = []
        failed = []

        for i in range(count):
            index = start_index + i

            # Generate name from pattern
            try:
                # Handle format specifiers like {n:02d}
                new_name = name_pattern.format(name=source_name, n=index)
            except:
                new_name = name_pattern.replace('{name}', source_name).replace('{n}', str(index))

            dest_path = f"{destination_folder}/{new_name}"

            result = duplicate_asset(source_path, dest_path, save_after)

            if result["success"]:
                created.append(dest_path)
            else:
                failed.append({
                    "path": dest_path,
                    "error": result["error"]
                })

        return _make_success({
            "created_assets": created,
            "created_count": len(created),
            "failed_assets": failed,
            "failed_count": len(failed),
            "source_path": source_path,
            "destination_folder": destination_folder
        })

    except Exception as e:
        return _make_error(f"Error in batch duplication: {e}")


def duplicate_to_folder(
    source_path: str,
    destination_folder: str,
    new_name: Optional[str] = None,
    save_after: bool = True
) -> Dict[str, Any]:
    """
    Duplicate an asset to a folder, optionally with a new name.

    Args:
        source_path: Source asset path
        destination_folder: Destination folder
        new_name: New asset name (uses source name if None)
        save_after: Save after duplication

    Returns:
        {success, data: {source_path, destination_path}, error}
    """
    if not source_path:
        return _make_error("Source path cannot be empty")

    if not destination_folder:
        return _make_error("Destination folder cannot be empty")

    # Use source name if no new name provided
    if not new_name:
        new_name = source_path.split('/')[-1]

    destination_path = f"{destination_folder}/{new_name}"

    return duplicate_asset(source_path, destination_path, save_after)


def duplicate_with_suffix(
    source_path: str,
    suffix: str,
    destination_folder: Optional[str] = None,
    save_after: bool = True
) -> Dict[str, Any]:
    """
    Duplicate an asset with a suffix added to its name.

    Args:
        source_path: Source asset path
        suffix: Suffix to add (e.g., '_Copy', '_V2')
        destination_folder: Destination folder (uses source folder if None)
        save_after: Save after duplication

    Returns:
        {success, data: {source_path, destination_path}, error}

    Example:
        duplicate_with_suffix('/Game/BP_Character', '_Boss')
        # Creates: /Game/BP_Character_Boss
    """
    if not source_path:
        return _make_error("Source path cannot be empty")

    if not suffix:
        return _make_error("Suffix cannot be empty")

    # Get source folder and name
    parts = source_path.rsplit('/', 1)
    source_folder = parts[0] if len(parts) > 1 else '/Game'
    source_name = parts[-1]

    # Use source folder if no destination specified
    if not destination_folder:
        destination_folder = source_folder

    # Create new name with suffix
    new_name = f"{source_name}{suffix}"
    destination_path = f"{destination_folder}/{new_name}"

    return duplicate_asset(source_path, destination_path, save_after)


def generate_unique_path(base_path: str, max_attempts: int = 100) -> Dict[str, Any]:
    """
    Generate a unique asset path by appending numbers.

    Args:
        base_path: Base asset path
        max_attempts: Maximum number of attempts to find unique name

    Returns:
        {success, data: {original_path, unique_path, suffix_used}, error}

    Example:
        generate_unique_path('/Game/BP_Enemy')
        # If BP_Enemy exists, returns /Game/BP_Enemy_1
        # If BP_Enemy_1 exists, returns /Game/BP_Enemy_2, etc.
    """
    if not base_path:
        return _make_error("Base path cannot be empty")

    # If base path doesn't exist, it's already unique
    if not unreal.EditorAssetLibrary.does_asset_exist(base_path):
        return _make_success({
            "original_path": base_path,
            "unique_path": base_path,
            "suffix_used": None
        })

    # Try adding numbers
    for i in range(1, max_attempts + 1):
        test_path = f"{base_path}_{i}"
        if not unreal.EditorAssetLibrary.does_asset_exist(test_path):
            return _make_success({
                "original_path": base_path,
                "unique_path": test_path,
                "suffix_used": f"_{i}"
            })

    return _make_error(f"Could not find unique path after {max_attempts} attempts")


def duplicate_multiple(
    source_paths: List[str],
    destination_folder: str,
    save_after: bool = True
) -> Dict[str, Any]:
    """
    Duplicate multiple assets to a folder.

    Args:
        source_paths: List of source asset paths
        destination_folder: Destination folder for all duplicates
        save_after: Save each duplicate

    Returns:
        {success, data: {created, failed, ...}, error}
    """
    if not source_paths:
        return _make_error("Source paths list cannot be empty")

    if not destination_folder:
        return _make_error("Destination folder cannot be empty")

    # Ensure destination folder exists
    if not unreal.EditorAssetLibrary.does_directory_exist(destination_folder):
        unreal.EditorAssetLibrary.make_directory(destination_folder)

    created = []
    failed = []

    for source_path in source_paths:
        result = duplicate_to_folder(source_path, destination_folder, save_after=save_after)

        if result["success"]:
            created.append(result["data"]["destination_path"])
        else:
            failed.append({
                "source_path": source_path,
                "error": result["error"]
            })

    return _make_success({
        "created": created,
        "created_count": len(created),
        "failed": failed,
        "failed_count": len(failed),
        "destination_folder": destination_folder
    })


# ============================================================================
# Private Helper Functions
# ============================================================================

def _make_success(data: Any) -> Dict[str, Any]:
    """Create a success result."""
    return {
        "success": True,
        "data": data,
        "error": None
    }


def _make_error(message: str) -> Dict[str, Any]:
    """Create an error result."""
    return {
        "success": False,
        "data": None,
        "error": message
    }
