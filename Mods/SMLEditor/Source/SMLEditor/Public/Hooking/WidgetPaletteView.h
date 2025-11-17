#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Misc/TextFilter.h"
#include "Widgets/Views/STreeView.h"
#include "Framework/Views/TreeFilterHandler.h"

class FWidgetTemplate;
class FReusableWidgetCatalogViewModel;

class SMLEDITOR_API FReusableWidgetViewModel : public TSharedFromThis<FReusableWidgetViewModel> {
public:
	virtual ~FReusableWidgetViewModel() = default;
	virtual FText GetName() const = 0;
	virtual bool IsTemplate() const = 0;
	virtual bool IsCategory() const { return false; }
	virtual void GetFilterStrings(TArray<FString>& OutStrings) const = 0;
	virtual bool HasFilteredChildTemplates() const { return false; }
	virtual TSharedRef<ITableRow> BuildRow(const TSharedRef<STableViewBase>& OwnerTable) = 0;
	virtual void GetChildren(TArray<TSharedPtr<FReusableWidgetViewModel>>& OutChildren) {}
	virtual bool IsFavorite() const { return false; }
	virtual void SetFavorite() {}
	virtual bool ShouldForceExpansion() const { return false; }
};

class SMLEDITOR_API FReusableWidgetTemplateViewModel : public FReusableWidgetViewModel {
public:
	virtual FText GetName() const override;
	virtual bool IsTemplate() const override;
	virtual void GetFilterStrings(TArray<FString>& OutStrings) const override;
	virtual TSharedRef<ITableRow> BuildRow(const TSharedRef<STableViewBase>& OwnerTable) override;
	FReply OnDraggingWidgetTemplateItem(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	void AddToFavorites();
	void RemoveFromFavorites();
	virtual bool IsFavorite() const override { return bIsFavorite; }
	virtual void SetFavorite() override { bIsFavorite = true; }

	TSharedPtr<FWidgetTemplate> Template;
	FReusableWidgetCatalogViewModel* FavortiesViewModel{nullptr};
private:
	bool bIsFavorite{false};
};

class SMLEDITOR_API FReusableWidgetHeaderViewModel : public FReusableWidgetViewModel {
public:
	virtual FText GetName() const override { return GroupName; }
	virtual bool IsTemplate() const override { return false; }
	virtual bool IsCategory() const override { return true; }
	virtual void GetFilterStrings(TArray<FString>& OutStrings) const override {}
	virtual bool HasFilteredChildTemplates() const override {
		for (const TSharedPtr<FReusableWidgetViewModel>& Child : Children) {
			if (Child && Child->HasFilteredChildTemplates()) {
				return true;
			}
		}
		return false;
	}
	virtual TSharedRef<ITableRow> BuildRow(const TSharedRef<STableViewBase>& OwnerTable) override;
	virtual void GetChildren(TArray<TSharedPtr<FReusableWidgetViewModel> >& OutChildren) override { OutChildren.Append(Children); }
	virtual bool ShouldForceExpansion() const override { return bForceExpansion; }
	void SetForceExpansion(const bool bInForceExpansion) { bForceExpansion = bInForceExpansion; }

	FText GroupName;
	TArray<TSharedPtr<FReusableWidgetViewModel>> Children;
private:
	bool bForceExpansion{false};
};

class SMLEDITOR_API FReusableWidgetCatalogViewModel : public TSharedFromThis<FReusableWidgetCatalogViewModel> {
public:
	DECLARE_MULTICAST_DELEGATE(FOnUpdating)
	DECLARE_MULTICAST_DELEGATE(FOnUpdated)

	FReusableWidgetCatalogViewModel();
	virtual ~FReusableWidgetCatalogViewModel();

	void RegisterToEvents();
	void Update();
	bool NeedUpdate() const { return bRebuildRequested; }
	static void AddToFavorites(const FReusableWidgetTemplateViewModel* WidgetTemplateViewModel);
	static void RemoveFromFavorites(const FReusableWidgetTemplateViewModel* WidgetTemplateViewModel);

	typedef TArray<TSharedPtr<FReusableWidgetViewModel>> ViewModelsArray;
	ViewModelsArray& GetWidgetViewModels() { return WidgetViewModels; }

