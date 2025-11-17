#include "Hooking/OverlayWidgetTreeEditor.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Hooking/WidgetBlueprintMixinEditor.h"
#include "Patching/WidgetBlueprintHook.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "SMLEditor"

FWidgetRootNode::FWidgetRootNode(UBlueprintGeneratedClass* InWidgetMixinTargetClass) {
	RootNodeName = InWidgetMixinTargetClass ? FText::Format(LOCTEXT("RootWidgetNodeName", "[{0}]"), FText::FromName(InWidgetMixinTargetClass->GetFName())) : FText::GetEmpty();
}

FText FWidgetRootNode::GetNameLabel(bool bIsEditingName) const {
	return RootNodeName;
}

FSlateFontInfo FWidgetRootNode::GetNameFont(bool bIsEditingName) const {
	return FCoreStyle::GetDefaultFontStyle("Bold", 10);
}

void SOverlayWidgetTreeEditor::CreateComponentFromClass( UClass* InComponentClass, UObject* InComponentAsset, UObject* InComponentTemplate )
{
}

FImmutableTemplateWidgetNode::FImmutableTemplateWidgetNode(UWidget* InWidgetReference) : WidgetReference(InWidgetReference) {
}

FText FImmutableTemplateWidgetNode::GetNameLabel(bool bIsEditingName) const {
	if (const UWidget* Widget = WidgetReference.Get()) {
		return bIsEditingName ? Widget->GetLabelText() : Widget->GetLabelTextWithMetadata();
	}
	return FText::GetEmpty();
}

FSlateFontInfo FImmutableTemplateWidgetNode::GetNameFont(bool bIsEditingName) const {
	if (const UWidget* Widget = WidgetReference.Get()) {
		if (!Widget->IsGeneratedName() && Widget->bIsVariable) {
			return FCoreStyle::GetDefaultFontStyle("Bold", 10);
		}
	}
	return FCoreStyle::GetDefaultFontStyle("Regular", 9);
}

void FImmutableTemplateWidgetNode::GetFilterStrings(TArray<FString>& OutStrings) const {
	FAbstractSubobjectTreeNode::GetFilterStrings(OutStrings);

	const UWidget* WidgetTemplate = WidgetReference.Get();
	if (WidgetTemplate && !WidgetTemplate->IsGeneratedName()) {
		OutStrings.Add(WidgetTemplate->GetClass()->GetName());
		OutStrings.Add(WidgetTemplate->GetClass()->GetDisplayNameText().ToString());
	}
}

const UObject* FImmutableTemplateWidgetNode::GetImmutableObject() const {
	return WidgetReference.Get();
}

UWidget* FImmutableTemplateWidgetNode::GetTemplateWidget() const {
	return WidgetReference.Get();
}

void SOverlayWidgetTreeEditor::Construct(const FArguments&, const TSharedPtr<FWidgetBlueprintMixinEditor>& InOwnerBlueprintEditor) {
	OwnerBlueprintEditor = InOwnerBlueprintEditor;
	SAbstractSubobjectTreeEditor::Construct(SAbstractSubobjectTreeEditor::FArguments());

	ChildSlot [
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")) [
			SNew(SVerticalBox)
			+SVerticalBox::Slot().Padding(4.0f).AutoHeight() [
				SAssignNew(SearchBoxPtr, SSearchBox)
				.HintText(LOCTEXT("SearchWidgets", "Search Widgets"))
				.OnTextChanged(this, &SOverlayWidgetTreeEditor::OnSearchChanged)
			]
			+SVerticalBox::Slot().FillHeight(1.0f) [
				SNew(SBorder)
				.Padding(0)
				.BorderImage(FAppStyle::GetBrush("NoBrush")) [
					SubobjectTreeView.ToSharedRef()
				]
			]
		]
	];

	// Expand all default nodes in the current tree
	for (const TSharedPtr<FAbstractSubobjectTreeNode>& ComponentNode : AllNodes) {
		SubobjectTreeView->SetItemExpansion(ComponentNode, true);
	}
}

bool SOverlayWidgetTreeEditor::IsEditingAllowed() const {
	// Editing allowed if the blueprint editor is in the editing mode, e.g. no PIE session is running
	if (const TSharedPtr<FWidgetBlueprintMixinEditor> BlueprintEditor = OwnerBlueprintEditor.Pin()) {
		return BlueprintEditor->InEditingMode();
	}
	return false;
}

FReply SOverlayWidgetTreeEditor::TryHandleAssetDragNDropOperation(const FDragDropEvent& DragDropEvent) {
	return FReply::Unhandled();
}

void SOverlayWidgetTreeEditor::HandleDeleteAction(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& NodesToDelete) {
}

void SOverlayWidgetTreeEditor::HandleRenameAction(const TSharedPtr<FAbstractSubobjectTreeNode>& SubobjectData, const FText& InNewsubobjectName) {
}

