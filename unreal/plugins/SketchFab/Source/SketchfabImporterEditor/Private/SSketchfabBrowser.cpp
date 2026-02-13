#include "SSketchfabBrowser.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AutomatedAssetImportData.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "Rendering/Texture2DResource.h"
#include "Slate/SlateGameResources.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

#include "Engine/StaticMesh.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "Misc/ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "SketchfabBrowser"

void SSketchfabBrowser::Construct(const FArguments& InArgs)
{
	if (GConfig)
	{
		GConfig->GetString(TEXT("SketchfabImporter"), TEXT("APIToken"), APIToken, GEditorPerProjectIni);
		GConfig->GetBool(TEXT("SketchfabImporter"), TEXT("CombineMeshes"), bCombineMeshes, GEditorPerProjectIni);
	}

	StatusText = LOCTEXT("Ready", "Ready to search Sketchfab");
	
	ChildSlot
	[
		SNew(SVerticalBox)
		
		// Header
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Title", "Sketchfab Model Browser"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
		]
		
		// API Token
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f, 5.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("APIToken", "API Token:"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
				.Text(FText::FromString(APIToken))
				.HintText(LOCTEXT("TokenHint", "Optional: Enter your Sketchfab API token for downloads"))
				.OnTextChanged(this, &SSketchfabBrowser::OnTokenChanged)
			]
		]
		
		// Search Bar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f, 5.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 0.0f, 5.0f, 0.0f)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("SearchHint", "Search for models..."))
				.OnTextChanged(this, &SSketchfabBrowser::OnSearchTextChanged)
				.OnTextCommitted(this, &SSketchfabBrowser::OnSearchTextCommitted)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SSketchfabBrowser::IsCombineMeshesChecked)
				.OnCheckStateChanged(this, &SSketchfabBrowser::OnCombineMeshesCheckStateChanged)
				.ToolTipText(LOCTEXT("CombineMeshesTooltip", "Combine imported static meshes into a single mesh"))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CombineMeshes", "Combine Imported Meshes"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Search", "Search"))
				.OnClicked_Lambda([this]() {
					OnSearchClicked();
					return FReply::Handled();
				})
			]
		]
		
		// Status
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f, 5.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return StatusText; })
			.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
		]
		
		// Results List
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(10.0f)
		[
			SAssignNew(ResultsListView, SListView<TSharedPtr<FSketchfabModelData>>)
			.ListItemsSource(&SearchResults)
			.OnGenerateRow(this, &SSketchfabBrowser::OnGenerateRow)
			.OnSelectionChanged(this, &SSketchfabBrowser::OnModelSelected)
			.SelectionMode(ESelectionMode::Single)
		]
		
		// Download Button
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Download", "Download & Import Selected"))
			.IsEnabled_Lambda([this]() { return SelectedModel.IsValid(); })
			.OnClicked_Lambda([this]() {
				if (SelectedModel.IsValid())
				{
					OnDownloadClicked(SelectedModel);
				}
				return FReply::Handled();
			})
		]
	];
}

SSketchfabBrowser::~SSketchfabBrowser()
{
}

void SSketchfabBrowser::OnSearchTextChanged(const FText& InText)
{
	SearchText = InText;
}

void SSketchfabBrowser::OnSearchTextCommitted(const FText& InText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		OnSearchClicked();
	}
}

void SSketchfabBrowser::OnTokenChanged(const FText& InText)
{
	APIToken = InText.ToString();
	if (GConfig)
	{
		GConfig->SetString(TEXT("SketchfabImporter"), TEXT("APIToken"), *APIToken, GEditorPerProjectIni);
		GConfig->Flush(false, GEditorPerProjectIni);
	}
}

