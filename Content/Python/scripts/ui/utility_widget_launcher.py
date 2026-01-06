"""
Editor Utility Widget launcher for Unreal Engine.

Launch, manage, and discover Editor Utility Widgets (EUWs) programmatically.
Useful for opening custom tool UIs from scripts or automating editor workflows.

Usage:
    from scripts.ui.utility_widget_launcher import launch_widget, close_widget

    # Launch a widget
    result = launch_widget('/Game/UI/Tools/EUW_MyTool')

    # Launch with a specific tab ID
    result = launch_widget_with_id('/Game/UI/Tools/EUW_MyTool', 'MyToolTab')

    # Close a widget
    close_widget('MyToolTab')

Functions return standardized dicts: {success: bool, data: {...}, error: str|None}
"""

import unreal
from typing import List, Dict, Any, Optional


def launch_widget(widget_path: str) -> Dict[str, Any]:
    """
    Launch an Editor Utility Widget.

    Opens the widget in a new editor tab with an auto-generated tab ID.

    Args:
        widget_path: Path to the EditorUtilityWidgetBlueprint asset

    Returns:
        {success, data: {widget_path, tab_id, widget_instance}, error}

    Example:
        result = launch_widget('/Game/UI/Tools/EUW_AssetBrowser')
    """
    if not widget_path:
        return _make_error("Widget path cannot be empty")

    try:
        # Load the widget blueprint
        if not unreal.EditorAssetLibrary.does_asset_exist(widget_path):
            return _make_error(f"Widget does not exist: {widget_path}")

        widget_bp = unreal.EditorAssetLibrary.load_asset(widget_path)
        if widget_bp is None:
            return _make_error(f"Failed to load widget: {widget_path}")

        if not isinstance(widget_bp, unreal.EditorUtilityWidgetBlueprint):
            return _make_error(f"Asset is not an EditorUtilityWidgetBlueprint: {widget_path}")

        # Get the EditorUtilitySubsystem
        subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
        if subsystem is None:
            return _make_error("Could not get EditorUtilitySubsystem")

        # Spawn the widget and get the tab ID
        widget_instance, tab_id = subsystem.spawn_and_register_tab_and_get_id(widget_bp)

        if widget_instance is None:
            return _make_error(f"Failed to spawn widget: {widget_path}")

        return _make_success({
            "widget_path": widget_path,
            "tab_id": str(tab_id),
            "widget_class": str(widget_instance.get_class().get_name()),
            "launched": True
        })

    except Exception as e:
        return _make_error(f"Error launching widget: {e}")


def launch_widget_with_id(
    widget_path: str,
    tab_id: str
) -> Dict[str, Any]:
    """
    Launch an Editor Utility Widget with a specific tab ID.

    Allows you to specify the tab ID for later reference.

    Args:
        widget_path: Path to the EditorUtilityWidgetBlueprint
        tab_id: Unique identifier for the tab

    Returns:
        {success, data: {widget_path, tab_id, widget_instance}, error}

    Example:
        result = launch_widget_with_id('/Game/UI/EUW_Tool', 'MyCustomTool')
        # Later: close_widget('MyCustomTool')
    """
    if not widget_path:
        return _make_error("Widget path cannot be empty")

    if not tab_id:
        return _make_error("Tab ID cannot be empty")

    try:
        # Load the widget blueprint
        if not unreal.EditorAssetLibrary.does_asset_exist(widget_path):
            return _make_error(f"Widget does not exist: {widget_path}")

        widget_bp = unreal.EditorAssetLibrary.load_asset(widget_path)
        if widget_bp is None:
            return _make_error(f"Failed to load widget: {widget_path}")

        if not isinstance(widget_bp, unreal.EditorUtilityWidgetBlueprint):
            return _make_error(f"Asset is not an EditorUtilityWidgetBlueprint: {widget_path}")

        # Get the EditorUtilitySubsystem
        subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
        if subsystem is None:
            return _make_error("Could not get EditorUtilitySubsystem")

        # Spawn with specific ID
        widget_instance = subsystem.spawn_and_register_tab_with_id(widget_bp, tab_id)

        if widget_instance is None:
            return _make_error(f"Failed to spawn widget with ID '{tab_id}': {widget_path}")

        return _make_success({
            "widget_path": widget_path,
            "tab_id": tab_id,
            "widget_class": str(widget_instance.get_class().get_name()),
            "launched": True
        })

    except Exception as e:
        return _make_error(f"Error launching widget: {e}")


def close_widget(tab_id: str) -> Dict[str, Any]:
    """
    Close an Editor Utility Widget by its tab ID.

    Args:
        tab_id: The tab ID of the widget to close

    Returns:
        {success, data: {tab_id, closed}, error}
    """
    if not tab_id:
        return _make_error("Tab ID cannot be empty")

    try:
        subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
        if subsystem is None:
            return _make_error("Could not get EditorUtilitySubsystem")

        # Close the tab
        closed = subsystem.close_tab_by_id(tab_id)

        return _make_success({
            "tab_id": tab_id,
            "closed": closed
        })

    except Exception as e:
        return _make_error(f"Error closing widget: {e}")


