"""
Asset iteration and discovery utilities for Unreal Engine.

Provides functions to find assets by type, path, name patterns, and metadata.
This is a foundational script - other scripts import and compose these functions.

Usage:
    from scripts.core.asset_iterator import find_assets_by_path, find_assets_by_type

    # Find all assets in a folder
    assets = find_assets_by_path('/Game/Blueprints')

    # Find all Blueprints recursively
    blueprints = find_assets_by_type('Blueprint', '/Game', recursive=True)

    # Find assets matching a name pattern
    enemies = find_assets_by_path('/Game/Characters', recursive=True, name_pattern='*Enemy*')

Functions return standardized dicts: {success: bool, data: {...}, error: str|None}
"""

import unreal
import fnmatch
from typing import List, Dict, Any, Optional, Callable


def find_assets_by_path(
    package_path: str,
    recursive: bool = False,
    include_only_on_disk: bool = False,
    name_pattern: Optional[str] = None
) -> Dict[str, Any]:
    """
    Find all assets in a specified folder path.

    Args:
        package_path: Content path (e.g., '/Game/Blueprints')
        recursive: Search subfolders if True
        include_only_on_disk: Only include saved assets (not memory-only)
        name_pattern: Optional glob pattern for asset names (e.g., '*Enemy*')

    Returns:
        {success, data: {assets: [...], count: int, path: str}, error}

    Each asset entry contains: name, path, class_name, package_path
    """
    if not package_path:
        return _make_error("Package path cannot be empty")

    try:
        # Get the asset registry
        registry = unreal.AssetRegistryHelpers.get_asset_registry()

        # Get assets by path
        asset_data_list = registry.get_assets_by_path(
            package_path,
            recursive,
            include_only_on_disk
        )

        if asset_data_list is None:
            return _make_success({
                "assets": [],
                "count": 0,
                "path": package_path,
                "recursive": recursive
            })

        # Convert to list of dicts
        assets = []
        for asset_data in asset_data_list:
            if not asset_data.is_valid():
                continue

            asset_info = _asset_data_to_dict(asset_data)

            # Apply name pattern filter if specified
            if name_pattern:
                if not fnmatch.fnmatch(asset_info["name"].lower(), name_pattern.lower()):
                    continue

            assets.append(asset_info)

        return _make_success({
            "assets": assets,
            "count": len(assets),
            "path": package_path,
            "recursive": recursive
        })

    except Exception as e:
        return _make_error(f"Failed to find assets by path: {e}")


def find_assets_by_type(
    class_name: str,
    search_path: Optional[str] = None,
    recursive: bool = True,
    include_subclasses: bool = True
) -> Dict[str, Any]:
    """
    Find assets of a specific class type.

    Args:
        class_name: Class name (e.g., 'Blueprint', 'StaticMesh', 'Material')
        search_path: Optional path to limit search scope (e.g., '/Game/Characters')
        recursive: Search subfolders when search_path is specified
        include_subclasses: Include derived classes

    Returns:
        {success, data: {assets: [...], count: int, class_name: str}, error}
    """
    if not class_name:
        return _make_error("Class name cannot be empty")

    try:
        # Resolve class path
        class_path = _resolve_class_path(class_name)

        # Get the asset registry
        registry = unreal.AssetRegistryHelpers.get_asset_registry()

        # Get assets by class
        asset_data_list = registry.get_assets_by_class(
            unreal.TopLevelAssetPath(class_path),
            include_subclasses
        )

        if asset_data_list is None:
            return _make_success({
                "assets": [],
                "count": 0,
                "class_name": class_name,
                "search_path": search_path
            })

        # Convert to list of dicts, optionally filtering by path
        assets = []
        for asset_data in asset_data_list:
            if not asset_data.is_valid():
                continue

            asset_info = _asset_data_to_dict(asset_data)

            # Filter by search path if specified
            if search_path:
                asset_package = str(asset_info["package_path"])
                if recursive:
                    if not asset_package.startswith(search_path):
                        continue
                else:
                    # Exact path match (without subfolders)
                    if asset_package != search_path:
                        continue

            assets.append(asset_info)

        return _make_success({
            "assets": assets,
            "count": len(assets),
            "class_name": class_name,
            "search_path": search_path
        })

    except Exception as e:
        return _make_error(f"Failed to find assets by type: {e}")


