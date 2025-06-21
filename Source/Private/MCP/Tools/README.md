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

## Long-Running Tools and Async Execution

NodeToCode now supports asynchronous execution for long-running tools (e.g., LLM translation, complex analysis). This prevents the MCP server from timing out and provides progress feedback to clients.

### How Async Tools Work

1. **Tool Declaration**: Mark a tool as long-running by setting `bIsLongRunning = true` in its definition
2. **SSE Streaming**: Long-running tools use Server-Sent Events (SSE) to stream progress and results
3. **Progress Reporting**: Tools can report progress incrementally during execution
4. **Cancellation**: Clients can cancel running tasks using the `nodetocode/cancelTask` method

### Creating a Long-Running Tool

To create a long-running tool, you need to:

1. Mark the tool as long-running in its definition
2. Create an async task class that implements `IN2CToolAsyncTask`
3. Register the async task in the tool manager

Here's an example:

```cpp
// In your tool's GetDefinition() method
FMcpToolDefinition FN2CMcpLongRunningTool::GetDefinition() const
{
    FMcpToolDefinition Definition(
        TEXT("long-running-tool"),
        TEXT("A tool that takes time to complete")
    );
    
    // Mark as long-running
    Definition.bIsLongRunning = true;
    
    // Define input schema as usual
    Definition.InputSchema = BuildInputSchema(Properties, Required);
    
    return Definition;
}
```

### Creating an Async Task

Create a task class that inherits from `FN2CToolAsyncTaskBase`:

```cpp
// N2CMyAsyncTask.h
#pragma once

#include "MCP/Async/N2CToolAsyncTaskBase.h"

class FN2CMyAsyncTask : public FN2CToolAsyncTaskBase
{
public:
    FN2CMyAsyncTask(const FGuid& TaskId, const FString& ProgressToken, 
        const TSharedPtr<FJsonObject>& Arguments)
        : FN2CToolAsyncTaskBase(TaskId, ProgressToken, TEXT("long-running-tool"), Arguments)
    {
    }

    virtual void Execute() override;
};

// N2CMyAsyncTask.cpp
void FN2CMyAsyncTask::Execute()
{
    // Check for cancellation at the start
    if (CheckCancellationAndReport())
    {
        return;
    }

    // Report initial progress
    ReportProgress(0.0f, TEXT("Starting processing..."));

    // Simulate long-running work
    for (int32 i = 0; i < 100; i++)
    {
        // Check cancellation periodically
        if (CheckCancellationAndReport())
        {
            return;
        }

        // Do some work...
        FPlatformProcess::Sleep(0.1f);

        // Report progress
        float Progress = (i + 1) / 100.0f;
        ReportProgress(Progress, FString::Printf(TEXT("Processing step %d of 100"), i + 1));
    }

    // Report completion
    FMcpToolCallResult Result = FMcpToolCallResult::CreateTextResult(TEXT("Processing complete!"));
    ReportComplete(Result);
}
```

### Registering the Async Task

Update `FN2CToolAsyncTaskManager::CreateAsyncTask` to create your task:

```cpp
if (ToolName == TEXT("long-running-tool"))
{
    return MakeShared<FN2CMyAsyncTask>(TaskId, ProgressToken, Arguments);
}
```

### Client Usage with Progress

When calling a long-running tool, clients should provide a progress token:

```bash
# Call with progress token
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "long-running-tool",
      "arguments": {...},
      "_meta": {
        "progressToken": "unique-progress-token-123"
      }
    },
    "id": 1
  }'

# The response will be streamed as SSE events:
# event: notification
# data: {"jsonrpc":"2.0","method":"nodetocode/taskStarted","params":{"taskId":"...","progressToken":"unique-progress-token-123"}}
#
# event: progress
# data: {"jsonrpc":"2.0","method":"notifications/progress","params":{"progressToken":"unique-progress-token-123","progress":0.25,"message":"Processing step 25 of 100"}}
#
# event: response
# data: {"jsonrpc":"2.0","id":1,"result":{"content":[{"type":"text","text":"Processing complete!"}]}}
```

### Cancelling a Task

To cancel a running task:

