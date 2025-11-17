#include "Hooking/WidgetPaletteView.h"
#include "Editor.h"
#include "UMGEditorProjectSettings.h"
#include "WidgetBlueprint.h"
#include "WidgetTemplate.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "DragDrop/WidgetTemplateDragDropOp.h"
#include "Settings/WidgetDesignerSettings.h"
#include "Templates/WidgetTemplateBlueprintClass.h"
#include "UObject/CoreRedirects.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBorder.h"

#define LOCTEXT_NAMESPACE "SMLEditor"

FText FReusableWidgetTemplateViewModel::GetName() const {
	return Template->Name;
}

bool FReusableWidgetTemplateViewModel::IsTemplate() const {
	return true;
}

void FReusableWidgetTemplateViewModel::GetFilterStrings(TArray<FString>& OutStrings) const {
	Template->GetFilterStrings(OutStrings);
}

TSharedRef<ITableRow> FReusableWidgetTemplateViewModel::BuildRow(const TSharedRef<STableViewBase>& OwnerTable) {
	return SNew(STableRow<TSharedPtr<FReusableWidgetViewModel>>, OwnerTable)
	.Padding(2.0f)
	.OnDragDetected(this, &FReusableWidgetTemplateViewModel::OnDraggingWidgetTemplateItem) [
		SNew(SReusablePaletteViewItem, SharedThis(this))
		.HighlightText(FavortiesViewModel, &FReusableWidgetCatalogViewModel::GetSearchText)
	];
}

FReply FReusableWidgetTemplateViewModel::OnDraggingWidgetTemplateItem(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) {
	return FReply::Handled().BeginDragDrop(FWidgetTemplateDragDropOp::New(Template));
}

void FReusableWidgetTemplateViewModel::AddToFavorites() {
	bIsFavorite = true;
	FavortiesViewModel->AddToFavorites(this);
}

void FReusableWidgetTemplateViewModel::RemoveFromFavorites() {
	bIsFavorite = false;
	FavortiesViewModel->RemoveFromFavorites(this);
}

TSharedRef<ITableRow> FReusableWidgetHeaderViewModel::BuildRow(const TSharedRef<STableViewBase>& OwnerTable) {
	return SNew(STableRow<TSharedPtr<FReusableWidgetViewModel>>, OwnerTable)
	.Style(FAppStyle::Get(), "UMGEditor.PaletteHeader")
	.Padding(5.0f)
	.ShowSelection(false) [
		SNew(STextBlock)
		.TransformPolicy(ETextTransformPolicy::ToUpper)
		.Text(GroupName)
		.Font(FAppStyle::Get().GetFontStyle("SmallFontBold"))
	];
}

FReusableWidgetCatalogViewModel::FReusableWidgetCatalogViewModel() {
	FavoriteHeader = MakeShareable(new FReusableWidgetHeaderViewModel());
	FavoriteHeader->GroupName = LOCTEXT("Favorites", "Favorites");
}

void FReusableWidgetCatalogViewModel::RegisterToEvents() {
	GEditor->OnBlueprintReinstanced().AddRaw(this, &FReusableWidgetCatalogViewModel::OnBlueprintReinstanced);
	FEditorDelegates::OnAssetsDeleted.AddSP(this, &FReusableWidgetCatalogViewModel::HandleOnAssetsDeleted);
	FCoreUObjectDelegates::ReloadCompleteDelegate.AddSP(this, &FReusableWidgetCatalogViewModel::OnReloadComplete);

	UWidgetPaletteFavorites* Favorites = GetDefault<UWidgetDesignerSettings>()->Favorites;
	Favorites->OnFavoritesUpdated.AddSP(this, &FReusableWidgetCatalogViewModel::OnFavoritesUpdated);
}

FReusableWidgetCatalogViewModel::~FReusableWidgetCatalogViewModel() {
	GEditor->OnBlueprintReinstanced().RemoveAll(this);
	FEditorDelegates::OnAssetsDeleted.RemoveAll(this);
	FCoreUObjectDelegates::ReloadCompleteDelegate.RemoveAll(this);

	UWidgetPaletteFavorites* Favorites = GetDefault<UWidgetDesignerSettings>()->Favorites;
	Favorites->OnFavoritesUpdated.RemoveAll(this);
}

