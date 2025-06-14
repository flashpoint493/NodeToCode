# NodeToCode MCP Tools

This directory contains the implementation of MCP (Model Context Protocol) tools for the NodeToCode plugin. The system uses an interface-based architecture with automatic registration, making it easy to add new tools without modifying existing code.

## Architecture Overview

### Folder Structure

```
MCP/Tools/
├── Framework (in MCP/Tools/)
│   ├── IN2CMcpTool.h           - Tool interface
│   ├── N2CMcpToolBase.h/.cpp   - Base class for tools
│   ├── N2CMcpToolRegistry.h/.cpp - Auto-registration system
│   ├── N2CMcpToolManager.h/.cpp  - Tool management
│   └── N2CMcpToolTypes.h/.cpp    - Type definitions
│
├── Implementations/            - Actual tool implementations
│   └── N2CMcpGetFocusedBlueprintTool.h/.cpp
│
└── README.md                   - This documentation
```

### Core Components

1. **`IN2CMcpTool`** - The interface that all MCP tools must implement
2. **`FN2CMcpToolBase`** - Base class providing common functionality for tools
3. **`FN2CMcpToolRegistry`** - Registry system with automatic tool registration
4. **`REGISTER_MCP_TOOL` macro** - Enables automatic registration during static initialization

### How It Works

Tools are automatically registered when the plugin loads using static initialization. Each tool:
- Implements the `IN2CMcpTool` interface (typically by inheriting from `FN2CMcpToolBase`)
- Uses the `REGISTER_MCP_TOOL` macro in its .cpp file
- Is automatically registered with the MCP tool manager when the server starts

## Creating a New MCP Tool

Follow these steps to add a new MCP tool:

### 1. Create the Tool Header File

Create a new header file in `Source/Private/MCP/Tools/Implementations/`:

```cpp
// N2CMcpYourToolName.h
#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

class FN2CMcpYourToolName : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    
    // Only override if your tool needs Game Thread execution
    // virtual bool RequiresGameThread() const override { return true; }
};
```

### 2. Create the Tool Implementation

Create the corresponding .cpp file in the same directory (`Source/Private/MCP/Tools/Implementations/`):

```cpp
// N2CMcpYourToolName.cpp
#include "N2CMcpYourToolName.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "Utils/N2CLogger.h"

// Auto-register this tool
REGISTER_MCP_TOOL(FN2CMcpYourToolName)

FMcpToolDefinition FN2CMcpYourToolName::GetDefinition() const
{
    FMcpToolDefinition Definition(
        TEXT("your-tool-name"),
        TEXT("Description of what your tool does")
    );
    
    // Define input schema
    TMap<FString, FString> Properties;
    Properties.Add(TEXT("param1"), TEXT("string"));
    Properties.Add(TEXT("param2"), TEXT("number"));
    
    TArray<FString> Required;
    Required.Add(TEXT("param1")); // Only param1 is required
    
    Definition.InputSchema = BuildInputSchema(Properties, Required);
    
    return Definition;
}

FMcpToolCallResult FN2CMcpYourToolName::Execute(const TSharedPtr<FJsonObject>& Arguments)
{
    // Extract parameters
    FString Param1;
    if (!Arguments->TryGetStringField(TEXT("param1"), Param1))
    {
        return FMcpToolCallResult::CreateErrorResult(TEXT("Missing required parameter: param1"));
    }
    
    double Param2 = 0.0;
    Arguments->TryGetNumberField(TEXT("param2"), Param2);
    
    // Your tool logic here
    FString Result = FString::Printf(TEXT("Processed %s with value %f"), *Param1, Param2);
    
    return FMcpToolCallResult::CreateTextResult(Result);
}
```

### 3. Tools Requiring Game Thread Execution

If your tool needs to access Unreal Editor APIs, it must run on the Game Thread:

