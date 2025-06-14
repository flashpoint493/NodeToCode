// Copyright Protospatial 2025. All Rights Reserved.

#include "N2CMcpSearchVariableTypesTool.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "MCP/Tools/N2CMcpToolTypes.h"
#include "Utils/N2CLogger.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "BlueprintEditorModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node.h"
#include "EdGraphSchema_K2.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Dom/JsonObject.h"


REGISTER_MCP_TOOL(FN2CMcpSearchVariableTypesTool)

FMcpToolDefinition FN2CMcpSearchVariableTypesTool::GetDefinition() const
{
	FMcpToolDefinition Definition(
		TEXT("search-variable-types"),
		TEXT("Searches for available variable types (primitives, classes, structs, enums) by name and returns matches with unique type identifiers")
	);

	// Build input schema manually (similar to search blueprint nodes tool)
	TSharedPtr<FJsonObject> Schema = MakeShareable(new FJsonObject);
	Schema->SetStringField(TEXT("type"), TEXT("object"));
	
	TSharedPtr<FJsonObject> Properties = MakeShareable(new FJsonObject);
	
	// searchTerm property
	TSharedPtr<FJsonObject> SearchTermProp = MakeShareable(new FJsonObject);
	SearchTermProp->SetStringField(TEXT("type"), TEXT("string"));
	SearchTermProp->SetStringField(TEXT("description"), TEXT("The text query to search for type names"));
	Properties->SetObjectField(TEXT("searchTerm"), SearchTermProp);

	// Category filter
	TSharedPtr<FJsonObject> CategoryProp = MakeShareable(new FJsonObject);
	CategoryProp->SetStringField(TEXT("type"), TEXT("string"));
	TArray<TSharedPtr<FJsonValue>> CategoryEnum;
	CategoryEnum.Add(MakeShareable(new FJsonValueString(TEXT("all"))));
	CategoryEnum.Add(MakeShareable(new FJsonValueString(TEXT("primitive"))));
	CategoryEnum.Add(MakeShareable(new FJsonValueString(TEXT("class"))));
	CategoryEnum.Add(MakeShareable(new FJsonValueString(TEXT("struct"))));
	CategoryEnum.Add(MakeShareable(new FJsonValueString(TEXT("enum"))));
	CategoryProp->SetArrayField(TEXT("enum"), CategoryEnum);
	CategoryProp->SetStringField(TEXT("default"), TEXT("all"));
	CategoryProp->SetStringField(TEXT("description"), TEXT("Filter results by type category"));
	Properties->SetObjectField(TEXT("category"), CategoryProp);

	// Include engine types
	TSharedPtr<FJsonObject> IncludeEngineProp = MakeShareable(new FJsonObject);
	IncludeEngineProp->SetStringField(TEXT("type"), TEXT("boolean"));
	IncludeEngineProp->SetBoolField(TEXT("default"), true);
	IncludeEngineProp->SetStringField(TEXT("description"), TEXT("Include engine-provided types in results"));
	Properties->SetObjectField(TEXT("includeEngineTypes"), IncludeEngineProp);

	// Max results
	TSharedPtr<FJsonObject> MaxResultsProp = MakeShareable(new FJsonObject);
	MaxResultsProp->SetStringField(TEXT("type"), TEXT("integer"));
	MaxResultsProp->SetNumberField(TEXT("default"), 50);
	MaxResultsProp->SetNumberField(TEXT("minimum"), 1);
	MaxResultsProp->SetNumberField(TEXT("maximum"), 200);
	MaxResultsProp->SetStringField(TEXT("description"), TEXT("Maximum number of results to return"));
	Properties->SetObjectField(TEXT("maxResults"), MaxResultsProp);

	Schema->SetObjectField(TEXT("properties"), Properties);
	
	// Required fields
	TArray<TSharedPtr<FJsonValue>> RequiredArray;
	RequiredArray.Add(MakeShareable(new FJsonValueString(TEXT("searchTerm"))));
	Schema->SetArrayField(TEXT("required"), RequiredArray);
	
	Definition.InputSchema = Schema;
	
	// Add read-only annotation
	AddReadOnlyAnnotation(Definition);
	
	return Definition;
}

