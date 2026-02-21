// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/SMPropertyGraph.h"

#include "SMTextPropertyGraph.generated.h"

struct FSMTextGraphProperty;

DECLARE_DELEGATE_OneParam(FSwitchTextEditAction, bool);

UCLASS()
class SMEXTENDEDEDITOR_API USMTextPropertyGraph : public USMPropertyGraph
{
	GENERATED_UCLASS_BODY()

public:
	/** The node used to format the text before the result node. */
	UPROPERTY()
	TObjectPtr<class UK2Node_FormatText> FormatTextNode;

	virtual ~USMTextPropertyGraph() override;
	
	UEdGraphPin* GetFormatTextNodePinChecked() const;

	// UObject
	virtual void PostEditUndo() override;
	// ~UObject
	
	// UEdGraph
	virtual void NotifyGraphChanged() override;
	virtual void NotifyGraphChanged(const FEdGraphEditAction& Action) override;
	// ~UEdGraph
	
	// USMPropertyGraph
	virtual void RefreshProperty(bool bModify = true, bool bSetFromPinFirst = true) override;
	virtual void ResetGraph() override;
	virtual void SetUsingGraphToEdit(bool bValue, bool bModify = true) override;
	virtual bool IsGraphBeingUsedToEdit() const override;
	virtual bool CanSetEditStatusFromReadOnlyVariable() const override { return false; }
	virtual void OnGraphManuallyCloned(USMPropertyGraph* OldGraph) override;
	virtual void OnGraphDeleted() override;
	// ~USMPropertyGraph

	/** The user has directly entered text. This performs a full refresh and update. */
	void CommitNewText(const FText& PlainText);
	/** Resets graph completely from updated text. */
	void SetNewText(const FText& PlainText, bool bReformatGraph = true, bool bModify = true);
	void SetNewText_NoTransaction(const FText& PlainText, bool bReformatGraph = true, bool bModify = true);
	
	/** Reparse the rich text body from the current plain text body. */
	void RefreshTextBody(bool bModify = true, bool bResetGraph = false, bool bOnlyIfChanged = false);

	/** Set the plain text body and parse into rich text. */
	void SetTextBody(const FText& PlainText, bool bModify = true, bool bReformatGraph = true);

	/** Return a copy of the rich text body. */
	FText GetRichTextBody() const;

	/** The original plain text. */
	FText GetPlainTextBody() const;

	/** The default text from the format text node. */
	FText GetFormatTextNodeText() const;

	/**
	 * Performs only a string comparison of the stored plain text string vs the format text node string.
	 * This might vary if the localization display changes.
	 */
	bool DoesPlainStringMatchFormatTextString() const;

	FSwitchTextEditAction SwitchTextEditAction;
	/** Toggles the actual text input widget into or out of edit mode. */
	void SetTextEditMode(bool bValue);

	/** Checks if this graph references a property by name. */
	bool ContainsProperty(const FName& InName) const;
	/** Checks if this graph references a function by name. */
	bool ContainsFunction(const FName& InName) const;

	/** True during a graph update. */
	bool IsUpdatingGraph() const { return bIsUpdatingGraph;}

	/** True if variable parsing has failed. */
	bool HasVariableParsingError() const { return bHasVariableParsingError; }

protected:
	void SetFormatTextNodeText(const FText& NewText, bool bForceSet = false);
	void FindAndSetFormatTextNode();

public:
	/**
	 * Updates the text from the current text on the format graph node.
	 * @param bForce Force the update even if the text hasn't changed.
	 * @param bFromLocalizationDisplayChange If this change was from a display string localization change. Such
	 * as the user turning on a localization preview.
	 */
	void SetTextFromFormatTextNode(bool bForce = false, bool bFromLocalizationDisplayChange = false);

private:
	/** Called when the game preview or editor localization has changed. */
	void HandleLocalizationDisplayChange();
	FDelegateHandle LocalizationDisplayChangeHandle;

	void BindLocalizationDisplayChangeDelegate();
	void UnbindLocalizationDisplayChangeDelegate();

	/** Return the hash of the property node's serialization functions combined with the global serialization functions. */
	uint32 GetCurrentSerializationFunctionHash(FSMTextGraphProperty* PropertyNode) const;
	
protected:
	/** Variable name to variable guid. */
	UPROPERTY()
	TMap<FName, FGuid> StoredProperties;

	/** Function name to function guid. */
	UPROPERTY()
	TMap<FName, FGuid> StoredFunctions;

	/**
	 * Rich processed text used for the text graph node to display.
	 */
	UPROPERTY()
	FText RichTextBody;

	/**
	 * Cache of PlainBodyText. Useful for comparisons when PlainBodyText
	 * has been automatically updated by a string table.
	 */
	UPROPERTY()
	FString PlainStringBody;

	/** The hash of the text serialization functions for this property. */
	UPROPERTY()
	uint32 TextSerializationFunctionHash = 0;

	TSharedPtr<class FOurEditableTextGraphPin> EditableTextProperty;

	/** Graph update in progress. */
	uint8 bIsUpdatingGraph: 1;

	/** Format text node is updating the text graph. */
	uint8 bUpdatingFromFormatTextNode: 1;

	/** The display text is updating specifically for a string table being loaded. */
	uint8 bUpdatingStringTableLocalizationDisplay: 1;

	/** Undo operation in progress. */
	uint8 bIsEditUndo: 1;

	/** Variable parsing has failed. */
	UPROPERTY()
	uint8 bHasVariableParsingError: 1;
};
