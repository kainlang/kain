// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMPreviewModeViewportView.h"
#include "SMPreviewEditorCommands.h"
#include "SMPreviewModeViewportClient.h"
#include "SMPreviewObject.h"
#include "Utilities/SMPreviewUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintEditor.h"

#include "EditorViewportCommands.h"
#include "RenderResource.h"
#include "SEditorViewportToolBarMenu.h"
#include "SMUnrealTypeDefs.h"
#include "STransformViewportToolbar.h"
#include "Slate/SceneViewport.h"

#define LOCTEXT_NAMESPACE "SSMPreviewModeViewportView"

class SSMPreviewEditorViewportToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SSMPreviewEditorViewportToolBar){}
		SLATE_ARGUMENT(TWeakPtr<SSMPreviewModeViewportView>, EditorViewport)
	SLATE_END_ARGS()

	/** Constructs this widget with the given parameters */
	void Construct(const FArguments& InArgs)
	{
		EditorViewport = InArgs._EditorViewport;
		static const FName DefaultForegroundName("DefaultForeground");

		this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FSMUnrealAppStyle::Get().GetBrush("NoBorder"))
			.ForegroundColor(FSMUnrealAppStyle::Get().GetSlateColor(DefaultForegroundName))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 2.0f)
				[
					SNew(SEditorViewportToolbarMenu)
					.ParentToolBar(SharedThis(this))
					.Cursor(EMouseCursor::Default)
					.Image("EditorViewportToolBar.MenuDropdown")
					.OnGetMenuContent(this, &SSMPreviewEditorViewportToolBar::GeneratePreviewMenu)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 2.0f)
				[
					SNew( SEditorViewportToolbarMenu )
					.ParentToolBar( SharedThis( this ) )
					.Cursor( EMouseCursor::Default )
					.Label(this, &SSMPreviewEditorViewportToolBar::GetCameraMenuLabel)
					.LabelIcon(this, &SSMPreviewEditorViewportToolBar::GetCameraMenuLabelIcon)
					.OnGetMenuContent(this, &SSMPreviewEditorViewportToolBar::GenerateCameraMenu)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 2.0f)
				[
					SNew( SEditorViewportToolbarMenu )
					.ParentToolBar( SharedThis( this ) )
					.Cursor( EMouseCursor::Default )
					.Label(this, &SSMPreviewEditorViewportToolBar::GetViewMenuLabel)
					.LabelIcon(this, &SSMPreviewEditorViewportToolBar::GetViewMenuLabelIcon)
					.OnGetMenuContent(this, &SSMPreviewEditorViewportToolBar::GenerateViewMenu)
				]
				+ SHorizontalBox::Slot()
				.Padding( 3.0f, 1.0f )
				.HAlign( HAlign_Right )
				[
					SNew(STransformViewportToolBar)
					.Viewport(EditorViewport.Pin().ToSharedRef())
					.CommandList(EditorViewport.Pin()->GetCommandList())
				]
			]
		];

		SViewportToolBar::Construct(SViewportToolBar::FArguments());
	}

	/** Creates the preview menu */
	TSharedRef<SWidget> GeneratePreviewMenu() const
	{
		TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid()? EditorViewport.Pin()->GetCommandList(): nullptr;

		const bool bInShouldCloseWindowAfterMenuSelection = true;

		FMenuBuilder PreviewOptionsMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);
		{
			PreviewOptionsMenuBuilder.BeginSection("BlueprintEditorPreviewOptions", NSLOCTEXT("BlueprintEditor", "PreviewOptionsMenuHeader", "Preview Viewport Options"));
			{
				PreviewOptionsMenuBuilder.AddMenuEntry(FSMPreviewEditorCommands::Get().ResetCamera);
				PreviewOptionsMenuBuilder.AddMenuEntry(FSMPreviewEditorCommands::Get().ShowGrid);
				PreviewOptionsMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().ToggleRealTime);
			}
			PreviewOptionsMenuBuilder.EndSection();
		}

		return PreviewOptionsMenuBuilder.MakeWidget();
	}

	FText GetCameraMenuLabel() const
	{
		if (EditorViewport.IsValid())
		{
			return GetCameraMenuLabelFromViewportType(EditorViewport.Pin()->GetViewportClient()->GetViewportType());
		}

		return NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Default", "Camera");
	}

	const FSlateBrush* GetCameraMenuLabelIcon() const
	{
		if (EditorViewport.IsValid())
		{
			return GetCameraMenuLabelIconFromViewportType( EditorViewport.Pin()->GetViewportClient()->GetViewportType() );
		}

		return FSMUnrealAppStyle::Get().GetBrush(NAME_None);
	}

	TSharedRef<SWidget> GenerateCameraMenu() const
	{
		TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid()? EditorViewport.Pin()->GetCommandList(): nullptr;

		const bool bInShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder CameraMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);

		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Perspective);

		CameraMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", NSLOCTEXT("BlueprintEditor", "CameraTypeHeader_Ortho", "Orthographic"));
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Bottom);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Left);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Right);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Back);
		CameraMenuBuilder.EndSection();

		return CameraMenuBuilder.MakeWidget();
	}

	FText GetViewMenuLabel() const
	{
		FText Label = NSLOCTEXT("BlueprintEditor", "ViewMenuTitle_Default", "View");

		if (EditorViewport.IsValid())
		{
			switch (EditorViewport.Pin()->GetViewportClient()->GetViewMode())
			{
			case VMI_Lit:
				Label = NSLOCTEXT("BlueprintEditor", "ViewMenuTitle_Lit", "Lit");
				break;

			case VMI_Unlit:
				Label = NSLOCTEXT("BlueprintEditor", "ViewMenuTitle_Unlit", "Unlit");
				break;

			case VMI_BrushWireframe:
				Label = NSLOCTEXT("BlueprintEditor", "ViewMenuTitle_Wireframe", "Wireframe");
				break;
			}
		}

		return Label;
	}

	const FSlateBrush* GetViewMenuLabelIcon() const
	{
		static FName LitModeIconName("EditorViewport.LitMode");
		static FName UnlitModeIconName("EditorViewport.UnlitMode");
		static FName WireframeModeIconName("EditorViewport.WireframeMode");

		FName Icon = NAME_None;

		if (EditorViewport.IsValid())
		{
			switch (EditorViewport.Pin()->GetViewportClient()->GetViewMode())
			{
			case VMI_Lit:
				Icon = LitModeIconName;
				break;

			case VMI_Unlit:
				Icon = UnlitModeIconName;
				break;

			case VMI_BrushWireframe:
				Icon = WireframeModeIconName;
				break;
			}
		}

		return FSMUnrealAppStyle::Get().GetBrush(Icon);
	}

	TSharedRef<SWidget> GenerateViewMenu() const
	{
		TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid() ? EditorViewport.Pin()->GetCommandList() : nullptr;

		const bool bInShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder ViewMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);

		ViewMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().LitMode, NAME_None, NSLOCTEXT("BlueprintEditor", "LitModeMenuOption", "Lit"));
		ViewMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().UnlitMode, NAME_None, NSLOCTEXT("BlueprintEditor", "UnlitModeMenuOption", "Unlit"));
		ViewMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().WireframeMode, NAME_None, NSLOCTEXT("BlueprintEditor", "WireframeModeMenuOption", "Wireframe"));

		return ViewMenuBuilder.MakeWidget();
	}

