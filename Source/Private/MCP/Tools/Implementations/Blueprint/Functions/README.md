# Blueprint Function Management Tools

This directory contains MCP tools for creating, opening, and managing Blueprint functions. These tools enable programmatic manipulation of Blueprint function graphs.

## Available Tools

### create-blueprint-function
- **Description**: Creates a new Blueprint function with specified parameters and opens it in the editor
- **Parameters**:
  - `functionName` (required): Name of the function to create
  - `blueprintPath` (optional): Asset path of the Blueprint
  - `parameters` (optional): Array of parameter definitions with type, direction, containers
  - `functionFlags` (optional): Configuration flags (isPure, isStatic, category, etc.)
- **Returns**: Function GUID, graph information, and success status
- **Use Case**: Programmatically creating functions with complex signatures

### open-blueprint-function
- **Description**: Opens a Blueprint function in the editor using its GUID
- **Parameters**:
  - `functionGuid` (required): The GUID of the function to open
  - `blueprintPath` (optional): Asset path of the Blueprint
  - `centerView` (optional): Center graph view on function entry
  - `selectNodes` (optional): Select all nodes in the function
- **Returns**: Function information and editor state
- **Use Case**: Navigating to specific functions after creation or from analysis

### delete-blueprint-function
- **Description**: Deletes a specific Blueprint function using its GUID
- **Parameters**:
  - `functionGuid` (required): The GUID of the function to delete
  - `blueprintPath` (optional): Asset path of the Blueprint
  - `force` (optional): Force deletion even with references
- **Returns**: Deleted function info and removed references
- **Use Case**: Cleaning up unused functions or refactoring

## Parameter Type System

### Basic Types
- Primitives: `"bool"`, `"int32"`, `"float"`, `"FString"`, `"FName"`, `"FText"`
- Vectors: `"FVector"`, `"FVector2D"`, `"FRotator"`, `"FTransform"`
- Colors: `"FLinearColor"`, `"FColor"`

### Object References
- Use `type: "object"` with `subType` for specific classes:
  ```json
  {
    "name": "Target",
    "type": "object",
    "subType": "Actor"
  }
  ```

### Container Types
- Arrays: `container: "array"`
- Sets: `container: "set"`
- Maps: `container: "map"` with `keyType` specification

### Example: Complex Function Creation
```bash
curl -X POST http://localhost:27000/mcp \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "create-blueprint-function",
      "arguments": {
        "functionName": "FindActorsByTag",
        "parameters": [
          {
            "name": "Tag",
            "type": "FName",
            "direction": "input"
          },
          {
            "name": "FoundActors",
            "type": "object",
            "subType": "Actor",
            "container": "array",
            "direction": "output"
          },
          {
            "name": "Count",
            "type": "int32",
            "direction": "output"
          }
        ],
        "functionFlags": {
          "isPure": false,
          "category": "Gameplay",
          "tooltip": "Finds all actors with the specified tag"
        }
      }
    },
    "id": 1
  }'
```

## Function Lifecycle Workflow

1. **Create Function**: Use `create-blueprint-function` to define the function
2. **Get Function GUID**: Save the returned `functionGuid` for future operations
3. **Open for Editing**: Use `open-blueprint-function` with the GUID
4. **Add Implementation**: Use graph manipulation tools to add nodes
5. **Delete if Needed**: Use `delete-blueprint-function` for cleanup

## Implementation Notes

- Functions are created using `UK2Node_FunctionEntry` and `UK2Node_FunctionResult`
- Parameter types are resolved through Unreal's type registry
- All operations use transactions for undo/redo support
- Function GUIDs persist across editor sessions