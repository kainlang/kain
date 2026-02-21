// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ISMStateMachineInterface.h"
#include "SMGraphProperty_Base.h"
#include "SMInputTypes.h"
#include "SMLogging.h"

#include "SMNodeInstance.generated.h"

class ISMEditorGraphNodeInterface;
class UInputComponent;
class USMInstance;
class USMNodeInstance;
class USMStateMachineInstance;
class UTexture2D;
class UWorld;
struct FSMNode_Base;

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("SMNodeInstances"), STAT_NodeInstances, STATGROUP_LogicDriver, SMSYSTEM_API);

/* Gets the value as defined on the struct. */
#define GET_NODE_STRUCT_VALUE(StructType, StructVariable) \
	if (StructType* StructOwner = GetOwningNodeAs<StructType>()) \
	{ \
		return StructOwner->StructVariable; \
	} \

/* Gets the node property value. */
#define GET_NODE_DEFAULT_VALUE_DIF_VAR(StructType, InstanceVariable, StructVariable) \
		GET_NODE_STRUCT_VALUE(StructType, StructVariable) \
		return InstanceVariable;

/* Gets the node property value. */
#define GET_NODE_DEFAULT_VALUE(StructType, Variable) \
		GET_NODE_DEFAULT_VALUE_DIF_VAR(StructType, Variable, Variable);


/* Sets both the node instance variable and the struct owner variable to the given value. */
#define SET_NODE_DEFAULT_VALUE_DIF_VAR(StructType, InstanceVariable, StructVariable, Value) \
		InstanceVariable = Value; \
		if (StructType* StructOwner = GetOwningNodeAs<StructType>()) \
		{ \
			StructOwner->StructVariable = Value; \
		}

/* Sets both the node instance variable and the struct owner variable to the given value. */
#define SET_NODE_DEFAULT_VALUE(StructType, Variable, Value) \
		SET_NODE_DEFAULT_VALUE_DIF_VAR(StructType, Variable, Variable, Value);

/**
 * This information will be viewable when selecting new nodes or hovering over nodes.
 */
USTRUCT()
struct SMSYSTEM_API FSMNodeDescription
{
	GENERATED_USTRUCT_BODY()

	/** The name of this node type. */
	UPROPERTY(EditDefaultsOnly, Category = "General", meta = (InstancedTemplate))
	FName Name;

	/** Which category this should fall under. */
	UPROPERTY(EditDefaultsOnly, Category = "General", meta = (InstancedTemplate))
	FText Category;

	/** The tooltip when selecting the action. */
	UPROPERTY(EditDefaultsOnly, Category = "General", meta = (InstancedTemplate, MultiLine = true))
	FText Description;
};

UENUM(BlueprintType)
enum class ESMExecutionEnvironment : uint8
{
	/**
	 * This node is running for an editor state machine. This is generally only valid
	 * during editor time construction scripts. Use this to allow the construction script
	 * to set default values during compile instead of recalculating values during run-time.
	 *
	 * When running with Editor Execution, only default values entered into public properties
	 * from the state machine graph will be available. Connecting a variable to a public
	 * property within the state machine graph will not evaluate until run-time. Additionally,
	 * the owning SMInstance will not be available at editor time since that is the class
	 * being compiled.
	 *
	 * To configure editor construction script settings, go under
	 * Project Settings -> Logic Driver -> Editor Node Construction Script Setting.
	 */
	EditorExecution,
	/**
	 * This node is running in a simulation or game.
	 */
	GameExecution
};

/**
 * Enumerates UEdGraphNode validity.
 */
UENUM()
enum class ESMValidEditorNode : uint8
{
	/** This is a valid UEdGraphNode obtained during editor design time. */
	IsValidEditorNode,
	/** This is not a valid UEdGraphNode which means execution is most likely running from a new instantiation or a development run-time. */
	IsNotValidEditorNode
};

UENUM(BlueprintType)
enum class ESMCompilerLogType : uint8
{
	/** An informational message. */
	Note,
	/** Warn of an issue but still allow the blueprint to compile. */
	Warning,
	/** An error will prevent the blueprint from compiling. */
	Error
};