ECheckBoxState SSketchfabBrowser::IsCombineMeshesChecked() const
{
	return bCombineMeshes ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SSketchfabBrowser::OnCombineMeshesCheckStateChanged(ECheckBoxState NewState)
{
	bCombineMeshes = (NewState == ECheckBoxState::Checked);
	if (GConfig)
	{
		GConfig->SetBool(TEXT("SketchfabImporter"), TEXT("CombineMeshes"), bCombineMeshes, GEditorPerProjectIni);
		GConfig->Flush(false, GEditorPerProjectIni);
	}
}

void SSketchfabBrowser::OnSearchClicked()
{
	FString Query = SearchText.ToString();
	if (Query.IsEmpty())
	{
		SetStatus("Please enter a search query");
		return;
	}
	
	PerformSearch(Query);
}

void SSketchfabBrowser::PerformSearch(const FString& Query)
{
	SetStatus(FString::Printf(TEXT("Searching for: %s..."), *Query));
	
	FString URL = FString::Printf(
		TEXT("https://api.sketchfab.com/v3/search?type=models&q=%s&downloadable=true&count=24"),
		*FGenericPlatformHttp::UrlEncode(Query)
	);
	
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(URL);
	Request->SetVerb("GET");
	
	if (!APIToken.IsEmpty())
	{
		Request->SetHeader("Authorization", FString::Printf(TEXT("Token %s"), *APIToken));
	}
	
	Request->OnProcessRequestComplete().BindSP(this, &SSketchfabBrowser::OnSearchResponseReceived);
	Request->ProcessRequest();
}

void SSketchfabBrowser::OnSearchResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		SetStatus("Search failed - check your connection");
		return;
	}
	
	FString ResponseStr = Response->GetContentAsString();
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);
	
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		SetStatus("Failed to parse search results");
		return;
	}
	
	SearchResults.Empty();
	
	const TArray<TSharedPtr<FJsonValue>>* ResultsArray;
	if (JsonObject->TryGetArrayField(TEXT("results"), ResultsArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : *ResultsArray)
		{
			TSharedPtr<FJsonObject> ModelObj = Value->AsObject();
			if (!ModelObj.IsValid()) continue;
			
			TSharedPtr<FSketchfabModelData> ModelData = MakeShared<FSketchfabModelData>();
			ModelData->UID = ModelObj->GetStringField(TEXT("uid"));
			ModelData->Name = ModelObj->GetStringField(TEXT("name"));
			
			// Get author
			TSharedPtr<FJsonObject> UserObj = ModelObj->GetObjectField(TEXT("user"));
			if (UserObj.IsValid())
			{
				ModelData->Author = UserObj->GetStringField(TEXT("displayName"));
			}
			
			// Get thumbnail
			TSharedPtr<FJsonObject> ThumbObj = ModelObj->GetObjectField(TEXT("thumbnails"));
			if (ThumbObj.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* ImagesArray;
				if (ThumbObj->TryGetArrayField(TEXT("images"), ImagesArray) && ImagesArray->Num() > 0)
				{
					TSharedPtr<FJsonObject> ImageObj = (*ImagesArray)[0]->AsObject();
					if (ImageObj.IsValid())
					{
						ModelData->ThumbnailURL = ImageObj->GetStringField(TEXT("url"));
					}
				}
			}
			
			// Get stats
			ModelData->LikeCount = ModelObj->GetIntegerField(TEXT("likeCount"));
			ModelData->ViewCount = ModelObj->GetIntegerField(TEXT("viewCount"));
			
			SearchResults.Add(ModelData);
			
			// Download thumbnail
			DownloadThumbnail(ModelData);
		}
		
		SetStatus(FString::Printf(TEXT("Found %d models"), SearchResults.Num()));
		ResultsListView->RequestListRefresh();
	}
	else
	{
		SetStatus("No results found");
	}
}

void SSketchfabBrowser::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& Item : SearchResults)
	{
		if (Item.IsValid() && Item->ThumbnailTexture)
		{
			Collector.AddReferencedObject(Item->ThumbnailTexture);
		}
	}
}

void SSketchfabBrowser::DownloadThumbnail(TSharedPtr<FSketchfabModelData> ModelData)
{
	if (!ModelData.IsValid() || ModelData->ThumbnailURL.IsEmpty()) return;
	
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(ModelData->ThumbnailURL);
	Request->SetVerb("GET");
	Request->OnProcessRequestComplete().BindSP(this, &SSketchfabBrowser::OnThumbnailDownloaded, ModelData);
	Request->ProcessRequest();
}

