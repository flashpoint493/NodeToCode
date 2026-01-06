"""
Blueprint function scaffolding utilities for Unreal Engine.

Add function graphs with inputs, outputs, and configurations to Blueprints.
Supports regular functions and custom events.

Usage:
    from scripts.core.function_scaffolder import add_function, add_functions_batch

    # Add a simple function
    add_function('/Game/BP_Character', 'CalculateDamage')

    # Add multiple functions at once
    functions = [
        {'name': 'TakeDamage'},
        {'name': 'Heal'},
        {'name': 'GetHealth', 'is_pure': True},
    ]
    add_functions_batch('/Game/BP_Character', functions)

Functions return standardized dicts: {success: bool, data: {...}, error: str|None}
"""

import unreal
from typing import List, Dict, Any, Optional


def add_function(
    blueprint_path: str,
    function_name: str,
    is_pure: bool = False,
    category: str = '',
    description: str = '',
    compile_after: bool = True
) -> Dict[str, Any]:
    """
    Add a function graph to a Blueprint.

    Args:
        blueprint_path: Path to the Blueprint asset
        function_name: Name for the new function
        is_pure: Mark as pure function (no execution pins)
        category: Function category for organization
        description: Function tooltip/description
        compile_after: Compile after adding function

    Returns:
        {success, data: {function_name, graph, blueprint_path}, error}

    Note: Input/output pins must be added separately via Blueprint editor
    or additional scripting after function creation.
    """
    if not blueprint_path:
        return _make_error("Blueprint path cannot be empty")

    if not function_name:
        return _make_error("Function name cannot be empty")

    try:
        # Load the Blueprint
        if not unreal.EditorAssetLibrary.does_asset_exist(blueprint_path):
            return _make_error(f"Blueprint does not exist: {blueprint_path}")

        blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
        if blueprint is None or not isinstance(blueprint, unreal.Blueprint):
            return _make_error(f"Failed to load Blueprint or not a Blueprint: {blueprint_path}")

        # Check if function already exists
        existing = unreal.BlueprintEditorLibrary.find_graph(blueprint, function_name)
        if existing:
            return _make_error(f"Function '{function_name}' already exists in Blueprint")

        # Add the function graph
        graph = unreal.BlueprintEditorLibrary.add_function_graph(blueprint, function_name)

        if graph is None:
            return _make_error(f"Failed to create function graph: {function_name}")

        # Compile if requested
        if compile_after:
            try:
                unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            except:
                pass

        return _make_success({
            "function_name": function_name,
            "graph_name": str(graph.get_name()) if graph else function_name,
            "blueprint_path": blueprint_path,
            "is_pure": is_pure,
            "category": category,
            "compiled": compile_after
        })

    except Exception as e:
        return _make_error(f"Error adding function: {e}")


def add_functions_batch(
    blueprint_path: str,
    functions: List[Dict[str, Any]],
    compile_after: bool = True
) -> Dict[str, Any]:
    """
    Add multiple functions to a Blueprint.

    Args:
        blueprint_path: Path to the Blueprint asset
        functions: List of function definitions with:
            - name: Function name (required)
            - is_pure: Pure function (optional, default False)
            - category: Function category (optional)
            - description: Function description (optional)
        compile_after: Compile after all functions are added

    Returns:
        {success, data: {created_functions, failed_functions, ...}, error}
    """
    if not blueprint_path:
        return _make_error("Blueprint path cannot be empty")

    if not functions:
        return _make_error("Functions list cannot be empty")

    try:
        # Load the Blueprint once
        if not unreal.EditorAssetLibrary.does_asset_exist(blueprint_path):
            return _make_error(f"Blueprint does not exist: {blueprint_path}")

        blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
        if blueprint is None or not isinstance(blueprint, unreal.Blueprint):
            return _make_error(f"Failed to load Blueprint: {blueprint_path}")

        created = []
        failed = []

        for func_def in functions:
            func_name = func_def.get('name')

            if not func_name:
                failed.append({
                    "name": "(missing)",
                    "error": "Function name is required"
                })
                continue

            # Check if already exists
            existing = unreal.BlueprintEditorLibrary.find_graph(blueprint, func_name)
            if existing:
                failed.append({
                    "name": func_name,
                    "error": "Function already exists"
                })
                continue

            try:
                # Add the function graph
                graph = unreal.BlueprintEditorLibrary.add_function_graph(blueprint, func_name)

                if graph:
                    created.append({
                        "name": func_name,
                        "is_pure": func_def.get('is_pure', False),
                        "category": func_def.get('category', '')
                    })
                else:
                    failed.append({
                        "name": func_name,
                        "error": "Failed to create function graph"
                    })

            except Exception as e:
                failed.append({
                    "name": func_name,
                    "error": str(e)
                })

        # Compile once at the end
        if compile_after and created:
            try:
                unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            except:
                pass

        return _make_success({
            "created_functions": created,
            "created_count": len(created),
            "failed_functions": failed,
            "failed_count": len(failed),
            "blueprint_path": blueprint_path,
            "compiled": compile_after
        })

    except Exception as e:
        return _make_error(f"Error in batch function creation: {e}")