def unregister_widget(tab_id: str) -> Dict[str, Any]:
    """
    Close and unregister an Editor Utility Widget.

    This removes the tab spawner entirely, not just closing the tab.

    Args:
        tab_id: The tab ID of the widget to unregister

    Returns:
        {success, data: {tab_id, unregistered}, error}
    """
    if not tab_id:
        return _make_error("Tab ID cannot be empty")

    try:
        subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
        if subsystem is None:
            return _make_error("Could not get EditorUtilitySubsystem")

        # Unregister the tab
        unregistered = subsystem.unregister_tab_by_id(tab_id)

        return _make_success({
            "tab_id": tab_id,
            "unregistered": unregistered
        })

    except Exception as e:
        return _make_error(f"Error unregistering widget: {e}")


def is_widget_open(tab_id: str) -> Dict[str, Any]:
    """
    Check if a widget tab exists.

    Args:
        tab_id: The tab ID to check

    Returns:
        {success, data: {tab_id, exists}, error}
    """
    if not tab_id:
        return _make_error("Tab ID cannot be empty")

    try:
        subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
        if subsystem is None:
            return _make_error("Could not get EditorUtilitySubsystem")

        exists = subsystem.does_tab_exist(tab_id)

        return _make_success({
            "tab_id": tab_id,
            "exists": exists
        })

    except Exception as e:
        return _make_error(f"Error checking widget: {e}")


def get_widget_instance(widget_path: str) -> Dict[str, Any]:
    """
    Get the active instance of an Editor Utility Widget.

    Returns the widget instance if it's currently open in an editor tab.

    Args:
        widget_path: Path to the EditorUtilityWidgetBlueprint

    Returns:
        {success, data: {widget_path, found, widget_class}, error}
    """
    if not widget_path:
        return _make_error("Widget path cannot be empty")

    try:
        # Load the widget blueprint
        if not unreal.EditorAssetLibrary.does_asset_exist(widget_path):
            return _make_error(f"Widget does not exist: {widget_path}")

        widget_bp = unreal.EditorAssetLibrary.load_asset(widget_path)
        if widget_bp is None:
            return _make_error(f"Failed to load widget: {widget_path}")

        if not isinstance(widget_bp, unreal.EditorUtilityWidgetBlueprint):
            return _make_error(f"Asset is not an EditorUtilityWidgetBlueprint: {widget_path}")

        # Get the subsystem
        subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
        if subsystem is None:
            return _make_error("Could not get EditorUtilitySubsystem")

        # Find the widget instance
        widget_instance = subsystem.find_utility_widget_from_blueprint(widget_bp)

        if widget_instance:
            return _make_success({
                "widget_path": widget_path,
                "found": True,
                "widget_class": str(widget_instance.get_class().get_name())
            })
        else:
            return _make_success({
                "widget_path": widget_path,
                "found": False,
                "widget_class": None
            })

    except Exception as e:
        return _make_error(f"Error getting widget instance: {e}")


def find_utility_widgets(
    folder_path: str,
    recursive: bool = True
) -> Dict[str, Any]:
    """
    Find all Editor Utility Widgets in a folder.

    Args:
        folder_path: Folder to search (e.g., '/Game/UI/Tools')
        recursive: Include subfolders

    Returns:
        {success, data: {widgets: [...], count: int}, error}
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
                "widgets": [],
                "count": 0,
                "folder_path": folder_path
            })

        widgets = []

        for asset_path in asset_paths:
            asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
            if asset_data and asset_data.is_valid():
                full_name = unreal.AssetRegistryHelpers.get_full_name(asset_data)
                if 'EditorUtilityWidgetBlueprint' in full_name:
                    widgets.append({
                        "path": asset_path,
                        "name": asset_path.split('/')[-1]
                    })

        return _make_success({
            "widgets": widgets,
            "count": len(widgets),
            "folder_path": folder_path
        })

    except Exception as e:
        return _make_error(f"Error finding widgets: {e}")


def launch_or_focus(widget_path: str, tab_id: str) -> Dict[str, Any]:
    """
    Launch a widget or focus it if already open.

    Useful for tools that should only have one instance.

    Args:
        widget_path: Path to the EditorUtilityWidgetBlueprint
        tab_id: Tab ID to use

    Returns:
        {success, data: {widget_path, tab_id, action}, error}
    """
    if not widget_path:
        return _make_error("Widget path cannot be empty")

    if not tab_id:
        return _make_error("Tab ID cannot be empty")

    try:
        subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
        if subsystem is None:
            return _make_error("Could not get EditorUtilitySubsystem")

        # Check if tab already exists
        if subsystem.does_tab_exist(tab_id):
            return _make_success({
                "widget_path": widget_path,
                "tab_id": tab_id,
                "action": "focused",
                "already_open": True
            })

        # Launch new widget
        result = launch_widget_with_id(widget_path, tab_id)

        if result["success"]:
            result["data"]["action"] = "launched"
            result["data"]["already_open"] = False

        return result

    except Exception as e:
        return _make_error(f"Error in launch_or_focus: {e}")


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