void SOverlayWidgetTreeEditor::HandleDragDropAction(const TSharedRef<FAbstractSubobjectTreeDragDropOp>& DragDropAction, const TSharedPtr<FAbstractSubobjectTreeNode>& DropOntoNode) {
}

TSharedRef<FAbstractSubobjectTreeDragDropOp> SOverlayWidgetTreeEditor::CreateDragDropActionFromNodes(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& SelectedNodes) const {
	return FOverlayWidgetTreeRowDragDropOp::Create(SelectedNodes);
}

void SOverlayWidgetTreeEditor::UpdateDragDropOntoNode(const TSharedRef<FAbstractSubobjectTreeDragDropOp>& DragDropAction, const TSharedPtr<FAbstractSubobjectTreeNode>& DropOntoNode, FText& OutFeedbackMessage) const {
}

void SOverlayWidgetTreeEditor::RefreshSubobjectTreeFromDataSource() {
	// Retrieve the current hook blueprint we are editing
	const UWidgetMixinBlueprint* BlueprintObject = nullptr;
	if (const TSharedPtr<FHookBlueprintEditor> BlueprintEditor = OwnerBlueprintEditor.Pin()) {
		BlueprintObject = Cast<UWidgetMixinBlueprint>(BlueprintEditor->GetBlueprintObj());
	}

	// Cache a list of all nodes that we had prior to this point
	const TArray<TSharedPtr<FAbstractSubobjectTreeNode>> AllNodesOld = AllNodes;
	const TMap<TWeakObjectPtr<const UObject>, TSharedPtr<FAbstractSubobjectTreeNode>> OldObjectToComponentLookup = ObjectToComponentDataCache;
	
	// Wipe hierarchy data from the nodes that we might want to keep around
	for (const TSharedPtr<FAbstractSubobjectTreeNode>& OldNodeObject : AllNodesOld) {
		OldNodeObject->ClearHierarchyData();
	}

	// Clean up the current list of nodes and the current node cache
	RootNodes.Empty();
	AllNodes.Empty();
	ObjectToComponentDataCache.Empty();

	// We cannot populate new set of nodes without a valid blueprint or without a valid target class
	if (BlueprintObject == nullptr || BlueprintObject->MixinTargetClass == nullptr) {
		return;
	}
	
	UWidgetBlueprintGeneratedClass* TargetWidgetBlueprintClass = Cast<UWidgetBlueprintGeneratedClass>(BlueprintObject->MixinTargetClass);
	UWidgetTree* TargetWidgetTreeTemplate = TargetWidgetBlueprintClass ? TargetWidgetBlueprintClass->GetWidgetTreeArchetype() : nullptr;

	// Allocate root node if not allocated yet
	if (RootNode == nullptr) {
		RootNode = MakeShared<FWidgetRootNode>(TargetWidgetBlueprintClass);
	}
	AllNodes.Add(RootNode);
	RootNodes.Add(RootNode);

	// Populate template widgets from the parent widget tree
	if (TargetWidgetTreeTemplate) {
		
		// Allocate nodes for each widget in the hierarchy
		TargetWidgetTreeTemplate->ForEachWidget([&](UWidget* TemplateWidget) {
			TSharedPtr<FAbstractSubobjectTreeNode> SubobjectTreeNode;

			// Attempt to find an existing node for this widget template
			if (const TSharedPtr<FAbstractSubobjectTreeNode>* ExistingNode = OldObjectToComponentLookup.Find(TemplateWidget)) {
				SubobjectTreeNode = *ExistingNode;
			} else {
				// Create a new component data otherwise
				SubobjectTreeNode = MakeShared<FImmutableTemplateWidgetNode>(TemplateWidget);
			}
			// Add the node to the all nodes list and the node to the tree node mapping
			AllNodes.Add(SubobjectTreeNode);
			ObjectToComponentDataCache.Add(TemplateWidget, SubobjectTreeNode);
		});

		// Resolve the widget hierarchy for the widget tree nodes
		TargetWidgetTreeTemplate->ForEachWidget([&](UWidget* TemplateWidget) {

			const TSharedPtr<FAbstractSubobjectTreeNode> WidgetNode = ObjectToComponentDataCache.FindChecked(TemplateWidget);

			TSharedPtr<FAbstractSubobjectTreeNode> ParentNode = RootNode;
			UPanelWidget* ParentTemplateWidget = TemplateWidget->GetParent();
			if (ParentTemplateWidget && ObjectToComponentDataCache.Contains(ParentTemplateWidget)) {
				ParentNode = ObjectToComponentDataCache.FindChecked(ParentTemplateWidget);
			}
			ParentNode->AddChildNode(WidgetNode);
		});
	}
}

