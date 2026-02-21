// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ISMSearch.h"

#include "Widgets/Views/STableRow.h"

class SSMSearchView;

class SSMSearchResultItemView : public SMultiColumnTableRow<TSharedPtr<ISMSearch::FSearchResult>>
{
public:
	SLATE_BEGIN_ARGS(SSMSearchResultItemView) {}

	SLATE_END_ARGS()

	static FName ColumnName_Error;
	static FName ColumnName_Asset;
	static FName ColumnName_Property;
	static FName ColumnName_Node;
	static FName ColumnName_Value;

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, TSharedPtr<SSMSearchView> InSearchView, TSharedPtr<ISMSearch::FSearchResult> InSearchResult,
		const TSharedRef<STableViewBase>& InOwnerTableView, const FString& InSearchString);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

	/** Extract a portion of the string for viewing containing the matched text. */
	FString MakeStringSnippet(const FString& InString) const;

private:
	bool IsAssetLoading() const;

private:
	/** An individual result item. */
	TSharedPtr<ISMSearch::FSearchResult> Item;

	/** The view owning us. */
	TWeakPtr<SSMSearchView> SearchViewOwner;

	/** The original search string. */
	FString SearchString;
};
