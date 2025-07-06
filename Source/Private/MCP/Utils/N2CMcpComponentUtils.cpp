// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "N2CMcpComponentUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

USimpleConstructionScript* FN2CMcpComponentUtils::GetBlueprintSCS(UBlueprint* Blueprint, FString& OutErrorMsg)
{
    if (!Blueprint)
    {
        OutErrorMsg = TEXT("NO_BLUEPRINT|Blueprint is null");
        return nullptr;
    }

    // Get or create the Simple Construction Script
    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        // Create a new SCS if one doesn't exist
        SCS = NewObject<USimpleConstructionScript>(Blueprint);
        Blueprint->SimpleConstructionScript = SCS;
        
        if (!SCS)
        {
            OutErrorMsg = TEXT("SCS_CREATE_FAILED|Failed to create Simple Construction Script");
            return nullptr;
        }
    }

    return SCS;
}

USCS_Node* FN2CMcpComponentUtils::FindSCSNodeByName(USimpleConstructionScript* SCS, const FString& ComponentName)
{
    if (!SCS || ComponentName.IsEmpty())
    {
        return nullptr;
    }

    // Search all nodes
    const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
    for (USCS_Node* Node : AllNodes)
    {
        if (Node && Node->GetVariableName() == *ComponentName)
        {
            return Node;
        }
    }

    return nullptr;
}

USCS_Node* FN2CMcpComponentUtils::CreateSCSNode(USimpleConstructionScript* SCS, UClass* ComponentClass, 
    const FString& ComponentName, FString& OutErrorMsg)
{
    if (!SCS)
    {
        OutErrorMsg = TEXT("NO_SCS|Simple Construction Script is null");
        return nullptr;
    }

    if (!ValidateComponentClass(ComponentClass, OutErrorMsg))
    {
        return nullptr;
    }

    // Generate name if not provided
    FString FinalComponentName = ComponentName;
    if (FinalComponentName.IsEmpty())
    {
        FinalComponentName = GenerateUniqueComponentName(SCS, ComponentClass);
    }
    else
    {
        // Ensure the name is unique
        if (FindSCSNodeByName(SCS, FinalComponentName))
        {
            OutErrorMsg = FString::Printf(TEXT("NAME_EXISTS|Component with name '%s' already exists"), *FinalComponentName);
            return nullptr;
        }
    }

    // Create the SCS node
    USCS_Node* NewNode = SCS->CreateNode(ComponentClass, *FinalComponentName);
    if (!NewNode)
    {
        OutErrorMsg = TEXT("NODE_CREATE_FAILED|Failed to create SCS node");
        return nullptr;
    }

    // Add to the SCS
    SCS->AddNode(NewNode);

    return NewNode;
}

