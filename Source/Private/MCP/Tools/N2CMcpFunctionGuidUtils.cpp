// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "N2CMcpFunctionGuidUtils.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"
#include "EdGraph/EdGraph.h"
#include "Misc/SecureHash.h"
#include "Kismet2/BlueprintEditorUtils.h"

const FString FN2CMcpFunctionGuidUtils::GuidMetadataKey = TEXT("NodeToCode_FunctionGuid");

FGuid FN2CMcpFunctionGuidUtils::GetOrCreateFunctionGuid(UEdGraph* FunctionGraph)
{
	if (!FunctionGraph)
	{
		return FGuid();
	}
	
	// First check if a GUID is already stored
	FGuid StoredGuid = GetStoredFunctionGuid(FunctionGraph);
	if (StoredGuid.IsValid())
	{
		return StoredGuid;
	}
	
	// Generate a deterministic GUID
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(FunctionGraph);
	if (!Blueprint)
	{
		return FGuid();
	}
	
	FGuid NewGuid = GenerateDeterministicGuid(Blueprint, FunctionGraph->GetName());
	
	// Store it for future use
	StoreFunctionGuid(FunctionGraph, NewGuid);
	
	return NewGuid;
}

FGuid FN2CMcpFunctionGuidUtils::GetStoredFunctionGuid(const UEdGraph* FunctionGraph)
{
	if (!FunctionGraph)
	{
		return FGuid();
	}
	
	// For now, since we can't store metadata on K2Node_FunctionEntry,
	// we'll generate a deterministic GUID based on the function
	// This ensures consistency across all tools
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(FunctionGraph);
	if (!Blueprint)
	{
		return FGuid();
	}
	
	return GenerateDeterministicGuid(Blueprint, FunctionGraph->GetName());
}

void FN2CMcpFunctionGuidUtils::StoreFunctionGuid(UEdGraph* FunctionGraph, const FGuid& Guid)
{
	// Since we can't store metadata on K2Node_FunctionEntry,
	// we rely on deterministic GUID generation instead
	// This function is left as a no-op for now
}

FGuid FN2CMcpFunctionGuidUtils::GenerateDeterministicGuid(const UBlueprint* Blueprint, const FString& FunctionName)
{
	if (!Blueprint || FunctionName.IsEmpty())
	{
		return FGuid();
	}
	
	// Create a unique string based on Blueprint path and function name
	FString UniqueString = FString::Printf(TEXT("%s::%s"), *Blueprint->GetPathName(), *FunctionName);
	
	// Generate a hash from the unique string
	FMD5 Md5Gen;
	Md5Gen.Update(reinterpret_cast<const uint8*>(*UniqueString), UniqueString.Len() * sizeof(TCHAR));
	
	uint8 Digest[16];
	Md5Gen.Final(Digest);
	
	// Convert the first 16 bytes of the hash to a GUID
	// Note: This is deterministic - the same input will always produce the same GUID
	FGuid DeterministicGuid(
		*reinterpret_cast<uint32*>(&Digest[0]),
		*reinterpret_cast<uint32*>(&Digest[4]),
		*reinterpret_cast<uint32*>(&Digest[8]),
		*reinterpret_cast<uint32*>(&Digest[12])
	);
	
	return DeterministicGuid;
}

UEdGraph* FN2CMcpFunctionGuidUtils::FindFunctionByGuid(UBlueprint* Blueprint, const FGuid& FunctionGuid)
{
	if (!Blueprint || !FunctionGuid.IsValid())
	{
		return nullptr;
	}
	
	// Search through all function graphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph)
		{
			continue;
		}
		
		FGuid GraphGuid = GetOrCreateFunctionGuid(Graph);
		if (GraphGuid == FunctionGuid)
		{
			return Graph;
		}
	}
	
	// Also search through event graphs for custom events
	// Note: Custom events don't have persistent GUIDs in this implementation
	// You would need to extend this to handle custom events if needed
	
	return nullptr;
}

UK2Node_FunctionEntry* FN2CMcpFunctionGuidUtils::GetFunctionEntryNode(UEdGraph* FunctionGraph)
{
	if (!FunctionGraph)
	{
		return nullptr;
	}
	
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
		{
			return EntryNode;
		}
	}
	
	return nullptr;
}