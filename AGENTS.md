# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# NodeToCode Unreal Engine 5 Plugin: Development Guide

# CRITICAL - START

## @Development/Organization/notion-overview.md

This document acts as the central overview of the Protospatial Notion workspace which will be the ever-evolving planning/organization/documentation area for both you and myself to make sure that we're always keeping track of tasks, objectives, and goals.
Always refer to the notion overview document at the beginning of a conversation so that you understand its structure, purpose, and how to navigate Notion workspace using the notionApi tool.

## Understanding NodeToCode's MCP Server Tools

@Source/Private/MCP/Tools/README.md

## Unreal Engine Source Path

G:\UE\UE_5.4\Engine\Source : This is where source files for Unreal Engine 5 can be found and searched through for research purposes. NEVER try to modify files here.

## TESTING COMPILATION

When testing code changes, please run build.sh (if on Mac) or build.ps1 (if on Windows) found in the root of the NodeToCode plugin folder (./build.sh or .\build.ps1). This will build the project and output timestamped build logs in the BuildLogs folder.

# CRITICAL - END



## 1. Overview

NodeToCode is an Unreal Engine 5 plugin for translating Blueprints into various programming languages (C++, Python, JavaScript, C#, Swift, Pseudocode) using Large Language Models (LLMs). It integrates into the Unreal Editor, providing tools to collect Blueprint nodes, manage LLM interactions, and display generated code with syntax highlighting.

**Key Features:**
*   **Blueprint-to-Code Translation:** Converts selected Blueprint graphs/entire Blueprints.
*   **Multi-LLM Support:** Integrates with OpenAI, Anthropic, Google Gemini, DeepSeek, local Ollama instances, and LM Studio.
*   **Editor Integration:** Adds a "Node to Code" toolbar button in the Blueprint Editor for translation, JSON export, and a dedicated plugin window.
*   **Interactive Code Editor:** Displays generated code with syntax highlighting and theme customization.
*   **Configuration:** Plugin settings for LLM provider selection, API keys (stored securely), target language, and logging.
*   **Reasoning Model Support:** Prepended model commands for controlling reasoning models and feature toggles.

## 2. Architecture

### 2.1. Plugin Module & Core Setup

*   **`NodeToCode.uplugin`**: Descriptor file (version, author, module definition: "NodeToCode" Editor module, PostEngineInit loading, Win64/Mac platforms).
*   **`Source/NodeToCode.Build.cs`**: Build script defining dependencies (Core, UnrealEd, BlueprintGraph, HTTP, Slate, UMG, etc.).
*   **`Source/Public/NodeToCode.h` & `Source/Private/NodeToCode.cpp`**:
    *   `FNodeToCodeModule`: Main plugin class.
        *   `StartupModule`: Initializes logging, HTTP timeouts (via `UHttpTimeoutConfig` for `DefaultEngine.ini`), user secrets, styles (`N2CStyle`, `FN2CCodeEditorStyle`), editor integration (`FN2CEditorIntegration`), and widget factories.
        *   `ShutdownModule`: Cleans up resources.
    *   `UHttpTimeoutConfig`: Helper to adjust global HTTP timeouts for LLM requests.

### 2.2. Core Translation Pipeline

Handles transforming Blueprint nodes into a structured, LLM-processable format.

*   **`Source/Core/N2CNodeCollector.h/.cpp` (`FN2CNodeCollector`)**: Singleton that gathers `UK2Node` instances and pin details from `UEdGraph`.
*   **`Source/Core/N2CNodeTranslator.h/.cpp` (`FN2CNodeTranslator`)**: Singleton converting collected `UK2Node`s into `FN2CBlueprint`. Manages ID generation, uses `FN2CNodeProcessorFactory` for node-specific property extraction, determines types via `FN2CNodeTypeRegistry`, handles nested graphs, and processes related structs/enums.
*   **`Source/Models/N2CBlueprint.h/.cpp`**: Defines primary data structures:
    *   `FN2CBlueprint`: Top-level container (version, metadata, graphs, structs, enums).
    *   `FN2CMetadata`: Blueprint name, type (`EN2CBlueprintType`), class.
    *   `FN2CGraph`: Single graph with nodes and `FN2CFlows`.
    *   `FN2CFlows`: Execution and data pin connections.
    *   `FN2CStruct`, `FN2CStructMember`, `FN2CEnum`, `FN2CEnumValue`: User-defined types.
*   **`Source/Models/N2CNode.h/.cpp`**: Defines `FN2CNodeDefinition` (node ID, `EN2CNodeType`, name, pins, flags) and `EN2CNodeType` (enum for various `UK2Node` kinds).
*   **`Source/Models/N2CPin.h/.cpp`**: Defines `FN2CPinDefinition` (pin ID, name, `EN2CPinType`, subtype, default value, flags) and `EN2CPinType` (enum for pin kinds). Includes compatibility checks.
*   **`Source/Core/N2CSerializer.h/.cpp` (`FN2CSerializer`)**: Static utility for serializing `FN2CBlueprint` to JSON and deserializing. Supports pretty-printing.
*   **`Source/Models/N2CTranslation.h`**: Defines LLM output structures:
    *   `FN2CTranslationResponse`: Contains `FN2CGraphTranslation[]` and `FN2CTranslationUsage`.
    *   `FN2CGraphTranslation`: Translated code for one graph.
    *   `FN2CGeneratedCode`: Declaration, implementation, and notes.
    *   `FN2CTranslationUsage`: Input/output token counts.

### 2.3. LLM Integration & Providers

Manages interactions with LLMs.

*   **`Source/LLM/N2CLLMModule.h/.cpp` (`UN2CLLMModule`)**: Singleton orchestrating LLM requests. Initializes based on `UN2CSettings`, manages the active `IN2CLLMService`, processes JSON input, uses `UN2CSystemPromptManager` for prompts, and saves translations.
*   **`Source/LLM/IN2CLLMService.h`**: Interface for LLM service providers (init, send request, get config, etc.).
*   **`Source/LLM/N2CBaseLLMService.h/.cpp` (`UN2CBaseLLMService`)**: Abstract base class for LLM services, providing common init and request flow.
*   **Provider Implementations (`Source/LLM/Providers/`)**:
    *   Service classes (e.g., `UN2COpenAIService`, `UN2CAnthropicService`, `UN2CGeminiService`, `UN2CDeepSeekService`, `UN2COllamaService`, `UN2CLMStudioService`) inheriting `UN2CBaseLLMService`.
    *   Implement provider-specific API endpoint, auth, headers, and payload formatting (using `UN2CLLMPayloadBuilder`).
*   **`Source/LLM/N2CLLMProviderRegistry.h/.cpp` (`UN2CLLMProviderRegistry`)**: Singleton factory for creating LLM service instances based on `EN2CLLMProvider`.
*   **`Source/LLM/N2CLLMPayloadBuilder.h/.cpp` (`UN2CLLMPayloadBuilder`)**: Utility for building provider-specific JSON request payloads. Includes `GetN2CResponseSchema()` for the expected LLM output format.
*   **`Source/LLM/N2CResponseParserBase.h/.cpp` (`UN2CResponseParserBase`)**: Abstract base for LLM response parsers. Handles common N2C JSON structure.
*   **Parser Implementations (`Source/LLM/Providers/`)**:
    *   Parser classes (e.g., `UN2COpenAIResponseParser`, `UN2CLMStudioResponseParser`) inheriting `UN2CResponseParserBase`.
    *   Handle provider-specific JSON response structures, error formats, and extract core message content and token usage.
*   **`Source/LLM/N2CHttpHandlerBase.h/.cpp` & `Source/LLM/N2CHttpHandler.h`**: `UN2CHttpHandlerBase` (abstract) and `UN2CHttpHandler` (conc rete) for sending HTTP POST requests to LLM APIs.
*   **`Source/LLM/N2CSystemPromptManager.h/.cpp` (`UN2CSystemPromptManager`)**: Loads and manages system prompts from `.md` files in `Content/Prompting/`. Handles language-specific prompts and prepends reference source files from settings.
*   **`Content/Prompting/CodeGen_[Language].md`**: Markdown files with detailed LLM instructions for each target language.
*   **`Source/LLM/N2CLLMTypes.h`**: Defines `EN2CLLMProvider`, `EN2CSystemStatus`, `FN2CLLMConfig`, and LLM-related delegates.
*   **`Source/LLM/N2CLLMModels.h/.cpp` & `Source/LLM/N2CLLMPricing.h`**: Enums for provider-specific models (`EN2COpenAIModel`, etc.), `FN2CLLMModel Utils` for model string IDs & pricing, and `FN2C[Provider]Pricing` structs.
*   **`Source/LLM/N2COllamaConfig.h`**: `FN2COllamaConfig` USTRUCT for Ollama-specific settings.

### 2.4. Editor Integration & UI Shell

Connects plugin logic to the Unreal Editor UI.

*   **`Source/Core/N2CEditorIntegration.h/.cpp` (`FN2CEditorIntegration`)**: Singleton for Blueprint Editor integration. Registers toolbar extensions, maps UI commands (`FN2CToolbarCommand`) to handlers (e.g., `ExecuteCollectNodesForEditor`). Tracks `LastFocusedGraph`.
*   **`Source/Core/N2CToolbarCommand.h/.cpp` (`FN2CToolbarCommand`)**: `TCommands` subclass defining toolbar UI actions (OpenWindow, CollectNodes, CopyJson).
*   **`Source/Core/N2CEditorWindow.h/.cpp` (`SN2CEditorWindow`)**: Slate widget for the main plugin UI. Registers a nomad tab spawner, embedding `NodeToCodeUI.uasset`.
*   **`Content/UI/NodeToCodeUI.uasset`**: Main Editor Utility Widget Blueprint for the plugin's UI.
*   **`Content/UI/Components/`**: `.uasset` EUW components for building `NodeToCodeUI.uasset`.
*   **`Source/Models/N2CStyle.h/.cpp` (`N2CStyle`)**: Manages plugin's global Slate style (toolbar icon).
*   **`Source/Core/N2CWidgetContainer.h/.cpp` (`UN2CWidgetContainer`)**: Persistent UObject outer for `NodeToCodeUI` to maintain state.

### 2.5. Code Editor UI and Syntax Highlighting

Provides the in-plugin code viewing component.

*   **`Source/Code Editor/Widgets/SN2CCodeEditor.h/.cpp` (`SN2CCodeEditor`)**: Core Slate widget for code display/editing, using `SMultiLineEditableText` and `FN2CRichTextSyntaxHighlighter`. Manages language, theme, font size.
*   **`Source/Code Editor/Widgets/N2CCodeEditorWidget.h/.cpp` (`UN2CCodeEditorWidget`)**: UMG wrapper for `SN2CCodeEditor`, for use in EUWs.
*   **`Source/Code Editor/Syntax/N2CSyntaxDefinition.h`**: Abstract base for language-specific syntax rules (keywords, operators, comments).
*   **Language-Specific Syntax Definitions (`Source/Code Editor/Syntax/`)**: Implementations like `FN2CCPPSyntaxDefinition` for C++, Python, JS, C#, Swift, Pseudocode.
*   **`Source/Code Editor/Syntax/N2CSyntaxDefinitionFactory.h/.cpp` (`FN2CSyntaxDefinitionFactory`)**: Singleton factory for `FN2CSyntaxDefinition` instances based on `EN2CCodeLanguage`.
*   **`Source/Code Editor/Syntax/N2CRichTextSyntaxHighlighter.h/.cpp` (`FN2CRichTextSyntaxHighlighter`)**: Performs syntax highlighting using a `FN2CSyntaxDefinition` and `FSyntaxTokenizer`. Applies `FTextBlockStyle` based on tokens.
*   **`Source/Code Editor/Syntax/N2CWhiteSpaceRun.h/.cpp` (`FN2CWhiteSpaceRun`)**: Custom `FSlateTextRun` for handling tab rendering.
*   **`Source/Code Editor/Models/N2CCodeEditorStyle.h/.cpp` (`FN2CCodeEditorStyle`)**: Manages global `FSlateStyleSet` for code editor syntax styles, sourced from `UN2CSettings`.
*   **`Source/Code Editor/Models/N2CCodeLanguage.h`**: `EN2CCodeLanguage` enum.

### 2.6. Node Processing Strategy

Handles diverse Blueprint node types using a strategy pattern.

*   **`Source/Utils/Processors/N2CNodeProcessor.h` (`IN2CNodeProcessor`)**: Interface for node processors.
*   **`Source/Utils/Processors/N2CBaseNodeProcessor.h/.cpp` (`FN2CBaseNodeProcessor`)**: Base implementation, handles common properties.
*   **`Source/Utils/Processors/N2CNodeProcessorFactory.h/.cpp` (`FN2CNodeProcessorFactory`)**: Singleton factory mapping `EN2CNodeType` to specific processor implementations.
*   **Specific Node Processors (`Source/Utils/Processors/`)**: Derived classes like `FN2CFunctionCallProcessor`, `FN2CEventProcessor`, `FN2CVariableProcessor`, etc., overriding `ExtractNodeProperties` for specific `UK2Node` types.

### 2.7. Utilities & Settings

General helpers and configuration.

*   **`Source/Core/N2CSettings.h/.cpp` (`UN2CSettings`)**: `UDeveloperSettings` class for all plugin configurations (LLM provider, API keys UI, model selections, Ollama config, LM Studio config, pricing, target language, logging severity, code editor themes `FN2CCodeEditorThemes`, reference source files `ReferenceSourceFilePaths`, custom translation output path `CustomTranslationOutputDirectory`).
*   **`Source/Core/N2CUserSecrets.h/.cpp` (`UN2CUserSecrets`)**: UObject for secure local storage of API keys in `secrets.json`.
*   **`Source/Utils/N2CLogger.h/.cpp` (`FN2CLogger`)**: Singleton for centralized logging, integrates with `UE_LOG`.
*   **`Source/Models/N2CLogging.h`**: Defines `EN2CLogSeverity`, `EN2CErrorCategory`, `FN2CError`, and `LogNodeToCode` category.
*   **`Source/Utils/N2CNodeTypeRegistry.h/.cpp` (`FN2CNodeTypeRegistry`)**: Singleton mapping `UK2Node` classes to `EN2CNodeType`.
*   **Validation Utilities (`Source/Utils/Validators/`)**: `FN2CBlueprintValidator`, `FN2CNodeValidator`, `FN2CPinValidator` for data integrity checks.
*   **`Source/Utils/N2CPinTypeCompatibility.h/.cpp` (`FN2CPinTypeCompatibility`)**: Static utility for checking pin connection compatibility.

### 2.8. Content Files

*   **`.github/`**: Funding and issue templates.
*   **`.gitignore`**: Standard Git ignore.
*   **`Content/Prompting/`**: `.md` files for LLM system prompts (e.g., `CodeGen_CPP.md`).
*   **`Content/UI/`**:
    *   **`Components/`**: `.uasset` EUW components for UI building (chat, code editor, browsers, etc.). Excludes backer/auth components.
    *   **`NodeToCodeUI.uasset`**: Main plugin EUW.
    *   **`Textures/Icons/`**: UI icon textures.
    *   **`Theming/`**: General UI theming assets.
*   **`LICENSE`**, **`README.md`**.
*   **`Resources/`**: Plugin/toolbar icons (`Icon128.png`, `button_icon.png`).
*   **`assets/`**: Documentation/promotional images.

### 2.9. Recent Major Additions

#### LM Studio Provider (Local LLM Support)

LM Studio integration provides local LLM capabilities with structured output support:

*   **`Source/LLM/Providers/N2CLMStudioService.h/.cpp`**: LM Studio service implementation using OpenAI-compatible API format.
    *   Default endpoint: `http://localhost:1234/v1/chat/completions`
    *   Structured output support using `response_format` with JSON schema
    *   No temperature parameter sent (allows LM Studio UI control)
    *   Bearer token authentication with "lm-studio" default API key
*   **`Source/LLM/Providers/N2CLMStudioResponseParser.h/.cpp`**: OpenAI-compatible response parser with LM Studio-specific stats logging.
    *   Extracts performance metrics (`tokens_per_second`, `time_to_first_token`)
    *   Logs model information (`arch`, `quant`, `context_length`)
    *   Standard token usage tracking
*   **Settings Integration**:
    *   `LMStudioModel`: Dynamic string-based model name
    *   `LMStudioEndpoint`: Configurable server endpoint
    *   `LMStudioPrependedModelCommand`: Command prefix for reasoning models
*   **Payload Builder Integration**: `ConfigureForLMStudio()` method with structured output and temperature exclusion.

#### Prepended Model Commands

Support for model-specific command prefixes for both local providers:

*   **Ollama Configuration** (`N2COllamaConfig.h`):
    *   `PrependedModelCommand`: Text prepended to user messages
    *   Use cases: `/no_think`, `/think`, reasoning control
*   **LM Studio Configuration** (`N2CSettings.h`):
    *   `LMStudioPrependedModelCommand`: Similar functionality for LM Studio
*   **Implementation**: Commands are prepended with `\n\n` separator before N2C JSON payload in both services.

#### Latest Model Support Updates

Recent model additions across providers:

*   **OpenAI Models**:
    *   GPT-4.1 (`gpt-4.1`)
    *   o3 (`o3`)
    *   o3-mini (`o3-mini`)
    *   o4-mini (`o4-mini`) - Set as default for cost-effectiveness
*   **Anthropic Models**:
    *   Claude 4 Opus (`claude-4-opus-20250514`)
    *   Claude 4 Sonnet (`claude-4-sonnet-20250514`) - Current default
*   **Gemini Models**:
    *   Gemini 2.5 Flash (`gemini-2.5-flash-preview-05-20`) - Free tier model
    *   Updated Gemini 2.5 Pro to latest version

#### Structured Output Enhancements

Comprehensive structured output support across providers:

*   **OpenAI**: `json_schema` type with strict schema validation
*   **LM Studio**: OpenAI-compatible with `strict: true` parameter
*   **Gemini**: `responseMimeType` and `responseSchema` in generation config
*   **DeepSeek**: `json_object` type format
*   **Ollama**: Direct schema in `format` field
*   **Schema Definition**: `GetN2CResponseSchema()` provides consistent N2C translation format

#### Batch Translation System

Batch translation of multiple Blueprint graphs from `FN2CTagInfo` arrays:

*   **`Source/Public/Models/N2CBatchTranslationTypes.h`**: Defines batch types and delegates:
    *   `EN2CBatchItemStatus`: Pending, Processing, Completed, Failed, Skipped
    *   `FN2CBatchTranslationItem`: Internal tracking struct per graph
    *   `FN2CBatchTranslationResult`: Summary with counts, timing, token usage
    *   `FOnBatchItemTranslationComplete(TagInfo, Response, bSuccess, ItemIndex, TotalCount)`
    *   `FOnBatchTranslationComplete(Result)`, `FOnBatchTranslationProgress`

*   **`Source/LLM/N2CBatchTranslationOrchestrator.h/.cpp`**: Singleton orchestrator:
    *   Loads blueprints via `FSoftObjectPath::TryLoad()` without editor focus
    *   Finds graphs by GUID in `UbergraphPages`, `FunctionGraphs`, `MacroGraphs`
    *   Sequential translation reusing `UN2CLLMModule::ProcessN2CJson()`
    *   Outputs to `BatchTranslation_{timestamp}/` with per-graph subfolders and `BatchSummary.json`

*   **`Source/BlueprintLibraries/N2CBatchTranslationBlueprintLibrary.h/.cpp`**: Blueprint API:
    *   `StartBatchTranslation(TArray<FN2CTagInfo>)` - Direct array input
    *   `TranslateGraphsWithTag(Tag, Category)` - All graphs with specific tag
    *   `TranslateGraphsInCategory(Category)` - All graphs in category
    *   `CancelBatchTranslation()`, `GetBatchProgress()`, `GetBatchOrchestrator()`

### 2.10. MCP (Model Context Protocol) Server Integration

The NodeToCode plugin includes a comprehensive MCP server implementation that exposes Blueprint functionality to AI assistants and other MCP clients. The implementation follows the Model Context Protocol specification for seamless integration with tools like Claude for Desktop.

#### MCP Server Architecture

The MCP server is built on HTTP transport with full JSON-RPC 2.0 compliance:

*   **Transport Layer**: HTTP server on port 27000 (configurable via settings)
*   **Protocol**: JSON-RPC 2.0 with proper error handling and batch support
*   **Lifecycle**: Auto-starts with plugin, graceful shutdown on exit
*   **Session Management**: Protocol version negotiation and session tracking

#### Core Server Components (`Source/MCP/Server/`)

*   **`N2CMcpHttpServerManager.h/.cpp`**: Central server management
    *   Singleton pattern for server lifecycle control
    *   HTTP routing: `/mcp` (main), `/mcp/health` (health check)
    *   CORS headers for local development
    *   Client notification channel management
    *   Session protocol version tracking

*   **`N2CMcpHttpRequestHandler.h/.cpp`**: Request processing engine
    *   Full JSON-RPC 2.0 message parsing and validation
    *   Method routing with extensible dispatch system
    *   Batch request processing support
    *   Protocol version negotiation (supports "2025-03-26", "2024-11-05")
    *   Comprehensive error responses with proper codes

*   **`N2CMcpJsonRpcTypes.h/.cpp`**: JSON-RPC type system
    *   `FJsonRpcRequest/Response/Error/Notification` structures
    *   `FJsonRpcUtils` for serialization/deserialization
    *   Standard error codes (-32700 to -32603)
    *   Message type detection and validation

*   **`IN2CMcpNotificationChannel.h`**: Notification transport abstraction
    *   Interface for future WebSocket/SSE support
    *   Enables server-initiated notifications
    *   Session-based channel management

#### Validation Framework (`Source/MCP/Validation/`)

*   **`N2CMcpRequestValidator.h/.cpp`**: Centralized request validation
    *   Method-specific validators (tools/call, resources/read, etc.)
    *   Generic validation helpers for common patterns
    *   Required/optional field validation
    *   Type checking with detailed error messages

#### Progress Tracking System (`Source/MCP/Progress/`)

*   **`N2CMcpProgressTracker.h/.cpp`**: Real-time operation tracking
    *   Thread-safe progress management
    *   Progress notifications via MCP protocol
    *   Percentage-based progress reporting
    *   Integration with notification broadcasting

#### Tool Subsystem (`Source/MCP/Tools/`)

The tool system provides a scalable framework for exposing Blueprint operations:

*   **Core Infrastructure**:
    *   `IN2CMcpTool.h`: Tool interface contract
    *   `N2CMcpToolBase.h/.cpp`: Base class with common functionality
    *   `N2CMcpToolTypes.h/.cpp`: Tool definition and result structures
    *   `N2CMcpToolManager.h/.cpp`: Thread-safe tool registry
    *   `N2CMcpToolRegistry.h/.cpp`: Automatic registration system

*   **Tool Registration**:
    *   `REGISTER_MCP_TOOL` macro for static initialization
    *   Self-contained tools auto-register on plugin load
    *   Game Thread execution support with timeouts
    *   Input schema validation framework

*   **Tool Organization** (`Implementations/`):
    *   **Blueprint/** - Blueprint-related tools
        *   **Analysis/** - Blueprint inspection (get-focused-blueprint, list-blueprint-functions, list-overridable-functions)
        *   **Functions/** - Function management (create/open/delete-blueprint-function)
        *   **Variables/** - Variable creation (create-variable, create-local-variable, search-variable-types)
        *   **Graph/** - Node operations (search-blueprint-nodes, add-bp-node-to-active-graph, connect-pins)
        *   **Organization/** - Tagging system (tag-blueprint-graph, list-blueprint-tags, remove-tag-from-graph)
    *   **Translation/** - Code translation tools (translate-focused-blueprint, get-available-translation-targets, get-available-llm-providers, get-translation-output-directory)
    *   **FileSystem/** - File operations (read-path, read-file)
    *   **ContentBrowser/** - Content browser operations (open-content-browser-path, read-content-browser-path)
    *   **Python/** - Python script management tools
        *   `run-python` - Execute Python code in UE environment
        *   `list-python-scripts` - List scripts by category
        *   `search-python-scripts` - Search by name/description/tags
        *   `get-python-script` - Get full script code and metadata
        *   `get-script-functions` - Get function signatures via AST (token-efficient)
        *   `save-python-script` - Save script to library
        *   `delete-python-script` - Remove script from library

#### Resource System (`Source/MCP/Resources/`)

Resources expose Blueprint data in a structured, queryable format:

*   **Core Infrastructure**:
    *   `N2CMcpResourceTypes.h/.cpp`: Resource definitions and contents
    *   `N2CMcpResourceManager.h/.cpp`: Resource registry with subscription support
    *   `IN2CMcpResource.h`: Resource interface for implementations

*   **Resource Types**:
    *   Static resources: Fixed URIs with direct handlers
    *   Dynamic resources: Template-based URI patterns
    *   Subscription support for real-time updates

*   **Implemented Resources** (`Implementations/`):
    *   `N2CMcpBlueprintResource.h/.cpp`:
        *   `nodetocode://blueprint/current`: Current focused Blueprint
        *   `nodetocode://blueprints/all`: List of open Blueprints
        *   `nodetocode://blueprint/{name}`: Blueprint by name (template)

#### Prompt System (`Source/MCP/Prompts/`)

Prompts provide guided interactions for common Blueprint tasks:

*   **Core Infrastructure**:
    *   `N2CMcpPromptTypes.h`: Prompt structures with type alias for arguments
    *   `N2CMcpPromptManager.h/.cpp`: Prompt registry and execution
    *   `IN2CMcpPrompt.h`: Interface for prompt implementations

*   **Type Safety**:
    *   `FMcpPromptArguments` type alias for `TMap<FString, FString>`
    *   Avoids macro expansion issues with template parameters
    *   Consistent argument passing across the system

*   **Implemented Prompts** (`Implementations/`):
    *   `N2CMcpCodeGenerationPrompt`: Generate code with language/style options
    *   `N2CMcpBlueprintAnalysisPrompt`: Analyze Blueprint structure
    *   `N2CMcpRefactorPrompt`: Suggest refactoring improvements
    *   `N2CMcpPythonScriptingPrompt`: UE Python scripting with Context7 + script reuse enforcement

#### MCP Protocol Implementation

The server implements these MCP methods:

*   **Core Methods**:
    *   `initialize`: Connection setup with capability negotiation
    *   `notifications/initialized`: Connection confirmation
    *   `ping`: Keep-alive testing

*   **Tool Methods**:
    *   `tools/list`: Enumerate available tools with schemas
    *   `tools/call`: Execute tools with arguments

*   **Resource Methods**:
    *   `resources/list`: List available resources and templates
    *   `resources/read`: Read resource contents

*   **Prompt Methods**:
    *   `prompts/list`: List available prompts
    *   `prompts/get`: Get prompt with argument substitution

*   **Advanced Features**:
    *   Batch request processing
    *   Progress notifications
    *   Protocol version negotiation
    *   Session management

#### Editor Integration Updates

*   **`FN2CEditorIntegration` Refactoring**:
    *   Modularized Blueprint collection and translation
    *   Helper methods for MCP tool usage:
        *   `GetFocusedBlueprintAsJson()`: High-level API
        *   `CollectNodesFromGraph()`: Node collection
        *   `TranslateNodesToN2CBlueprint()`: Translation
        *   `SerializeN2CBlueprintToJson()`: Serialization

*   **Plugin Settings** (`N2CSettings.h/.cpp`):
    *   `McpServerPort`: Configurable server port
    *   Exposed in Editor UI for runtime configuration

#### Testing and Development

Example test commands for the MCP server:

```bash
# Basic connectivity
curl -X GET http://localhost:27000/mcp/health

# Protocol initialization
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc": "2.0", "method": "initialize", "params": {"protocolVersion": "2025-03-26", "clientInfo": {"name": "test", "version": "1.0"}}, "id": 1}'

# List tools with schemas
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc": "2.0", "method": "tools/list", "id": 2}'

# Execute get-focused-blueprint tool
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc": "2.0", "method": "tools/call", "params": {"name": "get-focused-blueprint", "arguments": {}}, "id": 3}'

# List resources
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc": "2.0", "method": "resources/list", "id": 4}'

# Read current blueprint resource
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc": "2.0", "method": "resources/read", "params": {"uri": "nodetocode://blueprint/current"}, "id": 5}'

# Get code generation prompt
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc": "2.0", "method": "prompts/get", "params": {"name": "generate-code", "arguments": {"language": "python", "style": "concise"}}, "id": 6}'

# Batch request example
curl -X POST http://localhost:27000/mcp \
  -H "Content-Type: application/json" \
  -d '[{"jsonrpc": "2.0", "method": "ping", "id": 1}, {"jsonrpc": "2.0", "method": "tools/list", "id": 2}]'
```

#### Implementation Documentation

*   **Tool Development Guide**: `Source/Private/MCP/Tools/README.md`
    *   Creating new tools with step-by-step instructions
    *   Helper function reference
    *   Best practices for Game Thread handling
    *   Troubleshooting common issues

*   **MCP Improvement Plan**: `Development/Reference/N2C/n2c-mcp-server-improvements.md`
    *   Detailed implementation phases
    *   Architecture decisions and rationale
    *   Success metrics and validation

#### Related Projects

*   **NodeToCode MCP Bridge**: Python stdio-to-HTTP bridge for MCP clients
    *   Location: `Content/Python/mcp_bridge/nodetocode_bridge.py`
    *   Enables Claude for Desktop and other MCP client integration
    *   Handles protocol translation and session management

### 2.11. MCP Bridge (Python)

The NodeToCode MCP Bridge is a Python script that bridges MCP clients with the NodeToCode plugin's HTTP server:

*   **Location**: `Content/Python/mcp_bridge/nodetocode_bridge.py`
*   **Purpose**: Stdio-to-HTTP bridge for MCP protocol
*   **Key Features**:
    *   Reads JSON-RPC messages from stdin (from MCP clients like Claude)
    *   Forwards to NodeToCode's HTTP MCP server (localhost:27000)
    *   Relays responses back via stdout
    *   Handles session management and error conditions
*   **Usage**: `python nodetocode_bridge.py` or configure in MCP client settings

### 2.12. Python Script Management System

Lean Python scripting framework for LLM-powered UE automation, designed to work with Context7 MCP for API documentation.

#### Architecture
*   **Script Storage**: `Content/Python/scripts/<category>/` - Project-level, version-controlled
*   **Registry**: `Content/Python/scripts/script_registry.json` - Metadata index for fast search
*   **NodeToCode Module**: `Content/Python/nodetocode/` - Importable utilities

#### Key Module Files
*   `scripts.py`: Script CRUD operations (list, search, get, save, delete, get_script_functions)
*   `bridge.py`: Python wrappers for C++ bridge (tagging, LLM provider info)
*   `blueprint.py`: Blueprint operations (get_focused, compile, save)
*   `utils.py`: Common utilities and result formatting

#### Token-Efficient Function Discovery
*   `get_script_functions(name)`: Uses AST parsing to extract function signatures without loading full implementation
*   Returns: function names, parameters, type hints, docstrings, line numbers
*   ~80% token savings vs `get_script()` for discovery phase

#### Python-Only Mode
*   Setting: `bEnablePythonScriptOnlyMode` in Plugin Settings
*   Registers only essential tools: run-python, script management, translation tools
*   Designed for LLMs to write/reuse Python scripts via Context7 API docs

#### python-scripting Prompt
Enforces proper workflow:
1. Search existing scripts first (reuse over rewrite)
2. Use Context7 to lookup `radial-hks/unreal-python-stubhub` for UE Python API
3. Import and compose existing script functions
4. Structure new scripts as reusable modules
5. Save useful scripts to grow the library

### 3. Adding/Updating LLM Providers/Models (Concise Steps)

1.  **Define Core Types (`N2CLLMTypes.h`, `N2CLLMModels.h/.cpp`, `N2CLLMPricing.h`):**
    *   Add to `EN2CLLMProvider` enum (if new provider). Current providers: OpenAI, Anthropic, Gemini, DeepSeek, Ollama, LMStudio.
    *   Define/update model enum (e.g., `EN2CNewProviderModel`). Note: Ollama and LMStudio use dynamic string-based model names instead of enums.
    *   Update `FN2CLLMModelUtils`: Add `Get[ProviderName]ModelValue()`, `Get[ProviderName]Pricing()`, and static pricing map entry.
    *   If new pricing structure, define `FN2CNewProviderPricing`.
2.  **Plugin Settings (`N2CSettings.h/.cpp`):**
    *   Add UPROPERTY for new provider's model enum, API Key UI string, and pricing map (if new).
    *   Update `GetActiveApiKey()`, `GetActiveModel()`, and pricing getters.
    *   In constructor/`InitializePricing()`, set default model and pricing.
    *   In `PostEditChangeProperty()`, handle saving the new API key UI string to `UserSecrets`.
3.  **User Secrets (`N2CUserSecrets.h/.cpp`):**
    *   Add `FString` UPROPERTY for the new provider's API key.
    *   Update `LoadSecrets()` and `SaveSecrets()` to handle the new key.
4.  **Service Implementation (`Source/LLM/Providers/`):**
    *   Create `N2CNewProviderService.h/.cpp` inheriting `UN2CBaseLLMService`.
    *   Implement:
        *   `CreateResponseParser()`: Return new `UN2CNewProviderResponseParser`.
        *   `GetConfiguration()`: Provide endpoint, auth token, system prompt support.
        *   `GetProviderHeaders()`: Define HTTP headers.
        *   `FormatRequestPayload()`: Use `UN2CLLMPayloadBuilder` with `ConfigureForNewProvider()`.
        *   `GetDefaultEndpoint()`.
5.  **Response Parser (`Source/LLM/Providers/`):**
    *   Create `N2CNewProviderResponseParser.h/.cpp` inheriting `UN2CResponseParserBase`.
    *   Implement `ParseLLMResponse()`: Deserialize provider JSON, handle errors, extract core N2C JSON content and token usage, then call `Super::ParseLLMResponse()`.
6.  **Payload Builder (`N2CLLMPayloadBuilder.h/.cpp`):**
    *   Add `ConfigureForNewProvider()` method.
    *   Update setters (`SetTemperature`, `SetMaxTokens`, etc.) with a `case EN2CLLMProvider::NewProvider:` to handle provider-specific payload structures.
7.  **LLM Module Registration (`N2CLLMModule.cpp`):**
    *   In `InitializeProviderRegistry()`, call `Registry->RegisterProvider(EN2CLLMProvider::NewProvider, UN2CNewProviderService::StaticClass());`.
8.  **System Prompts (`Content/Prompting/`, `N2CSystemPromptManager.cpp` - Optional):**
    *   If needed, add new `.md` prompt files and update `LoadPrompts()` in `UN2CSystemPromptManager`.
9.  **Local Provider Configuration (for local LLMs like Ollama/LMStudio):**
    *   For local providers, consider configuration patterns:
        *   **Ollama Pattern**: Separate config struct (`FN2COllamaConfig`) with extensive parameters
        *   **LM Studio Pattern**: Simple settings directly in `UN2CSettings` with minimal configuration
    *   Key considerations for local providers:
        *   No API key authentication (or simple dummy keys)
        *   Custom endpoint configuration
        *   Free pricing (local models)
        *   Dynamic model names (strings vs enums)
        *   Temperature parameter handling (skip for LM Studio to allow UI control)
        *   Support for prepended model commands for reasoning control
10. **Pricing**
    *   Always assume that you are unsure about the pricing of models when implementing a new one unless explicitly told the prices. If you are not told what the prices are, then search for them online via official documentation or other reputable resources.

## Git Submodule Development Workflow

### Setting Up Multi-Version UE Plugin Development with Git Submodules

This workflow allows you to develop the NodeToCode plugin in a single location while testing it across multiple Unreal Engine project versions using git submodules.

#### Initial Setup

1. **Initialize Git in UE Test Projects**
   ```bash
   cd "/Volumes/NVME 1/Projects/NodeToCode/NodeToCodeHostProjects/NodeToCodeHost_5_4"
   git init
   git add .
   git commit -m "Initial project setup"
   ```

2. **Add Plugin as Git Submodule**
   ```bash
   # Using relative path (recommended for same filesystem)
   git submodule add ../../NodeToCode Plugins/NodeToCode
   
   # Or using absolute path
   git submodule add "/Volumes/NVME 1/Projects/NodeToCode/NodeToCode" Plugins/NodeToCode
   ```

3. **Handle File Transport Errors**
   If you encounter "fatal: transport 'file' not allowed":
   ```bash
   # Allow file protocol globally
   git config --global protocol.file.allow always
   
   # Or one-time allow
   git -c protocol.file.allow=always submodule add "/path/to/plugin" Plugins/NodeToCode
   ```

4. **Commit Submodule Addition**
   ```bash
   git add .gitmodules Plugins/NodeToCode
   git commit -m "Add NodeToCode plugin as submodule"
   ```

#### Development Workflow

1. **Update Submodule to Latest Changes**
   ```bash
   # Method 1: Update from parent project
   cd "/Volumes/NVME 1/Projects/NodeToCode/NodeToCodeHostProjects/NodeToCodeHost_5_4"
   git submodule update --remote --merge Plugins/NodeToCode
   
   # Method 2: Pull directly in submodule
   cd Plugins/NodeToCode
   git pull origin main  # or your working branch
   ```

2. **Make Changes in Submodule**
   ```bash
   cd Plugins/NodeToCode
   # Edit files, then commit
   git add .
   git commit -m "Your changes"
   git push origin your-branch
   ```

3. **Update Parent Project Reference**
   ```bash
   cd ../..  # Back to UE project root
   git add Plugins/NodeToCode
   git commit -m "Update NodeToCode plugin to latest"
   ```

4. **Track Specific Branch**
   ```bash
   git config -f .gitmodules submodule.Plugins/NodeToCode.branch your-branch-name
   git submodule update --remote
   ```

#### Working with Local Branches

When developing on a local branch that hasn't been pushed to GitHub:

1. **Use local file paths** instead of GitHub URLs
2. **Changes are immediately reflected** in all projects using the submodule
3. **No need to push** to remote before testing

#### Advantages of Submodule Approach

- ✓ Single plugin codebase for all UE versions
- ✓ Version control tracks specific plugin commits per project
- ✓ Can use different branches/versions per UE version
- ✓ Better for team collaboration
- ✓ Works seamlessly across machines

#### Rider Configuration with Submodules

1. **Open the UE test project** (not just the plugin)
2. Rider will recognize the submodule structure
3. **Enable VCS integration** for submodules:
   - `Settings` → `Version Control` → `Git`
   - Check "Update submodules"
4. Changes made in Rider are reflected in the submodule's git repository