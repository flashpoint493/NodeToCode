// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Prompts/Implementations/N2CMcpCodeGenerationPrompt.h"
#include "MCP/Resources/N2CMcpResourceManager.h"
#include "Utils/N2CLogger.h"

FMcpPromptDefinition FN2CMcpCodeGenerationPrompt::GetDefinition() const
{
	FMcpPromptDefinition Definition;
	Definition.Name = TEXT("generate-code");
	Definition.Description = TEXT("Generate code from the current Blueprint with customization options");
	
	// Language argument
	FMcpPromptArgument LangArg;
	LangArg.Name = TEXT("language");
	LangArg.Description = TEXT("Target programming language (cpp, python, csharp, javascript, swift, pseudocode)");
	LangArg.bRequired = false;
	Definition.Arguments.Add(LangArg);
	
	// Style argument
	FMcpPromptArgument StyleArg;
	StyleArg.Name = TEXT("style");
	StyleArg.Description = TEXT("Coding style preference (verbose, concise, documented)");
	StyleArg.bRequired = false;
	Definition.Arguments.Add(StyleArg);
	
	// Optimization argument
	FMcpPromptArgument OptimizationArg;
	OptimizationArg.Name = TEXT("optimization");
	OptimizationArg.Description = TEXT("Optimization level (readability, performance, size)");
	OptimizationArg.bRequired = false;
	Definition.Arguments.Add(OptimizationArg);
	
	return Definition;
}

FMcpPromptResult FN2CMcpCodeGenerationPrompt::GetPrompt(const TMap<FString, FString>& Arguments)
{
	return ExecuteOnGameThread<FMcpPromptResult>([Arguments]() -> FMcpPromptResult
	{
		FMcpPromptResult Result;
		
		// Get current Blueprint as resource
		FString BlueprintUri = TEXT("nodetocode://blueprint/current");
		FMcpResourceContents BlueprintResource = FN2CMcpResourceManager::Get().ReadResource(BlueprintUri);
		
		// Build the prompt with context
		FString Language = Arguments.FindRef(TEXT("language"));
		if (Language.IsEmpty())
		{
			Language = TEXT("C++"); // Default to C++
		}
		
		FString Style = Arguments.FindRef(TEXT("style"));
		if (Style.IsEmpty())
		{
			Style = TEXT("clear and well-documented");
		}
		
		FString Optimization = Arguments.FindRef(TEXT("optimization"));
		if (Optimization.IsEmpty())
		{
			Optimization = TEXT("readability");
		}
		
		// Create the prompt description
		Result.Description = FString::Printf(
			TEXT("Generate %s code from the current Blueprint with %s style optimized for %s"),
			*Language, *Style, *Optimization
		);
		
		// Create the user message
		FMcpPromptMessage UserMsg;
		UserMsg.Role = TEXT("user");
		UserMsg.Content.Type = TEXT("text");
		UserMsg.Content.Text = FString::Printf(
			TEXT("Please translate this Unreal Engine Blueprint to %s code. ")
			TEXT("Make the code %s and optimize for %s. ")
			TEXT("Focus on correctness and Unreal Engine best practices. ")
			TEXT("Include appropriate comments and follow standard %s conventions."),
			*Language, *Style, *Optimization, *Language
		);
		Result.Messages.Add(UserMsg);
		
		// Add the Blueprint as a resource reference
		FMcpPromptMessage ResourceMsg;
		ResourceMsg.Role = TEXT("user");
		ResourceMsg.Content.Type = TEXT("resource");
		ResourceMsg.Content.Resource = BlueprintResource;
		Result.Messages.Add(ResourceMsg);
		
		return Result;
	});
}

FMcpPromptDefinition FN2CMcpBlueprintAnalysisPrompt::GetDefinition() const
{
	FMcpPromptDefinition Definition;
	Definition.Name = TEXT("analyze-blueprint");
	Definition.Description = TEXT("Analyze a Blueprint's structure, complexity, and potential issues");
	
	// Focus argument
	FMcpPromptArgument FocusArg;
	FocusArg.Name = TEXT("focus");
	FocusArg.Description = TEXT("Analysis focus (complexity, performance, best-practices, all)");
	FocusArg.bRequired = false;
	Definition.Arguments.Add(FocusArg);
	
	// Detail level argument
	FMcpPromptArgument DetailArg;
	DetailArg.Name = TEXT("detail");
	DetailArg.Description = TEXT("Level of detail (summary, detailed, comprehensive)");
	DetailArg.bRequired = false;
	Definition.Arguments.Add(DetailArg);
	
	return Definition;
}

