#include "Hooking/WidgetBlueprintMixinEditor.h"
#include "BlueprintEditorTabs.h"
#include "SBlueprintEditorToolbar.h"
#include "UMGStyle.h"
#include "Hooking/OverlayWidgetTreeEditor.h"
#include "Hooking/WidgetPaletteView.h"
#include "Patching/WidgetBlueprintHook.h"

#define LOCTEXT_NAMESPACE "SMLEditor"

static const FName DesignerApplicationModeId(TEXT("DesignerMode"));
static const FName GraphApplicationModeId(TEXT("GraphMode"));
static const FName OverlayWidgetTreeId(TEXT("OverlayWidgetTree"));
static const FName PaletteId(TEXT("WidgetPalette"));

void FWidgetBlueprintMixinEditor::InitWidgetMixinBlueprintEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UWidgetMixinBlueprint* Blueprint) {
	PaletteViewModel = MakeShared<FReusablePaletteViewModel>();
	InitHookBlueprintEditor(Mode, InitToolkitHost, Blueprint);
}

void FWidgetBlueprintMixinEditor::RegisterApplicationModes(const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode, bool bNewlyCreated) {
	if (GetBlueprintObj()) {
		AddApplicationMode(DesignerApplicationModeId, MakeShareable(new FWidgetMixinDesignerEditorMode(SharedThis(this),
			DesignerApplicationModeId, &FWidgetBlueprintMixinEditor::GetLocalizedModeName)));
		AddApplicationMode(GraphApplicationModeId, MakeShareable(new FHookBlueprintEditorMode(SharedThis(this),
			GraphApplicationModeId, &FWidgetBlueprintMixinEditor::GetLocalizedModeName, false)));
		SetCurrentMode(DesignerApplicationModeId);
	}
}

FText FWidgetBlueprintMixinEditor::GetLocalizedModeName(const FName InMode) {
	if (InMode == DesignerApplicationModeId) {
		return LOCTEXT("WidgetMixinEditorMode_Designer", "Designer");
	}
	if (InMode == GraphApplicationModeId) {
		return LOCTEXT("WidgetMixinEditorMode_Graph", "Graph");
	}
	return FText::GetEmpty();
}

FWidgetMixinOverlayWidgetTreeEditorSummoner::FWidgetMixinOverlayWidgetTreeEditorSummoner(const TSharedPtr<FAssetEditorToolkit>& InHostingApp) : FWorkflowTabFactory(OverlayWidgetTreeId, InHostingApp) {
	TabLabel = LOCTEXT("OverlayWidgetTree_Label", "Hierarchy");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.Tabs.Palette");
}

TSharedRef<SWidget> FWidgetMixinOverlayWidgetTreeEditorSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const {
	return SNew(SOverlayWidgetTreeEditor, StaticCastSharedPtr<FWidgetBlueprintMixinEditor>(HostingApp.Pin()));
}

FText FWidgetMixinOverlayWidgetTreeEditorSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const {
	return LOCTEXT("OverlayWidgetTree_Tooltip", "Overlay Widget Tree Hierarchy allows creation of additional widgets at the various points in the Target Widget Tree.");
}

FWidgetMixinPaletteSummoner::FWidgetMixinPaletteSummoner(const TSharedPtr<FAssetEditorToolkit>& InHostingApp) : FWorkflowTabFactory(PaletteId, InHostingApp) {
	TabLabel = LOCTEXT("Palette_Label", "Palette");
	TabIcon = FSlateIcon(FUMGStyle::GetStyleSetName(), "Palette.TabIcon");
}

TSharedRef<SWidget> FWidgetMixinPaletteSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const {
	return SNew(SReusablePaletteView, StaticCastSharedPtr<FWidgetBlueprintMixinEditor>(HostingApp.Pin())->GetPaletteViewModel());
}

FText FWidgetMixinPaletteSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const {
	return LOCTEXT("Palette_Tooltip", "Drag widgets from the Palette into the Hierarchy to create new Widgets.");
}

FWidgetMixinDesignerEditorMode::FWidgetMixinDesignerEditorMode(const TSharedRef<FHookBlueprintEditor>& InBlueprintEditor, FName InModeName, FText(*GetLocalizedMode)(const FName)) : FBlueprintEditorApplicationMode(
	InBlueprintEditor, InModeName, GetLocalizedMode, false, true) {

	// Register mode toolbar
	if (UToolMenu* Toolbar = InBlueprintEditor->RegisterModeToolbarIfUnregistered(GetModeName())) {
		InBlueprintEditor->GetToolbarBuilder()->AddCompileToolbar(Toolbar);
		InBlueprintEditor->GetToolbarBuilder()->AddScriptingToolbar(Toolbar);
		InBlueprintEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(Toolbar);
		InBlueprintEditor->GetToolbarBuilder()->AddDebuggingToolbar(Toolbar);
	}

	// Register additional tab spawners
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FWidgetMixinPaletteSummoner(InBlueprintEditor)));
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FWidgetMixinOverlayWidgetTreeEditorSummoner(InBlueprintEditor)));

	// Register layout
	TabLayout = FTabManager::NewLayout(TEXT("Blueprints_WidgetMixinBlueprint_v2"))
	->AddArea(FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)->Split(
	FTabManager::NewSplitter()
		->SetOrientation(Orient_Horizontal)
		->Split(
		FTabManager::NewSplitter()
			->SetOrientation(Orient_Vertical)
			->SetSizeCoefficient(0.15f)
			->Split(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.50f)
				->AddTab(PaletteId, ETabState::OpenedTab)
			)->Split(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.50f)
				->AddTab(OverlayWidgetTreeId, ETabState::OpenedTab)
			)
     	)->Split(
     		FTabManager::NewSplitter()
     		->SetOrientation(Orient_Vertical)
     		->SetSizeCoefficient(0.60f)
     		->Split(
     		FTabManager::NewStack()
				 ->SetSizeCoefficient(0.80f)
				 ->AddTab("Document", ETabState::ClosedTab)
     		)
     		->Split(
     			FTabManager::NewStack()
     			->SetSizeCoefficient(0.20f)
     			->AddTab( FBlueprintEditorTabs::CompilerResultsID, ETabState::ClosedTab )
     		)
     	)->Split(
     		FTabManager::NewSplitter()
     		->SetOrientation(Orient_Vertical)
     		->SetSizeCoefficient(0.25f)
     		->Split(
     			FTabManager::NewStack()
     			->SetSizeCoefficient(0.60f)
     			->AddTab(FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab)
     		)
     	)
    ));
}

#undef LOCTEXT_NAMESPACE