void FReusableWidgetCatalogViewModel::Update() {
	if (bRebuildRequested) {
		OnUpdating.Broadcast();
		BuildWidgetList();
		bRebuildRequested = false;
		OnUpdated.Broadcast();
	}
}

void FReusableWidgetCatalogViewModel::BuildWidgetList() {
	WidgetViewModels.Reset();
	WidgetTemplateCategories.Reset();
	BuildClassWidgetList();

	const bool bHasFavorites = FavoriteHeader->Children.Num() != 0;
	FavoriteHeader->Children.Reset();

	UWidgetPaletteFavorites* FavoritesPalette = GetDefault<UWidgetDesignerSettings>()->Favorites;
	TArray<FString> FavoritesList = FavoritesPalette->GetFavorites();

	for (auto& Entry : WidgetTemplateCategories) {
		BuildWidgetTemplateCategory(Entry.Key, Entry.Value, FavoritesList);
	}
	for (const FString& FavoriteName : FavoritesList) {
		FavoritesPalette->Remove(FavoriteName);
	}

	WidgetViewModels.Sort([] (TSharedPtr<FReusableWidgetViewModel> A, TSharedPtr<FReusableWidgetViewModel> B){
		return B->GetName().CompareTo(A->GetName()) > 0;
	});
	if (FavoriteHeader->Children.Num() != 0) {
		FavoriteHeader->SetForceExpansion(!bHasFavorites);
		FavoriteHeader->Children.Sort([](TSharedPtr<FReusableWidgetViewModel> A, TSharedPtr<FReusableWidgetViewModel> B) { return B->GetName().CompareTo(A->GetName()) > 0; });
		WidgetViewModels.Insert(FavoriteHeader, 0);
	}

	const TSharedPtr<FReusableWidgetViewModel>* AdvancedSectionPtr = WidgetViewModels.FindByPredicate([](const TSharedPtr<FReusableWidgetViewModel>& Widget) {return Widget->GetName().CompareTo(LOCTEXT("Advanced", "Advanced")) == 0; });
	if (AdvancedSectionPtr) {
		TSharedPtr<FReusableWidgetViewModel> AdvancedSection = *AdvancedSectionPtr;
		WidgetViewModels.Remove(AdvancedSection);
		WidgetViewModels.Push(AdvancedSection);
	}
}

static bool CheckClassPathFiltersForWidgetClassPathName(const FString& WidgetClassPathName) {
	const UUMGEditorProjectSettings* UMGEditorProjectSettings = GetDefault<UUMGEditorProjectSettings>();

	if (!UMGEditorProjectSettings->bShowWidgetsFromEngineContent && WidgetClassPathName.StartsWith(TEXT("/Engine"))) {
		return false;
	}
	if (!UMGEditorProjectSettings->bShowWidgetsFromDeveloperContent && WidgetClassPathName.StartsWith(TEXT("/Game/Developers"))) {
		return false;
	}
	for (const FSoftClassPath& WidgetClassToHide : UMGEditorProjectSettings->WidgetClassesToHide) {
		if (WidgetClassPathName.Contains(WidgetClassToHide.ToString())) {
			return false;
		}
	}
	return true;
}