FMcpToolCallResult FN2CMcpSearchVariableTypesTool::Execute(const TSharedPtr<FJsonObject>& Arguments)
{
	return ExecuteOnGameThread([this, Arguments]() -> FMcpToolCallResult
	{
		// Validate Blueprint editor is active
		FBlueprintEditorModule& BPEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
		
		// Get all blueprint editors and find the focused one
		TArray<TSharedRef<IBlueprintEditor>> BlueprintEditors = BPEditorModule.GetBlueprintEditors();
		TSharedPtr<IBlueprintEditor> BlueprintEditor;
		double LatestActivationTime = 0.0;
		
		for (const TSharedRef<IBlueprintEditor>& Editor : BlueprintEditors)
		{
			double ActivationTime = Editor->GetLastActivationTime();
			if (ActivationTime > LatestActivationTime)
			{
				LatestActivationTime = ActivationTime;
				BlueprintEditor = Editor;
			}
		}
		
		if (!BlueprintEditor.IsValid())
		{
			return FMcpToolCallResult::CreateErrorResult(TEXT("No active Blueprint editor found"));
		}

		// Parse arguments
		FString SearchTerm;
		if (!Arguments->TryGetStringField(TEXT("searchTerm"), SearchTerm) || SearchTerm.IsEmpty())
		{
			return FMcpToolCallResult::CreateErrorResult(TEXT("searchTerm is required and cannot be empty"));
		}

		FString Category = TEXT("all");
		Arguments->TryGetStringField(TEXT("category"), Category);

		bool bIncludeEngineTypes = true;
		Arguments->TryGetBoolField(TEXT("includeEngineTypes"), bIncludeEngineTypes);

		int32 MaxResults = 50;
		if (Arguments->HasField(TEXT("maxResults")))
		{
			MaxResults = FMath::Clamp(Arguments->GetIntegerField(TEXT("maxResults")), 1, 200);
		}

		// Log the search request
		FN2CLogger& Logger = FN2CLogger::Get();
		Logger.Log(FString::Printf(TEXT("Searching variable types: '%s', Category: %s, MaxResults: %d"), 
			*SearchTerm, *Category, MaxResults), EN2CLogSeverity::Info, TEXT("SearchVariableTypes"));

		// Build type list based on category filter
		TArray<FVariableTypeInfo> AllTypes;
		
		if (Category == TEXT("all") || Category == TEXT("primitive"))
		{
			CollectPrimitiveTypes(AllTypes);
		}
		
		if (Category == TEXT("all") || Category == TEXT("class"))
		{
			CollectClassTypes(AllTypes, bIncludeEngineTypes);
		}
		
		if (Category == TEXT("all") || Category == TEXT("struct"))
		{
			CollectStructTypes(AllTypes, bIncludeEngineTypes);
		}
		
		if (Category == TEXT("all") || Category == TEXT("enum"))
		{
			CollectEnumTypes(AllTypes, bIncludeEngineTypes);
		}

		// Filter by search term
		TArray<FVariableTypeInfo> FilteredTypes = FilterTypesBySearchTerm(AllTypes, SearchTerm, MaxResults);

		// Build JSON result
		TSharedPtr<FJsonObject> Result = BuildJsonResult(FilteredTypes);

		// Convert result to string
		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

		Logger.Log(FString::Printf(TEXT("Found %d variable types matching '%s'"), 
			FilteredTypes.Num(), *SearchTerm), EN2CLogSeverity::Info, TEXT("SearchVariableTypes"));

		return FMcpToolCallResult::CreateTextResult(OutputString);
	});
}

