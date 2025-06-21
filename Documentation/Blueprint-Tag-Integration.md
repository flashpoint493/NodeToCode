# Blueprint Tag Integration Guide

This guide explains how to use the NodeToCode tagging functionality in Blueprint for UI integration.

## Overview

The tagging system allows you to organize and track Blueprint graphs with custom tags and categories. Tags are persistent (saved to disk) and can be managed both programmatically through MCP tools and manually through Blueprint UI.

## Key Components

### 1. Blueprint Function Library (`N2CTagBlueprintLibrary`)

Provides static functions accessible from any Blueprint:

#### Core Functions
- **Tag Focused Blueprint Graph** - Tags the currently focused graph in the Blueprint editor
- **Tag Blueprint Graph** - Tags a specific graph in a given Blueprint asset
- **Remove Tag** - Removes a tag from a graph
- **Get Tags For Graph** - Returns all tags for a specific graph
- **Get Graphs With Tag** - Finds all graphs with a specific tag
- **Get All Tags** - Returns all tags in the system
- **Search Tags** - Search for tags by partial match

#### Helper Functions
- **Get All Tag Names** - Returns unique tag names
- **Get All Categories** - Returns unique categories
- **Get Tag Statistics** - Returns tag system statistics
- **Graph Has Tag** - Checks if a graph has a specific tag

### 2. Tag Subsystem (`N2CTagSubsystem`)

Provides events and state management for the UI:

#### Events
- **OnBlueprintTagAdded** - Fired when a tag is added
- **OnBlueprintTagRemoved** - Fired when a tag is removed
- **OnTagsLoaded** - Fired when tags are loaded from disk
- **OnTagsSaved** - Fired when tags are saved to disk

#### Functions
- **Get Tag Subsystem** - Returns the singleton instance
- **Get Focused Graph Info** - Gets info about the currently focused graph
- **Refresh Tags** - Triggers a UI refresh

### 3. Data Structures

#### FN2CTagInfo (Blueprint Type)
```
- Tag (String) - The tag name
- Category (String) - Tag category (default: "Default")
- Description (String) - Optional description
- GraphGuid (String, Read-Only) - Unique graph identifier
- GraphName (String, Read-Only) - Name of the graph
- BlueprintPath (String, Read-Only) - Asset path to the Blueprint
- Timestamp (DateTime, Read-Only) - When the tag was created
```

## Usage Examples

### Basic Tagging Workflow

1. **Tag the focused graph:**
```blueprint
Tag Focused Blueprint Graph
  Tag: "PlayerMovement"
  Category: "Gameplay"
  Description: "Handles player movement logic"
  → Success (Branch)
    True: Print "Tag added successfully"
    False: Print Error Message
```

2. **List all tags for current graph:**
```blueprint
Get Tag Subsystem → Get Focused Graph Info
  → Is Valid (Branch)
    True: Get Tags For Graph (Graph Guid)
      → For Each Loop
        → Print Tag Info
```

3. **React to tag events:**
```blueprint
Event BeginPlay
  → Get Tag Subsystem
    → Bind Event to OnBlueprintTagAdded
      → Update UI Tag List
```

### Advanced UI Implementation

For a complete tag management UI:

1. **Initialize on widget construct:**
```blueprint
Event Construct
  → Get Tag Subsystem
    → Store reference
    → Bind all events
    → Refresh Tags
```

2. **Display current graph tags:**
```blueprint
Custom Event: Update Current Tags
  → Get Focused Graph Info
    → Get Tags For Graph
      → Clear tag list widget
      → For Each Tag
        → Create tag widget
        → Add to list
```

3. **Add new tag with validation:**
```blueprint
Button Click (Add Tag)
  → Get Tag Name from Input
  → Is Empty (Branch)
    False: Tag Focused Blueprint Graph
      → Success (Branch)
        True: Clear input, refresh UI
        False: Show error notification
```

4. **Search and filter tags:**
```blueprint
Text Changed (Search Box)
  → Search Tags (Search Term, Include Descriptions)
    → Update filtered tag display
```

## Best Practices

1. **Category Organization**
   - Use consistent categories across your project
   - Common categories: "Gameplay", "UI", "Audio", "VFX", "Debug"
   - Retrieve available categories with `Get All Categories`

2. **Tag Naming**
   - Use descriptive, searchable names
   - Consider prefixes for grouping (e.g., "FEAT_", "BUG_", "TODO_")
   - Avoid special characters that might cause issues

3. **Error Handling**
   - Always check success outputs from tag functions
   - Display error messages to users
   - Handle the case where no graph is focused

4. **Performance**
   - Cache tag lists when possible
   - Use events to update UI rather than polling
   - Batch operations when adding multiple tags

5. **Persistence**
   - Tags are automatically saved
   - Call `Save Tags` explicitly if needed
   - Tags survive editor restarts

## Integration with NodeToCode UI

The tag system can be integrated into the main NodeToCode UI widget:

1. Add a collapsible "Tags" section
2. Show tags for the current graph
3. Provide quick tag addition/removal
4. Include a tag browser/search
5. Show tag statistics in the UI

## Storage

Tags are stored in: `ProjectSaved/NodeToCode/Tags/BlueprintTags.json`

This file contains all tag data and can be version controlled if desired.