// Copyright 2026 K-Studio. All Rights Reserved.

// KClonerExpressionEvaluator.cpp
// uses ExprTk for math expressions
// kept in its own file cuz ExprTk is a BEAST of a header-only lib

#include "KClonerExpressionEvaluator.h"

// disable optim in debug or youll wait forever for compiles
#if !UE_BUILD_SHIPPING
UE_DISABLE_OPTIMIZATION
#endif

#pragma warning(push)
#pragma warning(disable: 4244) // conversion from 'double' to 'float'
#pragma warning(disable: 4267) // conversion from 'size_t' to 'int'
#pragma warning(disable: 4127) // conditional expression is constant
#pragma warning(disable: 4702) // unreachable code
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro
#pragma warning(disable: 4800) // Implicit conversion from 'type' to bool

// use floats not doubles - faster and good enough for mograph
#define exprtk_disable_string_capabilities
#define exprtk_disable_rtl_io_file
#define exprtk_disable_caseinsensitivity

// UE defines these as macros and ExprTk uses them as function names
// so we gotta hide em temporarily
#pragma push_macro("check")
#undef check
#pragma push_macro("verify")
#undef verify

// wrap the include so windows platform types dont conflict
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include "exprtk.hpp"

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#pragma pop_macro("verify")
#pragma pop_macro("check")

#pragma warning(pop)

#if !UE_BUILD_SHIPPING
UE_ENABLE_OPTIMIZATION
#endif

// hidden implementation - keeps ExprTk types out of header
struct FExprTkImpl
{
	typedef exprtk::symbol_table<float> SymbolTable;
	typedef exprtk::expression<float> Expression;
	typedef exprtk::parser<float> Parser;

	SymbolTable Symbols;
	Expression Expr;

  // the variables we expose to expressions
	float t = 0.0f;      // Time
	float i = 0.0f;      // Index
	float n = 0.0f;      // Count

	float x = 0.0f;      // Position X
	float y = 0.0f;      // Position Y
	float z = 0.0f;      // Position Z

	float rx = 0.0f;     // Rotation X (pitch)
	float ry = 0.0f;     // Rotation Y (yaw)
	float rz = 0.0f;     // Rotation Z (roll)

	float sx = 1.0f;     // Scale X
	float sy = 1.0f;     // Scale Y
	float sz = 1.0f;     // Scale Z

  // user-defined slider vars v0, v1, v2...
	TArray<float> v;     // v0, v1, v2, ...

	FExprTkImpl(int32 NumVars)
	{
    // alloc user vars array
		v.SetNum(FMath::Max(NumVars, 1));
		for (int32 idx = 0; idx < v.Num(); ++idx)
		{
			v[idx] = 0.0f;
		}

    // register all the vars with ExprTk
		Symbols.add_variable("t", t);
		Symbols.add_variable("i", i);
		Symbols.add_variable("n", n);

		Symbols.add_variable("x", x);
		Symbols.add_variable("y", y);
		Symbols.add_variable("z", z);

		Symbols.add_variable("rx", rx);
		Symbols.add_variable("ry", ry);
		Symbols.add_variable("rz", rz);

		Symbols.add_variable("sx", sx);
		Symbols.add_variable("sy", sy);
		Symbols.add_variable("sz", sz);

    // v0, v1, v2, ... for user sliders
		for (int32 idx = 0; idx < v.Num(); ++idx)
		{
			FString VarName = FString::Printf(TEXT("v%d"), idx);
			Symbols.add_variable(TCHAR_TO_ANSI(*VarName), v[idx]);
		}

    // constants - pi and e cuz people always need em
		Symbols.add_constant("pi", UE_PI);
		Symbols.add_constant("e", UE_EULERS_NUMBER);

    // hook it up
		Expr.register_symbol_table(Symbols);
	}
};

// ============================================================
// Compiled Expression wrapper
// ============================================================

FKClonerCompiledExpression::FKClonerCompiledExpression()
	: ImplPtr(nullptr)
	, bIsValid(false)
{
}

FKClonerCompiledExpression::~FKClonerCompiledExpression()
{
	if (ImplPtr)
	{
		delete static_cast<FExprTkImpl*>(ImplPtr);
		ImplPtr = nullptr;
	}
}

FKClonerCompiledExpression::FKClonerCompiledExpression(FKClonerCompiledExpression&& Other) noexcept
	: ImplPtr(Other.ImplPtr)
	, bIsValid(Other.bIsValid)
	, ErrorMessage(MoveTemp(Other.ErrorMessage))
{
	Other.ImplPtr = nullptr;
	Other.bIsValid = false;
}