bool FN2CMcpComponentUtils::ValidateComponentClass(UClass* ComponentClass, FString& OutErrorMsg)
{
    if (!ComponentClass)
    {
        OutErrorMsg = TEXT("INVALID_CLASS|Component class is null");
        return false;
    }

    // Check if it's actually a component class
    if (!ComponentClass->IsChildOf(UActorComponent::StaticClass()))
    {
        OutErrorMsg = FString::Printf(TEXT("NOT_COMPONENT|Class '%s' is not an ActorComponent"), 
            *ComponentClass->GetName());
        return false;
    }

    // Check if the class can be instantiated
    if (ComponentClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
    {
        OutErrorMsg = FString::Printf(TEXT("CANNOT_INSTANTIATE|Class '%s' cannot be instantiated (abstract, deprecated, or has newer version)"), 
            *ComponentClass->GetName());
        return false;
    }

    // Components don't need to be explicitly blueprintable - just check if they can be instantiated

    return true;
}

FString FN2CMcpComponentUtils::GenerateUniqueComponentName(USimpleConstructionScript* SCS, UClass* ComponentClass)
{
    if (!SCS || !ComponentClass)
    {
        return TEXT("Component");
    }

    // Get base name from class
    FString BaseName = ComponentClass->GetName();
    
    // Remove common prefixes
    if (BaseName.StartsWith(TEXT("U")))
    {
        BaseName = BaseName.RightChop(1);
    }
    
    // Remove "Component" suffix if present
    if (BaseName.EndsWith(TEXT("Component")))
    {
        BaseName = BaseName.LeftChop(9);
    }

    // Ensure we have a valid base name
    if (BaseName.IsEmpty())
    {
        BaseName = TEXT("Component");
    }

    // Find a unique name
    FString UniqueName = BaseName;
    int32 Counter = 1;
    
    while (FindSCSNodeByName(SCS, UniqueName))
    {
        UniqueName = FString::Printf(TEXT("%s%d"), *BaseName, Counter++);
    }

    return UniqueName;
}

bool FN2CMcpComponentUtils::SetupComponentAttachment(USCS_Node* ChildNode, USCS_Node* ParentNode, 
    const FString& SocketName, FString& OutErrorMsg)
{
    if (!ChildNode)
    {
        OutErrorMsg = TEXT("NO_CHILD|Child node is null");
        return false;
    }

    if (!ParentNode)
    {
        OutErrorMsg = TEXT("NO_PARENT|Parent node is null");
        return false;
    }

    // Verify both are scene components
    USceneComponent* ChildTemplate = Cast<USceneComponent>(ChildNode->ComponentTemplate);
    USceneComponent* ParentTemplate = Cast<USceneComponent>(ParentNode->ComponentTemplate);

    if (!ChildTemplate)
    {
        OutErrorMsg = TEXT("CHILD_NOT_SCENE|Child component is not a SceneComponent");
        return false;
    }

    if (!ParentTemplate)
    {
        OutErrorMsg = TEXT("PARENT_NOT_SCENE|Parent component is not a SceneComponent");
        return false;
    }

    // Check for circular attachment
    FName ParentName = ParentNode->ParentComponentOrVariableName;
    USCS_Node* CurrentParent = ParentName.IsNone() ? 
        nullptr : FindSCSNodeByName(ChildNode->GetSCS(), ParentName.ToString());
    
    while (CurrentParent)
    {
        if (CurrentParent == ChildNode)
        {
            OutErrorMsg = TEXT("CIRCULAR_ATTACHMENT|Circular attachment detected");
            return false;
        }
        ParentName = CurrentParent->ParentComponentOrVariableName;
        CurrentParent = ParentName.IsNone() ? 
            nullptr : FindSCSNodeByName(ChildNode->GetSCS(), ParentName.ToString());
    }

    // Set up the attachment
    ChildNode->SetParent(ParentNode);
    
    if (!SocketName.IsEmpty())
    {
        ChildNode->AttachToName = *SocketName;
    }

    return true;
}

TSharedPtr<FJsonObject> FN2CMcpComponentUtils::BuildComponentHierarchy(UBlueprint* Blueprint,
    const TArray<USCS_Node*>& RootNodes, bool bIncludeInherited, const FString& ComponentTypeFilter)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    TArray<TSharedPtr<FJsonValue>> ComponentsArray;

    // Build hierarchy for each root node
    for (USCS_Node* RootNode : RootNodes)
    {
        if (RootNode)
        {
            BuildNodeHierarchyRecursive(RootNode, false, ComponentTypeFilter, ComponentsArray);
        }
    }

    // Add inherited components if requested
    if (bIncludeInherited && Blueprint)
    {
        TArray<USCS_Node*> InheritedNodes;
        GetInheritedSCSNodes(Blueprint, InheritedNodes);
        
        for (USCS_Node* InheritedNode : InheritedNodes)
        {
            BuildNodeHierarchyRecursive(InheritedNode, true, ComponentTypeFilter, ComponentsArray);
        }
    }

    Result->SetArrayField(TEXT("components"), ComponentsArray);
    Result->SetNumberField(TEXT("totalCount"), ComponentsArray.Num());

    // Find root component name
    FString RootComponentName;
    if (RootNodes.Num() > 0 && RootNodes[0])
    {
        RootComponentName = RootNodes[0]->GetVariableName().ToString();
    }
    Result->SetStringField(TEXT("rootComponent"), RootComponentName);

    return Result;
}