```bash
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "nodetocode/cancelTask",
    "params": {
      "progressToken": "unique-progress-token-123"
    },
    "id": 2
  }'

# Response:
# {"jsonrpc":"2.0","id":2,"result":{"status":"cancellation_initiated"}}
```

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
  - `actionIdentifier` (string, required): The unique action identifier obtained from the `spawnMetadata.actionIdentifier` field in `search-blueprint-nodes` results. This MUST be the exact value from the search results (note: `>` is used as a delimiter for multi-line identifiers).
  - `location` (object, optional): The location to spawn the node at
    - `x` (number, default: 0): X coordinate
    - `y` (number, default: 0): Y coordinate
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `success`: Whether the node was successfully added
  - `nodeId`: Unique ID of the spawned node
  - `graphName`: Name of the graph the node was added to
  - `blueprintName`: Name of the Blueprint containing the graph

### connect-pins
- **Location**: `Implementations/N2CMcpConnectPinsTool.cpp`
- **Description**: Connect pins between Blueprint nodes using their GUIDs. Supports batch connections with transactional safety. Output data pins can connect to multiple input pins, while execution pins maintain single connections.
- **Parameters**:
  - `connections` (array, required): Array of pin connection objects to create. Each object has:
    - `from` (object, required):
      - `nodeGuid` (string, required): GUID of the source node.
      - `pinGuid` (string, required): GUID of the source pin.
      - `pinName` (string, optional): Pin name for fallback lookup if GUID match fails.
      - `pinDirection` (string, optional, enum: `"EGPD_Input"`, `"EGPD_Output"`): Expected direction of the pin, for validation.
    - `to` (object, required):
      - `nodeGuid` (string, required): GUID of the target node.
      - `pinGuid` (string, required): GUID of the target pin.
      - `pinName` (string, optional): Pin name for fallback lookup if GUID match fails.
      - `pinDirection` (string, optional, enum: `"EGPD_Input"`, `"EGPD_Output"`): Expected direction of the pin, for validation.
  - `options` (object, optional):
    - `transactionName` (string, default: "NodeToCode: Connect Pins"): Name for the undo transaction.
    - `breakExistingLinks` (boolean, default: true): Whether to break existing links on the pins before creating the new connection. When true:
      - Execution pins: All existing connections are broken (one-to-one connections)
      - Input data pins: Existing connection is broken (can only have one connection)
      - Output data pins: Existing connections are preserved (can connect to multiple inputs)
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `succeeded` (array of objects): Each object details a successful connection (`from`, `to`).
  - `failed` (array of objects): Each object details a failed connection (`from`, `to`, `errorCode`, `reason`).
  - `summary` (object): Contains `totalRequested`, `succeeded`, and `failed` counts.

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
    - `type` (string, required): Type identifier for the parameter (e.g., "bool", "FVector", "object", "/Script/CoreUObject.Vector"). For maps, this is the VALUE type.
    - `direction` (string, optional, default: 'input'): Either 'input' or 'output'
    - `subType` (string, optional): Sub-type identifier, used if `type` is generic (e.g., "object", "class", "struct", "enum"). Example: if `type` is "object", `subType` could be "Actor" or "/Script/Engine.Actor".
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

### open-blueprint-function
- **Location**: `Implementations/N2CMcpOpenBlueprintFunctionTool.cpp`
- **Description**: Opens a Blueprint function in the editor using its GUID. The function GUID can be obtained from `create-blueprint-function` or `list-blueprint-functions` tools.
- **Parameters**:
  - `functionGuid` (string, required): The GUID of the function to open.
  - `blueprintPath` (string, optional): Asset path of the Blueprint. If not provided, searches in the focused Blueprint first.
  - `centerView` (boolean, optional, default: true): If true, centers the graph view on the function entry node.
  - `selectNodes` (boolean, optional, default: true): If true, selects all nodes in the function.
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `success` (boolean): True if the function was opened successfully.
  - `functionName` (string): Display name of the opened function.
  - `functionGuid` (string): GUID of the opened function.
  - `blueprintName` (string): Name of the Blueprint.
  - `blueprintPath` (string): Asset path of the Blueprint.
  - `graphName` (string): Internal name of the function graph.
  - `editorState` (string): Indicates the state, e.g., "opened".

### delete-blueprint-function
- **Location**: `Implementations/N2CMcpDeleteBlueprintFunctionTool.cpp`
- **Description**: Deletes a specific Blueprint function using its GUID. Supports reference detection and forced deletion.
- **Parameters**:
  - `functionGuid` (string, required): The GUID of the function to delete.
  - `blueprintPath` (string, optional): Asset path of the Blueprint. If not provided, uses the currently focused Blueprint.
  - `force` (boolean, optional, default: false): If true, bypasses confirmation checks and forces deletion even if the function has references (references will be removed).
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `success` (boolean): True if the function was deleted successfully.
  - `deletedFunction` (object): Contains `name` and `guid` of the deleted function.
  - `referencesRemoved` (array of objects): Each object details a removed reference, containing `graphName`, `nodeId`, and `nodeTitle`.
  - `blueprintPath` (string): Asset path of the Blueprint.
  - `transactionId` (string): GUID of the undo transaction.