```cpp
class FN2CMcpEditorTool : public FN2CMcpToolBase
{
public:
    virtual bool RequiresGameThread() const override { return true; }
    
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override
    {
        // Use the base class helper for Game Thread execution
        return ExecuteOnGameThread([this, Arguments]() -> FMcpToolCallResult
        {
            // Your editor API calls here
            // This lambda will run on the Game Thread
            return FMcpToolCallResult::CreateTextResult(TEXT("Result"));
        });
    }
};
```

### 4. Build and Test

1. Build the plugin in your IDE
2. The tool will automatically register when the plugin loads
3. Test using curl or an MCP client:

```bash
# List available tools
curl -X POST http://localhost:10000/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc": "2.0", "method": "tools/list", "id": 1}'

# Call your tool
curl -X POST http://localhost:10000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "your-tool-name",
      "arguments": {
        "param1": "test",
        "param2": 42
      }
    },
    "id": 2
  }'
```

## Helper Functions

The `FN2CMcpToolBase` class provides several helper functions:

### `BuildInputSchema`
Creates a JSON schema for tool parameters:
```cpp
TMap<FString, FString> Properties;
Properties.Add(TEXT("message"), TEXT("string"));
Properties.Add(TEXT("count"), TEXT("number"));

TArray<FString> Required;
Required.Add(TEXT("message"));

Definition.InputSchema = BuildInputSchema(Properties, Required);
```

### `BuildEmptyObjectSchema`
For tools with no parameters:
```cpp
Definition.InputSchema = BuildEmptyObjectSchema();
```

### `AddReadOnlyAnnotation`
Marks a tool as read-only:
```cpp
AddReadOnlyAnnotation(Definition);
```

### `ExecuteOnGameThread`
Safely executes code on the Game Thread with timeout handling:
```cpp
return ExecuteOnGameThread([this]() -> FMcpToolCallResult
{
    // Code that must run on Game Thread
    return FMcpToolCallResult::CreateTextResult(TEXT("Done"));
}, 30.0f); // 30 second timeout
```

## Existing Tools

### get-focused-blueprint
- **Location**: `Implementations/N2CMcpGetFocusedBlueprintTool.cpp`
- **Description**: Collects and serializes the currently focused Blueprint graph into N2CJSON format
- **Parameters**: None
- **Requires Game Thread**: Yes
- **Returns**: N2CJSON representation of the focused Blueprint

### search-blueprint-nodes
- **Location**: `Implementations/N2CMcpSearchBlueprintNodesTool.cpp`
- **Description**: Searches for Blueprint nodes/actions relevant to a given query. Can perform context-sensitive search based on the current Blueprint or a global search
- **Parameters**:
  - `searchTerm` (string, required): The text query to search for
  - `contextSensitive` (boolean, optional, default: true): If true, performs a context-sensitive search using blueprintContext
  - `maxResults` (integer, optional, default: 20): Maximum number of results to return (1-100)
  - `blueprintContext` (object, optional): Information for context-sensitive search
    - `graphPath` (string): The asset path of the UEdGraph being viewed
    - `owningBlueprintPath` (string): The asset path of the UBlueprint asset
- **Requires Game Thread**: Yes
- **Returns**: Array of nodes in N2CJSON format with `spawnMetadata` containing:
  - `actionIdentifier`: Unique identifier for spawning (uses `>` as delimiter for multi-line identifiers)
  - `isContextSensitive`: Whether the node requires specific context

### add-bp-node-to-active-graph
- **Location**: `Implementations/N2CMcpAddBlueprintNodeTool.cpp`
- **Description**: Adds a Blueprint node to the currently active graph. IMPORTANT: The search-blueprint-nodes tool MUST have been used before this tool to find the node and get its actionIdentifier
- **Parameters**:
  - `nodeName` (string, required): The name of the node to add (e.g., 'Spawn Actor from Class')
  - `actionIdentifier` (string, required): The unique action identifier from search-blueprint-nodes spawnMetadata
  - `location` (object, optional): The location to spawn the node at
    - `x` (number, default: 0): X coordinate
    - `y` (number, default: 0): Y coordinate
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `success`: Whether the node was successfully added
  - `nodeId`: Unique ID of the spawned node
  - `graphName`: Name of the graph the node was added to
  - `blueprintName`: Name of the Blueprint containing the graph