void SSketchfabBrowser::OnThumbnailDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, TSharedPtr<FSketchfabModelData> ModelData)
{
	if (!bWasSuccessful || !Response.IsValid() || !ModelData.IsValid()) return;
	
	TArray<uint8> ImageData = Response->GetContent();
	
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
	
	if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(ImageData.GetData(), ImageData.Num()))
	{
		TArray<uint8> RawData;
		if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
		{
			// Create Texture
			int32 Width = ImageWrapper->GetWidth();
			int32 Height = ImageWrapper->GetHeight();
			
			UTexture2D* NewTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
			if (NewTexture)
			{
				void* TextureData = NewTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
				NewTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
				NewTexture->UpdateResource();
				
				ModelData->ThumbnailTexture = NewTexture;
				ModelData->ThumbnailBrush = MakeShared<FSlateImageBrush>(NewTexture, FVector2D(Width, Height));
				
				ResultsListView->RequestListRefresh();
			}
		}
	}
}

TSharedRef<ITableRow> SSketchfabBrowser::OnGenerateRow(TSharedPtr<FSketchfabModelData> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FSketchfabModelData>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		
		// Thumbnail
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(5.0f)
		[
			SNew(SBox)
			.WidthOverride(100.0f)
			.HeightOverride(100.0f)
			[
				SNew(SImage)
				.Image(Item->ThumbnailBrush.IsValid() ? Item->ThumbnailBrush.Get() : FAppStyle::GetBrush("PlaceholderButtonIcon"))
			]
		]
		
		// Info
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		.Padding(5.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Name))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("by %s"), *Item->Author)))
				.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("❤ %d  👁 %d"), Item->LikeCount, Item->ViewCount)))
				.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
			]
		]
	];
}

void SSketchfabBrowser::OnModelSelected(TSharedPtr<FSketchfabModelData> Item, ESelectInfo::Type SelectInfo)
{
	SelectedModel = Item;
}

void SSketchfabBrowser::OnDownloadClicked(TSharedPtr<FSketchfabModelData> ModelData)
{
	if (!ModelData.IsValid()) return;
	
	SetStatus(FString::Printf(TEXT("Requesting download for: %s..."), *ModelData->Name));
	
	// Get download URL
	FString URL = FString::Printf(TEXT("https://api.sketchfab.com/v3/models/%s/download"), *ModelData->UID);
	
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(URL);
	Request->SetVerb("GET");
	
	if (!APIToken.IsEmpty())
	{
		Request->SetHeader("Authorization", FString::Printf(TEXT("Token %s"), *APIToken));
	}
	
	Request->OnProcessRequestComplete().BindLambda([this, ModelData](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSuccess) {
		if (!bSuccess || !Resp.IsValid())
		{
			SetStatus("Download failed - API token may be required");
			return;
		}
		
		FString ResponseStr = Resp->GetContentAsString();
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);
		
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			FString DownloadURL;
			
			// Try GLB first, then GLTF
			TSharedPtr<FJsonObject> GlbObj = JsonObject->GetObjectField(TEXT("glb"));
			if (GlbObj.IsValid())
			{
				DownloadURL = GlbObj->GetStringField(TEXT("url"));
			}
			else
			{
				TSharedPtr<FJsonObject> GltfObj = JsonObject->GetObjectField(TEXT("gltf"));
				if (GltfObj.IsValid())
				{
					DownloadURL = GltfObj->GetStringField(TEXT("url"));
				}
			}
			
			if (!DownloadURL.IsEmpty())
			{
				ImportModel(ModelData->UID, DownloadURL);
			}
			else
			{
				SetStatus("No downloadable format found");
			}
		}
	});
	
	Request->ProcessRequest();
}

void SSketchfabBrowser::ImportModel(const FString& UID, const FString& DownloadURL)
{
	SetStatus(FString::Printf(TEXT("Downloading model...")));
	
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(DownloadURL);
	Request->SetVerb("GET");
	Request->OnProcessRequestComplete().BindSP(this, &SSketchfabBrowser::OnModelDownloaded, UID);
	Request->ProcessRequest();
}

