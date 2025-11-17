#include "Hooking/WidgetMixinBlueprintFactory.h"
#include "AssetTypeCategories.h"
#include "ClassViewerFilter.h"
#include "Blueprint/UserWidget.h"
#include "Hooking/EdGraphSchema_HookTarget.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Patching/BlueprintHookBlueprint.h"
#include "Patching/WidgetBlueprintHook.h"

#define LOCTEXT_NAMESPACE "SMLEditor"

class FWidgetBlueprintClassFilter : public IClassViewerFilter {
public:
	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override {
		if(InClass != nullptr) {
			const bool bGeneratedByBlueprint = Cast<UBlueprint>(InClass->ClassGeneratedBy) != nullptr;
			const bool bActorBased = InClass->IsChildOf(UUserWidget::StaticClass());
			return bGeneratedByBlueprint && bActorBased;
		}
		return false;
	}
	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override {
		const bool bActorBased = InUnloadedClassData->IsChildOf(UUserWidget::StaticClass());
		return bActorBased;
	}
};

UWidgetMixinBlueprintFactory::UWidgetMixinBlueprintFactory(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UWidgetMixinBlueprint::StaticClass();
	ParentClass = UUserWidgetMixin::StaticClass();
	BlueprintType = BPTYPE_Normal;
	bSkipClassPicker = true;
}

bool UWidgetMixinBlueprintFactory::ConfigureProperties() {
	// Null the target class to ensure one is selected
	MixinTargetClass = nullptr;

	// Fill in options
	FClassViewerInitializationOptions Options;

	// Show tree view selector with no root class and with filter to only allow selection of widget blueprint generated classes
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::TreeView;
	Options.bShowObjectRootClass = false;
	Options.bShowDefaultClasses = false;
	Options.ClassFilters.Add(MakeShared<FWidgetBlueprintClassFilter>());

	// Allow selecting unloaded blueprints and load them on selection
	Options.bShowUnloadedBlueprints = true;
	Options.bEnableClassDynamicLoading = true;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::Dynamic;
	
	// Allow overriding properties
	OnConfigurePropertiesDelegate.ExecuteIfBound(&Options);

	const FText TitleText = LOCTEXT("CreateBlueprintWidgetMixin_SelectTargetClassTitle", "Pick Mixin Target Widget Blueprint");
	UClass* ChosenMixinTargetClass = nullptr;
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenMixinTargetClass, UHookBlueprint::StaticClass());

	if (bPressedOk && Cast<UBlueprintGeneratedClass>(ChosenMixinTargetClass) != nullptr) {
		MixinTargetClass = Cast<UBlueprintGeneratedClass>(ChosenMixinTargetClass);
		return true;
	}
	return false;
}

FText UWidgetMixinBlueprintFactory::GetDisplayName() const {
	return LOCTEXT("WidgetBlueprintMixinFactoryDescription", "User Widget Mixin");
}

FName UWidgetMixinBlueprintFactory::GetNewAssetThumbnailOverride() const {
	return TEXT("ClassThumbnail.BlueprintInterface");
}

uint32 UWidgetMixinBlueprintFactory::GetMenuCategories() const {
	return EAssetTypeCategories::UI;
}

FText UWidgetMixinBlueprintFactory::GetToolTip() const {
	return LOCTEXT("WidgetBlueprintMixinTooltip", "Blueprint Mixin is a Blueprint asset that is automatically created for each User Widget Instance the Mixin is applied to. It can be used to add additional Widgets to the Widget Tree, Hook Widget Functions, or implement additional logic.");
}

UObject* UWidgetMixinBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) {
	check(Class->IsChildOf(UHookBlueprint::StaticClass()));
	check(MixinTargetClass && !MixinTargetClass->IsNative());

	// Create a new blueprint using the provided settings
	if (UWidgetMixinBlueprint* HookBlueprint = Cast<UWidgetMixinBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BlueprintType, Class,
		UWidgetMixinBlueprintGeneratedClass::StaticClass(), CallingContext))) {
		// Populate mixin target class and create overlay widget tree
		HookBlueprint->MixinTargetClass = MixinTargetClass;
		HookBlueprint->OverlayWidgetTree = NewObject<UOverlayWidgetTree>(HookBlueprint, TEXT("OverlayWidgetTree"), RF_Transactional | RF_ArchetypeObject);
		
		// Create hook target graph
		HookBlueprint->HookTargetGraph = FBlueprintEditorUtils::CreateNewGraph(HookBlueprint, TEXT("HookTargetGraph"), UEdGraph::StaticClass(), UEdGraphSchema_HookTarget::StaticClass());
		HookBlueprint->HookTargetGraph->bAllowDeletion = false;
		HookBlueprint->HookTargetGraph->bAllowRenaming = false;
		
		HookBlueprint->Status = BS_Dirty;
		HookBlueprint->Modify();
		return HookBlueprint;
	}
	return nullptr;
}

FString UWidgetMixinBlueprintFactory::GetDefaultNewAssetName() const {
	return FString(TEXT("NewWidgetMixin"));
}

#undef LOCTEXT_NAMESPACE