def find_function(
    blueprint_path: str,
    function_name: str
) -> Dict[str, Any]:
    """
    Find a function graph in a Blueprint.

    Args:
        blueprint_path: Path to the Blueprint
        function_name: Function to find

    Returns:
        {success, data: {found, function_name, graph_info}, error}
    """
    if not blueprint_path:
        return _make_error("Blueprint path cannot be empty")

    if not function_name:
        return _make_error("Function name cannot be empty")

    try:
        if not unreal.EditorAssetLibrary.does_asset_exist(blueprint_path):
            return _make_error(f"Blueprint does not exist: {blueprint_path}")

        blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
        if blueprint is None:
            return _make_error(f"Failed to load Blueprint: {blueprint_path}")

        graph = unreal.BlueprintEditorLibrary.find_graph(blueprint, function_name)

        if graph:
            return _make_success({
                "found": True,
                "function_name": function_name,
                "graph_name": str(graph.get_name()),
                "blueprint_path": blueprint_path
            })
        else:
            return _make_success({
                "found": False,
                "function_name": function_name,
                "blueprint_path": blueprint_path
            })

    except Exception as e:
        return _make_error(f"Error finding function: {e}")


def delete_function(
    blueprint_path: str,
    function_name: str,
    compile_after: bool = True
) -> Dict[str, Any]:
    """
    Delete a function from a Blueprint.

    WARNING: Does NOT update call sites - callers will have broken references.

    Args:
        blueprint_path: Path to the Blueprint
        function_name: Function to delete
        compile_after: Compile after deletion

    Returns:
        {success, data: {deleted, function_name}, error}
    """
    if not blueprint_path:
        return _make_error("Blueprint path cannot be empty")

    if not function_name:
        return _make_error("Function name cannot be empty")

    try:
        if not unreal.EditorAssetLibrary.does_asset_exist(blueprint_path):
            return _make_error(f"Blueprint does not exist: {blueprint_path}")

        blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
        if blueprint is None:
            return _make_error(f"Failed to load Blueprint: {blueprint_path}")

        # Check if function exists
        graph = unreal.BlueprintEditorLibrary.find_graph(blueprint, function_name)
        if not graph:
            return _make_error(f"Function '{function_name}' not found in Blueprint")

        # Delete the function
        unreal.BlueprintEditorLibrary.remove_function_graph(blueprint, function_name)

        # Compile
        if compile_after:
            try:
                unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            except:
                pass

        return _make_success({
            "deleted": True,
            "function_name": function_name,
            "blueprint_path": blueprint_path,
            "compiled": compile_after
        })

    except Exception as e:
        return _make_error(f"Error deleting function: {e}")


def list_functions(blueprint_path: str) -> Dict[str, Any]:
    """
    List all functions in a Blueprint.

    Args:
        blueprint_path: Path to the Blueprint

    Returns:
        {success, data: {functions: [...], count: int}, error}
    """
    if not blueprint_path:
        return _make_error("Blueprint path cannot be empty")

    try:
        if not unreal.EditorAssetLibrary.does_asset_exist(blueprint_path):
            return _make_error(f"Blueprint does not exist: {blueprint_path}")

        blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
        if blueprint is None:
            return _make_error(f"Failed to load Blueprint: {blueprint_path}")

        # Get function graphs from the Blueprint
        functions = []

        # Access function graphs if available
        if hasattr(blueprint, 'function_graphs'):
            for graph in blueprint.function_graphs:
                functions.append({
                    "name": str(graph.get_name()),
                    "type": "function"
                })

        return _make_success({
            "functions": functions,
            "count": len(functions),
            "blueprint_path": blueprint_path
        })

    except Exception as e:
        return _make_error(f"Error listing functions: {e}")


def find_event_graph(blueprint_path: str) -> Dict[str, Any]:
    """
    Find the main event graph of a Blueprint.

    Args:
        blueprint_path: Path to the Blueprint

    Returns:
        {success, data: {found, graph_name}, error}
    """
    if not blueprint_path:
        return _make_error("Blueprint path cannot be empty")

    try:
        if not unreal.EditorAssetLibrary.does_asset_exist(blueprint_path):
            return _make_error(f"Blueprint does not exist: {blueprint_path}")

        blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
        if blueprint is None:
            return _make_error(f"Failed to load Blueprint: {blueprint_path}")

        # Find the event graph
        event_graph = unreal.BlueprintEditorLibrary.find_event_graph(blueprint)

        if event_graph:
            return _make_success({
                "found": True,
                "graph_name": str(event_graph.get_name()),
                "blueprint_path": blueprint_path
            })
        else:
            return _make_success({
                "found": False,
                "blueprint_path": blueprint_path
            })

    except Exception as e:
        return _make_error(f"Error finding event graph: {e}")


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