void SOverlayWidgetTreeEditor::UpdateSelectionFromNodes(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& SelectedNodes) {
	if (const TSharedPtr<FWidgetBlueprintMixinEditor> BlueprintEditor = OwnerBlueprintEditor.Pin()) {
		// Collect all objects from the nodes that we want to edit
		TArray<UObject*> ComponentObjectsToEdit;
		FText InspectorTitle;
		for (const TSharedPtr<FAbstractSubobjectTreeNode>& ComponentData : SelectedNodes) {
			if (ComponentData->IsNodeEditable()) {
				if (UActorComponent* EditableActorComponent = Cast<UActorComponent>(ComponentData->GetMutableObject())) {
					ComponentObjectsToEdit.Add(EditableActorComponent);
					InspectorTitle = ComponentData->GetNameLabel(false);
				}
			}
		}

		// Show the inspector with the new selection if we have any valid objects
		if (!ComponentObjectsToEdit.IsEmpty()) {
			SKismetInspector::FShowDetailsOptions DetailsOptions(InspectorTitle, true);
			DetailsOptions.bShowComponents = false;
			BlueprintEditor->GetInspector()->ShowDetailsForObjects(ComponentObjectsToEdit, DetailsOptions);
		}
	}
}

TSharedRef<ITableRow> SOverlayWidgetTreeEditor::MakeTableRowWidget(TSharedPtr<FAbstractSubobjectTreeNode> InNodePtr, const TSharedRef<STableViewBase>& OwnerTable) {
	if (InNodePtr.IsValid()) {
		return SNew(SOverlayWidgetTreeRowWidget, SharedThis(this), InNodePtr, OwnerTable);	
	}
	return SNew(STableRow<TSharedPtr<FAbstractSubobjectTreeNode>>, OwnerTable);
}

FText SOverlayWidgetTreeEditor::CreateTooltipForWidgetClass(const UClass* WidgetClass) {
	if (WidgetClass->IsChildOf<UUserWidget>() && WidgetClass->ClassGeneratedBy) {
		const FString& Description = Cast<UWidgetBlueprint>(WidgetClass->ClassGeneratedBy)->BlueprintDescription;
		if (Description.Len() > 0) {
			return FText::FromString(Description);
		}
	}
	return WidgetClass->GetToolTipText();
}

TSharedRef<FOverlayWidgetTreeRowDragDropOp> FOverlayWidgetTreeRowDragDropOp::Create(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& InSourceNodes) {
	TSharedRef<FOverlayWidgetTreeRowDragDropOp> NewOperation = MakeShared<FOverlayWidgetTreeRowDragDropOp>();
	NewOperation->SourceNodes = InSourceNodes;
	NewOperation->Construct();
	return NewOperation;
}

void SOverlayWidgetTreeRowWidget::Construct(const FArguments& InArgs, TSharedPtr<SAbstractSubobjectTreeEditor> InOwnerEditor, const TSharedPtr<FAbstractSubobjectTreeNode> InNodePtr, TSharedPtr<STableViewBase> InOwnerTableView) {
	const bool bShowIconBrush = Cast<UWidget>(InNodePtr->GetImmutableObject()) != nullptr;
	
	SAbstractSubobjectTreeRowWidget::Construct(SAbstractSubobjectTreeRowWidget::FArguments()
		.ShowIconBrush(bShowIconBrush), InOwnerEditor, InNodePtr, InOwnerTableView);
}

const FSlateBrush* SOverlayWidgetTreeRowWidget::GetIconBrush() const {
	if (const UObject* WidgetTemplate = SubobjectData->GetImmutableObject()) {
		return FSlateIconFinder::FindIconBrushForClass(WidgetTemplate->GetClass());
	}
	return SAbstractSubobjectTreeRowWidget::GetIconBrush();
}

TSharedRef<SToolTip> SOverlayWidgetTreeRowWidget::CreateIconTooltipWidget() const {
	if (const UWidget* WidgetTemplate = Cast<UWidget>(SubobjectData->GetImmutableObject())) {
		return SNew(SToolTip).Text(SOverlayWidgetTreeEditor::CreateTooltipForWidgetClass(WidgetTemplate->GetClass()));
	}
	return SNew(SToolTip);
}

TSharedRef<SToolTip> SOverlayWidgetTreeRowWidget::CreateTooltipWidget() const {
	if (const UWidget* WidgetTemplate = Cast<UWidget>(SubobjectData->GetImmutableObject())) {
		if (WidgetTemplate && !WidgetTemplate->IsGeneratedName()) {
			return SNew(SToolTip).Text(FText::FromString(TEXT( "[" ) + WidgetTemplate->GetClass()->GetDisplayNameText().ToString() + TEXT( "]" )));
		}
	}
	return SNew(SToolTip);
}

#undef LOCTEXT_NAMESPACE