TSharedPtr<FJsonObject> FN2CMcpComponentUtils::SCSNodeToJson(USCS_Node* Node, bool bIsInherited)
{
    if (!Node)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> NodeJson = MakeShareable(new FJsonObject);

    // Basic info
    NodeJson->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
    
    UClass* ComponentClass = Node->ComponentClass;
    if (ComponentClass)
    {
        NodeJson->SetStringField(TEXT("className"), ComponentClass->GetName());
        NodeJson->SetStringField(TEXT("classPath"), ComponentClass->GetPathName());
    }

    // Node GUID
    NodeJson->SetStringField(TEXT("nodeGuid"), Node->VariableGuid.ToString());

    // Component type info
    bool bIsSceneComponent = Node->ComponentTemplate && Node->ComponentTemplate->IsA<USceneComponent>();
    NodeJson->SetBoolField(TEXT("isSceneComponent"), bIsSceneComponent);
    NodeJson->SetBoolField(TEXT("isRootComponent"), Node->ParentComponentOrVariableName.IsNone());

    // Parent info
    if (!Node->ParentComponentOrVariableName.IsNone())
    {
        NodeJson->SetStringField(TEXT("parentComponent"), Node->ParentComponentOrVariableName.ToString());
    }
    else
    {
        NodeJson->SetField(TEXT("parentComponent"), MakeShareable(new FJsonValueNull));
    }

    // Socket attachment
    NodeJson->SetStringField(TEXT("attachSocketName"), Node->AttachToName.ToString());

    // Transform for scene components
    if (bIsSceneComponent)
    {
        USceneComponent* SceneComp = Cast<USceneComponent>(Node->ComponentTemplate);
        if (SceneComp)
        {
            NodeJson->SetObjectField(TEXT("transform"), GetComponentTransformJson(SceneComp));
        }
    }

    NodeJson->SetBoolField(TEXT("isInherited"), bIsInherited);

    // Children array (will be populated by BuildNodeHierarchyRecursive)
    TArray<TSharedPtr<FJsonValue>> ChildrenArray;
    NodeJson->SetArrayField(TEXT("children"), ChildrenArray);

    return NodeJson;
}

void FN2CMcpComponentUtils::GetInheritedSCSNodes(UBlueprint* Blueprint, TArray<USCS_Node*>& OutInheritedNodes)
{
    if (!Blueprint)
    {
        return;
    }

    // Get parent Blueprint
    UBlueprint* ParentBP = nullptr;
    if (Blueprint->ParentClass)
    {
        ParentBP = Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy);
    }

    if (ParentBP && ParentBP->SimpleConstructionScript)
    {
        const TArray<USCS_Node*>& ParentNodes = ParentBP->SimpleConstructionScript->GetAllNodes();
        OutInheritedNodes.Append(ParentNodes);

        // Recursively get inherited nodes from grandparents
        GetInheritedSCSNodes(ParentBP, OutInheritedNodes);
    }
}

bool FN2CMcpComponentUtils::ApplyTransformToComponent(USceneComponent* SceneComponent, 
    const TSharedPtr<FJsonObject>& TransformJson, FString& OutErrorMsg)
{
    if (!SceneComponent)
    {
        OutErrorMsg = TEXT("NO_COMPONENT|Scene component is null");
        return false;
    }

    if (!TransformJson.IsValid())
    {
        // No transform to apply
        return true;
    }

    // Apply location
    const TSharedPtr<FJsonObject>* LocationObj;
    if (TransformJson->TryGetObjectField(TEXT("location"), LocationObj))
    {
        double X = 0, Y = 0, Z = 0;
        (*LocationObj)->TryGetNumberField(TEXT("x"), X);
        (*LocationObj)->TryGetNumberField(TEXT("y"), Y);
        (*LocationObj)->TryGetNumberField(TEXT("z"), Z);
        
        SceneComponent->SetRelativeLocation(FVector(X, Y, Z));
    }

    // Apply rotation
    const TSharedPtr<FJsonObject>* RotationObj;
    if (TransformJson->TryGetObjectField(TEXT("rotation"), RotationObj))
    {
        double Pitch = 0, Yaw = 0, Roll = 0;
        (*RotationObj)->TryGetNumberField(TEXT("pitch"), Pitch);
        (*RotationObj)->TryGetNumberField(TEXT("yaw"), Yaw);
        (*RotationObj)->TryGetNumberField(TEXT("roll"), Roll);
        
        SceneComponent->SetRelativeRotation(FRotator(Pitch, Yaw, Roll));
    }

    // Apply scale
    const TSharedPtr<FJsonObject>* ScaleObj;
    if (TransformJson->TryGetObjectField(TEXT("scale"), ScaleObj))
    {
        double X = 1, Y = 1, Z = 1;
        (*ScaleObj)->TryGetNumberField(TEXT("x"), X);
        (*ScaleObj)->TryGetNumberField(TEXT("y"), Y);
        (*ScaleObj)->TryGetNumberField(TEXT("z"), Z);
        
        SceneComponent->SetRelativeScale3D(FVector(X, Y, Z));
    }

    return true;
}