#if WITH_EDITOR
DECLARE_DELEGATE_TwoParams(FOnCompilerLogSignature, ESMCompilerLogType /* Severity */, const FString& /* Message */);
DECLARE_DELEGATE_SevenParams(FOnCompilerLogPropertySignature, const FName& /* PropertyName */, int32 /*ArrayIndex*/, const FString& /* Message */,
	ESMCompilerLogType /* Severity */, bool /* bHighlight */, bool /* bSilent */, const USMNodeInstance* /* NodeInstance */);
#endif

/**
 * The compiler log is only valid during the compile process of a state machine blueprint.
 */
UCLASS(NotBlueprintable, NotBlueprintType, Transient)
class SMSYSTEM_API USMCompilerLog : public UObject
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	FOnCompilerLogSignature OnCompilerLogEvent;
	FOnCompilerLogPropertySignature OnCompilerLogPropertyEvent;
#endif

	/**
	 * Output a message to the compiler log.
	 *
	 * @param Severity The log level to use.
	 * @param Message The text to output to the log.
	 */
	UFUNCTION(BlueprintCallable, Category = "LogicDriver|Compile", meta = (DevelopmentOnly))
	void Log(ESMCompilerLogType Severity, const FString& Message);

	/**
	 * Output a message to an exposed property on a node.
	 *
	 * @param PropertyName The name of the property to output to.
	 * @param NodeInstance The node instance the property belongs to. Most likely 'this' or 'self'. Automatically set when called in blueprints.
	 * @param Message The text to output to the log.
	 * @param Severity The log level to use.
	 * @param bHighlight Whether the property should be highlighted in addition to an info icon.
	 * @param bSilent When true the main compiler log won't be written to, allowing the blueprint to compile even when the severity is an error.
	 * @param ArrayIndex The index of the element if an array. Leave at -1 to include all elements in the array.
	 */
	UFUNCTION(BlueprintCallable, Category = "LogicDriver|Compile", meta = (DevelopmentOnly, DefaultToSelf = "NodeInstance", HidePin = "NodeInstance", AdvancedDisplay = 4))
	void LogProperty(FName PropertyName, const USMNodeInstance* NodeInstance, const FString& Message, ESMCompilerLogType Severity, bool bHighlight = false, bool bSilent = false, int32 ArrayIndex = -1);
};

/**
 * [Logic Driver] The abstract base node class all state machine nodes derive from.
 */
UCLASS(Abstract, NotBlueprintable, BlueprintType, ClassGroup = LogicDriver, hideCategories = (SMNodeInstance), meta = (DisplayName = "Node Class Base"))
class SMSYSTEM_API USMNodeInstance : public UObject, public ISMInstanceInterface
{
	GENERATED_BODY()
	
public:
	USMNodeInstance();
	
	// UObject
	virtual void Serialize(FArchive& Ar) override;
	virtual UWorld* GetWorld() const override;
	virtual void BeginDestroy() override;
	// ~UObject

	// ISMInstanceInterface
	/** The object which this node is running for. Determined by the owning state machine. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	virtual UObject* GetContext() const override;
	// ~ISMInstanceInterface

	/** Perform native initialization. Called before On[Node]Initialized for all node types. */
	virtual void NativeInitialize();

	/** Perform native cleanup. Called after On[Node]Shutdown for all node types. */
	virtual void NativeShutdown();

	/**
	 * Called when the immediate owning state machine blueprint is starting. If this is part of a reference
	 * then it will be called when the reference starts. If this is for a state machine node
	 * then it will only be called when the top level state machine starts.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnRootStateMachineStart();

	/**
	 * Called when the immediate owning state machine blueprint is stopping. If this is part of a reference
	 * then it will be called when the reference stops. If this is for a state machine node
	 * then it will only be called when the top level state machine stops.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnRootStateMachineStop();
	
	/** Signal the construction script should start. */
	void RunConstructionScript();

	/** The name of the protected ConstructionScript function. */
	static FName GetConstructionScriptFunctionName() { return GET_FUNCTION_NAME_CHECKED(USMNodeInstance, ConstructionScript); }

private:
	/** Restore specific archetype values. Currently only handles certain construction values which may be modified. */
	void RestoreArchetypeValuesPriorToConstruction();
	
protected:
	virtual void OnRootStateMachineStart_Implementation() {}
	virtual void OnRootStateMachineStop_Implementation() {}
	
