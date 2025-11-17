#include "Hooking/AssetDefinitionWidgetMixinBlueprint.h"
#include "Hooking/WidgetBlueprintMixinEditor.h"
#include "Patching/WidgetBlueprintHook.h"

#define LOCTEXT_NAMESPACE "SMLEditor"

FText UAssetDefinition_WidgetMixinBlueprint::GetAssetDisplayName() const {
	return LOCTEXT("WidgetBlueprintMixin_AssetDisplayName", "Widget Mixin");
}

FText UAssetDefinition_WidgetMixinBlueprint::GetAssetDisplayName(const FAssetData& AssetData) const {
	// Show a different name for widget mixin assets
	const FString TargetClassExportObjectPath = AssetData.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UHookBlueprint, MixinTargetClass));
	if (!TargetClassExportObjectPath.IsEmpty() && TargetClassExportObjectPath != TEXT("None")) {
		const FString TargetClassObjectPath = FPackageName::ExportTextPathToObjectPath(TargetClassExportObjectPath);
		FString TargetClassName = FPackageName::ObjectPathToObjectName(TargetClassObjectPath);
		TargetClassName.RemoveFromEnd(TEXT("_C"), ESearchCase::CaseSensitive);

		// If we have a valid mixin class name, show the full display name with the mixin target class, otherwise show a generic name
		if (!TargetClassName.IsEmpty()) {
			return FText::Format(LOCTEXT("WidgetBlueprintMixin_AssetFullDisplayName", "Widget Mixin ({0})"), FText::AsCultureInvariant(TargetClassName));
		}
	}
	return LOCTEXT("WidgetBlueprintMixin_AssetDisplayName", "Widget Mixin");
}

EAssetCommandResult UAssetDefinition_WidgetMixinBlueprint::OpenAssets(const FAssetOpenArgs& OpenArgs) const {
	TArray<FAssetData> OutAssetsThatFailedToLoad;
	for (UWidgetMixinBlueprint* Blueprint : OpenArgs.LoadObjects<UWidgetMixinBlueprint>({}, &OutAssetsThatFailedToLoad)) {
		const TSharedRef<FWidgetBlueprintMixinEditor> NewBlueprintEditor(new FWidgetBlueprintMixinEditor());
		NewBlueprintEditor->InitWidgetMixinBlueprintEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Blueprint);
	}
	return EAssetCommandResult::Handled;
}

TSoftClassPtr<UObject> UAssetDefinition_WidgetMixinBlueprint::GetAssetClass() const {
	return UWidgetMixinBlueprint::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_WidgetMixinBlueprint::GetAssetCategories() const {
	static const auto Categories = { EAssetCategoryPaths::UI };
	return Categories;
}

#undef LOCTEXT_NAMESPACE