bool FN2CMcpComponentUtils::PassesComponentTypeFilter(UClass* ComponentClass, const FString& TypeFilter)
{
    if (!ComponentClass || TypeFilter == TEXT("all"))
    {
        return true;
    }

    if (TypeFilter == TEXT("scene"))
    {
        return ComponentClass->IsChildOf(USceneComponent::StaticClass());
    }
    else if (TypeFilter == TEXT("actor"))
    {
        return ComponentClass->IsChildOf(UActorComponent::StaticClass()) && 
               !ComponentClass->IsChildOf(USceneComponent::StaticClass());
    }
    else if (TypeFilter == TEXT("primitive"))
    {
        return ComponentClass->IsChildOf(UPrimitiveComponent::StaticClass());
    }

    return true;
}

void FN2CMcpComponentUtils::BuildNodeHierarchyRecursive(USCS_Node* Node, bool bIsInherited, 
    const FString& ComponentTypeFilter, TArray<TSharedPtr<FJsonValue>>& OutJsonArray)
{
    if (!Node || !Node->ComponentClass)
    {
        return;
    }

    // Check type filter
    if (!PassesComponentTypeFilter(Node->ComponentClass, ComponentTypeFilter))
    {
        return;
    }

    // Convert node to JSON
    TSharedPtr<FJsonObject> NodeJson = SCSNodeToJson(Node, bIsInherited);
    if (!NodeJson.IsValid())
    {
        return;
    }

    // Process children
    TArray<TSharedPtr<FJsonValue>> ChildrenArray;
    TArray<USCS_Node*> ChildNodes = Node->GetChildNodes();
    
    for (USCS_Node* ChildNode : ChildNodes)
    {
        BuildNodeHierarchyRecursive(ChildNode, bIsInherited, ComponentTypeFilter, ChildrenArray);
    }

    NodeJson->SetArrayField(TEXT("children"), ChildrenArray);
    
    // Add to output array
    OutJsonArray.Add(MakeShareable(new FJsonValueObject(NodeJson)));
}

TSharedPtr<FJsonObject> FN2CMcpComponentUtils::GetComponentTransformJson(USceneComponent* Component)
{
    if (!Component)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> TransformJson = MakeShareable(new FJsonObject);

    // Location
    FVector Location = Component->GetRelativeLocation();
    TSharedPtr<FJsonObject> LocationJson = MakeShareable(new FJsonObject);
    LocationJson->SetNumberField(TEXT("x"), Location.X);
    LocationJson->SetNumberField(TEXT("y"), Location.Y);
    LocationJson->SetNumberField(TEXT("z"), Location.Z);
    TransformJson->SetObjectField(TEXT("location"), LocationJson);

    // Rotation
    FRotator Rotation = Component->GetRelativeRotation();
    TSharedPtr<FJsonObject> RotationJson = MakeShareable(new FJsonObject);
    RotationJson->SetNumberField(TEXT("pitch"), Rotation.Pitch);
    RotationJson->SetNumberField(TEXT("yaw"), Rotation.Yaw);
    RotationJson->SetNumberField(TEXT("roll"), Rotation.Roll);
    TransformJson->SetObjectField(TEXT("rotation"), RotationJson);

    // Scale
    FVector Scale = Component->GetRelativeScale3D();
    TSharedPtr<FJsonObject> ScaleJson = MakeShareable(new FJsonObject);
    ScaleJson->SetNumberField(TEXT("x"), Scale.X);
    ScaleJson->SetNumberField(TEXT("y"), Scale.Y);
    ScaleJson->SetNumberField(TEXT("z"), Scale.Z);
    TransformJson->SetObjectField(TEXT("scale"), ScaleJson);

    return TransformJson;
}

