"""
Asset validation utilities for Unreal Engine.

Validate assets against naming conventions, compilation status, and custom rules.
Useful for enforcing project standards and finding issues.

Usage:
    from scripts.utilities.asset_validator import validate_blueprint, check_naming

    # Validate a single Blueprint
    result = validate_blueprint('/Game/BP_Character')

    # Check naming conventions in a folder
    issues = check_naming('/Game/Blueprints', recursive=True)

Functions return standardized dicts: {success: bool, data: {...}, error: str|None}
"""

import unreal
import re
from typing import List, Dict, Any, Optional, Callable


def validate_blueprint(
    blueprint_path: str,
    compile_check: bool = True,
    naming_check: bool = True
) -> Dict[str, Any]:
    """
    Validate a single Blueprint.

    Args:
        blueprint_path: Path to the Blueprint
        compile_check: Check if Blueprint compiles without errors
        naming_check: Check naming conventions

    Returns:
        {success, data: {is_valid, issues, warnings, status}, error}
    """
    if not blueprint_path:
        return _make_error("Blueprint path cannot be empty")

    try:
        if not unreal.EditorAssetLibrary.does_asset_exist(blueprint_path):
            return _make_error(f"Blueprint does not exist: {blueprint_path}")

        blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
        if blueprint is None:
            return _make_error(f"Failed to load Blueprint: {blueprint_path}")

        issues = []
        warnings = []

        # Get asset name for naming check
        asset_name = blueprint_path.split('/')[-1]

        # Naming convention check
        if naming_check:
            if not asset_name.startswith('BP_'):
                # Check for other valid prefixes
                if not any(asset_name.startswith(p) for p in ['WBP_', 'GM_', 'GS_', 'PC_', 'AC_']):
                    warnings.append(f"Blueprint '{asset_name}' does not follow naming convention (expected BP_ prefix)")

        # Compilation check
        if compile_check and isinstance(blueprint, unreal.Blueprint):
            try:
                # Try to compile
                unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

                # Check compilation status
                # Note: In UE5, we can't easily get compilation errors via Python
                # This is a basic check
            except Exception as e:
                issues.append(f"Compilation error: {e}")

        is_valid = len(issues) == 0

        return _make_success({
            "is_valid": is_valid,
            "blueprint_path": blueprint_path,
            "asset_name": asset_name,
            "issues": issues,
            "issue_count": len(issues),
            "warnings": warnings,
            "warning_count": len(warnings),
            "compile_checked": compile_check,
            "naming_checked": naming_check
        })

    except Exception as e:
        return _make_error(f"Error validating Blueprint: {e}")


def validate_blueprints_in_folder(
    folder_path: str,
    recursive: bool = True,
    compile_check: bool = True,
    naming_check: bool = True
) -> Dict[str, Any]:
    """
    Validate all Blueprints in a folder.

    Args:
        folder_path: Content folder path
        recursive: Include subfolders
        compile_check: Check compilation
        naming_check: Check naming

    Returns:
        {success, data: {total, valid, invalid, issues_by_asset}, error}
    """
    if not folder_path:
        return _make_error("Folder path cannot be empty")

    try:
        # Get all assets in the folder
        asset_paths = unreal.EditorAssetLibrary.list_assets(
            folder_path,
            recursive,
            include_folder=False
        )

        if not asset_paths:
            return _make_success({
                "total": 0,
                "valid": 0,
                "invalid": 0,
                "issues_by_asset": {},
                "folder_path": folder_path
            })

        valid_count = 0
        invalid_count = 0
        issues_by_asset = {}

        for asset_path in asset_paths:
            # Check if it's a Blueprint
            asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
            if asset_data and asset_data.is_valid():
                class_name = unreal.AssetRegistryHelpers.get_full_name(asset_data).split(' ')[0]

                if 'Blueprint' in class_name:
                    result = validate_blueprint(asset_path, compile_check, naming_check)

                    if result["success"]:
                        data = result["data"]
                        if data["is_valid"]:
                            valid_count += 1
                        else:
                            invalid_count += 1
                            issues_by_asset[asset_path] = {
                                "issues": data["issues"],
                                "warnings": data["warnings"]
                            }
                    else:
                        invalid_count += 1
                        issues_by_asset[asset_path] = {
                            "issues": [result["error"]],
                            "warnings": []
                        }

        return _make_success({
            "total": valid_count + invalid_count,
            "valid": valid_count,
            "invalid": invalid_count,
            "issues_by_asset": issues_by_asset,
            "folder_path": folder_path,
            "recursive": recursive
        })

    except Exception as e:
        return _make_error(f"Error validating Blueprints: {e}")