	FOnUpdating OnUpdating;
	FOnUpdated OnUpdated;

	virtual void SetSearchText(const FText& InSearchText) { SearchText = InSearchText; }
	FText GetSearchText() const { return SearchText; }
protected:
	virtual void BuildWidgetList();
	void AddHeader(TSharedPtr<FReusableWidgetHeaderViewModel>& Header);
	void AddToFavoriteHeader(TSharedPtr<FReusableWidgetTemplateViewModel>& Favorite);
private:
	virtual void BuildWidgetTemplateCategory(FString& Category, TArray<TSharedPtr<FWidgetTemplate>>& Templates, TArray<FString>& FavoritesList) = 0;
	void BuildClassWidgetList();
	void AddWidgetTemplate(const TSharedPtr<FWidgetTemplate>& WidgetTemplate);
	void OnBlueprintReinstanced();
	void OnFavoritesUpdated();
	void OnReloadComplete(EReloadCompleteReason Reason);
	void HandleOnAssetsDeleted(const TArray<UClass*>& DeletedAssetClasses);

	typedef TArray<TSharedPtr<FWidgetTemplate>> FWidgetTemplateArray;
	TMap<FString, FWidgetTemplateArray> WidgetTemplateCategories;
	ViewModelsArray WidgetViewModels;
	bool bRebuildRequested{true};
	TSharedPtr<FReusableWidgetHeaderViewModel> FavoriteHeader;
	FText SearchText;
};

class SMLEDITOR_API FReusablePaletteViewModel : public FReusableWidgetCatalogViewModel {
public:
	virtual void BuildWidgetTemplateCategory(FString& Category, TArray<TSharedPtr<FWidgetTemplate>>& Templates, TArray<FString>& FavoritesList) override;
};

class SMLEDITOR_API SReusablePaletteViewItem : public SCompoundWidget {
public:
	SLATE_BEGIN_ARGS(SReusablePaletteViewItem) {}
		SLATE_ATTRIBUTE(FText, HighlightText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FReusableWidgetTemplateViewModel> InWidgetViewModel);
private:
	FText GetFavoriteToggleToolTipText() const;
	ECheckBoxState GetFavoritedState() const;
	void OnFavoriteToggled(ECheckBoxState InNewState);
	EVisibility GetFavoritedStateVisibility() const;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

	TSharedPtr<FReusableWidgetTemplateViewModel> WidgetViewModel;
};

class SMLEDITOR_API SReusablePaletteView : public SCompoundWidget {
public:
	SLATE_BEGIN_ARGS(SReusablePaletteView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<FReusablePaletteViewModel>& InPaletteViewModel);
	virtual ~SReusablePaletteView() override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	FText GetSearchText() const;
	void WidgetPalette_OnClick(TSharedPtr<FReusableWidgetViewModel> SelectedItem);
	TSharedPtr<FWidgetTemplate> GetSelectedTemplateWidget() const;
private:
	void OnGetChildren(TSharedPtr<FReusableWidgetViewModel> Item, TArray< TSharedPtr<FReusableWidgetViewModel> >& Children);
	TSharedRef<ITableRow> OnGenerateWidgetTemplateItem(TSharedPtr<FReusableWidgetViewModel> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnSearchChanged(const FText& InFilterText);
	void OnViewModelUpdating();
	void OnViewModelUpdated();
	void GetWidgetFilterStrings(TSharedPtr<FReusableWidgetViewModel> WidgetViewModel, TArray<FString>& OutStrings);

	TSharedPtr<FReusablePaletteViewModel> PaletteViewModel;
	TSharedPtr<TreeFilterHandler<TSharedPtr<FReusableWidgetViewModel>>> FilterHandler;
	TArray<TSharedPtr<FReusableWidgetViewModel>> TreeWidgetViewModels;
	TSharedPtr<STreeView<TSharedPtr<FReusableWidgetViewModel>>> WidgetTemplatesView;
	TSharedPtr<class SSearchBox> SearchBoxPtr;
	TSharedPtr<TTextFilter<TSharedPtr<FReusableWidgetViewModel>>> WidgetFilter;
	TSet<TSharedPtr<FReusableWidgetViewModel>> ExpandedItems;
	bool bRefreshRequested{};
	bool bAllowEditorWidget{};
};