bool FN2CMcpComponentUtils::DeleteSCSNode(USimpleConstructionScript* SCS, USCS_Node* NodeToDelete, 
    bool bDeleteChildren, FString& OutErrorMsg)
{
    if (!SCS)
    {
        OutErrorMsg = TEXT("NO_SCS|Simple Construction Script is null");
        return false;
    }

    if (!NodeToDelete)
    {
        OutErrorMsg = TEXT("NO_NODE|Node to delete is null");
        return false;
    }

    // Check if node can be deleted
    bool bIsInherited = false;
    const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
    if (!AllNodes.Contains(NodeToDelete))
    {
        // Check if it's an inherited node
        UBlueprint* Blueprint = Cast<UBlueprint>(SCS->GetOuter());
        if (Blueprint)
        {
            TArray<USCS_Node*> InheritedNodes;
            GetInheritedSCSNodes(Blueprint, InheritedNodes);
            bIsInherited = InheritedNodes.Contains(NodeToDelete);
        }
    }

    if (!CanDeleteNode(NodeToDelete, bIsInherited, OutErrorMsg))
    {
        return false;
    }

    // Handle children
    TArray<USCS_Node*> ChildNodes = NodeToDelete->GetChildNodes();
    if (!bDeleteChildren && ChildNodes.Num() > 0)
    {
        // Reparent children to the parent of the node being deleted
        USCS_Node* ParentNode = nullptr;
        if (!NodeToDelete->ParentComponentOrVariableName.IsNone())
        {
            ParentNode = FindSCSNodeByName(SCS, NodeToDelete->ParentComponentOrVariableName.ToString());
        }

        for (USCS_Node* ChildNode : ChildNodes)
        {
            if (ParentNode)
            {
                ChildNode->SetParent(ParentNode);
            }
            else
            {
                // Make it a root node
                ChildNode->SetParent(static_cast<USCS_Node*>(nullptr));
            }
        }
    }
    else if (bDeleteChildren)
    {
        // Recursively delete all children
        TArray<USCS_Node*> AllChildren;
        GetAllChildNodes(NodeToDelete, AllChildren);

        // Delete from deepest to shallowest
        for (int32 i = AllChildren.Num() - 1; i >= 0; i--)
        {
            SCS->RemoveNode(AllChildren[i]);
        }
    }

    // Remove the node from SCS
    SCS->RemoveNode(NodeToDelete);

    return true;
}

void FN2CMcpComponentUtils::GetAllChildNodes(USCS_Node* Node, TArray<USCS_Node*>& OutChildren)
{
    if (!Node)
    {
        return;
    }

    TArray<USCS_Node*> DirectChildren = Node->GetChildNodes();
    for (USCS_Node* Child : DirectChildren)
    {
        OutChildren.Add(Child);
        // Recursively get children of children
        GetAllChildNodes(Child, OutChildren);
    }
}

bool FN2CMcpComponentUtils::CanDeleteNode(USCS_Node* Node, bool bIsInherited, FString& OutErrorMsg)
{
    if (!Node)
    {
        OutErrorMsg = TEXT("NO_NODE|Node is null");
        return false;
    }

    if (bIsInherited)
    {
        OutErrorMsg = TEXT("INHERITED_NODE|Cannot delete components inherited from parent Blueprint");
        return false;
    }

    // Check if it's the default scene root
    if (Node->ComponentTemplate && Node->ComponentTemplate->GetFName() == TEXT("DefaultSceneRoot"))
    {
        // Only prevent deletion if it has children
        if (Node->GetChildNodes().Num() > 0)
        {
            OutErrorMsg = TEXT("DEFAULT_ROOT_WITH_CHILDREN|Cannot delete DefaultSceneRoot when it has child components");
            return false;
        }
    }

    return true;
}