def check_naming(
    folder_path: str,
    recursive: bool = True,
    conventions: Optional[Dict[str, str]] = None
) -> Dict[str, Any]:
    """
    Check assets against naming conventions.

    Args:
        folder_path: Folder to check
        recursive: Include subfolders
        conventions: Custom conventions (or use defaults)
            Format: {'AssetType': 'Prefix_'}

    Returns:
        {success, data: {total, valid, invalid, issues}, error}
    """
    if not folder_path:
        return _make_error("Folder path cannot be empty")

    try:
        # Use default conventions if not provided
        if conventions is None:
            conventions = get_default_naming_conventions()

        # Get all assets
        asset_paths = unreal.EditorAssetLibrary.list_assets(
            folder_path,
            recursive,
            include_folder=False
        )

        if not asset_paths:
            return _make_success({
                "total": 0,
                "valid": 0,
                "invalid": 0,
                "issues": [],
                "folder_path": folder_path
            })

        valid_count = 0
        invalid_count = 0
        issues = []

        for asset_path in asset_paths:
            asset_name = asset_path.split('/')[-1]
            asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)

            if not asset_data or not asset_data.is_valid():
                continue

            # Get asset class
            full_name = unreal.AssetRegistryHelpers.get_full_name(asset_data)
            class_name = full_name.split(' ')[0] if full_name else ""

            # Find matching convention
            expected_prefix = None
            for asset_type, prefix in conventions.items():
                if asset_type.lower() in class_name.lower():
                    expected_prefix = prefix
                    break

            if expected_prefix:
                if asset_name.startswith(expected_prefix):
                    valid_count += 1
                else:
                    invalid_count += 1
                    issues.append({
                        "path": asset_path,
                        "name": asset_name,
                        "class": class_name,
                        "expected_prefix": expected_prefix,
                        "message": f"Expected prefix '{expected_prefix}' but got '{asset_name[:len(expected_prefix)]}'"
                    })
            else:
                valid_count += 1  # No convention for this type

        return _make_success({
            "total": valid_count + invalid_count,
            "valid": valid_count,
            "invalid": invalid_count,
            "issues": issues,
            "folder_path": folder_path,
            "conventions_used": conventions
        })

    except Exception as e:
        return _make_error(f"Error checking naming conventions: {e}")


def get_default_naming_conventions() -> Dict[str, str]:
    """
    Get default UE naming conventions.

    Returns:
        Dict mapping asset types to expected prefixes
    """
    return {
        'Blueprint': 'BP_',
        'WidgetBlueprint': 'WBP_',
        'Material': 'M_',
        'MaterialInstance': 'MI_',
        'StaticMesh': 'SM_',
        'SkeletalMesh': 'SK_',
        'Texture': 'T_',
        'ParticleSystem': 'PS_',
        'NiagaraSystem': 'NS_',
        'SoundCue': 'SC_',
        'SoundWave': 'SW_',
        'AnimBlueprint': 'ABP_',
        'AnimMontage': 'AM_',
        'AnimSequence': 'AS_',
        'BlendSpace': 'BS_',
        'DataTable': 'DT_',
        'DataAsset': 'DA_',
        'Enum': 'E_',
        'Struct': 'F_',
        'RenderTarget': 'RT_',
        'CurveFloat': 'Curve_',
        'CurveVector': 'Curve_',
    }


def find_assets_without_prefix(
    folder_path: str,
    prefix: str,
    asset_type: str = 'Blueprint',
    recursive: bool = True
) -> Dict[str, Any]:
    """
    Find assets of a specific type that don't have the expected prefix.

    Args:
        folder_path: Folder to search
        prefix: Expected prefix (e.g., 'BP_')
        asset_type: Asset type to check (e.g., 'Blueprint')
        recursive: Include subfolders

    Returns:
        {success, data: {assets_without_prefix: [...], count: int}, error}
    """
    if not folder_path:
        return _make_error("Folder path cannot be empty")

    if not prefix:
        return _make_error("Prefix cannot be empty")

    try:
        asset_paths = unreal.EditorAssetLibrary.list_assets(
            folder_path,
            recursive,
            include_folder=False
        )

        if not asset_paths:
            return _make_success({
                "assets_without_prefix": [],
                "count": 0,
                "folder_path": folder_path
            })

        without_prefix = []

        for asset_path in asset_paths:
            asset_name = asset_path.split('/')[-1]
            asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)

            if not asset_data or not asset_data.is_valid():
                continue

            full_name = unreal.AssetRegistryHelpers.get_full_name(asset_data)
            class_name = full_name.split(' ')[0] if full_name else ""

            if asset_type.lower() in class_name.lower():
                if not asset_name.startswith(prefix):
                    without_prefix.append({
                        "path": asset_path,
                        "name": asset_name,
                        "class": class_name
                    })

        return _make_success({
            "assets_without_prefix": without_prefix,
            "count": len(without_prefix),
            "folder_path": folder_path,
            "expected_prefix": prefix,
            "asset_type": asset_type
        })

    except Exception as e:
        return _make_error(f"Error finding assets: {e}")


def check_for_empty_blueprints(
    folder_path: str,
    recursive: bool = True
) -> Dict[str, Any]:
    """
    Find Blueprints that appear to be empty (no variables, no functions).

    Args:
        folder_path: Folder to search
        recursive: Include subfolders

    Returns:
        {success, data: {empty_blueprints: [...], count: int}, error}
    """
    if not folder_path:
        return _make_error("Folder path cannot be empty")

    try:
        asset_paths = unreal.EditorAssetLibrary.list_assets(
            folder_path,
            recursive,
            include_folder=False
        )

        if not asset_paths:
            return _make_success({
                "empty_blueprints": [],
                "count": 0,
                "folder_path": folder_path
            })

        empty_blueprints = []

        for asset_path in asset_paths:
            asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)

            if not asset_data or not asset_data.is_valid():
                continue

            full_name = unreal.AssetRegistryHelpers.get_full_name(asset_data)
            if 'Blueprint' not in full_name:
                continue

            # Load and check Blueprint content
            blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
            if blueprint and isinstance(blueprint, unreal.Blueprint):
                # Check if Blueprint appears empty
                has_content = False

                # Check for function graphs
                if hasattr(blueprint, 'function_graphs'):
                    if len(blueprint.function_graphs) > 0:
                        has_content = True

                # If no functions, might be empty
                if not has_content:
                    empty_blueprints.append({
                        "path": asset_path,
                        "name": asset_path.split('/')[-1]
                    })

        return _make_success({
            "empty_blueprints": empty_blueprints,
            "count": len(empty_blueprints),
            "folder_path": folder_path
        })

    except Exception as e:
        return _make_error(f"Error checking for empty Blueprints: {e}")


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