	/**
	 * A construction script that runs in the editor when the blueprint is modified.
	 * During run-time it will run after all nodes have instantiated.
	 *
	 * CAUTION:
	 * Any values set here while running with editor execution will replace the
	 * instance default values in state machine graphs when that state machine is compiled.
	 *
	 * When running with Editor Execution, only default values entered into public properties
	 * from the state machine graph will be available. Connecting a variable to a public
	 * property within the state machine graph will not evaluate until run-time. Additionally,
	 * the owning SMInstance will not be available at editor time since that is the class
	 * being compiled.
	 *
	 * If construction scripts aren't working in the editor, you may need to adjust your
	 * settings to `Standard`.
	 * 
	 * To configure editor construction script settings, go under
	 * Project Settings -> Logic Driver -> Editor Node Construction Script Setting.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Default")
	void ConstructionScript();

#if WITH_EDITORONLY_DATA
public:
	/** Checks for user override on native classes if editor construction scripts should be skipped. */
	bool ShouldSkipNativeEditorConstructionScripts() const { return bSkipNativeEditorConstructionScripts; }

protected:
	/**
	 * Tell the state machine compiler to skip editor construction scripts for this native class.
	 * 
	 * This is primarily an optimization to improve performance when construction scripts aren't used on native classes.
	 * For blueprint classes the state machine compiler can easily check if there is logic defined, but not for native classes.
	 * 
	 * Children class may override this behavior, and blueprint children will override if there is any construction script
	 * logic defined.
	 */
	UPROPERTY()
	bool bSkipNativeEditorConstructionScripts;

private:
	friend class USMGraphNode_StateNode;
	friend class USMGraphNode_TransitionEdge;
	
	// If editor construction scripts are defined; set during bp compile.
	// Should always match CDO. Variable is looked up by name in several places.
	UPROPERTY()
	bool bHasEditorConstructionScripts = true;
	
#endif
	
private:
	friend struct FSMNode_Base;

	// If run-time construction scripts are defined; set during bp compile.
	// Should always match CDO. Variable is looked up by name in several places.
	UPROPERTY()
	bool bHasGameConstructionScripts = true;
	
public:
	/**
	 * Retrieve an owning blueprint state machine.
	 *
	 * @warning This call is unavailable during editor construction scripts because the USMInstance class is in
	 * the process of being regenerated.
	 *
	 * @param bTopMostInstance If the state machine is a reference return the top most instance.
	 * @return The state machine instance this node is running for.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMInstance* GetStateMachineInstance(bool bTopMostInstance = false) const;

	/** Set during initialization of the state machine. */
	void SetOwningNode(FSMNode_Base* Node, bool bInIsEditorExecution = false);

	/** Reference to the owning node within a state machine. */
	const FSMNode_Base* GetOwningNode() const { return OwningNode; }

	/** Reference the owning struct node as a given type. */
	template<typename T>
	T* GetOwningNodeAs() const { return static_cast<T*>(OwningNode); }
	
	/** Some nodes such as references may have special handling for returning a container node. */
	virtual const FSMNode_Base* GetOwningNodeContainer() const { return GetOwningNode(); }
	
	/** The instance of the direct state machine node this node is part of. Every node except the root state machine has an owning state machine node. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMStateMachineInstance* GetOwningStateMachineNodeInstance() const;

	/** Return the server interface if there is one. This may be null. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Network")
	TScriptInterface<ISMStateMachineNetworkedInterface> GetNetworkInterface() const;
	
	/** The current time spent in the state. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	virtual float GetTimeInState() const;

	/**
	 * @deprecated Use either USMStateInstance_Base::IsEndState(), or USMStateMachineInstance::IsStateMachineInEndState() depending on use case.
	 *
	 * Legacy method available to all node types determining if the state machine node is in an end state or a
	 * state node is an end state.
	 *
	 * When called from state machine nodes it checks if the state machine is in an end state.
	 * When called from other state types it is equivalent to calling USMStateInstance_Base::IsEndState() on the state.
	 * When called from transitions it is equivalent to calling IsInEndState() on the previous state.
	 *
	 * Note that overloading this method has no effect on whether a state is considered an end state or not. This method
	 * is not used internally in the plugin and only meant for observers.
	 */
	UE_DEPRECATED(5.1, "This function is deprecated. Use either `USMStateInstance_Base::IsEndState()`, or `USMStateMachineInstance::IsStateMachineInEndState()` depending on use case.")
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance", meta = (DeprecatedFunction, DeprecationMessage = "Use either `Is End State` on state node classes, or `Is State Machine in End State` on state machine node classes depending on use case."))
	virtual bool IsInEndState() const;

