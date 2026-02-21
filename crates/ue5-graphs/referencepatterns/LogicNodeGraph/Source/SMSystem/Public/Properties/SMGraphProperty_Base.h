// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMNodeWidgetInfo.h"
#include "ExposedFunctions/SMExposedFunctions.h"

#include "EdGraph/EdGraphPin.h"
#include "Engine/MemberReference.h"

#include "SMGraphProperty_Base.generated.h"

struct FSMNode_Base;

/**
 * The base graph properties containing the bare essentials for run-time.
 * Any run-time graph property types should inherit from this.
 */
USTRUCT()
struct SMSYSTEM_API FSMGraphProperty_Base_Runtime
{
	GENERATED_BODY()

	TArray<FSMExposedFunctionHandler>* GraphEvaluator;
	
	FSMGraphProperty_Base_Runtime();
	virtual ~FSMGraphProperty_Base_Runtime() = default;

	virtual void Initialize(FSMNode_Base* InOwningNode);
	virtual void Execute(void* Params = nullptr);
	
	virtual uint8* GetResult() const { return nullptr; }
	virtual void SetResult(uint8* Value) {}

	virtual const FGuid& SetGuid(const FGuid& NewGuid);
	const FGuid& GetGuid() const { return Guid; }
	virtual const FGuid& SetOwnerGuid(const FGuid& NewGuid);
	
	/** Returns the graph property owner of this node. Likely itself. */
	const FGuid& GetOwnerGuid() const { return OwnerGuid; }

	/** Set whether the variable contains a default value only. */
	void SetIsDefaultValueOnly(bool bNewValue) { bIsDefaultValueOnly = bNewValue; }
	
	/** Does this variable only contain a default value. */
	bool GetIsDefaultValueOnly() const { return bIsDefaultValueOnly; }

	/** Get the property name of the result field. */
	virtual FName GetResultPropertyName() const { return NAME_None; }

	/**
	 * If set then the linked property is the one that is actually executing, but this struct
	 * is the one being read from.
	 */
	FSMGraphProperty_Base_Runtime* LinkedProperty;
	
protected:
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid Guid;

	/** The graph property owner. If this struct is defined within a node class and instanced
	 * to a state machine then the guid of class CDO is the owner. */
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid OwnerGuid;

	/** If only a default value is assigned (no variable connected) */
	UPROPERTY()
	uint8 bIsDefaultValueOnly: 1;

	/** The node this graph property belongs to. */
	FSMNode_Base* OwningNode;
};

/**
 * Graph properties which represent a variable exposed on a node. Only for run-time use.
 */
USTRUCT()
struct SMSYSTEM_API FSMGraphProperty_Runtime : public FSMGraphProperty_Base_Runtime
{
	GENERATED_BODY()

	FSMGraphProperty_Runtime();
};


/**
 * Wrapper so templates can be mapped to their graph properties and stored in a UPROPERTY.
 */
USTRUCT()
struct SMSYSTEM_API FSMGraphPropertyTemplateOwner
{
	GENERATED_BODY()
		
	UPROPERTY()
	TArray<FSMGraphProperty_Base_Runtime> VariableGraphProperties;
};


/**
 * EDITOR: The base struct for graph properties exposed on a node. Contains additional properties for configuration and compilation.
 * Any graph property types should inherit from this.
 */
USTRUCT()
struct SMSYSTEM_API FSMGraphProperty_Base : public FSMGraphProperty_Base_Runtime
{
	GENERATED_BODY()
	
	FSMGraphProperty_Base();
	
	// FSMGraphProperty_Base_Runtime
	virtual const FGuid& SetGuid(const FGuid& NewGuid) override;
	// ~FSMGraphProperty_Base_Runtime
	const FGuid& SetGuid(const FGuid& NewGuid, int32 Index, bool bCountTemplate = true);
	const FGuid& GenerateNewGuid();
	const FGuid& GenerateNewGuidIfNotValid();

	void InvalidateGuid();
	
	const FGuid& SetTemplateGuid(const FGuid& NewGuid, bool bRefreshGuid = false);
	const FGuid& GetTemplateGuid() const { return TemplateGuid; }

	const FGuid& GetUnmodifiedGuid() const { return GuidUnmodified; }
	
	/** Used if this class should automatically assign itself to exposed variable properties. */
	virtual bool ShouldAutoAssignVariable() const { return VariableName != NAME_None; }
	
	/** Checked during duplication if the guid should be assigned from the variable. */
	virtual bool ShouldGenerateGuidFromVariable() const { return ShouldAutoAssignVariable(); }
	
	/** If this variable should be considered read only when displaying on the node. */
	virtual bool IsVariableReadOnly() const;

	/** If this variable should not be displayed on the node. */
	virtual bool IsVariableHidden() const;
	
