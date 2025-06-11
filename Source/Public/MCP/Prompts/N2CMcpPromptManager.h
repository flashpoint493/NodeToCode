// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "N2CMcpPromptTypes.h"

/**
 * Entry for a registered prompt with its handler
 */
struct FMcpPromptEntry
{
	FMcpPromptDefinition Definition;
	FMcpPromptGetDelegate Handler;
	bool bRequiresGameThread = false;
};

/**
 * Manages MCP prompts for the NodeToCode plugin
 * Singleton pattern for global prompt management
 */
class NODETOCODE_API FN2CMcpPromptManager
{
public:
	/**
	 * Gets the singleton instance
	 */
	static FN2CMcpPromptManager& Get();

	/**
	 * Registers a prompt
	 * @param Definition The prompt definition
	 * @param Handler The handler delegate for getting the prompt
	 * @param bRequiresGameThread Whether the handler must run on game thread
	 * @return true if successfully registered
	 */
	bool RegisterPrompt(const FMcpPromptDefinition& Definition, 
	                   const FMcpPromptGetDelegate& Handler,
	                   bool bRequiresGameThread = false);

	/**
	 * Unregisters a prompt
	 * @param Name The name of the prompt to unregister
	 * @return true if successfully unregistered
	 */
	bool UnregisterPrompt(const FString& Name);

	/**
	 * Lists all available prompts
	 * @param Cursor Optional cursor for pagination (not implemented in MVP)
	 * @return Array of prompt definitions
	 */
	TArray<FMcpPromptDefinition> ListPrompts(const FString& Cursor = TEXT("")) const;

	/**
	 * Gets a prompt with the provided arguments
	 * @param Name The name of the prompt
	 * @param Arguments The arguments to pass to the prompt
	 * @return The prompt result
	 */
	FMcpPromptResult GetPrompt(const FString& Name, const FMcpPromptArguments& Arguments);

	/**
	 * Checks if a prompt exists
	 * @param Name The prompt name to check
	 * @return true if the prompt exists
	 */
	bool IsPromptRegistered(const FString& Name) const;

	/**
	 * Validates arguments for a prompt
	 * @param Name The prompt name
	 * @param Arguments The arguments to validate
	 * @param OutError Error message if validation fails
	 * @return true if arguments are valid
	 */
	bool ValidatePromptArguments(const FString& Name, const FMcpPromptArguments& Arguments, FString& OutError) const;

	/**
	 * Clears all registered prompts
	 */
	void ClearAllPrompts();

private:
	// Private constructor for singleton
	FN2CMcpPromptManager() = default;

	// Non-copyable
	FN2CMcpPromptManager(const FN2CMcpPromptManager&) = delete;
	FN2CMcpPromptManager& operator=(const FN2CMcpPromptManager&) = delete;

	/** Prompts mapped by name */
	TMap<FString, FMcpPromptEntry> Prompts;

	/** Thread safety */
	mutable FCriticalSection PromptLock;
};