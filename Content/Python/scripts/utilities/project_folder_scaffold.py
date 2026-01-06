"""
Project folder scaffolding utilities for Unreal Engine.

Create standardized folder structures for common project patterns.
Supports custom folder lists and predefined templates.

Usage:
    from scripts.utilities.project_folder_scaffold import create_folders, create_from_template

    # Create custom folders
    create_folders(['/Game/MyFeature/Blueprints', '/Game/MyFeature/Data'])

    # Create standard feature folders
    create_feature_folders('/Game/Features/Combat', ['Blueprints', 'Data', 'UI'])

    # Use a predefined template
    create_from_template('/Game/Features/Inventory', 'feature_standard')

Functions return standardized dicts: {success: bool, data: {...}, error: str|None}
"""

import unreal
from typing import List, Dict, Any, Optional


def create_folder(path: str) -> Dict[str, Any]:
    """
    Create a single folder in the Content Browser.

    Args:
        path: Content path (e.g., '/Game/MyFolder')

    Returns:
        {success, data: {path, created, already_existed}, error}
    """
    if not path:
        return _make_error("Path cannot be empty")

    try:
        # Check if already exists
        already_exists = unreal.EditorAssetLibrary.does_directory_exist(path)

        if already_exists:
            return _make_success({
                "path": path,
                "created": False,
                "already_existed": True
            })

        # Create the directory
        result = unreal.EditorAssetLibrary.make_directory(path)

        if result:
            return _make_success({
                "path": path,
                "created": True,
                "already_existed": False
            })
        else:
            return _make_error(f"Failed to create directory: {path}")

    except Exception as e:
        return _make_error(f"Error creating folder: {e}")


def create_folders(paths: List[str]) -> Dict[str, Any]:
    """
    Create multiple folders.

    Args:
        paths: List of content paths to create

    Returns:
        {success, data: {created, already_existed, failed, details}, error}
    """
    if not paths:
        return _make_error("Paths list cannot be empty")

    created = []
    already_existed = []
    failed = []
    details = []

    for path in paths:
        result = create_folder(path)
        details.append(result)

        if result["success"]:
            if result["data"]["created"]:
                created.append(path)
            else:
                already_existed.append(path)
        else:
            failed.append({
                "path": path,
                "error": result["error"]
            })

    return _make_success({
        "created": created,
        "created_count": len(created),
        "already_existed": already_existed,
        "already_existed_count": len(already_existed),
        "failed": failed,
        "failed_count": len(failed),
        "total_requested": len(paths)
    })


def create_feature_folders(
    base_path: str,
    subfolders: List[str]
) -> Dict[str, Any]:
    """
    Create a feature folder with subfolders.

    Args:
        base_path: Base content path for the feature (e.g., '/Game/Features/Combat')
        subfolders: List of subfolder names to create (e.g., ['Blueprints', 'Data'])

    Returns:
        {success, data: {base_path, created_paths, ...}, error}
    """
    if not base_path:
        return _make_error("Base path cannot be empty")

    if not subfolders:
        return _make_error("Subfolders list cannot be empty")

    # Build full paths
    paths = [base_path]  # Create base folder too
    for subfolder in subfolders:
        # Clean up subfolder name
        clean_name = subfolder.strip().replace('\\', '/').strip('/')
        if clean_name:
            paths.append(f"{base_path}/{clean_name}")

    # Create all folders
    result = create_folders(paths)

    if result["success"]:
        result["data"]["base_path"] = base_path
        result["data"]["subfolders"] = subfolders

    return result


def create_from_template(
    base_path: str,
    template_name: str
) -> Dict[str, Any]:
    """
    Create folder structure from a predefined template.

    Args:
        base_path: Base content path
        template_name: Name of template to use (case-insensitive)

    Returns:
        {success, data: {base_path, template_used, created_paths, ...}, error}

    Available templates:
        - 'feature_standard': Blueprints, Data, UI, Materials, Audio
        - 'character': Blueprints, Meshes, Animations, Materials, Audio
        - 'level': Blueprints, Geometry, Lighting, Audio, Data
        - 'ui_module': Widgets, Styles, Icons, Data
        - 'plugin_content': Public, Private, Resources
        - 'gameplay_system': Blueprints, Data, Config, Tests
        - 'weapon': Blueprints, Meshes, Materials, Audio, VFX
        - 'environment': Props, Foliage, Materials, Decals, Lighting
    """
    if not base_path:
        return _make_error("Base path cannot be empty")

    templates = get_available_templates()
    template_key = template_name.lower().replace(' ', '_').replace('-', '_')

    if template_key not in templates:
        available = ', '.join(templates.keys())
        return _make_error(f"Unknown template: '{template_name}'. Available: {available}")

    subfolders = templates[template_key]
    result = create_feature_folders(base_path, subfolders)

    if result["success"]:
        result["data"]["template_used"] = template_name
        result["data"]["template_folders"] = subfolders

    return result


def get_available_templates() -> Dict[str, List[str]]:
    """
    Get all available folder templates.

    Returns:
        Dict mapping template names to their folder lists
    """
    return {
        'feature_standard': [
            'Blueprints',
            'Data',
            'UI',
            'Materials',
            'Audio'
        ],
        'character': [
            'Blueprints',
            'Meshes',
            'Animations',
            'Materials',
            'Audio'
        ],
        'level': [
            'Blueprints',
            'Geometry',
            'Lighting',
            'Audio',
            'Data'
        ],
        'ui_module': [
            'Widgets',
            'Styles',
            'Icons',
            'Data'
        ],
        'plugin_content': [
            'Public',
            'Private',
            'Resources'
        ],
        'gameplay_system': [
            'Blueprints',
            'Data',
            'Config',
            'Tests'
        ],
        'weapon': [
            'Blueprints',
            'Meshes',
            'Materials',
            'Audio',
            'VFX'
        ],
        'environment': [
            'Props',
            'Foliage',
            'Materials',
            'Decals',
            'Lighting'
        ],
        'collectible': [
            'Blueprints',
            'Meshes',
            'Materials',
            'Icons',
            'Audio'
        ],
        'ai': [
            'Blueprints',
            'BehaviorTrees',
            'Blackboards',
            'Data',
            'Debug'
        ]
    }


def folder_exists(path: str) -> bool:
    """
    Check if a folder exists in the Content Browser.

    Args:
        path: Content path to check

    Returns:
        True if folder exists, False otherwise
    """
    if not path:
        return False

    try:
        return unreal.EditorAssetLibrary.does_directory_exist(path)
    except:
        return False


def delete_folder(path: str, force: bool = False) -> Dict[str, Any]:
    """
    Delete a folder from the Content Browser.

    WARNING: This is destructive and may clear undo history.

    Args:
        path: Content path to delete
        force: Must be True to confirm deletion

    Returns:
        {success, data: {path, deleted}, error}
    """
    if not path:
        return _make_error("Path cannot be empty")

    if not force:
        return _make_error("Deletion requires force=True to confirm")

    try:
        if not folder_exists(path):
            return _make_error(f"Folder does not exist: {path}")

        result = unreal.EditorAssetLibrary.delete_directory(path)

        return _make_success({
            "path": path,
            "deleted": result
        })

    except Exception as e:
        return _make_error(f"Error deleting folder: {e}")


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
