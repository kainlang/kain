// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMGraphProperty_Base.h"
#include "SMTextNodeWidgetInfo.h"

#include "SMTextGraphProperty.generated.h"

/**
 * Helper for converting object data to text in FSMTextGraphProperty.
 */
USTRUCT()
struct SMEXTENDEDRUNTIME_API FSMTextSerializer
{
	GENERATED_BODY()

	/**
	* When an object is placed in the text graph this function will be dynamically found from the object and executed.
	* The function should be pure and return only text.
	*
	* This is dynamically looked up during run-time. If empty no function is looked up.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Text Conversion")
	FName ToTextDynamicFunctionName;

	/**
	 * When an object is placed in the text graph this function will be looked up from the object and executed.
	 * The function should be pure and return only text.
	 *
	 * This function must exist on the class of the object placed in the text graph.
	 *
	 * If empty no function is looked up.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Text Conversion")
	TArray<FName> ToTextFunctionNames;

	bool HasToTextFunctions() const
	{
		return ToTextFunctionNames.Num() > 0 || ToTextDynamicFunctionName != NAME_None;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FSMTextSerializer& TextSerializer)
	{
		uint32 Hash = GetTypeHash(TextSerializer.ToTextDynamicFunctionName);
		for (const FName& Name : TextSerializer.ToTextFunctionNames)
		{
			Hash = HashCombine(Hash, GetTypeHash(Name));
		}

		return Hash;
	}
};

/**
 * DO NOT USE THIS DIRECTLY. Use FSMTextGraphProperty instead.
 * 
 * Runtime variant of the text graph property for Logic Driver state machines.
 * This is automatically placed in state machine blueprints when FSMTextGraphProperty is used.
 */
USTRUCT(BlueprintInternalUseOnly)
struct SMEXTENDEDRUNTIME_API FSMTextGraphProperty_Runtime : public FSMGraphProperty_Base_Runtime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = Result, meta = (AlwaysAsPin))
	FText Result;

	// FSMGraphProperty_Base_Runtime
	virtual uint8* GetResult() const override { return (uint8*)&Result; }
	virtual void SetResult(uint8* Value) override;
	// ~FSMGraphProperty_Base_Runtime
	
	UPROPERTY(EditDefaultsOnly, Category = "Text Serializer", meta = (NoLogicDriverExport))
	FSMTextSerializer TextSerializer;
};

/**
	For Logic Driver node classes only.
	
	The state machine graph node which owns the class will display this property
	as an editable text box directly on the node and create a blueprint graph
	to parse the text.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Text Graph Property", HasNativeBreak = "/Script/SMExtendedRuntime.SMExtendedGraphPropertyHelpers.BreakTextGraphProperty"))
struct SMEXTENDEDRUNTIME_API FSMTextGraphProperty : public FSMGraphProperty_Base
{
	GENERATED_BODY()

	FSMTextGraphProperty();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Result, meta = (AlwaysAsPin))
	FText Result;

	// FSMGraphProperty_Base_Runtime
	virtual uint8* GetResult() const override { return (uint8*)&Result; }
	virtual void SetResult(uint8* Value) override;
	virtual FName GetResultPropertyName() const override { return GET_MEMBER_NAME_CHECKED(FSMTextGraphProperty, Result); }
	// ~FSMGraphProperty_Base_Runtime
	
	// FSMGraphProperty_Base
	virtual bool ShouldAutoAssignVariable() const override { return false; }
	virtual bool ShouldCompileReadOnlyVariables() const override { return true; }
	
#if WITH_EDITOR
	virtual void GetVariableDetailsCustomization(FVariableDetailsCustomizationConfiguration& OutCustomizationConfiguration) const override
	{
		Super::GetVariableDetailsCustomization(OutCustomizationConfiguration);
		// This should be used on the default value of the text graph property instead.
		OutCustomizationConfiguration.bShowWidgetInfo = false;
	}

	virtual const FSMNodeWidgetInfo* GetWidgetInfo() const override { return &WidgetInfo; }
#endif
	
#if WITH_EDITORONLY_DATA
	/**
	 * If this property is considered thread safe in the editor. Nodes check this during compile
	 * and will update the overall editor thread safety of the owning node.
	 */
	virtual bool IsEditorThreadSafe() const override { return false; }
#endif
	// ~FSMGraphProperty_Base
	
#if WITH_EDITORONLY_DATA
public:
	UE_DEPRECATED(5.1, "This function is deprecated. Use DisplayOrder metadata for native properties or adjust the blueprint variable order in blueprints.")
	virtual int32 GetVerticalDisplayOrder() const override { return WidgetInfo.DisplayOrder_DEPRECATED; }
	virtual bool ShouldDefaultToEditMode() const override { return false; }
	virtual bool AllowToggleGraphEdit() const override { return !IsVariableReadOnly(); }

#endif

	UPROPERTY(EditDefaultsOnly, Category = "Text Serializer", meta = (NoLogicDriverExport))
	FSMTextSerializer TextSerializer;
	
	// WidgetInfo should be EDITORONLY_DATA but bp nativization will throw warnings unless it's always included.
	/** Configure the widget display properties. Only valid in the editor. */
	UPROPERTY(EditDefaultsOnly, Category = "Node Widget", meta = (NoLogicDriverExport))
	FSMTextNodeWidgetInfo WidgetInfo;

	/**
	 * Configure rich text info. Applying a style may override any style defined under WidgetInfo.
	 * Only valid in the editor.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Node Widget", meta = (NoLogicDriverExport))
	FSMTextNodeRichTextInfo RichTextInfo;
};