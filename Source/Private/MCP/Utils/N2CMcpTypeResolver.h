// Copyright Protospatial 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h" // For FEdGraphPinType

class UObject;

class NODETOCODE_API FN2CMcpTypeResolver
{
public:
    /**
     * Resolves a type identifier string (and associated sub-type, container info) into an FEdGraphPinType.
     *
     * @param TypeIdentifier The primary type string (e.g., "bool", "FVector", "object", "/Script/CoreUObject.Vector").
     * @param SubTypeIdentifier The sub-type string, used for objects, classes, structs, enums (e.g., "Actor", "/Game/MyStruct.MyStruct").
     * @param ContainerTypeStr String representation of the container type ("none", "array", "set", "map").
     * @param KeyTypeIdentifierStr String representation of the key type for map containers.
     * @param bIsReference Whether the pin is a reference.
     * @param bIsConst Whether the pin is const.
     * @param OutPinType The FEdGraphPinType to populate.
     * @param OutErrorMsg Error message if resolution fails.
     * @return True if resolution was successful, false otherwise.
     */
    static bool ResolvePinType(
        const FString& TypeIdentifier,
        const FString& SubTypeIdentifier,
        const FString& ContainerTypeStr,
        const FString& KeyTypeIdentifierStr,
        bool bIsReference,
        bool bIsConst,
        FEdGraphPinType& OutPinType,
        FString& OutErrorMsg
    );

private:
    // Internal helper methods for resolving specific type categories
    static bool ResolvePrimitiveTypeInternal(const FString& TypeIdentifier, FEdGraphPinType& OutPinType);
    static bool ResolveMathTypeInternal(const FString& TypeIdentifier, FEdGraphPinType& OutPinType);
    static bool ResolveSpecialTypeInternal(const FString& TypeIdentifier, FEdGraphPinType& OutPinType);
    static bool ResolveObjectLikeTypeInternal(const FString& TypeIdentifier, const FString& SubTypeIdentifier, FEdGraphPinType& OutPinType, FString& OutErrorMsg);
    static bool ResolveFullObjectPathInternal(const FString& FullPath, FEdGraphPinType& OutPinType, FString& OutErrorMsg);
    static UObject* FindObjectByPathOrName(const FString& PathOrName, UClass* ExpectedClass = nullptr);
};