### list-blueprint-functions
- **Location**: `Implementations/N2CMcpListBlueprintFunctionsTool.cpp`
- **Description**: Lists all functions defined in a Blueprint with their parameters and metadata
- **Parameters**:
  - `blueprintPath` (string, optional): Asset path of the Blueprint. If not provided, uses focused Blueprint
  - `includeInherited` (boolean, optional, default: false): Whether to include inherited functions from parent classes
  - `includeOverridden` (boolean, optional, default: false): Whether to include overridden parent functions
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `blueprintName`: Name of the Blueprint
  - `blueprintPath`: Full asset path of the Blueprint
  - `parentClass`: Name of the parent class (if any)
  - `functions`: Array of function objects, each containing:
    - `name`: Function name
    - `type`: Function type (Function/CustomEvent)
    - `guid`: Function's unique identifier
    - `inputs`: Array of input parameters
    - `outputs`: Array of output parameters
    - `flags`: Function flags (isPure, isStatic, isConst, etc.)
    - `graphInfo`: Graph information (name, node count)
  - `functionCount`: Total number of functions

### create-blueprint-function
- **Location**: `Implementations/N2CMcpCreateBlueprintFunctionTool.cpp`
- **Description**: Creates a new Blueprint function with specified parameters and opens it in the editor
- **Parameters**:
  - `functionName` (string, required): Name of the function to create
  - `blueprintPath` (string, optional): Asset path of the Blueprint. If not provided, uses focused Blueprint
  - `parameters` (array, optional): Array of parameter definitions, each containing:
    - `name` (string, required): Parameter name
    - `type` (string, required): Parameter type (e.g., 'bool', 'int', 'float', 'string', 'vector', 'object')
    - `direction` (string, optional, default: 'input'): Either 'input' or 'output'
    - `subType` (string, optional): For object/class/struct/enum types, the specific type
    - `container` (string, optional, default: 'none'): Container type ('none', 'array', 'set', 'map')
    - `keyType` (string, optional): For map containers, the key type
    - `isReference` (boolean, optional, default: false): Whether the parameter is passed by reference
    - `isConst` (boolean, optional, default: false): Whether the parameter is const
    - `defaultValue` (string, optional): Default value for the parameter
  - `functionFlags` (object, optional): Function configuration flags
    - `isPure` (boolean): Whether the function is pure (no exec pins)
    - `isCallInEditor` (boolean): Whether the function can be called in editor
    - `isStatic` (boolean): Whether the function is static
    - `isConst` (boolean): Whether the function is const
    - `category` (string): Function category for organization
    - `keywords` (string): Keywords for searching
    - `tooltip` (string): Function tooltip
    - `accessSpecifier` (string): 'public', 'protected', or 'private'
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `success`: Whether the function was created successfully
  - `functionName`: Name of the created function
  - `functionGuid`: GUID of the created function
  - `blueprintName`: Name of the Blueprint
  - `blueprintPath`: Full asset path of the Blueprint
  - `graphInfo`: Information about the created graph
  - `message`: Success message

### create-variable
- **Location**: `Implementations/N2CMcpCreateVariableTool.cpp`
- **Description**: Creates a new variable in a Blueprint
- **Parameters**:
  - `variableName` (string, required): Name of the variable to create
  - `variableType` (string, required): Type of the variable
  - `blueprintPath` (string, optional): Asset path of the Blueprint. If not provided, uses focused Blueprint
  - `category` (string, optional): Category for organizing the variable
  - `isInstanceEditable` (boolean, optional): Whether the variable can be edited on instances
  - `defaultValue` (string, optional): Default value for the variable
- **Requires Game Thread**: Yes
- **Returns**: Success/error information