### create-variable
- **Location**: `Implementations/N2CMcpCreateVariableTool.cpp`
- **Description**: Creates a new member variable in the active Blueprint. For map variables: 'typeIdentifier' specifies the map's VALUE type, and 'mapKeyTypeIdentifier' specifies the map's KEY type.
- **Parameters**:
  - `variableName` (string, required): Name for the new variable.
  - `typeIdentifier` (string, required): Type identifier for the variable. For non-container types, this is the variable's type (e.g., 'bool', 'FVector', '/Script/Engine.Actor'). For 'array' or 'set' containers, this is the element type. For 'map' containers, this specifies the map's VALUE type; the KEY type is specified by 'mapKeyTypeIdentifier'.
  - `blueprintPath` (string, optional): Asset path of the Blueprint. If not provided, uses the focused Blueprint.
  - `defaultValue` (string, optional, default: ""): Optional default value for the variable.
  - `category` (string, optional, default: "Default"): Category to organize the variable in.
  - `isInstanceEditable` (boolean, optional, default: true): Whether the variable can be edited per-instance.
  - `isBlueprintReadOnly` (boolean, optional, default: false): Whether the variable is read-only in Blueprint graphs.
  - `tooltip` (string, optional, default: ""): Tooltip description for the variable.
  - `replicationCondition` (string, optional, enum: `"none"`, `"replicated"`, `"repnotify"`, default: `"none"`): Network replication setting.
  - `containerType` (string, optional, enum: `"none"`, `"array"`, `"set"`, `"map"`, default: `"none"`): Container type for the variable.
  - `mapKeyTypeIdentifier` (string, optional): For 'map' containerType, this specifies the map's KEY type identifier (e.g., 'Name', 'int32', '/Script/CoreUObject.Guid'). Required if containerType is 'map'.
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `success` (boolean): True if the variable was created successfully.
  - `variableName` (string): The requested variable name.
  - `actualName` (string): The actual unique name assigned to the variable.
  - `blueprintName` (string): Name of the Blueprint where the variable was created.
  - `typeInfo` (object): Detailed information about the resolved variable type, including `category`, `className`/`structName`/`enumName`/`typeName`, and `path`. For maps, this describes the value type; key type info is part of the map container description.
  - `containerType` (string): The container type used (e.g., "none", "array", "set", "map").
  - `message` (string): A success or error message.

### search-variable-types
- **Location**: `Implementations/N2CMcpSearchVariableTypesTool.cpp`
- **Description**: Searches for available variable types (primitives, classes, structs, enums) by name and returns matches with unique type identifiers.
- **Parameters**:
  - `searchTerm` (string, required): The text query to search for type names.
  - `category` (string, optional, enum: `"all"`, `"primitive"`, `"class"`, `"struct"`, `"enum"`, default: `"all"`): Filter results by type category.
  - `includeEngineTypes` (boolean, optional, default: true): Include engine-provided types in results.
  - `maxResults` (integer, optional, default: 50, min: 1, max: 200): Maximum number of results to return.
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `types` (array of objects): Each object represents a type and includes `typeName`, `typeIdentifier`, `category`, `description`, `icon` (optional), `parentClass` (for classes), `isAbstract` (for classes), and `values` (for enums).
  - `totalMatches` (integer): The number of types found matching the criteria.

### create-local-variable
- **Location**: `Implementations/N2CMcpCreateLocalVariableTool.cpp`
- **Description**: Creates a new local variable in the currently focused Blueprint function. For map variables: 'typeIdentifier' specifies the map's VALUE type, and 'mapKeyTypeIdentifier' specifies the map's KEY type.
- **Parameters**:
  - `variableName` (string, required): Name for the new local variable.
  - `typeIdentifier` (string, required): Type identifier for the variable's value. For non-container types, this is the variable's type (e.g., 'bool', 'FVector', '/Script/Engine.Actor'). For 'array' or 'set' containers, this is the element type. For 'map' containers, this specifies the map's VALUE type; the KEY type is specified by 'mapKeyTypeIdentifier'.
  - `defaultValue` (string, optional, default: ""): Optional default value for the variable.
  - `tooltip` (string, optional, default: ""): Tooltip description for the variable.
  - `containerType` (string, optional, enum: `"none"`, `"array"`, `"set"`, `"map"`, default: `"none"`): Container type for the variable.
  - `mapKeyTypeIdentifier` (string, optional): For 'map' containerType, this specifies the map's KEY type identifier (e.g., 'Name', 'int32'). Required if containerType is 'map'.
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `success` (boolean): True if the local variable was created successfully.
  - `variableName` (string): The requested variable name.
  - `actualName` (string): The actual unique name assigned to the local variable.
  - `typeInfo` (object): Detailed information about the resolved variable type. For maps, this describes the value type.
  - `containerType` (string): The container type used (e.g., "none", "array", "set", "map").
  - `note` (string, optional): A note, especially for map variables, regarding Blueprint limitations.
  - `functionName` (string): Name of the function where the local variable was created.
  - `blueprintName` (string): Name of the Blueprint owning the function.
  - `message` (string): A success or error message.

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