FMcpPromptResult FN2CMcpBlueprintAnalysisPrompt::GetPrompt(const TMap<FString, FString>& Arguments)
{
	return ExecuteOnGameThread<FMcpPromptResult>([Arguments]() -> FMcpPromptResult
	{
		FMcpPromptResult Result;
		
		// Get current Blueprint as resource
		FString BlueprintUri = TEXT("nodetocode://blueprint/current");
		FMcpResourceContents BlueprintResource = FN2CMcpResourceManager::Get().ReadResource(BlueprintUri);
		
		// Build the prompt with context
		FString Focus = Arguments.FindRef(TEXT("focus"));
		if (Focus.IsEmpty())
		{
			Focus = TEXT("all aspects");
		}
		
		FString Detail = Arguments.FindRef(TEXT("detail"));
		if (Detail.IsEmpty())
		{
			Detail = TEXT("detailed");
		}
		
		// Create the prompt description
		Result.Description = FString::Printf(
			TEXT("Analyze Blueprint focusing on %s with %s level of detail"),
			*Focus, *Detail
		);
		
		// Create the user message
		FMcpPromptMessage UserMsg;
		UserMsg.Role = TEXT("user");
		UserMsg.Content.Type = TEXT("text");
		UserMsg.Content.Text = FString::Printf(
			TEXT("Please analyze this Unreal Engine Blueprint. ")
			TEXT("Focus on %s and provide a %s analysis. ")
			TEXT("Identify potential issues, suggest improvements, and highlight best practices. ")
			TEXT("Consider performance implications and maintainability."),
			*Focus, *Detail
		);
		Result.Messages.Add(UserMsg);
		
		// Add the Blueprint as a resource reference
		FMcpPromptMessage ResourceMsg;
		ResourceMsg.Role = TEXT("user");
		ResourceMsg.Content.Type = TEXT("resource");
		ResourceMsg.Content.Resource = BlueprintResource;
		Result.Messages.Add(ResourceMsg);
		
		return Result;
	});
}

FMcpPromptDefinition FN2CMcpRefactorPrompt::GetDefinition() const
{
	FMcpPromptDefinition Definition;
	Definition.Name = TEXT("refactor-blueprint");
	Definition.Description = TEXT("Suggest refactoring improvements for Blueprint code");
	
	// Goal argument
	FMcpPromptArgument GoalArg;
	GoalArg.Name = TEXT("goal");
	GoalArg.Description = TEXT("Refactoring goal (simplify, optimize, modularize, clean)");
	GoalArg.bRequired = false;
	Definition.Arguments.Add(GoalArg);
	
	// Language argument
	FMcpPromptArgument LangArg;
	LangArg.Name = TEXT("language");
	LangArg.Description = TEXT("Target language for refactored code (cpp, python, etc.)");
	LangArg.bRequired = false;
	Definition.Arguments.Add(LangArg);
	
	return Definition;
}

FMcpPromptResult FN2CMcpRefactorPrompt::GetPrompt(const TMap<FString, FString>& Arguments)
{
	return ExecuteOnGameThread<FMcpPromptResult>([Arguments]() -> FMcpPromptResult
	{
		FMcpPromptResult Result;
		
		// Get current Blueprint as resource
		FString BlueprintUri = TEXT("nodetocode://blueprint/current");
		FMcpResourceContents BlueprintResource = FN2CMcpResourceManager::Get().ReadResource(BlueprintUri);
		
		// Build the prompt with context
		FString Goal = Arguments.FindRef(TEXT("goal"));
		if (Goal.IsEmpty())
		{
			Goal = TEXT("improve overall quality");
		}
		
		FString Language = Arguments.FindRef(TEXT("language"));
		if (Language.IsEmpty())
		{
			Language = TEXT("C++");
		}
		
		// Create the prompt description
		Result.Description = FString::Printf(
			TEXT("Refactor Blueprint to %s, targeting %s"),
			*Goal, *Language
		);
		
		// Create the user message
		FMcpPromptMessage UserMsg;
		UserMsg.Role = TEXT("user");
		UserMsg.Content.Type = TEXT("text");
		UserMsg.Content.Text = FString::Printf(
			TEXT("Please refactor this Unreal Engine Blueprint to %s. ")
			TEXT("Target language is %s. ")
			TEXT("Provide specific refactoring suggestions with code examples. ")
			TEXT("Explain the benefits of each suggested change. ")
			TEXT("Maintain functionality while improving code quality."),
			*Goal, *Language
		);
		Result.Messages.Add(UserMsg);
		
		// Add the Blueprint as a resource reference
		FMcpPromptMessage ResourceMsg;
		ResourceMsg.Role = TEXT("user");
		ResourceMsg.Content.Type = TEXT("resource");
		ResourceMsg.Content.Resource = BlueprintResource;
		Result.Messages.Add(ResourceMsg);
		
		return Result;
	});
}