def list_assets_simple(
    directory_path: str,
    recursive: bool = True,
    include_folders: bool = False
) -> Dict[str, Any]:
    """
    Get a simple list of asset paths in a directory.

    This is a faster, simpler alternative when you only need paths.
    Uses EditorAssetLibrary.list_assets() internally.

    Args:
        directory_path: Content path (e.g., '/Game/Blueprints')
        recursive: Search subfolders if True
        include_folders: Include folder paths in results

    Returns:
        {success, data: {paths: [...], count: int}, error}
    """
    if not directory_path:
        return _make_error("Directory path cannot be empty")

    try:
        paths = unreal.EditorAssetLibrary.list_assets(
            directory_path,
            recursive,
            include_folders
        )

        return _make_success({
            "paths": list(paths) if paths else [],
            "count": len(paths) if paths else 0,
            "directory": directory_path,
            "recursive": recursive
        })

    except Exception as e:
        return _make_error(f"Failed to list assets: {e}")


def get_asset_info(asset_path: str) -> Dict[str, Any]:
    """
    Get detailed information about a single asset.

    Args:
        asset_path: Full asset path (e.g., '/Game/Blueprints/BP_Character')

    Returns:
        {success, data: {name, path, class_name, package_path, exists, is_valid}, error}
    """
    if not asset_path:
        return _make_error("Asset path cannot be empty")

    try:
        # Check if exists
        exists = unreal.EditorAssetLibrary.does_asset_exist(asset_path)

        if not exists:
            return _make_success({
                "name": asset_path.split('/')[-1],
                "path": asset_path,
                "exists": False,
                "is_valid": False
            })

        # Get asset data
        asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)

        if not asset_data or not asset_data.is_valid():
            return _make_success({
                "name": asset_path.split('/')[-1],
                "path": asset_path,
                "exists": True,
                "is_valid": False
            })

        info = _asset_data_to_dict(asset_data)
        info["exists"] = True
        info["is_valid"] = True

        return _make_success(info)

    except Exception as e:
        return _make_error(f"Failed to get asset info: {e}")


def get_blueprints_by_parent(
    parent_class_path: str,
    search_path: Optional[str] = None
) -> Dict[str, Any]:
    """
    Find all Blueprint assets inheriting from a specific parent class.

    Args:
        parent_class_path: Parent class path (e.g., '/Script/Engine.Actor',
                          or simple names like 'Actor', 'Pawn', 'Character')
        search_path: Optional path to limit search

    Returns:
        {success, data: {blueprints: [...], count: int, parent_class: str}, error}
    """
    if not parent_class_path:
        return _make_error("Parent class path cannot be empty")

    try:
        # Resolve simple class names to full paths
        resolved_path = _resolve_class_path(parent_class_path)

        # First get all Blueprints
        result = find_assets_by_type('Blueprint', search_path, recursive=True)

        if not result["success"]:
            return result

        # Filter by parent class
        matching_blueprints = []
        for asset_info in result["data"]["assets"]:
            try:
                # Load the asset to check parent class
                asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_info["path"])
                if asset_data and asset_data.is_valid():
                    # Get the Blueprint's parent class
                    bp_class = unreal.AssetRegistryHelpers.get_class(asset_data)
                    if bp_class:
                        parent = bp_class.get_super_class()
                        if parent:
                            parent_path = parent.get_path_name()
                            if resolved_path in parent_path or parent_class_path in parent_path:
                                matching_blueprints.append(asset_info)
            except:
                # Skip assets that can't be analyzed
                continue

        return _make_success({
            "blueprints": matching_blueprints,
            "count": len(matching_blueprints),
            "parent_class": parent_class_path
        })

    except Exception as e:
        return _make_error(f"Failed to find blueprints by parent: {e}")


def iterate_assets(
    assets: List[Dict[str, Any]],
    callback: Callable[[unreal.Object], Any],
    load_assets: bool = True
) -> Dict[str, Any]:
    """
    Iterate over assets and apply a callback function to each.

    Args:
        assets: List of asset info dicts from find_* functions
        callback: Function to call on each asset (receives loaded UObject)
        load_assets: Load assets into memory before callback

    Returns:
        {success, data: {results: [...], processed: int, failed: int}, error}
    """
    if not assets:
        return _make_success({
            "results": [],
            "processed": 0,
            "failed": 0
        })

    results = []
    failed = 0

    for asset_info in assets:
        try:
            asset_path = asset_info.get("path", "")

            if load_assets:
                # Load the asset
                asset = unreal.EditorAssetLibrary.load_asset(asset_path)
                if asset:
                    result = callback(asset)
                    results.append({
                        "path": asset_path,
                        "result": result
                    })
                else:
                    failed += 1
            else:
                # Just pass the asset info
                result = callback(asset_info)
                results.append({
                    "path": asset_path,
                    "result": result
                })

        except Exception as e:
            failed += 1
            results.append({
                "path": asset_info.get("path", "unknown"),
                "error": str(e)
            })

    return _make_success({
        "results": results,
        "processed": len(results) - failed,
        "failed": failed
    })