### get-available-translation-targets
- **Location**: `Implementations/N2CMcpGetAvailableTranslationTargetsTool.cpp`
- **Description**: Returns the list of programming languages that NodeToCode can translate Blueprints into, including metadata about each language
- **Parameters**: None
- **Requires Game Thread**: No (only accesses static enum data and settings)
- **Returns**: Object containing:
  - `languages`: Array of language objects, each containing:
    - `id`: Language identifier (e.g., "cpp", "python")
    - `displayName`: Human-readable name (e.g., "C++", "Python")
    - `description`: Description of the language variant/features
    - `fileExtensions`: Array of file extensions (e.g., [".h", ".cpp"])
    - `category`: Language category ("compiled", "scripted", or "documentation")
    - `features`: Additional features or notes about the language
    - `syntaxHighlightingSupported`: Whether syntax highlighting is available
  - `defaultLanguage`: The default language (always "cpp")
  - `currentLanguage`: Currently selected language in settings
  - `languageCount`: Total number of available languages

### get-available-llm-providers
- **Location**: `Implementations/N2CMcpGetAvailableLLMProvidersTool.cpp`
- **Description**: Returns the list of configured LLM providers available for Blueprint translation. Checks which providers have valid API keys set and includes local providers
- **Parameters**: None
- **Requires Game Thread**: Yes (accessing settings and user secrets)
- **Returns**: Object containing:
  - `providers`: Array of provider objects, each containing:
    - `id`: Provider identifier (e.g., "openai", "anthropic", "ollama")
    - `displayName`: Human-readable name (e.g., "OpenAI", "Ollama (Local)")
    - `configured`: Whether the provider is configured (always true in results)
    - `isLocal`: Whether this is a local provider (Ollama, LM Studio)
    - `currentModel`: Currently selected model for this provider
    - `availableModels` (cloud providers only): Array of available models with:
      - `id`: Model identifier
      - `name`: Model display name
      - `supportsSystemPrompts` (OpenAI only): Whether the model supports system prompts
    - `endpoint` (local providers only): API endpoint URL
    - `supportsStructuredOutput`: Whether the provider supports structured JSON output
    - `supportsSystemPrompts`: Whether the provider/model supports system prompts
  - `currentProvider`: Currently selected provider in settings
  - `configuredProviderCount`: Number of providers that are configured and available

### get-translation-output-directory
- **Location**: `Implementations/N2CMcpGetTranslationOutputDirectoryTool.cpp`
- **Description**: Returns the translation output directory configuration from NodeToCode settings. Shows whether a custom directory is set or if the default location is being used
- **Parameters**: None
- **Requires Game Thread**: Yes (accessing UObject settings)
- **Returns**: Object containing:
  - `isCustomDirectorySet`: Boolean indicating if a custom directory is configured
  - `customDirectory` (if custom is set): The configured custom directory path
  - `defaultDirectory` (if using default): The default directory path (ProjectSaved/NodeToCode/Translations)
  - `absolutePath`: The absolute path to the output directory (resolved from relative paths)
  - `directoryExists`: Boolean indicating if the directory currently exists
  - `status`: Human-readable status message about the directory configuration
  - `projectDirectory`: Path to the project file for context
  - `projectSavedDirectory`: Path to the project's Saved directory
  - `usage`: Object containing:
    - `description`: Explanation of what this directory is used for
    - `structure`: Format of subdirectories created for each translation
    - `autoCreateIfMissing`: Whether the directory is created automatically if missing