void FN2CMcpSearchVariableTypesTool::CollectPrimitiveTypes(TArray<FVariableTypeInfo>& OutTypes) const
{
	// Basic primitive types - create struct instances properly
	FVariableTypeInfo BoolType;
	BoolType.TypeName = TEXT("Boolean");
	BoolType.TypeIdentifier = TEXT("bool");
	BoolType.Category = TEXT("primitive");
	BoolType.Description = TEXT("True/False value");
	BoolType.Icon = TEXT("boolean");
	OutTypes.Add(BoolType);
	
	FVariableTypeInfo ByteType;
	ByteType.TypeName = TEXT("Byte");
	ByteType.TypeIdentifier = TEXT("uint8");
	ByteType.Category = TEXT("primitive");
	ByteType.Description = TEXT("8-bit unsigned integer (0-255)");
	ByteType.Icon = TEXT("byte");
	OutTypes.Add(ByteType);
	
	FVariableTypeInfo IntType;
	IntType.TypeName = TEXT("Integer");
	IntType.TypeIdentifier = TEXT("int32");
	IntType.Category = TEXT("primitive");
	IntType.Description = TEXT("32-bit signed integer");
	IntType.Icon = TEXT("integer");
	OutTypes.Add(IntType);
	
	FVariableTypeInfo Int64Type;
	Int64Type.TypeName = TEXT("Integer64");
	Int64Type.TypeIdentifier = TEXT("int64");
	Int64Type.Category = TEXT("primitive");
	Int64Type.Description = TEXT("64-bit signed integer");
	Int64Type.Icon = TEXT("integer64");
	OutTypes.Add(Int64Type);
	
	FVariableTypeInfo FloatType;
	FloatType.TypeName = TEXT("Float");
	FloatType.TypeIdentifier = TEXT("float");
	FloatType.Category = TEXT("primitive");
	FloatType.Description = TEXT("Single precision decimal");
	FloatType.Icon = TEXT("float");
	OutTypes.Add(FloatType);
	
	FVariableTypeInfo DoubleType;
	DoubleType.TypeName = TEXT("Double");
	DoubleType.TypeIdentifier = TEXT("double");
	DoubleType.Category = TEXT("primitive");
	DoubleType.Description = TEXT("Double precision decimal");
	DoubleType.Icon = TEXT("double");
	OutTypes.Add(DoubleType);
	
	FVariableTypeInfo StringType;
	StringType.TypeName = TEXT("String");
	StringType.TypeIdentifier = TEXT("FString");
	StringType.Category = TEXT("primitive");
	StringType.Description = TEXT("Text string");
	StringType.Icon = TEXT("string");
	OutTypes.Add(StringType);
	
	FVariableTypeInfo TextType;
	TextType.TypeName = TEXT("Text");
	TextType.TypeIdentifier = TEXT("FText");
	TextType.Category = TEXT("primitive");
	TextType.Description = TEXT("Localized text");
	TextType.Icon = TEXT("text");
	OutTypes.Add(TextType);
	
	FVariableTypeInfo NameType;
	NameType.TypeName = TEXT("Name");
	NameType.TypeIdentifier = TEXT("FName");
	NameType.Category = TEXT("primitive");
	NameType.Description = TEXT("Lightweight name identifier");
	NameType.Icon = TEXT("name");
	OutTypes.Add(NameType);
}

void FN2CMcpSearchVariableTypesTool::CollectClassTypes(TArray<FVariableTypeInfo>& OutTypes, bool bIncludeEngineTypes) const
{
	// Get variable type tree from K2 schema
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>> TypeTree;
	K2Schema->GetVariableTypeTree(TypeTree, ETypeTreeFilter::None);

	// Process the type tree for class types
	ProcessTypeTree(TypeTree, OutTypes, TEXT("class"), bIncludeEngineTypes);

	// Also query asset registry for unloaded Blueprint classes
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> BlueprintAssets;
	AssetRegistry.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Blueprint")), BlueprintAssets);
	
	ProcessBlueprintAssets(BlueprintAssets, OutTypes);
}

void FN2CMcpSearchVariableTypesTool::CollectStructTypes(TArray<FVariableTypeInfo>& OutTypes, bool bIncludeEngineTypes) const
{
	// Get variable type tree from K2 schema
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>> TypeTree;
	K2Schema->GetVariableTypeTree(TypeTree, ETypeTreeFilter::None);

	// Process the type tree for struct types
	ProcessTypeTree(TypeTree, OutTypes, TEXT("struct"), bIncludeEngineTypes);

	// Query asset registry for user-defined structs
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> StructAssets;
	AssetRegistry.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("UserDefinedStruct")), StructAssets);

	for (const FAssetData& AssetData : StructAssets)
	{
		FVariableTypeInfo TypeInfo;
		TypeInfo.TypeName = AssetData.AssetName.ToString();
		TypeInfo.TypeIdentifier = AssetData.GetObjectPathString();
		TypeInfo.Category = TEXT("struct");
		TypeInfo.Description = TEXT("User-defined struct");

		OutTypes.Add(TypeInfo);
	}
}