	/** State has updated at least once. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	virtual bool HasUpdated() const;
	
	/** If this node is active. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool IsActive() const;

	/** Retrieve the node name. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	const FString& GetNodeName() const;
	
	/** Unique identifier taking into account qualified path. Unique across blueprints if called after Instance initialization. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	const FGuid& GetGuid() const;
	
	/** Retrieve the icon representing this node. Null by default. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Display")
	UTexture2D* GetNodeIcon() const;

	/** Retrieve the size to use when displaying the icon. Leave 0,0 to auto size. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Display")
	FVector2D GetNodeIconSize() const;

	/** Retrieve the tint to use when displaying the icon. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Display")
	FLinearColor GetNodeIconTintColor() const;
	
	/**
	 * Evaluate graphs of properties exposed directly on this node.
	 * @param bTargetOnly Only evaluate graph properties for the target node instance (such as a self target). Useful for only evaluating properties of select state stack instances.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void EvaluateGraphProperties(bool bTargetOnly = false);

	/** Retrieve the template guid. The template guid cannot be modified at runtime. */
	const FGuid& GetTemplateGuid() const { return TemplateGuid; }

	/** Retrieve the node position in the graph. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	const FVector2D& GetNodePosition() const;

	/** True after the initialize sequence is called on this node and false after shutdown is called. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool IsInitialized() const { return bIsInitialized; }

	/** Helper for checking if the node is ready to respond to input events. */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"), Category="Logic Driver|NodeInstance|Input (Node)")
	bool IsInitializedAndReadyForInputEvents() const;

public:

#if WITH_EDITORONLY_DATA
	/**
	 * Customize how exposed graph properties are displayed on the node.
	 * 
	 * Match the variable name with the variable you want to override.
	 * Property must be instance editable.
	 *
	 * These values can be edited directly on the variable's details panel in the blueprint
	 * editor. Elements will be added automatically in this case.
	 * 
	 * This exact property is only visible in the class defaults if the Project Editor
	 * setting bEnableVariableCustomization is false.
	 *
	 * NOTE: This array and all elements are not safe to modify through C++ unless done so
	 * in the C++ constructor or by property handles. Otherwise changes may not be
	 * propagated to instances in graphs.
	 *
	 * If this needs to be programatically modified, it should be done so through the
	 * editor module's FSMNodeInstanceUtils ExposedPropertyOverride methods.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Properties", meta = (InstancedTemplate, HideOnNode))
	TArray<FSMGraphProperty> ExposedPropertyOverrides;
#endif

#if WITH_EDITOR
	/** Searches the ExposedPropertyOverrides to find a property by name. O(n). */
	FSMGraphProperty* FindExposedPropertyOverrideByName(const FName& VariableName) const;

protected:
	/** Return an existing override or adds a new one. O(n). */
	FSMGraphProperty* FindOrAddExposedPropertyOverrideByName(const FName& VariableName);
#endif
	
public:
	/**
	 * Should graph properties evaluate even if they only contain default values. This includes properties that
	 * have values directly entered into a node without any blueprint expressions connected, such as typing a
	 * value into a string field.
	 *
	 * When false default values entered into an exposed property won't ever evaluate and the value at compile time
	 * will be used until modified at run-time. If any blueprint pins are connected to the property then this setting
	 * doesn't apply.
	 *
	 * Setting this to off is an optimization and may improve performance. However, if you modify the value at
	 * run-time it will no longer reset (re-evaluate) to the default value On State Begin. This is on by default
	 * for backwards compatibility and to maintain consistent behavior with variable evaluation.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Properties", meta = (InstancedTemplate, HideOnNode))
	uint8 bEvalDefaultProperties: 1;

	/**
	 * Properties marked as public will be exposed on this node as a graph.
	 * 
	 * When this is true that graph will automatically evaluate on state entry.
	 * When this is false you should manually call EvaluateGraphProperties().
	 * 
	 * Graph properties are only valid for nodes deriving from USMStateInstance_Base.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Properties", meta = (InstancedTemplate, HideOnNode))
	uint8 bAutoEvalExposedProperties: 1;
	
#if WITH_EDITOR
public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/**
	 * True if an array property was added or removed through the details panel.
	 */
	bool WasArrayPropertyModified(const FName& PropertyName) const;