### read-path
- **Location**: `Implementations/N2CMcpReadPathTool.cpp`
- **Description**: Lists all files and folders in a directory within the Unreal Engine project. Enforces security boundaries to prevent directory traversal outside the project
- **Parameters**:
  - `relativePath` (string, required): Relative path within the project directory to list. **IMPORTANT**: Use empty string `""` for project root, NOT `"."` or `"/"`. Examples: `""` for root, `"Config"` for Config folder, `"Content/Blueprints"` for nested paths
- **Requires Game Thread**: Yes (file system operations)
- **Returns**: Object containing:
  - `success`: Boolean indicating if the operation succeeded
  - `path`: The relative path that was requested
  - `absolutePath`: The absolute path that was listed
  - `fileCount`: Number of files found
  - `directoryCount`: Number of directories found
  - `files`: Array of file names (not full paths)
  - `directories`: Array of directory names (not full paths)
- **Security Features**:
  - Path jail enforcement - prevents access outside project directory
  - Collapses relative paths (.. and .) before validation
  - No symlink following (UE default behavior)
  - Read-only operations only
- **Error Cases**:
  - Returns error if path traversal is attempted
  - Returns error if directory does not exist
  - Returns error if relativePath parameter is missing
- **Implementation Notes**:
  - Uses IPlatformFile::IterateDirectory with a custom visitor pattern for reliable directory traversal
  - Alternative approaches like IFileManager::FindFiles may not work correctly for directory listing in all UE5 contexts
  - Changes to this tool require editor restart to take effect due to MCP server caching

### read-file
- **Location**: `Implementations/N2CMcpReadFileTool.cpp`
- **Description**: Reads the contents of a file within the Unreal Engine project. Enforces security boundaries to prevent directory traversal outside the project. Supports text files up to 500KB in size. Binary files like .uasset and .umap are not supported
- **Parameters**:
  - `relativePath` (string, required): Relative path to the file within the project directory. **IMPORTANT**: Use empty string `""` for project root, NOT `"."` or `"/"`. Examples: `"README.md"` for root file, `"Config/DefaultEngine.ini"` for Config folder file
- **Requires Game Thread**: Yes (file system operations)
- **Returns**: Object containing:
  - `success`: Boolean indicating if the operation succeeded
  - `path`: The relative path that was requested
  - `absolutePath`: The absolute path to the file
  - `content`: The file content as string
  - `size`: File size in bytes
  - `contentType`: MIME type based on file extension (e.g., "text/plain", "application/json")
  - `extension`: File extension without dot
  - `lastModified`: File modification timestamp
- **Security Features**:
  - Same path jail enforcement as read-path tool
  - File size limit (500KB) to prevent memory exhaustion
  - Binary file blocking (.uasset and .umap files explicitly rejected)
  - Text files only
  - Read-only operations
- **Error Cases**:
  - Returns error if path traversal is attempted
  - Returns error if file does not exist
  - Returns error if file is too large (>500KB)
  - Returns error if file is .uasset or .umap
  - Returns error if file read fails
- **Supported Content Types**:
  - Programming languages: C/C++, Python, JavaScript, TypeScript, C#, Swift, Java, Rust, Go, etc.
  - Data formats: JSON, XML, YAML, Markdown, HTML, CSS
  - Configuration: INI, TOML, properties files
  - Unreal Engine: .uproject, .uplugin files
  - Scripts: Shell, Batch, PowerShell
  - Documentation: Text, Log, CSV files

### tag-blueprint-graph
- **Location**: `Implementations/N2CMcpTagBlueprintGraphTool.cpp`
- **Description**: Tags the currently focused Blueprint graph with a name and category for organization and tracking
- **Parameters**:
  - `tag` (string, required): The tag name to apply
  - `category` (string, optional, default: "Default"): The category for organizing tags
  - `description` (string, optional): Additional notes about the tag
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `success`: Boolean indicating if the tagging was successful
  - `taggedGraph`: Object containing:
    - `tag`: The tag name that was applied
    - `category`: The category used
    - `graphGuid`: GUID of the tagged graph
    - `graphName`: Name of the tagged graph
    - `blueprintPath`: Asset path of the owning Blueprint
    - `timestamp`: ISO 8601 timestamp when the tag was applied
  - `message`: Success or error message
- **Features**:
  - Idempotent - tagging the same graph with the same tag/category is safe
  - Persistent - tags are saved to disk and survive editor restarts
  - Timestamped - each tag includes when it was applied
- **Error Cases**:
  - Returns error if no Blueprint graph is currently focused
  - Returns error if the current graph is not valid for tagging (no GUID)