void FN2CMcpSearchVariableTypesTool::CollectEnumTypes(TArray<FVariableTypeInfo>& OutTypes, bool bIncludeEngineTypes) const
{
	// Get variable type tree from K2 schema
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>> TypeTree;
	K2Schema->GetVariableTypeTree(TypeTree, ETypeTreeFilter::None);

	// Process the type tree for enum types
	ProcessTypeTree(TypeTree, OutTypes, TEXT("enum"), bIncludeEngineTypes);

	// Query asset registry for user-defined enums
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> EnumAssets;
	AssetRegistry.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("UserDefinedEnum")), EnumAssets);

	for (const FAssetData& AssetData : EnumAssets)
	{
		FVariableTypeInfo TypeInfo;
		TypeInfo.TypeName = AssetData.AssetName.ToString();
		TypeInfo.TypeIdentifier = AssetData.GetObjectPathString();
		TypeInfo.Category = TEXT("enum");
		TypeInfo.Description = TEXT("User-defined enumeration");

		// Try to load the enum to get values
		if (UUserDefinedEnum* Enum = Cast<UUserDefinedEnum>(AssetData.GetAsset()))
		{
			for (int32 i = 0; i < Enum->NumEnums() - 1; ++i) // -1 to skip MAX value
			{
				TypeInfo.EnumValues.Add(Enum->GetDisplayNameTextByIndex(i).ToString());
			}
		}

		OutTypes.Add(TypeInfo);
	}
}

void FN2CMcpSearchVariableTypesTool::ProcessTypeTree(const TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>>& TypeTree, 
	TArray<FVariableTypeInfo>& OutTypes, const FString& Category, bool bIncludeEngineTypes) const
{
	for (const TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>& TypeInfo : TypeTree)
	{
		if (!TypeInfo.IsValid())
		{
			continue;
		}

		// Determine category from pin type
		FString TypeCategory;
		if (TypeInfo->GetPinType(false).PinCategory == UEdGraphSchema_K2::PC_Object ||
			TypeInfo->GetPinType(false).PinCategory == UEdGraphSchema_K2::PC_Class ||
			TypeInfo->GetPinType(false).PinCategory == UEdGraphSchema_K2::PC_Interface)
		{
			TypeCategory = TEXT("class");
		}
		else if (TypeInfo->GetPinType(false).PinCategory == UEdGraphSchema_K2::PC_Struct)
		{
			TypeCategory = TEXT("struct");
		}
		else if (TypeInfo->GetPinType(false).PinCategory == UEdGraphSchema_K2::PC_Enum ||
				 TypeInfo->GetPinType(false).PinCategory == UEdGraphSchema_K2::PC_Byte)
		{
			TypeCategory = TEXT("enum");
		}
		else
		{
			continue; // Skip other categories
		}

		// Check if this matches our desired category
		if (Category != TEXT("all") && Category != TypeCategory)
		{
			continue;
		}

		// Get the object from the pin type
		UObject* TypeObject = TypeInfo->GetPinType(false).PinSubCategoryObject.Get();
		if (!TypeObject)
		{
			continue;
		}

		FString TypePath = TypeObject->GetPathName();
		
		// Filter engine types if requested
		if (!bIncludeEngineTypes && IsEngineType(TypePath))
		{
			continue;
		}

		// Create type info
		FVariableTypeInfo VarTypeInfo;
		VarTypeInfo.TypeName = TypeInfo->GetDescription().ToString();
		VarTypeInfo.TypeIdentifier = TypePath;
		VarTypeInfo.Category = TypeCategory;
		VarTypeInfo.Description = GetTypeDescription(TypeObject);

		// Handle class-specific info
		if (UClass* Class = Cast<UClass>(TypeObject))
		{
			VarTypeInfo.bIsAbstract = Class->HasAnyClassFlags(CLASS_Abstract);
			if (Class->GetSuperClass())
			{
				VarTypeInfo.ParentClass = Class->GetSuperClass()->GetPathName();
			}
		}
		// Handle enum-specific info
		else if (UEnum* Enum = Cast<UEnum>(TypeObject))
		{
			for (int32 i = 0; i < Enum->NumEnums() - 1; ++i) // -1 to skip MAX value
			{
				VarTypeInfo.EnumValues.Add(Enum->GetNameStringByIndex(i));
			}
		}

		OutTypes.Add(VarTypeInfo);

		// Process children recursively
		if (TypeInfo->Children.Num() > 0)
		{
			ProcessTypeTree(TypeInfo->Children, OutTypes, Category, bIncludeEngineTypes);
		}
	}
}

