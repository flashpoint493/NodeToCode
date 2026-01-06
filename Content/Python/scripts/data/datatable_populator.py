"""
DataTable population utilities for Unreal Engine.

Populate DataTables from various data sources programmatically.
Supports JSON and CSV input formats.

Usage:
    from scripts.data.datatable_populator import populate_from_json, get_datatable_info

    # Populate from JSON string
    json_data = '[{"Name": "Sword", "Damage": 10}, {"Name": "Axe", "Damage": 15}]'
    populate_from_json('/Game/DT_Items', json_data)

    # Get info about a DataTable
    info = get_datatable_info('/Game/DT_Items')

Functions return standardized dicts: {success: bool, data: {...}, error: str|None}
"""

import unreal
import json
from typing import List, Dict, Any, Optional


def populate_from_json(
    datatable_path: str,
    json_string: str,
    clear_existing: bool = False
) -> Dict[str, Any]:
    """
    Populate a DataTable from a JSON string.

    The JSON should be an array of objects where each object represents a row.
    Each object MUST have a "Name" or "RowName" field for the row identifier.

    Args:
        datatable_path: Path to the DataTable asset
        json_string: JSON array of row objects
        clear_existing: If True, clears existing rows before adding (not supported in all UE versions)

    Returns:
        {success, data: {datatable_path, rows_processed}, error}

    Example JSON:
        [
            {"Name": "Sword", "Damage": 10, "Weight": 2.5},
            {"Name": "Axe", "Damage": 15, "Weight": 4.0}
        ]
    """
    if not datatable_path:
        return _make_error("DataTable path cannot be empty")

    if not json_string:
        return _make_error("JSON string cannot be empty")

    try:
        # Validate JSON
        try:
            data = json.loads(json_string)
        except json.JSONDecodeError as e:
            return _make_error(f"Invalid JSON: {e}")

        if not isinstance(data, list):
            return _make_error("JSON must be an array of objects")

        # Load the DataTable
        if not unreal.EditorAssetLibrary.does_asset_exist(datatable_path):
            return _make_error(f"DataTable does not exist: {datatable_path}")

        datatable = unreal.EditorAssetLibrary.load_asset(datatable_path)
        if datatable is None:
            return _make_error(f"Failed to load DataTable: {datatable_path}")

        if not isinstance(datatable, unreal.DataTable):
            return _make_error(f"Asset is not a DataTable: {datatable_path}")

        # Populate using fill_from_json_string
        # Note: The JSON format for UE's fill_from_json_string is slightly different
        # It expects the "Name" field to be the row name
        result = datatable.fill_from_json_string(json_string)

        # Save the DataTable
        unreal.EditorAssetLibrary.save_asset(datatable_path, only_if_is_dirty=False)

        return _make_success({
            "datatable_path": datatable_path,
            "json_processed": True,
            "rows_in_json": len(data),
            "saved": True
        })

    except Exception as e:
        return _make_error(f"Error populating DataTable: {e}")


def populate_from_csv(
    datatable_path: str,
    csv_string: str
) -> Dict[str, Any]:
    """
    Populate a DataTable from a CSV string.

    The first row must be headers, and must include a "Name" or "---" column
    for row identifiers.

    Args:
        datatable_path: Path to the DataTable
        csv_string: CSV data with headers

    Returns:
        {success, data: {datatable_path, csv_processed}, error}

    Example CSV:
        Name,Damage,Weight
        Sword,10,2.5
        Axe,15,4.0
    """
    if not datatable_path:
        return _make_error("DataTable path cannot be empty")

    if not csv_string:
        return _make_error("CSV string cannot be empty")

    try:
        # Load the DataTable
        if not unreal.EditorAssetLibrary.does_asset_exist(datatable_path):
            return _make_error(f"DataTable does not exist: {datatable_path}")

        datatable = unreal.EditorAssetLibrary.load_asset(datatable_path)
        if datatable is None:
            return _make_error(f"Failed to load DataTable: {datatable_path}")

        if not isinstance(datatable, unreal.DataTable):
            return _make_error(f"Asset is not a DataTable: {datatable_path}")

        # Populate using fill_from_csv_string
        result = datatable.fill_from_csv_string(csv_string)

        # Save the DataTable
        unreal.EditorAssetLibrary.save_asset(datatable_path, only_if_is_dirty=False)

        return _make_success({
            "datatable_path": datatable_path,
            "csv_processed": True,
            "saved": True
        })

    except Exception as e:
        return _make_error(f"Error populating DataTable from CSV: {e}")