void FReusableWidgetCatalogViewModel::BuildClassWidgetList() {
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt) {
		UClass* WidgetClass = *ClassIt;

		if (WidgetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Hidden | CLASS_HideDropDown) || !WidgetClass->IsChildOf(UWidget::StaticClass())) {
			continue;
		}
		const FString WidgetPathName = WidgetClass->GetPathName();
		if (!CheckClassPathFiltersForWidgetClassPathName(WidgetPathName)) {
			continue;
		}

		const bool bIsSkeletonClass = WidgetClass->HasAnyFlags(RF_Transient) && WidgetClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
		if (bIsSkeletonClass) {
			continue;
		}
		if (IsEditorOnlyObject(WidgetClass)) {
			continue;
		}
		
		if (WidgetClass->IsChildOf(UUserWidget::StaticClass())) {
			AddWidgetTemplate(MakeShared<FWidgetTemplateBlueprintClass>(FAssetData(WidgetClass), WidgetClass));
		} else {
			AddWidgetTemplate(MakeShared<FWidgetTemplateClass>(WidgetClass));
		}
	}

	// Iterate all Blueprint Generated Classes. We want to include both source and compiled BPs, so do NOT skip AR filtered assets
	FARFilter AssetRegistryFilter;
	AssetRegistryFilter.ClassPaths.Add(UBlueprintGeneratedClass::StaticClass()->GetClassPathName());
	AssetRegistryFilter.bRecursiveClasses = true;
	IAssetRegistry::Get()->EnumerateAssets(AssetRegistryFilter, [&](const FAssetData& BlueprintClassAssetData) {
		// If asset is loaded we already included it above as a part of class iteration
		if (BlueprintClassAssetData.IsAssetLoaded()) {
			return true;
		}

		// Retrieve the name of the parent class from the blueprint asset
		const UClass* NativeParentClass = nullptr;
		FString NativeParentClassName;
		BlueprintClassAssetData.GetTagValue(FBlueprintTags::NativeParentClassPath, NativeParentClassName);
		if (NativeParentClassName.IsEmpty()) {
			return true;
		}

		// Check core redirects to make sure the type is available
		const FString NativeParentClassPath = FPackageName::ExportTextPathToObjectPath(NativeParentClassName);
		if (NativeParentClassPath.StartsWith(TEXT("/"))) {
			const FString RedirectedClassPath = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class, FCoreRedirectObjectName(NativeParentClassPath)).ToString();
			NativeParentClass = UClass::TryFindTypeSlow<UClass>(RedirectedClassPath);
		}

		if (NativeParentClass == nullptr || !NativeParentClass->IsChildOf(UWidget::StaticClass())) {
			return true;
		}
		const EClassFlags ClassFlags = static_cast<EClassFlags>(BlueprintClassAssetData.GetTagValueRef<uint32>(FBlueprintTags::ClassFlags));
		if (EnumHasAnyFlags(ClassFlags, CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Hidden | CLASS_HideDropDown)) {
			return true;
		}
		const FString WidgetPathName = BlueprintClassAssetData.GetObjectPathString();
		if (!CheckClassPathFiltersForWidgetClassPathName(WidgetPathName)) {
			return true;
		}
		
		AddWidgetTemplate(MakeShared<FWidgetTemplateBlueprintClass>(BlueprintClassAssetData, nullptr));
		return true;
	}, false);
}

void FReusableWidgetCatalogViewModel::AddHeader(TSharedPtr<FReusableWidgetHeaderViewModel>& Header) {
	WidgetViewModels.Add(Header);
}

void FReusableWidgetCatalogViewModel::AddToFavoriteHeader(TSharedPtr<FReusableWidgetTemplateViewModel>& Favorite) {
	if (FavoriteHeader) {
		FavoriteHeader->Children.Add(Favorite);
	}
}

void FReusableWidgetCatalogViewModel::AddWidgetTemplate(const TSharedPtr<FWidgetTemplate>& Template) {
	const FString Category = *Template->GetCategory().ToString();
	const TArray<FString>& CategoriesToHide = GetDefault<UUMGEditorProjectSettings>()->CategoriesToHide;
	
	for (const FString& CategoryName : CategoriesToHide) {
		if (Category == CategoryName) {
			return;
		}
	}
	FWidgetTemplateArray& Group = WidgetTemplateCategories.FindOrAdd(Category);
	Group.Add(Template);
}

void FReusableWidgetCatalogViewModel::OnBlueprintReinstanced() {
	bRebuildRequested = true;
}

void FReusableWidgetCatalogViewModel::OnFavoritesUpdated() {
	bRebuildRequested = true;
}

