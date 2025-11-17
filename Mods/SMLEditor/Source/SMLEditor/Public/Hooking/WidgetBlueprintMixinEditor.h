#pragma once

#include "CoreMinimal.h"
#include "Hooking/HookBlueprintEditor.h"

class FReusablePaletteViewModel;
class UWidgetMixinBlueprint;

class SMLEDITOR_API FWidgetBlueprintMixinEditor : public FHookBlueprintEditor {
public:
	/** Called to initialize and open the widget mixin blueprint editor */
	void InitWidgetMixinBlueprintEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UWidgetMixinBlueprint* Blueprint);

	FORCEINLINE TSharedPtr<FReusablePaletteViewModel> GetPaletteViewModel() const { return PaletteViewModel; }
protected:
	TSharedPtr<FReusablePaletteViewModel> PaletteViewModel;
	
	// Begin FBlueprintEditor interface
	virtual void RegisterApplicationModes(const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode, bool bNewlyCreated) override;
	// End FBlueprintEditor interface

	static FText GetLocalizedModeName(const FName InMode);
};

struct FWidgetMixinOverlayWidgetTreeEditorSummoner : FWorkflowTabFactory {
	explicit FWidgetMixinOverlayWidgetTreeEditorSummoner( const TSharedPtr<FAssetEditorToolkit>& InHostingApp);

	// Begin FWorkflowTabFactory interface
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;
	// End FWorkflowTabFactory interface
};

struct FWidgetMixinPaletteSummoner : FWorkflowTabFactory {
	explicit FWidgetMixinPaletteSummoner( const TSharedPtr<FAssetEditorToolkit>& InHostingApp);

	// Begin FWorkflowTabFactory interface
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;
	// End FWorkflowTabFactory interface
};

class FWidgetMixinDesignerEditorMode : public FBlueprintEditorApplicationMode {
public:
	explicit FWidgetMixinDesignerEditorMode(const TSharedRef<FHookBlueprintEditor>& InBlueprintEditor, FName InModeName, FText(*GetLocalizedMode)(const FName));
protected:
	// Set of spawnable tabs
	FWorkflowAllowedTabSet TabFactories;
private:
	TWeakObjectPtr<UWidgetMixinBlueprint> BlueprintPtr;
};