	/**
	 * If a read only graph should be compiled into the blueprint. If false then
	 * only the default value is stored.
	 */
	virtual bool ShouldCompileReadOnlyVariables() const { return false; }

#if WITH_EDITOR
	struct FVariableDetailsCustomizationConfiguration
	{
		bool bShowReadOnly = true;
		bool bShowHidden = true;
		bool bShowWidgetInfo = true;
	};
	/** Called during variable customization when a variable in a node blueprint is selected. */
	virtual void GetVariableDetailsCustomization(FVariableDetailsCustomizationConfiguration& OutCustomizationConfiguration) const
	{
		OutCustomizationConfiguration = FVariableDetailsCustomizationConfiguration();
	}

	/** Return the widget info for this property. */
	virtual const FSMNodeWidgetInfo* GetWidgetInfo() const { return nullptr; }
#endif
	
#if WITH_EDITORONLY_DATA
	/**
	 * If this property is considered thread safe in the editor. Nodes check this during compile
	 * and will update the overall editor thread safety of the owning node.
	 */
	virtual bool IsEditorThreadSafe() const { return true; }
#endif
	
	/** The node variable name to override. */
	UPROPERTY(EditDefaultsOnly, Category = "Variable", meta = (ExposeOverrideOnly))
	FName VariableName;
	
	UPROPERTY()
	FEdGraphPinType VariableType;

	UPROPERTY()
	FMemberReference MemberReference;

	UPROPERTY()
	bool bIsInArray;
	
	// bReadOnly should not be set directly unless used by USMNodeInstance::ExposedPropertyOverrides
	// or USMNodeInstance::SetVariableReadOnly.
	
	/** Display the variable as read only on the node. */
	UPROPERTY(EditDefaultsOnly, Category = "Variable", meta = (ExposeOverrideOnly))
	bool bReadOnly;

	/** Prevent the variable from being displayed on the node. The property graph is still created and this variable will evaluate
	 * with EvaluateGraphProperties regardless of hidden status. */
	UPROPERTY(EditDefaultsOnly, Category = "Variable", meta = (ExposeOverrideOnly))
	bool bHidden;
	
public:
#if WITH_EDITORONLY_DATA
	/** Return the editor graph to use. The outer provided will be the editor module so the class can be retrieved. */
	virtual UClass* GetGraphClass(UObject* Outer) const;
	/** Return the editor graph schema to use. The outer provided will be the editor module so the class can be retrieved. */
	virtual UClass* GetGraphSchemaClass(UObject* Outer) const;
	/** Return the simple name of the editor module to use for looking up the graph classes. */
	virtual const FString& GetGraphModuleName() const;
	/** Load and return the editor module package. This is what will be passed to GetGraphClass and GetGraphSchemaClass. */
	virtual UPackage* GetEditorModule() const;
	/** The desired name of this property. This may be used at some display points in the editor. */
	virtual const FString& GetPropertyDisplayName() const;
	/**
	 * The desired vertical location on the graph node for this widget to be displayed.
	 * @deprecated Use DisplayOrder metadata for native properties or adjust the blueprint variable order in blueprints. 
	 */
	UE_DEPRECATED(5.1, "This function is deprecated. Use DisplayOrder metadata for native properties or adjust the blueprint variable order in blueprints.")
	virtual int32 GetVerticalDisplayOrder() const { return 0; }
	/** Should property be able to toggle between edit and view modes. */
	virtual bool AllowToggleGraphEdit() const { return false; }
	/** Should property default to view or edit mode. */
	virtual bool ShouldDefaultToEditMode() const { return true; }

	/** Returns either the property display name or the RealDisplayName. */
	FText GetDisplayName() const;

	/** Set when loaded in a graph node. */
	UPROPERTY()
	FText RealDisplayName;
	
	/** Set when loaded in a graph node in the event this is stored within an array. */
	UPROPERTY()
	int32 ArrayIndex;
protected:
	UPROPERTY()
	FName GraphClassName;
	UPROPERTY()
	FName GraphSchemaClassName;
	UPROPERTY()
	FString GraphModuleClassName;

	UPROPERTY(Transient)
	mutable TObjectPtr<UClass> CachedGraphClass;
	UPROPERTY(Transient)
	mutable TObjectPtr<UClass> CachedSchemaClass;
#endif
	
protected:
	/** The guid without the template. */
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid GuidUnmodified;

	/** The guid of the template this belongs to. */
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid TemplateGuid;

	UPROPERTY()
	int32 GuidIndex;
};

/**
 * EDITOR: Graph properties which represent a variable exposed on a node. Contains additional properties for configuration and compilation.
 */
USTRUCT()
struct SMSYSTEM_API FSMGraphProperty : public FSMGraphProperty_Base
{
	GENERATED_BODY()

	FSMGraphProperty();

#if WITH_EDITOR
	virtual const FSMNodeWidgetInfo* GetWidgetInfo() const override { return &WidgetInfo; }
#endif
	
#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.1, "This function is deprecated. Use DisplayOrder metadata for native properties or adjust the blueprint variable order in blueprints.")
	virtual int32 GetVerticalDisplayOrder() const override { return WidgetInfo.DisplayOrder_DEPRECATED; }

	/** Customize the appearance of the node property widget. */
	UPROPERTY(EditDefaultsOnly, Category = "Widget")
	FSMTextDisplayWidgetInfo WidgetInfo;
#endif
};