### search-variable-types
- **Location**: `Implementations/N2CMcpSearchVariableTypesTool.cpp`
- **Description**: Searches for available variable types that can be used in Blueprints
- **Parameters**:
  - `searchTerm` (string, optional): Term to search for in type names
  - `category` (string, optional): Filter by category (e.g., 'primitive', 'struct', 'object')
- **Requires Game Thread**: Yes
- **Returns**: Array of available types with their categories and paths

### list-overridable-functions
- **Location**: `Implementations/N2CMcpListOverridableFunctionsTool.cpp`
- **Description**: Lists all functions that can be overridden from parent classes and interfaces
- **Parameters**:
  - `blueprintPath` (string, optional): Asset path of the Blueprint. If not provided, uses focused Blueprint
  - `includeImplemented` (boolean, optional, default: false): Whether to include functions that are already implemented/overridden
  - `filterByCategory` (string, optional): Filter functions by category (e.g., 'Input', 'Collision', 'Animation')
  - `searchTerm` (string, optional): Search term to filter function names
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `blueprintName`: Name of the Blueprint
  - `blueprintPath`: Full asset path of the Blueprint
  - `parentClass`: Name of the parent class
  - `implementedInterfaces`: Array of implemented interface names
  - `functions`: Array of overridable function objects, each containing:
    - `name`: Function name
    - `displayName`: User-friendly display name
    - `isImplemented`: Whether the function is already implemented
    - `sourceType`: Where the function comes from ('ParentClass' or 'Interface')
    - `sourceName`: Name of the parent class or interface
    - `functionType`: Type of function ('Event' or 'Function')
    - `implementationType`: 'BlueprintImplementableEvent' or 'BlueprintNativeEvent'
    - `category`: Function category
    - `inputs`: Array of input parameters
    - `outputs`: Array of output parameters
    - `metadata`: Function metadata (tooltip, keywords, etc.)
    - `flags`: Function flags (isConst, isStatic, isReliable, etc.)
  - `functionCount`: Total number of overridable functions

## Example Workflows

### Searching and Adding Blueprint Nodes

The `search-blueprint-nodes` and `add-bp-node-to-active-graph` tools work together to allow AI assistants to discover and place Blueprint nodes:

```bash
# Step 1: Search for nodes
curl -X POST http://localhost:10000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "search-blueprint-nodes",
      "arguments": {
        "searchTerm": "spawn actor",
        "contextSensitive": true,
        "maxResults": 5
      }
    },
    "id": 1
  }'

# Response includes nodes with spawnMetadata containing actionIdentifier

# Step 2: Add a node using the actionIdentifier from search results
curl -X POST http://localhost:10000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "add-bp-node-to-active-graph",
      "arguments": {
        "nodeName": "Spawn Actor from Class",
        "actionIdentifier": "spawnactorfromclass>createnew>|game>spawnactorfromclass>createnew>|game",
        "location": {
          "x": 200,
          "y": 100
        }
      }
    },
    "id": 2
  }'
```

## Best Practices

1. **Tool Naming**: Use kebab-case for tool names (e.g., `get-blueprint-info`)
2. **Error Handling**: Always validate input parameters and return clear error messages
3. **Logging**: Use `FN2CLogger` for debugging and error reporting
4. **Thread Safety**: If your tool doesn't need Editor APIs, avoid requiring Game Thread execution
5. **Documentation**: Include clear descriptions in your tool definition
6. **Input Validation**: Check all required parameters before processing
7. **Tool Dependencies**: If tools work together (like search and add nodes), clearly document the workflow

## Troubleshooting

### Tool Not Appearing in tools/list
- Ensure the `REGISTER_MCP_TOOL` macro is in the .cpp file
- Check that the .cpp file is included in the build
- Look for registration errors in the log

### Tool Execution Fails
- Check the log for error messages
- Verify input parameters match the schema
- For Game Thread tools, ensure timeout is sufficient

### Threading Issues
- Tools accessing Editor APIs must override `RequiresGameThread()` to return true
- Use `ExecuteOnGameThread` helper for safe execution
- Adjust timeout if operations take longer than expected