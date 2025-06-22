# Content Browser Tools

This directory contains MCP tools for interacting with Unreal Engine's Content Browser. These tools enable navigation, asset discovery, and Blueprint opening through the content browser interface.

## Available Tools

### open-content-browser-path
- **Description**: Opens a specified path in the focused content browser
- **Parameters**:
  - `path` (required): Content browser path (e.g., '/Game/Blueprints')
  - `select_item` (optional): Specific item to select
  - `create_if_missing` (optional): Create folder if it doesn't exist
- **Returns**: Navigation status and selected paths
- **Use Case**: Programmatic content browser navigation

### read-content-browser-path
- **Description**: Returns assets and folders at a content browser path
- **Parameters**:
  - `path` (required): Content browser path to read
  - `page` (optional): Page number for pagination
  - `page_size` (optional): Items per page (1-200)
  - `filter_type` (optional): Filter by asset type
  - `filter_name` (optional): Filter by name contains
  - `sync_browser` (optional): Sync content browser to path
- **Returns**: Paginated list of assets and folders
- **Use Case**: Discovering and filtering content

### open-blueprint-asset
- **Description**: Opens a Blueprint asset in the Blueprint Editor
- **Parameters**:
  - `asset_path` (required): Full asset path
  - `bring_to_front` (optional): Bring editor to front
  - `focus_graph` (optional): Specific graph to focus
- **Returns**: Blueprint information and editor state
- **Use Case**: Opening Blueprints for editing

### search-content-browser
- **Description**: Search for assets across the entire content browser by name or type
- **Parameters**:
  - `query` (optional): Search query for asset names (partial match, case-insensitive)
  - `assetType` (optional): Filter by type (All, Blueprint, Material, Texture, StaticMesh, SkeletalMesh, Sound, Animation, ParticleSystem, DataAsset, DataTable)
  - `includeEngineContent` (optional, default: false): Include Engine content in search
  - `includePluginContent` (optional, default: true): Include Plugin content in search
  - `maxResults` (optional, default: 50): Maximum results to return (1-200)
- **Returns**: Ranked search results with relevance scores
- **Use Case**: Finding assets across entire project

## Content Browser Paths

### Valid Path Roots
- `/Game` - Project content
- `/Engine` - Engine content
- `/EnginePresets` - Engine preset content
- `/Paper2D` - Paper2D plugin content
- `/NodeToCode` - NodeToCode plugin content
- `/Plugins` - Other plugin content

### Path Format
- Must start with `/`
- Use forward slashes
- No `..` traversal allowed
- Case-sensitive

## Asset Types for Filtering

- `"All"` - All asset types
- `"Blueprint"` - Blueprint classes
- `"Material"` - Materials and instances
- `"Texture"` - Texture assets
- `"StaticMesh"` - Static mesh assets
- `"Folder"` - Folders only

## Usage Examples

### Navigating Content Browser
```bash
# Navigate to Blueprints folder
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "open-content-browser-path",
      "arguments": {
        "path": "/Game/Blueprints"
      }
    },
    "id": 1
  }'

# Create and navigate to new folder
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "open-content-browser-path",
      "arguments": {
        "path": "/Game/AI/BehaviorTrees",
        "create_if_missing": true
      }
    },
    "id": 2
  }'
```

### Browsing Assets
```bash
# List all Blueprints in a folder
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-content-browser-path",
      "arguments": {
        "path": "/Game/Blueprints",
        "filter_type": "Blueprint"
      }
    },
    "id": 1
  }'

# Search for player-related assets
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-content-browser-path",
      "arguments": {
        "path": "/Game",
        "filter_name": "Player",
        "page_size": 20
      }
    },
    "id": 2
  }'
```

### Opening Blueprints
```bash
# Open a Blueprint
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "open-blueprint-asset",
      "arguments": {
        "asset_path": "/Game/Blueprints/BP_PlayerCharacter.BP_PlayerCharacter"
      }
    },
    "id": 1
  }'

# Open and focus specific graph
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "open-blueprint-asset",
      "arguments": {
        "asset_path": "/Game/Blueprints/BP_GameMode.BP_GameMode",
        "focus_graph": "ConstructionScript"
      }
    },
    "id": 2
  }'
```

## Pagination for Large Directories

When dealing with large asset folders (e.g., Megascans):

```bash
# Page 1 of results
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-content-browser-path",
      "arguments": {
        "path": "/Game/Megascans/Surfaces",
        "page": 1,
        "page_size": 50
      }
    },
    "id": 1
  }'

# Check response for has_more: true, then get page 2
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-content-browser-path",
      "arguments": {
        "path": "/Game/Megascans/Surfaces",
        "page": 2,
        "page_size": 50
      }
    },
    "id": 2
  }'
```

## Complete Workflow Example

### Finding and Opening a Blueprint
```bash
# 1. Navigate to Blueprints folder
open-content-browser-path "/Game/Blueprints"

# 2. List available Blueprints
read-content-browser-path "/Game/Blueprints" filter_type="Blueprint"

# 3. Search for specific Blueprint
read-content-browser-path "/Game/Blueprints" filter_name="Player"

# 4. Open the found Blueprint
open-blueprint-asset "/Game/Blueprints/BP_PlayerCharacter.BP_PlayerCharacter"

# 5. Focus on specific functionality
open-blueprint-asset "/Game/Blueprints/BP_PlayerCharacter.BP_PlayerCharacter" focus_graph="HandleInput"
```

### Global Asset Search
```bash
# Search for all player-related assets
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "search-content-browser",
      "arguments": {
        "query": "Player"
      }
    },
    "id": 1
  }'

# Find all Blueprints in the project
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "search-content-browser",
      "arguments": {
        "assetType": "Blueprint",
        "maxResults": 100
      }
    },
    "id": 2
  }'

# Search for materials including engine content
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "search-content-browser",
      "arguments": {
        "query": "Metal",
        "assetType": "Material",
        "includeEngineContent": true
      }
    },
    "id": 3
  }'
```

## Implementation Notes

- Content browser operations use `IContentBrowserSingleton`
- Asset filtering uses `FARFilter` for efficient queries
- Pagination prevents timeouts with large directories
- Asset paths must include the asset name twice (UE convention)
- All operations require Game Thread execution