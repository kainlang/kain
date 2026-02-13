// Copyright 2026 K-Studio. All Rights Reserved.

// KClonerExpressionEvaluator.h
// Wrapper around ExprTk for evaluating math expressions in modifiers
// Compiled separately to minimize build time impact

#pragma once

#include "CoreMinimal.h"

/**
 * Compiled expression ready for fast evaluation.
 * This is an opaque handle - implementation hidden in cpp.
 */
class FKClonerCompiledExpression
{
public:
	FKClonerCompiledExpression();
	~FKClonerCompiledExpression();

	// Non-copyable, movable
	FKClonerCompiledExpression(const FKClonerCompiledExpression&) = delete;
	FKClonerCompiledExpression& operator=(const FKClonerCompiledExpression&) = delete;
	FKClonerCompiledExpression(FKClonerCompiledExpression&& Other) noexcept;
	FKClonerCompiledExpression& operator=(FKClonerCompiledExpression&& Other) noexcept;

	bool IsValid() const { return bIsValid; }
	const FString& GetError() const { return ErrorMessage; }

private:
	friend class FKClonerExpressionEvaluator;
	void* ImplPtr = nullptr;  // Hides ExprTk types
	bool bIsValid = false;
	FString ErrorMessage;
};

/**
 * Expression evaluator for K-Cloner modifiers.
 * Uses ExprTk under the hood for fast math expression evaluation.
 * 
 * Thread-safe for evaluation (not for compilation).
 */
class KCLONER_API FKClonerExpressionEvaluator
{
public:
	/**
	 * Compile an expression string into a reusable compiled form.
	 * 
	 * @param Expression The expression string (e.g., "x += sin(t + i * 0.1) * v0;")
	 * @param NumVariables Number of user variables (v0, v1, v2, ...)
	 * @param OutCompiled The compiled expression output
	 * @return true if compilation succeeded
	 */
	static bool Compile(const FString& Expression, int32 NumVariables, FKClonerCompiledExpression& OutCompiled);

	/**
	 * Evaluate a compiled expression.
	 * 
	 * @param Compiled The pre-compiled expression
	 * @param Time Current time in seconds
	 * @param Index Instance index
	 * @param Count Total instance count
	 * @param InOutPosition Current position (modified in-place)
	 * @param InOutRotation Current rotation in degrees (modified in-place)
	 * @param InOutScale Current scale (modified in-place)
	 * @param Variables User-defined variable values (v0, v1, v2, ...)
	 */
	static void Evaluate(
		FKClonerCompiledExpression& Compiled,
		float Time,
		int32 Index,
		int32 Count,
		FVector& InOutPosition,
		FVector& InOutRotation,
		FVector& InOutScale,
		const TArray<float>& Variables
	);

	/**
	 * Quick validation without full compilation.
	 * 
	 * @param Expression The expression to validate
	 * @param OutError Error message if invalid
	 * @return true if the expression appears valid
	 */
	static bool Validate(const FString& Expression, FString& OutError);
};
