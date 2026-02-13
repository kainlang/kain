#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Http.h"
#include "UObject/GCObject.h"
#include "Engine/Texture2D.h"

struct FSketchfabModelData
{
	FString UID;
	FString Name;
	FString Author;
	FString ThumbnailURL;
	FString DownloadURL;
	int32 LikeCount;
	int32 ViewCount;
	TObjectPtr<UTexture2D> ThumbnailTexture;
	TSharedPtr<FSlateBrush> ThumbnailBrush;
	
	FSketchfabModelData() 
		: LikeCount(0)
		, ViewCount(0)
		, ThumbnailTexture(nullptr)
	{}
};

class SSketchfabBrowser : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SSketchfabBrowser) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	
	virtual ~SSketchfabBrowser();

private:
	// Search
	FText SearchText;
	FString APIToken;
	bool bCombineMeshes;
	TArray<TSharedPtr<FSketchfabModelData>> SearchResults;
	TSharedPtr<SListView<TSharedPtr<FSketchfabModelData>>> ResultsListView;
	
	// UI Callbacks
	void OnSearchTextChanged(const FText& InText);
	void OnSearchTextCommitted(const FText& InText, ETextCommit::Type CommitType);
	void OnSearchClicked();
	void OnTokenChanged(const FText& InText);
	ECheckBoxState IsCombineMeshesChecked() const;
	void OnCombineMeshesCheckStateChanged(ECheckBoxState NewState);
	
	// HTTP
	void PerformSearch(const FString& Query);
	void OnSearchResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void DownloadThumbnail(TSharedPtr<FSketchfabModelData> ModelData);
	void OnThumbnailDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, TSharedPtr<FSketchfabModelData> ModelData);
	
	// List View
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FSketchfabModelData> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnModelSelected(TSharedPtr<FSketchfabModelData> Item, ESelectInfo::Type SelectInfo);
	void OnDownloadClicked(TSharedPtr<FSketchfabModelData> ModelData);
	
	// Import
	void ImportModel(const FString& UID, const FString& DownloadURL);
	void OnModelDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString ModelName);
	
	// Selected model
	TSharedPtr<FSketchfabModelData> SelectedModel;
	
	// Status
	FText StatusText;
	void SetStatus(const FString& Status);

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SSketchfabBrowser"); }
};