void FN2CMcpSearchVariableTypesTool::ProcessBlueprintAssets(const TArray<FAssetData>& BlueprintAssets, 
	TArray<FVariableTypeInfo>& OutTypes) const
{
	for (const FAssetData& AssetData : BlueprintAssets)
	{
		// Get the generated class tag
		FString GeneratedClassPath;
		if (!AssetData.GetTagValue("GeneratedClass", GeneratedClassPath))
		{
			continue;
		}

		// Extract class name from the path
		FString ClassName = AssetData.AssetName.ToString();
		
		FVariableTypeInfo TypeInfo;
		TypeInfo.TypeName = ClassName;
		TypeInfo.TypeIdentifier = GeneratedClassPath;
		TypeInfo.Category = TEXT("class");
		TypeInfo.Description = TEXT("Blueprint class");
		TypeInfo.bIsAbstract = false;

		// Get parent class if available
		FString ParentClassPath;
		if (AssetData.GetTagValue("ParentClass", ParentClassPath))
		{
			TypeInfo.ParentClass = ParentClassPath;
		}

		OutTypes.Add(TypeInfo);
	}
}

TArray<FN2CMcpSearchVariableTypesTool::FVariableTypeInfo> FN2CMcpSearchVariableTypesTool::FilterTypesBySearchTerm(
	const TArray<FVariableTypeInfo>& AllTypes, const FString& SearchTerm, int32 MaxResults) const
{
	TArray<FVariableTypeInfo> Results;

	// Tokenize search term for better matching
	TArray<FString> SearchTokens;
	SearchTerm.ParseIntoArray(SearchTokens, TEXT(" "), true);

	// Convert tokens to lowercase for case-insensitive search
	for (FString& Token : SearchTokens)
	{
		Token = Token.ToLower();
	}

	// Score-based filtering for better results
	struct FScoredType
	{
		FVariableTypeInfo TypeInfo;
		int32 Score;
	};
	TArray<FScoredType> ScoredTypes;

	for (const FVariableTypeInfo& TypeInfo : AllTypes)
	{
		FString LowerTypeName = TypeInfo.TypeName.ToLower();
		FString LowerDescription = TypeInfo.Description.ToLower();
		
		int32 Score = 0;
		bool bMatchesAll = true;

		// Check if all search tokens match
		for (const FString& Token : SearchTokens)
		{
			bool bTokenFound = false;
			
			// Exact match in type name gets highest score
			if (LowerTypeName == Token)
			{
				Score += 100;
				bTokenFound = true;
			}
			// Type name starts with token
			else if (LowerTypeName.StartsWith(Token))
			{
				Score += 50;
				bTokenFound = true;
			}
			// Type name contains token
			else if (LowerTypeName.Contains(Token))
			{
				Score += 25;
				bTokenFound = true;
			}
			// Description contains token
			else if (LowerDescription.Contains(Token))
			{
				Score += 10;
				bTokenFound = true;
			}

			if (!bTokenFound)
			{
				bMatchesAll = false;
				break;
			}
		}

		if (bMatchesAll && Score > 0)
		{
			ScoredTypes.Add({TypeInfo, Score});
		}
	}

	// Sort by score (highest first)
	ScoredTypes.Sort([](const FScoredType& A, const FScoredType& B)
	{
		return A.Score > B.Score;
	});

	// Take top results up to MaxResults
	for (int32 i = 0; i < FMath::Min(ScoredTypes.Num(), MaxResults); ++i)
	{
		Results.Add(ScoredTypes[i].TypeInfo);
	}

	return Results;
}