	/**
	 * This is true only if a user has manually changed a pin value on an exposed variable,
	 * and only during PostEditChangeProperty.
	 */
	bool IsNodePinChanging() const;

private:
	void ResetArrayCheck();
	
private:
	friend class USMGraphNode_Base;
	friend class USMGraphK2Node_PropertyNode_Base;
	
	FName ArrayPropertyChanged;
	int32 ArrayIndexChanged;
	uint32 ArrayChangeType;

	/** Set from the editor when an exposed pin value is changing. */
	bool bIsNodePinChanging;
#endif

protected:
	/**
	 * Validate the node instance at state machine compile time. Use the CompilerLog->Log() function
	 * to output messages and report errors.
	 *
	 * Called by the kismet compiler prior to compilation.
	 *
	 * @param CompilerLog The compiler log specific to this compiler context. Write messages with Log().
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Default", meta = (DevelopmentOnly))
	void OnPreCompileValidate(USMCompilerLog* CompilerLog) const;
	
protected:
	virtual void OnPreCompileValidate_Implementation(USMCompilerLog* CompilerLog) const {}
	virtual void ConstructionScript_Implementation() {}
	virtual UTexture2D* GetNodeIcon_Implementation() const;
	virtual FVector2D GetNodeIconSize_Implementation() const;
	virtual FLinearColor GetNodeIconTintColor_Implementation() const;
	
protected:
	/**
	 * The icon to use when displaying this node.
	 * This exists in run-time as well in case this image is needed for purposes outside of editor use.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Display", meta = (DisplayPriority = 1, NodeBaseOnly))
	TObjectPtr<UTexture2D> NodeIcon;

	/** The size of the node icon. Leave 0,0 to auto size. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Display", meta = (DisplayPriority = 2, NodeBaseOnly))
	FVector2D NodeIconSize;

	/** The tint color to apply to the node icon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Display", meta = (DisplayPriority = 3, NodeBaseOnly))
	FLinearColor NodeIconTintColor;

public:	
	/**
	 * Sets the display name of the node. Valid from editor construction scripts only.
	 * bShowDisplayNameOnly must be set to true for the display name to be visible.
	 * 
	 * Development Only!
	 */
	UFUNCTION(BlueprintCallable, Category = "Node Display", meta = (DevelopmentOnly))
	void SetDisplayName(FName NewDisplayName);

	/**
	 * Sets the text description of the node. This generally only impacts the tooltip in the state machine graph.
	 * Valid from editor construction scripts only.
	 * 
	 * Development Only!
	 */
	UFUNCTION(BlueprintCallable, Category = "Node Display", meta = (DevelopmentOnly))
	void SetNodeDescriptionText(FText NewDescription);

	/**
	 * The text description of the node. Either returns the instance NodeDescription.Description or class metadata.
	 *
	 * Development only!
	 *
	 * @return The text description in the editor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Node Display", meta = (DevelopmentOnly))
	FText GetNodeDescriptionText() const;

	/**
	 * Sets the color of the node. Requires UseCustomColor set to true. Valid from editor construction scripts only.
	 * 
	 * Development Only!
	 */
	UFUNCTION(BlueprintCallable, Category = "Node Display", meta = (DevelopmentOnly))
	void SetNodeColor(FLinearColor NewColor);

	/**
	 * Tells the node to use a custom color. Valid from editor construction scripts only.
	 * 
	 * Development Only!
	 */
	UFUNCTION(BlueprintCallable, Category = "Node Display", meta = (DevelopmentOnly))
	void SetUseCustomColor(bool bValue);

	/**
	 * Tells the node to use a custom icon. Valid from editor construction scripts only.
	 * Override the GetNodeIcon function to dynamically set the icon.
	 * 
	 * Development Only!
	 */
	UFUNCTION(BlueprintCallable, Category = "Node Display", meta = (DevelopmentOnly))
	void SetUseCustomIcon(bool bValue);

