// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "N2CMcpGetFocusedBlueprintTool.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "Core/N2CEditorIntegration.h"
#include "Core/N2CNodeTranslator.h"
#include "Utils/N2CLogger.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

// Auto-register this tool
REGISTER_MCP_TOOL(FN2CMcpGetFocusedBlueprintTool)

FMcpToolDefinition FN2CMcpGetFocusedBlueprintTool::GetDefinition() const
{
	FMcpToolDefinition Definition(
		TEXT("get-focused-blueprint"),
		TEXT("Collects and serializes the currently focused Blueprint graph in the Unreal Editor into NodeToCode's N2CJSON format.")
	);
	
	// This tool takes no input parameters
	Definition.InputSchema = BuildEmptyObjectSchema();
	
	// Add read-only annotation
	AddReadOnlyAnnotation(Definition);
	
	return Definition;
}

FMcpToolCallResult FN2CMcpGetFocusedBlueprintTool::Execute(const TSharedPtr<FJsonObject>& Arguments)
{
	// Since this tool requires Game Thread execution, use the base class helper
	return ExecuteOnGameThread([this]() -> FMcpToolCallResult
	{
		FString ErrorMsg;
		FString JsonOutput = FN2CEditorIntegration::Get().GetFocusedBlueprintAsJson(false /* no pretty print */, ErrorMsg);
		
		if (!JsonOutput.IsEmpty())
		{
			// Enhance the JSON with GUID information
			FString EnhancedJson = EnhanceJsonWithGuids(JsonOutput);
			
			FN2CLogger::Get().Log(TEXT("get-focused-blueprint tool successfully retrieved Blueprint JSON with GUID enhancement"), EN2CLogSeverity::Info);
			return FMcpToolCallResult::CreateTextResult(EnhancedJson);
		}
		else
		{
			FN2CLogger::Get().LogWarning(FString::Printf(TEXT("get-focused-blueprint tool failed: %s"), *ErrorMsg));
			return FMcpToolCallResult::CreateErrorResult(ErrorMsg);
		}
	});
}

FString FN2CMcpGetFocusedBlueprintTool::EnhanceJsonWithGuids(const FString& JsonString) const
{
	// Parse the JSON
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		FN2CLogger::Get().LogError(TEXT("Failed to parse JSON for GUID enhancement"));
		return JsonString; // Return original if parsing fails
	}
	
	// Get the translator instance to access the ID maps
	const FN2CNodeTranslator& Translator = FN2CNodeTranslator::Get();
	const TMap<FGuid, FString>& NodeIDMap = Translator.GetNodeIDMap();
	const TMap<FGuid, FString>& PinIDMap = Translator.GetPinIDMap();
	
	// Create reverse maps (ID -> GUID)
	TMap<FString, FGuid> ReverseNodeIDMap;
	for (const auto& Pair : NodeIDMap)
	{
		ReverseNodeIDMap.Add(Pair.Value, Pair.Key);
	}
	
	TMap<FString, FGuid> ReversePinIDMap;
	for (const auto& Pair : PinIDMap)
	{
		ReversePinIDMap.Add(Pair.Value, Pair.Key);
	}
	
	// Process each graph in the blueprint
	const TArray<TSharedPtr<FJsonValue>>* GraphsArray;
	if (RootObject->TryGetArrayField(TEXT("graphs"), GraphsArray))
	{
		for (const TSharedPtr<FJsonValue>& GraphValue : *GraphsArray)
		{
			TSharedPtr<FJsonObject> GraphObject = GraphValue->AsObject();
			if (!GraphObject.IsValid()) continue;
			
			// Process nodes in this graph
			const TArray<TSharedPtr<FJsonValue>>* NodesArray;
			if (GraphObject->TryGetArrayField(TEXT("nodes"), NodesArray))
			{
				for (const TSharedPtr<FJsonValue>& NodeValue : *NodesArray)
				{
					TSharedPtr<FJsonObject> NodeObject = NodeValue->AsObject();
					if (!NodeObject.IsValid()) continue;
					
					// Get the short ID
					FString ShortNodeID;
					if (NodeObject->TryGetStringField(TEXT("id"), ShortNodeID))
					{
						// Create the nested ID structure
						TSharedPtr<FJsonObject> IdsObject = MakeShareable(new FJsonObject);
						IdsObject->SetStringField(TEXT("short"), ShortNodeID);
						
						// Find and add the GUID if available
						if (FGuid* NodeGuid = ReverseNodeIDMap.Find(ShortNodeID))
						{
							IdsObject->SetStringField(TEXT("guid"), NodeGuid->ToString(EGuidFormats::DigitsWithHyphens));
						}
						
						// Replace the simple ID with the nested structure
						NodeObject->RemoveField(TEXT("id"));
						NodeObject->SetObjectField(TEXT("ids"), IdsObject);
					}
					
					// Process pins
					TArray<FString> PinArrayNames = { TEXT("inputPins"), TEXT("outputPins") };
					for (const FString& PinArrayName : PinArrayNames)
					{
						const TArray<TSharedPtr<FJsonValue>>* PinsArray;
						if (NodeObject->TryGetArrayField(PinArrayName, PinsArray))
						{
							for (const TSharedPtr<FJsonValue>& PinValue : *PinsArray)
							{
								TSharedPtr<FJsonObject> PinObject = PinValue->AsObject();
								if (!PinObject.IsValid()) continue;
								
								// Get the short pin ID
								FString ShortPinID;
								if (PinObject->TryGetStringField(TEXT("id"), ShortPinID))
								{
									// Create the nested ID structure for pin
									TSharedPtr<FJsonObject> PinIdsObject = MakeShareable(new FJsonObject);
									PinIdsObject->SetStringField(TEXT("short"), ShortPinID);
									
									// Find and add the GUID if available
									if (FGuid* PinGuid = ReversePinIDMap.Find(ShortPinID))
									{
										PinIdsObject->SetStringField(TEXT("guid"), PinGuid->ToString(EGuidFormats::DigitsWithHyphens));
									}
									
									// Add pin name as fallback identifier
									FString PinName;
									if (PinObject->TryGetStringField(TEXT("name"), PinName))
									{
										PinIdsObject->SetStringField(TEXT("name"), PinName);
									}
									
									// Replace the simple ID with the nested structure
									PinObject->RemoveField(TEXT("id"));
									PinObject->SetObjectField(TEXT("ids"), PinIdsObject);
								}
							}
						}
					}
				}
			}
		}
	}
	
	// Serialize back to JSON string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	
	return OutputString;
}