void FReusableWidgetCatalogViewModel::OnReloadComplete(EReloadCompleteReason Reason) {
	bRebuildRequested = true;
}

void FReusableWidgetCatalogViewModel::HandleOnAssetsDeleted(const TArray<UClass*>& DeletedAssetClasses) {
	for (const UClass* DeletedAssetClass : DeletedAssetClasses) {
		if ((DeletedAssetClass == nullptr) || DeletedAssetClass->IsChildOf(UWidgetBlueprint::StaticClass())) {
			bRebuildRequested = true;
		}
	}
}

void FReusablePaletteViewModel::BuildWidgetTemplateCategory(FString& Category, TArray<TSharedPtr<FWidgetTemplate>>& Templates, TArray<FString>& FavoritesList) {
	TSharedPtr<FReusableWidgetHeaderViewModel> Header = MakeShareable(new FReusableWidgetHeaderViewModel());
	Header->GroupName = FText::FromString(Category);
	
	for (const TSharedPtr<FWidgetTemplate>& Template : Templates) {
		TSharedPtr<FReusableWidgetTemplateViewModel> TemplateViewModel = MakeShareable(new FReusableWidgetTemplateViewModel());
		TemplateViewModel->Template = Template;
		TemplateViewModel->FavortiesViewModel = this;
		Header->Children.Add(TemplateViewModel);

		const int32 Index = FavoritesList.Find(Template->Name.ToString());
		if (Index != INDEX_NONE) {
			TemplateViewModel->SetFavorite();
			
			TSharedPtr<FReusableWidgetTemplateViewModel> FavoriteTemplateViewModel = MakeShareable(new FReusableWidgetTemplateViewModel());
			FavoriteTemplateViewModel->Template = Template;
			FavoriteTemplateViewModel->FavortiesViewModel = this;
			FavoriteTemplateViewModel->SetFavorite();

			AddToFavoriteHeader(FavoriteTemplateViewModel);
			FavoritesList.RemoveAt(Index);
		}
	}
	Header->Children.Sort([](const TSharedPtr<FReusableWidgetViewModel>& A, const TSharedPtr<FReusableWidgetViewModel>& B) { return B->GetName().CompareTo(A->GetName()) > 0; });
	AddHeader(Header);
}

void FReusableWidgetCatalogViewModel::AddToFavorites(const FReusableWidgetTemplateViewModel* WidgetTemplateViewModel) {
	UWidgetPaletteFavorites* Favorites = GetDefault<UWidgetDesignerSettings>()->Favorites;
	Favorites->Add(WidgetTemplateViewModel->GetName().ToString());
}

void FReusableWidgetCatalogViewModel::RemoveFromFavorites(const FReusableWidgetTemplateViewModel* WidgetTemplateViewModel) {
	UWidgetPaletteFavorites* Favorites = GetDefault<UWidgetDesignerSettings>()->Favorites;
	Favorites->Remove(WidgetTemplateViewModel->GetName().ToString());
}

FText SReusablePaletteViewItem::GetFavoriteToggleToolTipText() const {
	if (GetFavoritedState() == ECheckBoxState::Checked) {
		return LOCTEXT("Unfavorite", "Click to remove this widget from your favorites.");
	}
	return LOCTEXT("Favorite", "Click to add this widget to your favorites.");
}

ECheckBoxState SReusablePaletteViewItem::GetFavoritedState() const {
	if (WidgetViewModel->IsFavorite()) {
		return ECheckBoxState::Checked;
	}
	return ECheckBoxState::Unchecked;
}

void SReusablePaletteViewItem::OnFavoriteToggled(ECheckBoxState InNewState) {
	if (InNewState == ECheckBoxState::Checked) {
		WidgetViewModel->AddToFavorites();
	} else {
		WidgetViewModel->RemoveFromFavorites();
	}
}

EVisibility SReusablePaletteViewItem::GetFavoritedStateVisibility() const {
	return GetFavoritedState() == ECheckBoxState::Checked || IsHovered() ? EVisibility::Visible : EVisibility::Hidden;
}