def get_datatable_info(datatable_path: str) -> Dict[str, Any]:
    """
    Get information about a DataTable.

    Args:
        datatable_path: Path to the DataTable

    Returns:
        {success, data: {row_struct, row_names, row_count}, error}
    """
    if not datatable_path:
        return _make_error("DataTable path cannot be empty")

    try:
        if not unreal.EditorAssetLibrary.does_asset_exist(datatable_path):
            return _make_error(f"DataTable does not exist: {datatable_path}")

        datatable = unreal.EditorAssetLibrary.load_asset(datatable_path)
        if datatable is None:
            return _make_error(f"Failed to load DataTable: {datatable_path}")

        if not isinstance(datatable, unreal.DataTable):
            return _make_error(f"Asset is not a DataTable: {datatable_path}")

        # Get row names
        row_names = datatable.get_row_names()

        # Get struct info if available
        row_struct_name = ""
        try:
            if hasattr(datatable, 'row_struct'):
                row_struct = datatable.row_struct
                if row_struct:
                    row_struct_name = str(row_struct.get_name())
        except:
            pass

        return _make_success({
            "datatable_path": datatable_path,
            "row_struct": row_struct_name,
            "row_names": [str(name) for name in row_names] if row_names else [],
            "row_count": len(row_names) if row_names else 0
        })

    except Exception as e:
        return _make_error(f"Error getting DataTable info: {e}")


def export_to_json(datatable_path: str) -> Dict[str, Any]:
    """
    Export a DataTable to JSON string.

    Args:
        datatable_path: Path to the DataTable

    Returns:
        {success, data: {datatable_path, json_string}, error}
    """
    if not datatable_path:
        return _make_error("DataTable path cannot be empty")

    try:
        if not unreal.EditorAssetLibrary.does_asset_exist(datatable_path):
            return _make_error(f"DataTable does not exist: {datatable_path}")

        datatable = unreal.EditorAssetLibrary.load_asset(datatable_path)
        if datatable is None:
            return _make_error(f"Failed to load DataTable: {datatable_path}")

        if not isinstance(datatable, unreal.DataTable):
            return _make_error(f"Asset is not a DataTable: {datatable_path}")

        # Export to JSON using the built-in function
        # Note: This returns the JSON as a string
        json_string = datatable.get_table_as_json_string()

        return _make_success({
            "datatable_path": datatable_path,
            "json_string": json_string
        })

    except Exception as e:
        return _make_error(f"Error exporting DataTable to JSON: {e}")


def export_to_csv(datatable_path: str) -> Dict[str, Any]:
    """
    Export a DataTable to CSV string.

    Args:
        datatable_path: Path to the DataTable

    Returns:
        {success, data: {datatable_path, csv_string}, error}
    """
    if not datatable_path:
        return _make_error("DataTable path cannot be empty")

    try:
        if not unreal.EditorAssetLibrary.does_asset_exist(datatable_path):
            return _make_error(f"DataTable does not exist: {datatable_path}")

        datatable = unreal.EditorAssetLibrary.load_asset(datatable_path)
        if datatable is None:
            return _make_error(f"Failed to load DataTable: {datatable_path}")

        if not isinstance(datatable, unreal.DataTable):
            return _make_error(f"Asset is not a DataTable: {datatable_path}")

        # Export to CSV
        csv_string = datatable.get_table_as_csv_string()

        return _make_success({
            "datatable_path": datatable_path,
            "csv_string": csv_string
        })

    except Exception as e:
        return _make_error(f"Error exporting DataTable to CSV: {e}")


def list_datatables(
    folder_path: str,
    recursive: bool = True
) -> Dict[str, Any]:
    """
    Find all DataTables in a folder.

    Args:
        folder_path: Folder to search
        recursive: Include subfolders

    Returns:
        {success, data: {datatables: [...], count: int}, error}
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
                "datatables": [],
                "count": 0,
                "folder_path": folder_path
            })

        datatables = []

        for asset_path in asset_paths:
            asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
            if asset_data and asset_data.is_valid():
                full_name = unreal.AssetRegistryHelpers.get_full_name(asset_data)
                if 'DataTable' in full_name:
                    datatables.append({
                        "path": asset_path,
                        "name": asset_path.split('/')[-1]
                    })

        return _make_success({
            "datatables": datatables,
            "count": len(datatables),
            "folder_path": folder_path
        })

    except Exception as e:
        return _make_error(f"Error listing DataTables: {e}")


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