### list-blueprint-tags
- **Location**: `Implementations/N2CMcpListBlueprintTagsTool.cpp`
- **Description**: Lists tags that have been applied to Blueprint graphs. Can filter by graph GUID, tag name, or category
- **Parameters** (all optional):
  - `graphGuid` (string): Filter tags for a specific graph by its GUID
  - `tag` (string): Filter graphs that have a specific tag name
  - `category` (string): Filter tags by category (used with `tag` for more specific filtering)
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `tags`: Array of tag objects, each containing:
    - `tag`: The tag name
    - `category`: The tag category
    - `description`: Tag description
    - `graphGuid`: GUID of the tagged graph
    - `graphName`: Name of the tagged graph
    - `blueprintPath`: Asset path of the owning Blueprint
    - `timestamp`: ISO 8601 timestamp when the tag was applied
  - `count`: Number of tags returned
  - `summary`: Object containing:
    - `uniqueTagNames`: Array of all unique tag names in the system
    - `uniqueCategories`: Array of all unique categories in the system
  - `appliedFilters`: Object showing which filters were applied
- **Filter Behavior**:
  - No parameters: Returns all tags in the system
  - `graphGuid` only: Returns all tags for that specific graph
  - `tag` only: Returns all graphs with that tag name
  - `tag` + `category`: Returns all graphs with that specific tag/category combination
  - `category` only: Returns all tags in that category

### remove-tag-from-graph
- **Location**: `Implementations/N2CMcpRemoveTagFromGraphTool.cpp`
- **Description**: Removes a specific tag from a Blueprint graph by its GUID and tag name. If multiple tags with the same name exist in different categories, all will be removed. The operation is idempotent - removing a non-existent tag is not an error.
- **Parameters**:
  - `graphGuid` (string, required): The GUID of the graph to remove tag from
  - `tag` (string, required): The tag name to remove
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `success`: Always true (idempotent operation)
  - `message`: Description of what happened
  - `removedTag` (if tag was found): Object containing:
    - `tag`: The tag name that was removed
    - `category`: The category of the removed tag (first one if multiple)
    - `graphGuid`: The GUID of the graph
  - `remainingTags`: Number of tags remaining on the graph after removal
- **Idempotency**:
  - Removing a non-existent tag returns success with a message indicating no action was taken
  - This allows safe retry of operations without errors
- **Behavior**:
  - If multiple tags with the same name exist in different categories, all are removed
  - The tool logs how many instances were removed if more than one

### open-content-browser-path
- **Location**: `Implementations/N2CMcpOpenContentBrowserPathTool.cpp`
- **Description**: Opens a specified path in the focused content browser, allowing navigation of the project structure. Can optionally create folders and select specific assets.
- **Parameters**:
  - `path` (string, required): Content browser path to navigate to (e.g., '/Game/Blueprints')
  - `select_item` (string, optional): Specific item to select after navigation
  - `create_if_missing` (boolean, optional, default: false): Whether to create the folder if it doesn't exist
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `success`: Whether the navigation was successful
  - `navigated_path`: The path that was navigated to (normalized)
  - `selected_item`: The item that was selected (if any)
  - `created_folder`: Whether a new folder was created
  - `current_selected_paths`: Array of currently selected paths after navigation
- **Path Validation**:
  - Paths must start with '/' and be in allowed directories
  - Allowed roots: /Game, /Engine, /EnginePresets, /Paper2D, /NodeToCode, /Plugins
  - No directory traversal allowed (../ patterns are blocked)
- **Error Cases**:
  - Invalid path format
  - Path outside allowed directories
  - Path doesn't exist (when create_if_missing is false)
  - Failed to create folder (when create_if_missing is true)

### read-content-browser-path
- **Location**: `Implementations/N2CMcpReadContentBrowserPathTool.cpp`
- **Description**: Returns blueprint assets and folders at the specified path in the content browser. Supports pagination, filtering by type and name.
- **Parameters**:
  - `path` (string, required): Content browser path to read (e.g., '/Game/Blueprints')
  - `page` (integer, optional, default: 1): Page number for pagination (1-based)
  - `page_size` (integer, optional, default: 50, min: 1, max: 200): Number of items per page
  - `filter_type` (string, optional, default: "All"): Filter by asset type
    - Enum values: "All", "Blueprint", "Material", "Texture", "StaticMesh", "Folder"
  - `filter_name` (string, optional, default: ""): Filter by name contains (case-insensitive)
  - `sync_browser` (boolean, optional, default: true): Whether to sync the primary content browser to this path