void SReusablePaletteViewItem::Construct(const FArguments& InArgs, TSharedPtr<FReusableWidgetTemplateViewModel> InWidgetViewModel) {
	WidgetViewModel = InWidgetViewModel;

	ChildSlot [
		SNew(SHorizontalBox)
		.ToolTip(WidgetViewModel->Template->GetToolTip())
		+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [	
			SNew(SCheckBox)
			.ToolTipText(this, &SReusablePaletteViewItem::GetFavoriteToggleToolTipText)
			.IsChecked(this, &SReusablePaletteViewItem::GetFavoritedState)
			.OnCheckStateChanged(this, &SReusablePaletteViewItem::OnFavoriteToggled)
			.Style(FAppStyle::Get(), "UMGEditor.Palette.FavoriteToggleStyle")
			.Visibility(this, &SReusablePaletteViewItem::GetFavoritedStateVisibility)
		]
		+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [
			SNew(SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image(WidgetViewModel->Template->GetIcon())
		]
		+SHorizontalBox::Slot().AutoWidth().Padding(2, 0, 0, 0).VAlign(VAlign_Center) [
			SNew(STextBlock)
			.Text(InWidgetViewModel->GetName())
			.HighlightText(InArgs._HighlightText)
		]
	];
}

FReply SReusablePaletteViewItem::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) {
	return WidgetViewModel->Template->OnDoubleClicked();
};

void SReusablePaletteView::Construct(const FArguments&, const TSharedPtr<FReusablePaletteViewModel>& InPaletteViewModel) {
	PaletteViewModel = InPaletteViewModel;

	// Register to the update of the viewmodel.
	PaletteViewModel->OnUpdating.AddRaw(this, &SReusablePaletteView::OnViewModelUpdating);
	PaletteViewModel->OnUpdated.AddRaw(this, &SReusablePaletteView::OnViewModelUpdated);

	WidgetFilter = MakeShared<TTextFilter<TSharedPtr<FReusableWidgetViewModel>>>(
		TTextFilter<TSharedPtr<FReusableWidgetViewModel>>::FItemToStringArray::CreateSP(this, &SReusablePaletteView::GetWidgetFilterStrings));

	FilterHandler = MakeShared<TreeFilterHandler<TSharedPtr<FReusableWidgetViewModel>>>();
	FilterHandler->SetFilter(WidgetFilter.Get());
	FilterHandler->SetRootItems(&PaletteViewModel->GetWidgetViewModels(), &TreeWidgetViewModels);
	FilterHandler->SetGetChildrenDelegate(TreeFilterHandler<TSharedPtr<FReusableWidgetViewModel>>::FOnGetChildren::CreateRaw(this, &SReusablePaletteView::OnGetChildren));

	WidgetTemplatesView = SNew(STreeView<TSharedPtr<FReusableWidgetViewModel>>)
	.ItemHeight(1.0f)
	.SelectionMode(ESelectionMode::SingleToggle)
	.OnGenerateRow(this, &SReusablePaletteView::OnGenerateWidgetTemplateItem)
	.OnGetChildren(FilterHandler.ToSharedRef(), &TreeFilterHandler<TSharedPtr<FReusableWidgetViewModel>>::OnGetFilteredChildren)
	.OnMouseButtonClick(this, &SReusablePaletteView::WidgetPalette_OnClick)
	.TreeItemsSource(&TreeWidgetViewModels);

	FilterHandler->SetTreeView(WidgetTemplatesView.Get());

	ChildSlot [
		SNew(SVerticalBox)
		+SVerticalBox::Slot().Padding(4.0f).AutoHeight() [
			SAssignNew(SearchBoxPtr, SSearchBox)
			.HintText(LOCTEXT("SearchPalette", "Search Palette"))
			.OnTextChanged(this, &SReusablePaletteView::OnSearchChanged)
		]
		+SVerticalBox::Slot().FillHeight(1.0f) [
			SNew(SScrollBorder, WidgetTemplatesView.ToSharedRef()) [
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
				.Padding(0.0f) [
					WidgetTemplatesView.ToSharedRef()
				]
			]
		]
	];
	bRefreshRequested = true;
	PaletteViewModel->Update();
}

