# Blueprint Compilation Tools

This directory contains MCP tools for compiling Blueprint assets in Unreal Engine. These tools provide programmatic access to Blueprint compilation functionality, enabling automated validation and build processes.

## Available Tools

### compile-blueprint
- **Description**: Compiles a Blueprint using the same functionality as the Compile button in the Blueprint editor
- **Parameters**:
  - `blueprintPath` (optional): Asset path of the Blueprint to compile. If not provided, uses the currently focused Blueprint
  - `skipGarbageCollection` (optional): Skip garbage collection during compilation for performance (default: true)
- **Returns**: Comprehensive compilation results including errors, warnings, status changes, and timing
- **Use Case**: Automated Blueprint validation, CI/CD pipelines, batch compilation

## Common Usage Patterns

### Compiling the Focused Blueprint
```bash
# Compile the currently focused Blueprint
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "compile-blueprint",
      "arguments": {}
    },
    "id": 1
  }'
```

### Compiling a Specific Blueprint
```bash
# Compile a Blueprint by path
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "compile-blueprint",
      "arguments": {
        "blueprintPath": "/Game/Blueprints/BP_PlayerCharacter"
      }
    },
    "id": 1
  }'
```

### Performance-Optimized Compilation
```bash
# Compile with garbage collection enabled for thorough validation
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "compile-blueprint",
      "arguments": {
        "blueprintPath": "/Game/Blueprints/BP_GameMode",
        "skipGarbageCollection": false
      }
    },
    "id": 1
  }'
```

## Response Format

The tool returns detailed compilation results:

```json
{
  "success": true,
  "blueprintName": "BP_PlayerCharacter",
  "blueprintPath": "/Game/Blueprints/BP_PlayerCharacter",
  "compilationStatus": {
    "previousStatus": "Dirty",
    "currentStatus": "UpToDate",
    "statusCode": 3
  },
  "compilationTime": 0.125,
  "results": {
    "errorCount": 0,
    "warningCount": 1,
    "noteCount": 2,
    "messages": [
      {
        "severity": "Warning",
        "message": "Variable 'Health' is never used"
      },
      {
        "severity": "Note",
        "message": "Compilation completed successfully"
      }
    ]
  },
  "message": "Blueprint compiled successfully with 1 warning"
}
```

### Status Values
- `Unknown`: Blueprint status is unknown
- `Dirty`: Blueprint has unsaved changes that need compilation
- `Error`: Blueprint has compilation errors
- `UpToDate`: Blueprint is compiled and error-free
- `BeingCreated`: Blueprint is currently being created
- `UpToDateWithWarnings`: Blueprint compiled with warnings

## Error Handling

The tool provides specific error codes for different failure scenarios:

- `ASSET_NOT_FOUND`: The specified Blueprint path is invalid
- `NO_ACTIVE_BLUEPRINT`: No Blueprint path provided and no Blueprint is focused
- `COMPILATION_FAILED`: A critical error occurred during compilation
- `INTERNAL_ERROR`: An unexpected exception occurred

## Implementation Notes

- Uses `FKismetEditorUtilities::CompileBlueprint()` - the same API as the editor
- Captures all messages from `FCompilerResultsLog` for detailed diagnostics
- Automatically refreshes the Blueprint action database after compilation
- Executes on the Game Thread as required for Blueprint operations
- Supports performance optimization through garbage collection control

## Integration with CI/CD

This tool is ideal for continuous integration workflows:

1. **Pre-commit Validation**: Compile Blueprints before committing changes
2. **Automated Testing**: Verify Blueprint compilation as part of test suites
3. **Batch Processing**: Compile multiple Blueprints programmatically
4. **Error Reporting**: Parse JSON results for automated error tracking

## Future Enhancements

Planned improvements for the compilation tools:

- Batch compilation support for multiple Blueprints
- Dependency compilation (compile all dependent Blueprints)
- Integration with Blueprint validation tools
- Progress reporting for long-running compilations
- Compilation performance metrics and optimization suggestions