	/**
	 * Sets the read only status of an exposed variable. Valid from editor construction scripts only.
	 * 
	 * Development Only!
	 *
	 * @param VariableName The name of the exposed variable as defined in this node class.
	 * @param bSetIsReadOnly Set the variable read only status.
	 */
	UFUNCTION(BlueprintCallable, Category = "Node Display", meta = (DevelopmentOnly))
	void SetVariableReadOnly(FName VariableName, bool bSetIsReadOnly);

	/**
	 * Sets the hidden status of an exposed variable. Valid from editor construction scripts only.
	 * 
	 * Development Only!
	 *
	 * @param VariableName The name of the exposed variable as defined in this node class.
	 * @param bSetHidden Set the variable hidden status.
	 */
	UFUNCTION(BlueprintCallable, Category = "Node Display", meta = (DevelopmentOnly))
	void SetVariableHidden(FName VariableName, bool bSetHidden);
	
	/**
	 * Checks if this node is running for an editor state machine. This is generally only valid
	 * during editor time construction scripts. Use this to allow the construction script
	 * to set default values during compile instead of recalculating values during run-time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool IsEditorExecution() const;

	/**
	 * Determine if this node is running for editor construction scripts or for a game.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance", Meta = (ExpandEnumAsExecs = "ExecutionEnvironment"))
	void WithExecutionEnvironment(ESMExecutionEnvironment& ExecutionEnvironment);

	/**
	 * Return the UEdGraphNode owning this node instance template. This is only valid in the editor while designing state machines.
	 * If valid this means you are editing the node in the state machine at editor time.
	 *
	 * This can be used in editor construction scripts or editor only methods like PostEditChangeProperty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Editor", meta = (DevelopmentOnly))
	UPARAM(DisplayName="EditorNode") TScriptInterface<ISMEditorGraphNodeInterface> GetOwningEditorGraphNode() const;

	/**
	 * Return the UEdGraphNode owning this node instance template. This is only valid in the editor while designing state machines.
	 * If valid this means you are editing the node in the state machine graph at editor time.
	 *
	 * This can be used in editor construction scripts.
	 *
	 * @param EditorNode The UEdGraphNode containing this node instance template.
	 * @param IsValidNode The execution path to take if the UEdGraph node is valid or not.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Editor", BlueprintPure = false, meta = (DevelopmentOnly, DisplayName = "Try Get Owning Editor Graph Node", ExpandEnumAsExecs = "IsValidNode"))
	void K2_TryGetOwningEditorGraphNode(TScriptInterface<ISMEditorGraphNodeInterface>& EditorNode, ESMValidEditorNode& IsValidNode) const;

	/** If this node can be created on a new thread. */
	bool IsInitializationThreadSafe() const;
	
	/**
	 * Resets all properties back to their defaults. Exposed graph properties will also be reset and may need to be re-evaluated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance", meta = (DisplayName = "Reset Variables"))
	void ResetVariables();

	bool GetResetVariablesOnInitialize() const { return bResetVariablesOnInitialize; }
	
protected:
	/**
	 * Resets all properties back to their default values when the node is initialized.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, AdvancedDisplay, Category = "Properties", meta = (InstancedTemplate))
	bool bResetVariablesOnInitialize;
	
#if WITH_EDITORONLY_DATA
public:
	bool HasCustomColor() const { return bUseCustomColors; }
	const FLinearColor& GetNodeColor() const { return NodeColor; }
	const FSMNodeDescription& GetNodeDescription() const { return NodeDescription; }
	bool HasCustomIcon() const { return bDisplayCustomIcon; }

	/** The default name which should be used. */
	FString GetNodeDisplayName() const;

	/** Sets the template guid. Editor use only. */
	void SetTemplateGuid(const FGuid& NewTemplateGuid) { TemplateGuid = NewTemplateGuid; }