TSharedPtr<FJsonObject> FN2CMcpSearchVariableTypesTool::BuildJsonResult(const TArray<FVariableTypeInfo>& FilteredTypes) const
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> TypesArray;

	for (const FVariableTypeInfo& TypeInfo : FilteredTypes)
	{
		TSharedPtr<FJsonObject> TypeObject = MakeShareable(new FJsonObject);
		
		TypeObject->SetStringField(TEXT("typeName"), TypeInfo.TypeName);
		TypeObject->SetStringField(TEXT("typeIdentifier"), TypeInfo.TypeIdentifier);
		TypeObject->SetStringField(TEXT("category"), TypeInfo.Category);
		TypeObject->SetStringField(TEXT("description"), TypeInfo.Description);
		
		if (!TypeInfo.Icon.IsEmpty())
		{
			TypeObject->SetStringField(TEXT("icon"), TypeInfo.Icon);
		}

		if (TypeInfo.Category == TEXT("class"))
		{
			if (!TypeInfo.ParentClass.IsEmpty())
			{
				TypeObject->SetStringField(TEXT("parentClass"), TypeInfo.ParentClass);
			}
			TypeObject->SetBoolField(TEXT("isAbstract"), TypeInfo.bIsAbstract);
		}
		else if (TypeInfo.Category == TEXT("enum") && TypeInfo.EnumValues.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> EnumValuesArray;
			for (const FString& EnumValue : TypeInfo.EnumValues)
			{
				EnumValuesArray.Add(MakeShareable(new FJsonValueString(EnumValue)));
			}
			TypeObject->SetArrayField(TEXT("values"), EnumValuesArray);
		}

		TypesArray.Add(MakeShareable(new FJsonValueObject(TypeObject)));
	}

	Result->SetArrayField(TEXT("types"), TypesArray);
	Result->SetNumberField(TEXT("totalMatches"), FilteredTypes.Num());

	return Result;
}

bool FN2CMcpSearchVariableTypesTool::IsEngineType(const FString& TypePath) const
{
	return TypePath.StartsWith(TEXT("/Script/")) || 
		   TypePath.StartsWith(TEXT("/Engine/"));
}

FString FN2CMcpSearchVariableTypesTool::GetTypeDescription(const UObject* TypeObject) const
{
	if (!TypeObject)
	{
		return TEXT("");
	}

	if (const UClass* Class = Cast<UClass>(TypeObject))
	{
		// Try to get tooltip metadata
		if (Class->HasMetaData(TEXT("ToolTip")))
		{
			return Class->GetMetaData(TEXT("ToolTip"));
		}
		
		// Default descriptions for common classes
		if (Class == AActor::StaticClass())
		{
			return TEXT("Base class for all Actors that can be placed in a level");
		}
		else if (Class == UActorComponent::StaticClass())
		{
			return TEXT("Base class for components that can be attached to Actors");
		}
		
		return FString::Printf(TEXT("%s class"), Class->IsNative() ? TEXT("Native") : TEXT("Blueprint"));
	}
	else if (const UScriptStruct* Struct = Cast<UScriptStruct>(TypeObject))
	{
		if (Struct->HasMetaData(TEXT("ToolTip")))
		{
			return Struct->GetMetaData(TEXT("ToolTip"));
		}
		
		// Common struct descriptions
		FString StructName = Struct->GetName();
		if (StructName == TEXT("Vector"))
		{
			return TEXT("3D vector with X, Y, Z components");
		}
		else if (StructName == TEXT("Rotator"))
		{
			return TEXT("Rotation in 3D space (Pitch, Yaw, Roll)");
		}
		else if (StructName == TEXT("Transform"))
		{
			return TEXT("3D transformation (Location, Rotation, Scale)");
		}
		
		return TEXT("Structure");
	}
	else if (const UEnum* Enum = Cast<UEnum>(TypeObject))
	{
		if (Enum->HasMetaData(TEXT("ToolTip")))
		{
			return Enum->GetMetaData(TEXT("ToolTip"));
		}
		
		return TEXT("Enumeration");
	}

	return TEXT("");
}