SReusablePaletteView::~SReusablePaletteView() {
	PaletteViewModel->OnUpdating.RemoveAll(this);
	PaletteViewModel->OnUpdated.RemoveAll(this);

	if (FilterHandler->GetIsEnabled()) {
		FilterHandler->SetIsEnabled(false);
		FilterHandler->RefreshAndFilterTree();
	}
}

FText SReusablePaletteView::GetSearchText() const {
	return WidgetFilter->GetRawFilterText();
}

void SReusablePaletteView::OnSearchChanged(const FText& InFilterText) {
	bRefreshRequested = true;
	FilterHandler->SetIsEnabled(!InFilterText.IsEmpty());
	WidgetFilter->SetRawFilterText(InFilterText);
	SearchBoxPtr->SetError(WidgetFilter->GetFilterErrorText());
	PaletteViewModel->SetSearchText(InFilterText);
}

void SReusablePaletteView::WidgetPalette_OnClick(TSharedPtr<FReusableWidgetViewModel> SelectedItem) {
	if (!SelectedItem.IsValid()) {
		return;
	}
	if (SelectedItem->IsCategory()) {
		if (const TSharedPtr<FReusableWidgetHeaderViewModel> CategoryHeader = StaticCastSharedPtr<FReusableWidgetHeaderViewModel>(SelectedItem)) {
			if (const TSharedPtr<ITableRow> TableRow = WidgetTemplatesView->WidgetFromItem(CategoryHeader)) {
				TableRow->ToggleExpansion();
			}
		}
	}
}

TSharedPtr<FWidgetTemplate> SReusablePaletteView::GetSelectedTemplateWidget() const {
	TArray<TSharedPtr<FReusableWidgetViewModel>> SelectedTemplates = WidgetTemplatesView.Get()->GetSelectedItems();
	if (SelectedTemplates.Num() == 1) {
		TSharedPtr<FReusableWidgetTemplateViewModel> TemplateViewModel = StaticCastSharedPtr<FReusableWidgetTemplateViewModel>(SelectedTemplates[0]);
		if (TemplateViewModel.IsValid()) {
			return TemplateViewModel->Template;
		}
	}
	return nullptr;
}

void SReusablePaletteView::OnGetChildren(TSharedPtr<FReusableWidgetViewModel> Item, TArray< TSharedPtr<FReusableWidgetViewModel> >& Children) {
	return Item->GetChildren(Children);
}

TSharedRef<ITableRow> SReusablePaletteView::OnGenerateWidgetTemplateItem(TSharedPtr<FReusableWidgetViewModel> Item, const TSharedRef<STableViewBase>& OwnerTable) {
	return Item->BuildRow(OwnerTable);
}

void SReusablePaletteView::OnViewModelUpdating() {
	WidgetTemplatesView->GetExpandedItems(ExpandedItems);
}

void SReusablePaletteView::OnViewModelUpdated() {
	bRefreshRequested = true;

	for (TSharedPtr<FReusableWidgetViewModel>& ExpandedItem : ExpandedItems) {
		for (TSharedPtr<FReusableWidgetViewModel>& ViewModel : PaletteViewModel->GetWidgetViewModels()) {
			if (ViewModel->GetName().EqualTo(ExpandedItem->GetName()) || ViewModel->ShouldForceExpansion()) {
				WidgetTemplatesView->SetItemExpansion(ViewModel, true);
			}
		}
	}
	ExpandedItems.Reset();
}

void SReusablePaletteView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) {
	if (bRefreshRequested) {
		bRefreshRequested = false;
		FilterHandler->RefreshAndFilterTree();
	}
}

void SReusablePaletteView::GetWidgetFilterStrings(TSharedPtr<FReusableWidgetViewModel> WidgetViewModel, TArray<FString>& OutStrings) {
	WidgetViewModel->GetFilterStrings(OutStrings);
}

#undef LOCTEXT_NAMESPACE