- **Requires Game Thread**: Yes
- **Returns**: Object containing:
  - `items`: Array of content items, each containing:
    - `path`: Full asset path
    - `name`: Display name
    - `type`: Asset type (Blueprint, Material, etc.)
    - `class`: UE class path
    - `is_folder`: Whether this is a folder
  - `total_count`: Total number of items matching filters
  - `page`: Current page number
  - `page_size`: Items per page
  - `has_more`: Whether more pages exist
- **Features**:
  - Pagination for large directories (e.g., Megascans folders)
  - Type-based filtering (Blueprint, Material, Texture, etc.)
  - Case-insensitive name filtering
  - Optional content browser synchronization
- **Error Cases**:
  - Invalid path format
  - Path doesn't exist
  - Path outside allowed directories
  - Invalid pagination parameters

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

### Browsing Project Directories

The `read-path` tool allows browsing directories within the Unreal Engine project safely:

```bash
# List the project root directory
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-path",
      "arguments": {
        "relativePath": ""
      }
    },
    "id": 1
  }'

# List the Config directory
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-path",
      "arguments": {
        "relativePath": "Config"
      }
    },
    "id": 2
  }'

# List translation output directories
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-path",
      "arguments": {
        "relativePath": "Saved/NodeToCode/Translations"
      }
    },
    "id": 3
  }'

# Example response:
# {
#   "jsonrpc": "2.0",
#   "id": 2,
#   "result": {
#     "content": [{
#       "type": "text",
#       "text": "{
#         \"success\": true,
#         \"path\": \"Config\",
#         \"absolutePath\": \"/path/to/project/Config\",
#         \"fileCount\": 5,
#         \"directoryCount\": 0,
#         \"files\": [\"DefaultEditor.ini\", \"DefaultEngine.ini\", \"DefaultGame.ini\", \"DefaultInput.ini\", \"HoloLens/HoloLensEngine.ini\"],
#         \"directories\": []
#       }"
#     }]
#   }
# }

# Attempt directory traversal (will fail)
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-path",
      "arguments": {
        "relativePath": "../../../System"
      }
    },
    "id": 4
  }'
# Response: Error - "Access denied: Path traversal detected"
```

### Reading Files

The `read-file` tool allows reading file contents within the Unreal Engine project safely:

```bash
# Read a project configuration file
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-file",
      "arguments": {
        "relativePath": "Config/DefaultEngine.ini"
      }
    },
    "id": 1
  }'

# Read a plugin file
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-file",
      "arguments": {
        "relativePath": "Plugins/NodeToCode/NodeToCode.uplugin"
      }
    },
    "id": 2
  }'

# Read a translation output file
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-file",
      "arguments": {
        "relativePath": "Saved/NodeToCode/Translations/MyBlueprint_2025-01-01-12.00.00/EventGraph.cpp"
      }
    },
    "id": 3
  }'

# Example response:
# {
#   "jsonrpc": "2.0",
#   "id": 1,
#   "result": {
#     "content": [{
#       "type": "text",
#       "text": "{
#         \"success\": true,
#         \"path\": \"Config/DefaultEngine.ini\",
#         \"absolutePath\": \"/path/to/project/Config/DefaultEngine.ini\",
#         \"content\": \"[/Script/EngineSettings.GameMapsSettings]\\n...\",
#         \"size\": 1234,
#         \"contentType\": \"text/plain\",
#         \"extension\": \"ini\",
#         \"lastModified\": \"2025-01-01 12:00:00\"
#       }"
#     }]
#   }
# }

# Attempt to read file outside project (will fail)
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-file",
      "arguments": {
        "relativePath": "../../../etc/passwd"
      }
    },
    "id": 4
  }'
# Response: Error - "Access denied: Path traversal detected"
```

### Tagging and Organizing Blueprint Graphs

The `tag-blueprint-graph`, `list-blueprint-tags`, and `remove-tag-from-graph` tools work together to organize and track Blueprint development:

```bash
# Tag the currently focused Blueprint graph
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "tag-blueprint-graph",
      "arguments": {
        "tag": "PlayerMovement",
        "category": "Gameplay",
        "description": "Implements player movement including walk, run, and jump"
      }
    },
    "id": 1
  }'

# List all tags in the system
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "list-blueprint-tags",
      "arguments": {}
    },
    "id": 2
  }'

# Find all graphs tagged with "PlayerMovement"
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "list-blueprint-tags",
      "arguments": {
        "tag": "PlayerMovement"
      }
    },
    "id": 3
  }'

# Get all tags for a specific graph
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "list-blueprint-tags",
      "arguments": {
        "graphGuid": "123e4567-e89b-12d3-a456-426614174000"
      }
    },
    "id": 4
  }'

# Remove a tag from a graph
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "remove-tag-from-graph",
      "arguments": {
        "graphGuid": "123e4567-e89b-12d3-a456-426614174000",
        "tag": "PlayerMovement"
      }
    },
    "id": 5
  }'

# Example response for tag-blueprint-graph:
# {
#   "jsonrpc": "2.0",
#   "id": 1,
#   "result": {
#     "content": [{
#       "type": "text",
#       "text": "{
#         \"success\": true,
#         \"taggedGraph\": {
#           \"tag\": \"PlayerMovement\",
#           \"category\": \"Gameplay\",
#           \"graphGuid\": \"123e4567-e89b-12d3-a456-426614174000\",
#           \"graphName\": \"EventGraph\",
#           \"blueprintPath\": \"/Game/Blueprints/BP_ThirdPersonCharacter\",
#           \"timestamp\": \"2025-01-20T10:30:00Z\"
#         },
#         \"message\": \"Successfully tagged EventGraph with 'PlayerMovement'\"
#       }"
#     }]
#   }
# }

# Example response for remove-tag-from-graph:
# {
#   "jsonrpc": "2.0",
#   "id": 5,
#   "result": {
#     "content": [{
#       "type": "text",
#       "text": "{
#         \"success\": true,
#         \"message\": \"Tag 'PlayerMovement' removed from graph\",
#         \"removedTag\": {
#           \"tag\": \"PlayerMovement\",
#           \"category\": \"Gameplay\",
#           \"graphGuid\": \"123e4567-e89b-12d3-a456-426614174000\"
#         },
#         \"remainingTags\": 2
#       }"
#     }]
#   }
# }
```

### Navigating Content Browser

The `open-content-browser-path` tool allows programmatic navigation of the content browser:

```bash
# Navigate to a specific folder
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
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

# Navigate and create folder if missing
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
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

# Navigate and select a specific asset
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "open-content-browser-path",
      "arguments": {
        "path": "/Game/Blueprints",
        "select_item": "BP_PlayerCharacter"
      }
    },
    "id": 3
  }'

# Example success response:
# {
#   "jsonrpc": "2.0",
#   "id": 2,
#   "result": {
#     "content": [{
#       "type": "text",
#       "text": "{
#         \"success\": true,
#         \"navigated_path\": \"/Game/AI/BehaviorTrees\",
#         \"selected_item\": \"\",
#         \"created_folder\": true,
#         \"current_selected_paths\": [\"/Game/AI/BehaviorTrees\"]
#       }"
#     }]
#   }
# }

# Example error for invalid path:
# {
#   "jsonrpc": "2.0",
#   "id": 1,
#   "result": {
#     "error": {
#       "message": "Invalid path: Path '../../../System' is not in an allowed content directory"
#     }
#   }
# }
```

### Browsing Content Browser Assets

The `read-content-browser-path` tool allows browsing and filtering assets in the content browser:

```bash
# List all items in a folder
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-content-browser-path",
      "arguments": {
        "path": "/Game/Blueprints"
      }
    },
    "id": 1
  }'

# Filter by asset type
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-content-browser-path",
      "arguments": {
        "path": "/Game/Materials",
        "filter_type": "Material",
        "page_size": 20
      }
    },
    "id": 2
  }'

# Search by name and paginate
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "read-content-browser-path",
      "arguments": {
        "path": "/Game/Blueprints",
        "filter_name": "Player",
        "page": 2,
        "page_size": 10,
        "sync_browser": false
      }
    },
    "id": 3
  }'

# Example response:
# {
#   "jsonrpc": "2.0",
#   "id": 1,
#   "result": {
#     "content": [{
#       "type": "text",
#       "text": "{
#         \"items\": [
#           {
#             \"path\": \"/Game/Blueprints/BP_PlayerCharacter\",
#             \"name\": \"BP_PlayerCharacter\",
#             \"type\": \"Blueprint\",
#             \"class\": \"/Script/Engine.Blueprint\",
#             \"is_folder\": false
#           },
#           {
#             \"path\": \"/Game/Blueprints/Components\",
#             \"name\": \"Components\",
#             \"type\": \"Folder\",
#             \"class\": \"Folder\",
#             \"is_folder\": true
#           }
#         ],
#         \"total_count\": 15,
#         \"page\": 1,
#         \"page_size\": 50,
#         \"has_more\": false
#       }"
#     }]
#   }
# }
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