private:
	/** Reference to the parent viewport */
	TWeakPtr<SSMPreviewModeViewportView> EditorViewport;
};

void SSMPreviewModeViewportView::Construct(const FArguments& InArgs, TSharedPtr<FSMBlueprintEditor> InStateMachineEditor)
{
	BlueprintEditorPtr = InStateMachineEditor;
	if (USMBlueprint* Blueprint = BlueprintEditorPtr.Pin()->GetStateMachineBlueprint())
	{
		USMPreviewObject* PreviewObject = Blueprint->GetPreviewObject();
		
		AdvancedPreviewScene = MakeShareable(new FSMAdvancedPreviewScene(FPreviewScene::ConstructionValues(), BlueprintEditorPtr.Pin()));
		AdvancedPreviewScene->SetFloorVisibility(true);
		
		SEditorViewport::Construct(SEditorViewport::FArguments());

		AdvancedPreviewScene->SetSceneViewport(SceneViewport, ViewportOverlay);
		AdvancedPreviewScene->SetPreviewObject(PreviewObject);
		
		PreviewObject->SetPreviewWorld(GetWorld());
	}

	SystemViewportClient->SetSceneViewport(SceneViewport);
	BlueprintEditorPtr.Pin()->SetPreviewClient(SystemViewportClient);
}

SSMPreviewModeViewportView::~SSMPreviewModeViewportView()
{
	if (SystemViewportClient.IsValid())
	{
		SystemViewportClient->Viewport = nullptr;
		SystemViewportClient.Reset();
	}

	if (BlueprintEditorPtr.IsValid())
	{
		BlueprintEditorPtr.Pin()->SetPreviewClient(nullptr);
	}
}

TSharedRef<SEditorViewport> SSMPreviewModeViewportView::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SSMPreviewModeViewportView::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SSMPreviewModeViewportView::OnFloatingButtonClicked()
{
}

TSharedRef<FEditorViewportClient> SSMPreviewModeViewportView::MakeEditorViewportClient()
{
	check(AdvancedPreviewScene.IsValid());
	
	SystemViewportClient = MakeShareable(new FSMPreviewModeViewportClient(*AdvancedPreviewScene.Get(), SharedThis(this)));
	SystemViewportClient->VisibilityDelegate.BindSP(this, &SSMPreviewModeViewportView::IsViewportClientVisible);
	return SystemViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SSMPreviewModeViewportView::MakeViewportToolbar()
{
	return
		SNew(SSMPreviewEditorViewportToolBar)
		.EditorViewport(SharedThis(this))
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());
}

void SSMPreviewModeViewportView::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);
}

void SSMPreviewModeViewportView::BindCommands()
{
	SEditorViewport::BindCommands();

	const FSMPreviewEditorCommands& Commands = FSMPreviewEditorCommands::Get();

	CommandList->MapAction(
		Commands.ResetCamera,
		FExecuteAction::CreateSP(SystemViewportClient.Get(), &FSMPreviewModeViewportClient::ResetCamera));
	
	CommandList->MapAction(
		Commands.ShowGrid,
		FExecuteAction::CreateSP(SystemViewportClient.Get(), &FSMPreviewModeViewportClient::ToggleShowGrid),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(SystemViewportClient.Get(), &FSMPreviewModeViewportClient::GetShowGrid));
}

bool SSMPreviewModeViewportView::IsViewportClientVisible() const
{
	// Prevent ticking if the viewport widget isn't visible.
	// Also fixes a 5.3 bug where hit proxies can be checked before RHI is initialized.
	return (IsVisible() && SceneViewport.IsValid() && ((FRenderResource*)SceneViewport.Get())->IsInitialized());
}

#undef LOCTEXT_NAMESPACE