	static FName GetNodeDescriptionPropertyName() { return GET_MEMBER_NAME_CHECKED(USMNodeInstance, NodeDescription); }
	static FName GetHideFromDropDownIfRulesFailPropertyName() { return GET_MEMBER_NAME_CHECKED(USMNodeInstance, bHideFromDropDownIfRulesFail); }
	
protected:
	/** Describe the node. This provides information to the context menu and to tooltips. */
	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, Category = "General", meta = (InstancedTemplate, ShowOnlyInnerProperties))
	FSMNodeDescription NodeDescription;

	/** The standard color for this node. */
	UPROPERTY(EditDefaultsOnly, Category = "Color", meta = (EditCondition = "bUseCustomColors", DisplayPriority = 1))
	FLinearColor NodeColor;
	
	/** Override editor default icon with the custom icon chosen. */
	UPROPERTY(EditDefaultsOnly, Category = "Display", meta = (DisplayPriority = 0, NodeBaseOnly))
	uint8 bDisplayCustomIcon: 1;
	
	/** Override editor preference colors. */
	UPROPERTY(EditDefaultsOnly, Category = "Color", meta = (DisplayPriority = 0))
	uint8 bUseCustomColors: 1;

	/** Hide this class from being selected in the node class drop down if its rules fail. */
	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, Category = "Behavior", AdvancedDisplay, meta = (InstancedTemplate))
	uint8 bHideFromDropDownIfRulesFail: 1;
#endif
	
public:
	void SetIsThreadSafe(bool bNewValue) { bIsThreadSafe = bNewValue; }
	
private:
	/** If this node can be created on a new thread with async initialization. Valid for game and editor sessions. */
	UPROPERTY(EditDefaultsOnly, Category = "Async Initialization", meta = (InstancedTemplate))
	uint8 bIsThreadSafe: 1;
	
#if WITH_EDITORONLY_DATA
public:
	void SetIsEditorThreadSafe(const bool bNewValue) { bIsEditorThreadSafe = bNewValue; }
	bool GetIsEditorThreadSafe() const { return bIsEditorThreadSafe; }
	
private:
	/**
	 * If this node can be created on a new thread with async initialization when playing in the editor.
	 * Nodes may contain editor only code that isn't always thread safe, such as slate styling found in
	 * TextGraph properties.
	 *
	 * If you experience crashes in the editor with async initialization consider turning this off.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Async Initialization", meta = (InstancedTemplate, EditCondition = "bIsThreadSafe"))
	uint8 bIsEditorThreadSafe: 1;

	/** If this node is executing for the editor, such as through construction scripts. */
	uint8 bIsEditorExecution: 1;

#endif

	///////////////////////
	/// Input
	///////////////////////
protected:
	/** Allow input bindings if the owning state machine supports them. */
	void EnableInput();
	
	/** Disable input bindings. */
	void DisableInput();

protected:
	UFUNCTION()
	void OnContextPawnControllerChanged(APawn* Pawn, AController* NewController);
	
public:
	TEnumAsByte<ESMNodeInput::Type> GetInputType() const { return AutoReceiveInput; }
	int32 GetInputPriority() const { return InputPriority; }
	bool GetBlockInput() const { return bBlockInput; }

	/**
	 * Retrieve the input component this node created with #AutoReceiveInput.
	 * The input component will only be valid if AutoReceiveInput is not disabled
	 * and this node is initialized.
	 *
	 * @return The UInputComponent or nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input (Node)")
	UInputComponent* GetInputComponent() const { return InputComponent; }
	
protected:
	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> InputComponent;

	/**
	 * Automatically registers this node to receive input from a player.
	 * Input is valid only from when the node is initialized and until it is shutdown.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input (Node)", meta = (InstancedTemplate))
	TEnumAsByte<ESMNodeInput::Type> AutoReceiveInput;
	
	/**
	 * The priority of this input component when pushed in to the stack.
	 * If AutoReceiveInput is set to UseOwningStateMachine this has no effect.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input (Node)", meta = (InstancedTemplate))
	int32 InputPriority = 3;

	/**
	 * Whether any components lower on the input stack should be allowed to receive input.
	 * If AutoReceiveInput is set to UseOwningStateMachine this has no effect.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input (Node)", meta = (InstancedTemplate))
	uint8 bBlockInput: 1;
	///////////////////////
	/// End Input
	///////////////////////
	
private:
	/**
	 * Return the frame this node was initialized for this run. Resets each time a run is initialized or shutdown.
	 * Primarily used for determining if input events should fire.
	 */
	uint64 RunInitializedFrame;

	/** True from NativeInitialize. */
	uint8 bIsInitialized: 1;
	
	/** The owning node in the state machine instance. */
	FSMNode_Base* OwningNode;

	/** Assigned from the editor and used in tracking specific templates. */
	UPROPERTY()
	FGuid TemplateGuid;
};