// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UEdGraphNode;
class USMEditorInstance;
struct FSMNode_Base;

struct FSMConstructionConfiguration
{
	/** Construction scripts will not run if the blueprint is being compiled. */
	bool bSkipOnCompile = true;

	/** Requires the construction script refresh the slate node completely. */
	bool bFullRefreshNeeded = true;

	/** Signal not to the dirty the asset. This is ignored if the BP has structural modifications. */
	bool bDoNotDirty = false;

	/** If this is being triggered from a load. */
	bool bFromLoad = false;
};

/** Configuration options for conditionally compiling. */
struct FSMConditionalCompileConfiguration
{
	FSMConditionalCompileConfiguration() = default;

	/** Calls EnsureCachedDependenciesUpToDate. */
	bool bUpdateDependencies = true;

	/** Calls ForceRecreateProperties on all nodes. */
	bool bRecreateGraphProperties = false;

	/** If the BP should compile this tick. */
	bool bCompileNow = false;
};

/**
 * Disable editor construction scripts only within the scope of this struct.
 */
struct SMSYSTEMEDITOR_API FSMDisableConstructionScriptsOnScope
{
	FSMDisableConstructionScriptsOnScope();
	~FSMDisableConstructionScriptsOnScope();

	void Cancel();

private:
	bool bOriginallyEnabled;
};
