// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "ISMEditorGraphPropertyNodeInterface.generated.h"

/**
 * Generic logging enum.
 */
UENUM()
enum class ESMLogType : uint8
{
	Note,
	Warning,
	Error
};

UINTERFACE(MinimalApi, DisplayName = "Editor Graph Property", meta = (CannotImplementInterfaceInBlueprint))
class USMEditorGraphPropertyNodeInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for accessing editor graph properties from non-editor modules.
 */
class ISMEditorGraphPropertyNodeInterface
{
	GENERATED_BODY()

public:
	/**
	 * Return the highlight color to use based on the severity.
	 *
	 * @param InSeverity The severity log type.
	 * @return The hard coded linear color matching the severity.
	 */
	static FLinearColor GetHighlightColorFromSeverity(const ESMLogType InSeverity)
	{
		FLinearColor Color;

		switch(InSeverity)
		{
		case ESMLogType::Note:
			Color = FLinearColor(0.f, 0.6f, 0.75f);
			break;
		case ESMLogType::Warning:
			Color = FLinearColor(0.86f, 0.68f, 0.02f);
			break;
		case ESMLogType::Error:
			Color = FLinearColor::Red;
			break;
		default:
			checkNoEntry();
			break;
		}

		return Color;
	}

	/**
	 * Highlight a property on the owning graph node.
	 *
	 * @param bEnable Enable or disable the highlight.
	 * @param Color The color of the highlight.
	 * @param bClearOnCompile If the highlight should clear when the owning state machine blueprint is compiled.
	 */
	UFUNCTION(BlueprintCallable, Category = NodeVisual, meta = (DevelopmentOnly, AdvancedDisplay="bClearOnCompile"))
	virtual void SetHighlight(bool bEnable, FLinearColor Color = FLinearColor(1.f, 0.84f, 0.f, 1.2), bool bClearOnCompile = false) = 0;

	/**
	 * Show a notification icon on the property.
	 *
	 * @param bEnable Show or hide the notification.
	 * @param Severity The severity of the log type.
	 * @param Message The tooltip to display when hovering the icon.
	 * @param bClearOnCompile If the notification should clear when the owning state machine blueprint is compiled.
	 */
	UFUNCTION(BlueprintCallable, Category = NodeVisual, meta = (DevelopmentOnly, AdvancedDisplay="bClearOnCompile"))
	virtual void SetNotification(bool bEnable, ESMLogType Severity = ESMLogType::Note, const FString& Message = TEXT(""), bool bClearOnCompile = false) = 0;

	/**
	 * Show a notification icon and highlight the property based on the severity.
	 *
	 * @param bEnable Show or hide the notification and highlight.
	 * @param Severity The severity of the log type.
	 * @param Message The tooltip to display when hovering the icon.
	 * @param bClearOnCompile If the notification and highlight should clear when the owning state machine blueprint is compiled.
	 */
	UFUNCTION(BlueprintCallable, Category = NodeVisual, meta = (DevelopmentOnly, AdvancedDisplay="bClearOnCompile"))
	virtual void SetNotificationAndHighlight(bool bEnable, ESMLogType Severity = ESMLogType::Note, const FString& Message = TEXT(""), bool bClearOnCompile = false) = 0;

	/**
	 * Reset the property back to the class defaults, completely resetting the graph.
	 */
	UFUNCTION(BlueprintCallable, Category = NodeData, meta = (DevelopmentOnly))
	virtual void ResetProperty() = 0;

#if WITH_EDITOR
	/**
	 * Refresh an exposed property's blueprint pin on the UEdGraphNode from the property's current value.
	 * This can be used if you programatically update an exposed property outside of editor construction scripts
	 * such as through PostEditChangeProperty and want that value reflected on the node.
	 *
	 * This should NOT be called during editor construction scripts as they will automatically refresh property pins.
	 */
	virtual void RefreshPropertyPinFromValue() = 0;

	/**
	 * Refresh an exposed property value from the matching pin on the UEdGraphNode.
	 * This can be used if you need to make sure the current value in C++ matches what is entered into the
	 * blueprint pin.
	 *
	 * This should NOT be called during editor construction scripts as they will automatically refresh property values.
	 */
	virtual void RefreshPropertyValueFromPin() = 0;
#endif
};