def filter_assets(
    assets: List[Dict[str, Any]],
    name_pattern: Optional[str] = None,
    class_filter: Optional[str] = None,
    path_prefix: Optional[str] = None
) -> Dict[str, Any]:
    """
    Filter a list of assets by various criteria.

    Args:
        assets: List of asset info dicts to filter
        name_pattern: Glob pattern for asset names (e.g., '*Enemy*')
        class_filter: Class name to match
        path_prefix: Path prefix to match

    Returns:
        {success, data: {assets: [...], count: int, original_count: int}, error}
    """
    if not assets:
        return _make_success({
            "assets": [],
            "count": 0,
            "original_count": 0
        })

    filtered = []

    for asset in assets:
        # Name pattern filter
        if name_pattern:
            name = asset.get("name", "")
            if not fnmatch.fnmatch(name.lower(), name_pattern.lower()):
                continue

        # Class filter
        if class_filter:
            class_name = asset.get("class_name", "")
            if class_filter.lower() not in class_name.lower():
                continue

        # Path prefix filter
        if path_prefix:
            path = asset.get("path", "")
            if not path.startswith(path_prefix):
                continue

        filtered.append(asset)

    return _make_success({
        "assets": filtered,
        "count": len(filtered),
        "original_count": len(assets)
    })


# ============================================================================
# Private Helper Functions
# ============================================================================

def _asset_data_to_dict(asset_data: unreal.AssetData) -> Dict[str, Any]:
    """Convert AssetData to a serializable dictionary."""
    try:
        # Get full name for class info
        full_name = unreal.AssetRegistryHelpers.get_full_name(asset_data)
        class_name = full_name.split(' ')[0] if full_name else "Unknown"

        return {
            "name": str(asset_data.asset_name),
            "path": str(asset_data.get_full_name()).split(' ')[-1] if asset_data.get_full_name() else "",
            "class_name": class_name,
            "package_path": str(asset_data.package_name),
            "package_name": str(asset_data.package_name)
        }
    except Exception as e:
        return {
            "name": "Unknown",
            "path": "",
            "class_name": "Unknown",
            "package_path": "",
            "error": str(e)
        }


def _resolve_class_path(class_name: str) -> str:
    """
    Resolve a simple class name to a full class path.

    Args:
        class_name: Simple name like 'Blueprint' or full path

    Returns:
        Full class path like '/Script/Engine.Blueprint'
    """
    # Already a full path
    if class_name.startswith('/'):
        return class_name

    # Common class mappings
    class_mappings = {
        # Engine classes
        'blueprint': '/Script/Engine.Blueprint',
        'actor': '/Script/Engine.Actor',
        'pawn': '/Script/Engine.Pawn',
        'character': '/Script/Engine.Character',
        'staticmesh': '/Script/Engine.StaticMesh',
        'skeletalmesh': '/Script/Engine.SkeletalMesh',
        'material': '/Script/Engine.Material',
        'materialinstance': '/Script/Engine.MaterialInstanceConstant',
        'texture': '/Script/Engine.Texture2D',
        'texture2d': '/Script/Engine.Texture2D',
        'soundcue': '/Script/Engine.SoundCue',
        'soundwave': '/Script/Engine.SoundWave',
        'particlesystem': '/Script/Engine.ParticleSystem',
        'niagarasystem': '/Script/Niagara.NiagaraSystem',
        'datatable': '/Script/Engine.DataTable',
        'dataasset': '/Script/Engine.DataAsset',
        'level': '/Script/Engine.World',
        'world': '/Script/Engine.World',
        # UMG classes
        'widgetblueprint': '/Script/UMGEditor.WidgetBlueprint',
        'userwidget': '/Script/UMG.UserWidget',
        # Animation classes
        'animblueprint': '/Script/Engine.AnimBlueprint',
        'animsequence': '/Script/Engine.AnimSequence',
        'animmontage': '/Script/Engine.AnimMontage',
        'blendspace': '/Script/Engine.BlendSpace',
        'skeleton': '/Script/Engine.Skeleton',
        # Editor classes
        'editorutilitywidget': '/Script/Blutility.EditorUtilityWidgetBlueprint',
        'editorutilityblueprint': '/Script/Blutility.EditorUtilityBlueprint',
    }

    # Lookup (case-insensitive)
    lower_name = class_name.lower().replace(' ', '')
    if lower_name in class_mappings:
        return class_mappings[lower_name]

    # Fallback: assume it's in Engine module
    return f'/Script/Engine.{class_name}'


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