void SSketchfabBrowser::OnModelDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString ModelName)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		SetStatus("Model download failed");
		return;
	}
	
	// Save to temp file
	FString TempDir = FPaths::ProjectIntermediateDir() / TEXT("SketchfabImports");
	IFileManager::Get().MakeDirectory(*TempDir, true);
	
	FString SafeModelName = ModelName.Replace(TEXT(" "), TEXT("_"));
	FString TempPath = TempDir / FString::Printf(TEXT("%s.glb"), *SafeModelName);
	
	if (!FFileHelper::SaveArrayToFile(Response->GetContent(), *TempPath))
	{
		SetStatus("Failed to save downloaded model to disk");
		return;
	}
	
	SetStatus(FString::Printf(TEXT("Importing: %s..."), *ModelName));
	
	// Fast-track import: No dialogs, pure velocity
	UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
	ImportData->Filenames.Add(TempPath);
	ImportData->DestinationPath = TEXT("/Game/SketchfabImports") / SafeModelName;
	ImportData->bReplaceExisting = true;
	
	// Configure Interchange Pipeline defaults based on UI toggle
	UInterchangeGenericAssetsPipeline* PipelineDefaults = GetMutableDefault<UInterchangeGenericAssetsPipeline>();
	bool bOldCombine = false;
	
	// Helper to set boolean property via reflection
	auto SetCombineMeshes = [](UObject* TargetObj, bool bValue) -> bool
	{
		if (!TargetObj) return false;
		if (FBoolProperty* BoolProp = FindFProperty<FBoolProperty>(TargetObj->GetClass(), TEXT("bCombineStaticMeshes")))
		{
			bool bCurrent = BoolProp->GetPropertyValue_InContainer(TargetObj);
			BoolProp->SetPropertyValue_InContainer(TargetObj, bValue);
			return bCurrent;
		}
		// Try without 'b' prefix just in case
		if (FBoolProperty* BoolProp = FindFProperty<FBoolProperty>(TargetObj->GetClass(), TEXT("CombineStaticMeshes")))
		{
			bool bCurrent = BoolProp->GetPropertyValue_InContainer(TargetObj);
			BoolProp->SetPropertyValue_InContainer(TargetObj, bValue);
			return bCurrent;
		}
		return false;
	};

	// Find CommonMeshesProperties sub-object
	UObject* CommonMeshesSettings = nullptr;
	if (FObjectProperty* SubProp = FindFProperty<FObjectProperty>(PipelineDefaults->GetClass(), TEXT("CommonMeshesProperties")))
	{
		CommonMeshesSettings = SubProp->GetObjectPropertyValue_InContainer(PipelineDefaults);
	}

	if (CommonMeshesSettings)
	{
		bOldCombine = SetCombineMeshes(CommonMeshesSettings, bCombineMeshes);
		PipelineDefaults->SaveConfig();
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	TArray<UObject*> ImportedAssets = AssetToolsModule.Get().ImportAssetsAutomated(ImportData);
	
	// Restore defaults
	if (CommonMeshesSettings)
	{
		SetCombineMeshes(CommonMeshesSettings, bOldCombine);
		PipelineDefaults->SaveConfig();
	}
	
	if (ImportedAssets.Num() > 0)
	{
		SetStatus(FString::Printf(TEXT("Successfully imported %s to /Game/SketchfabImports"), *ModelName));
		
		// Ping the asset in the content browser so the user sees it immediately
		FAssetRegistryModule::AssetCreated(ImportedAssets[0]);
		
		if (GEditor)
		{
			GEditor->SyncBrowserToObjects(ImportedAssets);
		}
	}
	else
	{
		SetStatus("Import failed - ensure Interchange or GLTF importer is enabled.");
	}
}

void SSketchfabBrowser::SetStatus(const FString& Status)
{
	StatusText = FText::FromString(Status);
}

#undef LOCTEXT_NAMESPACE
