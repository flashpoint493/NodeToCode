"""
NodeToCode Python Module for Unreal Engine

Provides Blueprint manipulation and script management utilities accessible from
the run-python MCP tool. All functions return standardized dictionaries with
'success', 'data', and 'error' keys.

Quick Start:
    import nodetocode as n2c

    # Get the currently focused blueprint
    bp = n2c.get_focused_blueprint()
    if bp['success']:
        print(f"Blueprint: {bp['data']['name']}")
        print(f"Status: {bp['data']['status']}")

    # Compile the blueprint
    result = n2c.compile_blueprint()
    if result['success']:
        print("Compilation successful!")

    # Save the blueprint
    result = n2c.save_blueprint()
    if result['data']['was_saved']:
        print("Saved to disk")

Script Management:
    # Search for existing scripts
    matches = n2c.search_scripts("health system")

    # Run a saved script
    result = n2c.run_script("create_health_system", max_hp=100)

    # Save a new script for reuse
    n2c.save_script("my_script", code, "Description", tags=["gameplay"])

Return Format:
    All functions return a dictionary with:
    - 'success': bool - True if the operation succeeded
    - 'data': dict or None - Operation-specific result data
    - 'error': str or None - Error message if success is False

Available Functions:
    Blueprint Operations:
    - get_focused_blueprint() - Get info about the focused Blueprint
    - compile_blueprint(path=None) - Compile a Blueprint
    - save_blueprint(path=None) - Save a Blueprint to disk
    - load_blueprint(path) - Load a Blueprint by path

    Script Management:
    - list_scripts(category, limit) - List available scripts
    - search_scripts(query, limit) - Search scripts by name/description/tags
    - get_script(name) - Load script code and metadata
    - get_script_functions(name) - Get function signatures (token-efficient)
    - run_script(name, **kwargs) - Execute a saved script
    - save_script(name, code, description, tags, category) - Save new script
    - delete_script(name) - Remove a script
    - get_script_stats() - Get script library statistics

    NodeToCode-Specific (Tagging & LLM):
    - tag_graph(tag, category, description) - Tag the focused graph
    - list_tags(category, tag) - List all tags with optional filters
    - remove_tag(graph_guid, tag) - Remove a tag from a graph
    - get_llm_providers() - Get all available LLM providers
    - get_active_provider() - Get current LLM provider info

    Utilities:
    - get_editor_subsystem(class) - Get an editor subsystem
    - is_blueprint_valid(obj) - Check if object is a Blueprint
    - log_info/warning/error(msg) - Log messages
"""

__version__ = "1.0.0"
__author__ = "Protospatial"

# Blueprint operations
from .blueprint import (
    get_focused_blueprint,
    compile_blueprint,
    save_blueprint,
    load_blueprint,
)

# Script management
from .scripts import (
    list_scripts,
    search_scripts,
    get_script,
    get_script_functions,
    run_script,
    save_script,
    delete_script,
    get_script_stats,
)

# NodeToCode-specific features (tagging, LLM info)
from .bridge import (
    tag_graph,
    list_tags,
    remove_tag,
    get_llm_providers,
    get_active_provider,
)

# Utility functions
from .utils import (
    get_editor_subsystem,
    is_blueprint_valid,
    get_project_content_dir,
    get_project_dir,
    log_info,
    log_warning,
    log_error,
    make_success_result,
    make_error_result,
)

__all__ = [
    # Version info
    '__version__',
    '__author__',
    # Blueprint operations
    'get_focused_blueprint',
    'compile_blueprint',
    'save_blueprint',
    'load_blueprint',
    # Script management
    'list_scripts',
    'search_scripts',
    'get_script',
    'get_script_functions',
    'run_script',
    'save_script',
    'delete_script',
    'get_script_stats',
    # NodeToCode-specific (tagging, LLM info)
    'tag_graph',
    'list_tags',
    'remove_tag',
    'get_llm_providers',
    'get_active_provider',
    # Utilities
    'get_editor_subsystem',
    'is_blueprint_valid',
    'get_project_content_dir',
    'get_project_dir',
    'log_info',
    'log_warning',
    'log_error',
    'make_success_result',
    'make_error_result',
]
