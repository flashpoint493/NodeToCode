// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UBlueprint;
class USimpleConstructionScript;
class USCS_Node;
class UActorComponent;
class USceneComponent;
class UClass;

/**
 * Utility class for MCP tools that work with Blueprint components
 */
class NODETOCODE_API FN2CMcpComponentUtils
{
public:
    /**
     * Gets the Simple Construction Script from a Blueprint, creating one if necessary
     * @param Blueprint The Blueprint to get the SCS from
     * @param OutErrorMsg Error message if operation fails
     * @return The SCS or nullptr on failure
     */
    static USimpleConstructionScript* GetBlueprintSCS(UBlueprint* Blueprint, FString& OutErrorMsg);

    /**
     * Finds an SCS node by component name
     * @param SCS The Simple Construction Script to search
     * @param ComponentName The name of the component to find
     * @return The SCS node or nullptr if not found
     */
    static USCS_Node* FindSCSNodeByName(USimpleConstructionScript* SCS, const FString& ComponentName);

    /**
     * Creates a new SCS node for a component class
     * @param SCS The Simple Construction Script to add the node to
     * @param ComponentClass The class of component to create
     * @param ComponentName Optional custom name (will be generated if empty)
     * @param OutErrorMsg Error message if operation fails
     * @return The created SCS node or nullptr on failure
     */
    static USCS_Node* CreateSCSNode(USimpleConstructionScript* SCS, UClass* ComponentClass, 
        const FString& ComponentName, FString& OutErrorMsg);

    /**
     * Validates that a component class can be instantiated in a Blueprint
     * @param ComponentClass The class to validate
     * @param OutErrorMsg Error message if validation fails
     * @return True if the class is valid for Blueprint use
     */
    static bool ValidateComponentClass(UClass* ComponentClass, FString& OutErrorMsg);

    /**
     * Generates a unique component name based on the class
     * @param SCS The SCS to check for name conflicts
     * @param ComponentClass The class to base the name on
     * @return A unique component name
     */
    static FString GenerateUniqueComponentName(USimpleConstructionScript* SCS, UClass* ComponentClass);

    /**
     * Sets up attachment between two SCS nodes
     * @param ChildNode The node to attach
     * @param ParentNode The node to attach to
     * @param SocketName Optional socket name for attachment
     * @param OutErrorMsg Error message if operation fails
     * @return True if attachment was successful
     */
    static bool SetupComponentAttachment(USCS_Node* ChildNode, USCS_Node* ParentNode, 
        const FString& SocketName, FString& OutErrorMsg);

    /**
     * Builds a hierarchical JSON representation of the component tree
     * @param Blueprint The Blueprint containing the components
     * @param RootNodes The root nodes to start from
     * @param bIncludeInherited Whether to include inherited components
     * @param ComponentTypeFilter Filter for component types ("all", "scene", "actor", "primitive")
     * @return JSON object representing the component hierarchy
     */
    static TSharedPtr<FJsonObject> BuildComponentHierarchy(UBlueprint* Blueprint,
        const TArray<USCS_Node*>& RootNodes, bool bIncludeInherited, const FString& ComponentTypeFilter);

    /**
     * Converts a single SCS node to JSON representation
     * @param Node The SCS node to convert
     * @param bIsInherited Whether this node is from a parent class
     * @return JSON object representing the node
     */
    static TSharedPtr<FJsonObject> SCSNodeToJson(USCS_Node* Node, bool bIsInherited);

    /**
     * Gets inherited SCS nodes from parent Blueprints
     * @param Blueprint The Blueprint to get inherited nodes from
     * @param OutInheritedNodes Array to fill with inherited nodes
     */
    static void GetInheritedSCSNodes(UBlueprint* Blueprint, TArray<USCS_Node*>& OutInheritedNodes);

    /**
     * Applies a transform to a scene component template
     * @param SceneComponent The component to apply the transform to
     * @param TransformJson JSON object containing location, rotation, and scale
     * @param OutErrorMsg Error message if operation fails
     * @return True if transform was applied successfully
     */
    static bool ApplyTransformToComponent(USceneComponent* SceneComponent, 
        const TSharedPtr<FJsonObject>& TransformJson, FString& OutErrorMsg);

    /**
     * Checks if a component class passes the type filter
     * @param ComponentClass The class to check
     * @param TypeFilter The filter ("all", "scene", "actor", "primitive")
     * @return True if the class passes the filter
     */
    static bool PassesComponentTypeFilter(UClass* ComponentClass, const FString& TypeFilter);

    /**
     * Deletes a component node from the SCS
     * @param SCS The Simple Construction Script containing the node
     * @param NodeToDelete The node to delete
     * @param bDeleteChildren Whether to delete child nodes as well
     * @param OutErrorMsg Error message if operation fails
     * @return True if deletion was successful
     */
    static bool DeleteSCSNode(USimpleConstructionScript* SCS, USCS_Node* NodeToDelete, 
        bool bDeleteChildren, FString& OutErrorMsg);

    /**
     * Gets all child nodes of a component recursively
     * @param Node The parent node
     * @param OutChildren Array to fill with all child nodes
     */
    static void GetAllChildNodes(USCS_Node* Node, TArray<USCS_Node*>& OutChildren);

    /**
     * Checks if a node can be safely deleted
     * @param Node The node to check
     * @param bIsInherited Whether the node is from a parent Blueprint
     * @param OutErrorMsg Error message if node cannot be deleted
     * @return True if the node can be deleted
     */
    static bool CanDeleteNode(USCS_Node* Node, bool bIsInherited, FString& OutErrorMsg);

private:
    /**
     * Recursively builds JSON for a node and its children
     * @param Node The node to process
     * @param bIsInherited Whether this node is inherited
     * @param ComponentTypeFilter Type filter to apply
     * @param OutJsonArray Array to add the JSON objects to
     */
    static void BuildNodeHierarchyRecursive(USCS_Node* Node, bool bIsInherited, 
        const FString& ComponentTypeFilter, TArray<TSharedPtr<FJsonValue>>& OutJsonArray);

    /**
     * Gets the transform of a component as JSON
     * @param Component The component to get the transform from
     * @return JSON object with location, rotation, and scale
     */
    static TSharedPtr<FJsonObject> GetComponentTransformJson(USceneComponent* Component);
};