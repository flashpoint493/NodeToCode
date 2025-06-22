# Blueprint Variable Tools

This directory contains MCP tools for creating variables and searching variable types in Blueprints. These tools handle both member variables and local function variables.

## Available Tools

### create-variable
- **Description**: Creates a new member variable in the active Blueprint
- **Parameters**:
  - `variableName` (required): Name for the new variable
  - `typeIdentifier` (required): Type identifier (for maps, this is the VALUE type)
  - `blueprintPath` (optional): Asset path of the Blueprint
  - `defaultValue` (optional): Default value for the variable
  - `category` (optional): Category for organization
  - `isInstanceEditable` (optional): Per-instance editability
  - `isBlueprintReadOnly` (optional): Read-only in graphs
  - `tooltip` (optional): Description tooltip
  - `replicationCondition` (optional): Network replication setting
  - `containerType` (optional): Container type (none/array/set/map)
  - `mapKeyTypeIdentifier` (optional): Key type for maps
- **Returns**: Created variable name, type info, and Blueprint details
- **Use Case**: Adding member variables to Blueprint classes

### create-local-variable
- **Description**: Creates a new local variable in the currently focused Blueprint function
- **Parameters**:
  - `variableName` (required): Name for the local variable
  - `typeIdentifier` (required): Type identifier (for maps, this is the VALUE type)
  - `defaultValue` (optional): Default value
  - `tooltip` (optional): Description
  - `containerType` (optional): Container type
  - `mapKeyTypeIdentifier` (optional): Key type for maps
- **Returns**: Created variable details and function context
- **Use Case**: Adding local variables within function scope

### search-variable-types
- **Description**: Searches for available variable types by name
- **Parameters**:
  - `searchTerm` (required): Text to search for
  - `category` (optional): Filter by type category (all/primitive/class/struct/enum)
  - `includeEngineTypes` (optional): Include engine-provided types
  - `maxResults` (optional): Maximum results to return
- **Returns**: Matching types with identifiers and metadata
- **Use Case**: Discovering available types for variable creation

### get-blueprint-member-variables
- **Description**: Retrieves all member variables from the currently focused Blueprint
- **Parameters**: None
- **Returns**: Complete list of member variables with:
  - Variable names and categories
  - Type information (including container types)
  - Default values
  - Property flags (editable, read-only, replicated, etc.)
  - Metadata (tooltips, display names, etc.)
  - GUID identifiers
- **Use Case**: Inspecting Blueprint variables, generating documentation, finding variables to use in nodes

## Type Identifier System

### Primitive Types
Direct type names: `"bool"`, `"int32"`, `"float"`, `"FString"`, `"FVector"`

### Class Types
Full path format: `"/Script/Engine.Actor"`, `"/Script/Engine.Pawn"`

### Struct Types
Full path or name: `"/Script/CoreUObject.Vector"` or `"FVector"`

### Enum Types
Full path format: `"/Script/Engine.ECollisionChannel"`

### Container Examples

#### Array Variable
```json
{
  "variableName": "ActorArray",
  "typeIdentifier": "/Script/Engine.Actor",
  "containerType": "array"
}
```

#### Map Variable
```json
{
  "variableName": "NameToActorMap",
  "typeIdentifier": "/Script/Engine.Actor",
  "containerType": "map",
  "mapKeyTypeIdentifier": "FName"
}
```

## Workflow Examples

### Finding and Creating Variables
```bash
# Step 1: Search for Actor-related types
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "search-variable-types",
      "arguments": {
        "searchTerm": "Actor",
        "category": "class"
      }
    },
    "id": 1
  }'

# Step 2: Create a variable using found type
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "create-variable",
      "arguments": {
        "variableName": "TargetActor",
        "typeIdentifier": "/Script/Engine.Actor",
        "category": "Gameplay",
        "isInstanceEditable": true,
        "tooltip": "The actor to interact with"
      }
    },
    "id": 2
  }'
```

### Creating Complex Container Variables
```bash
# Create a map from String to Transform
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "create-variable",
      "arguments": {
        "variableName": "SavedTransforms",
        "typeIdentifier": "FTransform",
        "containerType": "map",
        "mapKeyTypeIdentifier": "FString",
        "category": "Save System"
      }
    },
    "id": 1
  }'
```

### Inspecting Blueprint Variables
```bash
# Get all member variables from the focused Blueprint
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "get-blueprint-member-variables",
      "arguments": {}
    },
    "id": 1
  }'
```

## Implementation Notes

- Type resolution uses Unreal's reflection system
- Variable names are automatically made unique if conflicts exist
- Local variables are created as `UK2Node_VariableGet/Set` nodes
- Container types are handled through `FEdGraphPinType` configuration
- All operations support undo/redo through transactions