FKClonerCompiledExpression& FKClonerCompiledExpression::operator=(FKClonerCompiledExpression&& Other) noexcept
{
	if (this != &Other)
	{
		if (ImplPtr)
		{
			delete static_cast<FExprTkImpl*>(ImplPtr);
		}
		ImplPtr = Other.ImplPtr;
		bIsValid = Other.bIsValid;
		ErrorMessage = MoveTemp(Other.ErrorMessage);
		Other.ImplPtr = nullptr;
		Other.bIsValid = false;
	}
	return *this;
}

// ============================================================
// Static evaluator interface
// ============================================================

bool FKClonerExpressionEvaluator::Compile(const FString& Expression, int32 NumVariables, FKClonerCompiledExpression& OutCompiled)
{
  // nuke old impl if recompiling
	if (OutCompiled.ImplPtr)
	{
		delete static_cast<FExprTkImpl*>(OutCompiled.ImplPtr);
		OutCompiled.ImplPtr = nullptr;
	}
	OutCompiled.bIsValid = false;
	OutCompiled.ErrorMessage.Empty();

  // empty = noop, not an error
	if (Expression.IsEmpty())
	{
		OutCompiled.bIsValid = true;
		return true;
	}

  // alloc the impl
	FExprTkImpl* Impl = new FExprTkImpl(NumVariables);
	OutCompiled.ImplPtr = Impl;

  // try to compile with ExprTk
	FExprTkImpl::Parser Parser;
	
	// Convert to ANSI for ExprTk
	std::string ExprStr = TCHAR_TO_ANSI(*Expression);
	
	bool bSuccess = Parser.compile(ExprStr, Impl->Expr);
	
	if (!bSuccess)
	{
    // shit, parse failed - grab error msg
		OutCompiled.ErrorMessage = FString::Printf(TEXT("Expression error: %s"), 
			ANSI_TO_TCHAR(Parser.error().c_str()));
		
    // dump detailed errors to log for debugging
		for (std::size_t i = 0; i < Parser.error_count(); ++i)
		{
			auto Error = Parser.get_error(i);
			UE_LOG(LogTemp, Warning, TEXT("K-Cloner Expression Error [%d]: %s at position %d"),
				(int32)i,
				ANSI_TO_TCHAR(Error.diagnostic.c_str()),
				(int32)Error.token.position);
		}

		delete Impl;
		OutCompiled.ImplPtr = nullptr;
		return false;
	}

	OutCompiled.bIsValid = true;
	return true;
}

void FKClonerExpressionEvaluator::Evaluate(
	FKClonerCompiledExpression& Compiled,
	float Time,
	int32 Index,
	int32 Count,
	FVector& InOutPosition,
	FVector& InOutRotation,
	FVector& InOutScale,
	const TArray<float>& Variables)
{
	if (!Compiled.bIsValid || !Compiled.ImplPtr)
	{
		return;
	}

	FExprTkImpl* Impl = static_cast<FExprTkImpl*>(Compiled.ImplPtr);

  // shove inputs into the impl
	Impl->t = Time;
	Impl->i = static_cast<float>(Index);
	Impl->n = static_cast<float>(Count);

	Impl->x = InOutPosition.X;
	Impl->y = InOutPosition.Y;
	Impl->z = InOutPosition.Z;

	Impl->rx = InOutRotation.X;
	Impl->ry = InOutRotation.Y;
	Impl->rz = InOutRotation.Z;

	Impl->sx = InOutScale.X;
	Impl->sy = InOutScale.Y;
	Impl->sz = InOutScale.Z;

  // copy user slider values
	for (int32 idx = 0; idx < FMath::Min(Variables.Num(), Impl->v.Num()); ++idx)
	{
		Impl->v[idx] = Variables[idx];
	}

  // RUN IT
	Impl->Expr.value();

  // pull outputs back out
	InOutPosition.X = Impl->x;
	InOutPosition.Y = Impl->y;
	InOutPosition.Z = Impl->z;

	InOutRotation.X = Impl->rx;
	InOutRotation.Y = Impl->ry;
	InOutRotation.Z = Impl->rz;

	InOutScale.X = Impl->sx;
	InOutScale.Y = Impl->sy;
	InOutScale.Z = Impl->sz;
}

bool FKClonerExpressionEvaluator::Validate(const FString& Expression, FString& OutError)
{
	if (Expression.IsEmpty())
	{
		return true;
	}

	FKClonerCompiledExpression TempCompiled;
	bool bResult = Compile(Expression, 10, TempCompiled);  // Allow up to 10 user vars
	
	if (!bResult)
	{
		OutError = TempCompiled.GetError();
	}

	return bResult;
}
