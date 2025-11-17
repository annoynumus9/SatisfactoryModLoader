#include "Hooking/AbstractSubobjectTree.h"
#include "AssetSelection.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Framework/Commands/GenericCommands.h"
#include "Misc/TextFilter.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

FSlateFontInfo FAbstractSubobjectTreeNode::GetNameFont(bool bIsEditingName) const {
	return FCoreStyle::GetDefaultFontStyle("Regular", 9);
}

void SAbstractSubobjectTreeEditor::Construct(const FArguments& InArgs) {
	RegisterEditorCommands();
	RefreshSubobjectTreeFromDataSource();

	SearchBoxTreeFilter = MakeShared<TTextFilter<TSharedPtr<FAbstractSubobjectTreeNode>>>(
		TTextFilter<TSharedPtr<FAbstractSubobjectTreeNode>>::FItemToStringArray::CreateSP(this, &SAbstractSubobjectTreeEditor::GetNodeFilterStrings));

	FilterHandler = MakeShared<TreeFilterHandler<TSharedPtr<FAbstractSubobjectTreeNode>>>();
	FilterHandler->SetFilter(SearchBoxTreeFilter.Get());
	FilterHandler->SetRootItems(&RootNodes, &FilteredRootNodes);
	FilterHandler->SetGetChildrenDelegate(TreeFilterHandler<TSharedPtr<FAbstractSubobjectTreeNode>>::FOnGetChildren::CreateRaw(this, &SAbstractSubobjectTreeEditor::OnGetChildrenForTree));

	SubobjectTreeView = SNew(SAbstractSubobjectTreeView, SharedThis(this))
		.TreeItemsSource(&FilteredRootNodes)
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow(this, &SAbstractSubobjectTreeEditor::MakeTableRowWidget)
		.OnGetChildren(FilterHandler.ToSharedRef(), &TreeFilterHandler<TSharedPtr<FAbstractSubobjectTreeNode>>::OnGetFilteredChildren)
		.OnSetExpansionRecursive(this, &SAbstractSubobjectTreeEditor::SetItemExpansionRecursive)
		.OnSelectionChanged(this, &SAbstractSubobjectTreeEditor::OnTreeSelectionChanged)
		// TODO: Create context menu later.
		//.OnContextMenuOpening(this, &SAbstractSubobjectTreeEditor::CreateContextMenu)
		.OnItemScrolledIntoView(this, &SAbstractSubobjectTreeEditor::OnItemScrolledIntoView)
		.ClearSelectionOnClick(true)
		.HighlightParentNodesForSelection(true)
		.ItemHeight(InArgs._ItemHeight);

	FilterHandler->SetTreeView(SubobjectTreeView.Get());
	FilterHandler->RefreshAndFilterTree();
}

FReply SAbstractSubobjectTreeEditor::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) {
	// Process generic keyboard actions from commands (like copy and paste)
	if (CommandList->ProcessCommandBindings(InKeyEvent)) {
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SAbstractSubobjectTreeEditor::RegisterEditorCommands() {
	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction(FGenericCommands::Get().Delete,
		FUIAction(FExecuteAction::CreateSP(this, &SAbstractSubobjectTreeEditor::OnDeleteNodes),
		FCanExecuteAction::CreateSP(this, &SAbstractSubobjectTreeEditor::CanDeleteNodes))
	);
	CommandList->MapAction(FGenericCommands::Get().Rename,
		FUIAction(FExecuteAction::CreateSP(this, &SAbstractSubobjectTreeEditor::OnRenameSubobject),
		FCanExecuteAction::CreateSP(this, &SAbstractSubobjectTreeEditor::CanRenameSubobject))
	);
}

TArray<TSharedPtr<FAbstractSubobjectTreeNode>> SAbstractSubobjectTreeEditor::GetSelectedNodes() const {
	TArray<TSharedPtr<FAbstractSubobjectTreeNode>> SelectedTreeNodes = SubobjectTreeView->GetSelectedItems();

	// Sort the nodes from the parent to the child
	SelectedTreeNodes.StableSort([](const TSharedPtr<FAbstractSubobjectTreeNode>& A, const TSharedPtr<FAbstractSubobjectTreeNode>& B) {
		return B.IsValid() && B->IsAttachedToParent(A);
	});
	return SelectedTreeNodes;
}

TSharedRef<ITableRow> SAbstractSubobjectTreeEditor::MakeTableRowWidget(TSharedPtr<FAbstractSubobjectTreeNode> InNodePtr, const TSharedRef<STableViewBase>& OwnerTable) {
	if (InNodePtr.IsValid()) {
		return SNew(SAbstractSubobjectTreeRowWidget, SharedThis(this), InNodePtr, OwnerTable);	
	}
	return SNew(STableRow<TSharedPtr<FAbstractSubobjectTreeNode>>, OwnerTable);
}

void SAbstractSubobjectTreeEditor::OnGetChildrenForTree(const TSharedPtr<FAbstractSubobjectTreeNode> InNodePtr, TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& OutChildren) {
	// Populate the children nodes using the function on the component data
	if (InNodePtr) {
		InNodePtr->GetChildrenNodes(OutChildren);
	}
}

void SAbstractSubobjectTreeEditor::SetItemExpansionRecursive(const TSharedPtr<FAbstractSubobjectTreeNode> InNodeToChange, bool bInExpansionState) {
	if (SubobjectTreeView.IsValid() && InNodeToChange.IsValid()) {
		// Expand the provided item
		SubobjectTreeView->SetItemExpansion(InNodeToChange, bInExpansionState);

		// Retrieve the children of the provided item and expand them too
		TArray<TSharedPtr<FAbstractSubobjectTreeNode>> Children;
		InNodeToChange->GetChildrenNodes(Children);
		for (const TSharedPtr<FAbstractSubobjectTreeNode>& Child : Children) {
			SetItemExpansionRecursive(Child, bInExpansionState);
		}
	}
}

void SAbstractSubobjectTreeEditor::GetNodeFilterStrings(TSharedPtr<FAbstractSubobjectTreeNode> Item, TArray<FString>& OutStrings) {
	if (Item.IsValid()) {
		Item->GetFilterStrings(OutStrings);
	}
}

void SAbstractSubobjectTreeEditor::OnTreeSelectionChanged(TSharedPtr<FAbstractSubobjectTreeNode>, ESelectInfo::Type) {
	// Update the details panel state from a list of the selected nodes
	UpdateSelectionFromNodes(SubobjectTreeView->GetSelectedItems());
}

void SAbstractSubobjectTreeEditor::OnItemScrolledIntoView(TSharedPtr<FAbstractSubobjectTreeNode> InItem, const TSharedPtr<ITableRow>& InWidget) {
	// If we have a component rename pending, once the component is scrolled into the view, we need to start the edit mode on the name label
	if (PendingRenameSubobjectData.IsValid() && PendingRenameSubobjectData == InItem) {
		if (const TSharedPtr<SAbstractSubobjectTreeRowWidget> RowWidget = StaticCastSharedPtr<SAbstractSubobjectTreeRowWidget>(InWidget)) {
			RowWidget->StartEditingSubobjectName();
		}
		PendingRenameSubobjectData.Reset();
	}
}

void SAbstractSubobjectTreeEditor::UpdateTree() {
	// Refresh the component tree now that we have saved the selection state
	RefreshSubobjectTreeFromDataSource();

	// Request the refresh of the component tree widget
	if (FilterHandler.IsValid()) {
		FilterHandler->RefreshAndFilterTree();
	}
}

void SAbstractSubobjectTreeEditor::OnSearchChanged(const FText& InFilterText) {
	const bool bFilteringEnabled = !InFilterText.IsEmpty();
	if (bFilteringEnabled != FilterHandler->GetIsEnabled()) {
		FilterHandler->SetIsEnabled(bFilteringEnabled);
	}
	SearchBoxTreeFilter->SetRawFilterText(InFilterText);
	if (SearchBoxPtr.IsValid()) {
		SearchBoxPtr->SetError(SearchBoxTreeFilter->GetFilterErrorText());
	}
	FilterHandler->RefreshAndFilterTree();
}

FText SAbstractSubobjectTreeEditor::GetSearchText() const {
	return SearchBoxTreeFilter->GetRawFilterText();
}

bool SAbstractSubobjectTreeEditor::CanRenameSubobject() const {
	const TArray<TSharedPtr<FAbstractSubobjectTreeNode>> SelectedNodes = GetSelectedNodes();
	return IsEditingAllowed() && (SelectedNodes.Num() == 1 && SelectedNodes[0]->IsNodeEditable());
}

void SAbstractSubobjectTreeEditor::OnRenameSubobject() {
	const TArray<TSharedPtr<FAbstractSubobjectTreeNode>> SelectedNodes = GetSelectedNodes();
	if (SelectedNodes.Num() == 1 && SelectedNodes[0]->IsNodeEditable()) {
		PendingRenameSubobjectData = SelectedNodes[0];
		SubobjectTreeView->RequestScrollIntoView(SelectedNodes[0]);
	}
}

bool SAbstractSubobjectTreeEditor::CanDeleteNodes() const {
	if(!IsEditingAllowed()) {
		return false;
	}
	// Components can be deleted if their nodes are editable
	const TArray<TSharedPtr<FAbstractSubobjectTreeNode>> SelectedNodes = GetSelectedNodes();
	for (const TSharedPtr<FAbstractSubobjectTreeNode>& SelectedNode : SelectedNodes) {
		if (!SelectedNode->IsNodeEditable()) {
			return false;
		}
	}
	return true;
}

void SAbstractSubobjectTreeEditor::OnDeleteNodes() {
	const TArray<TSharedPtr<FAbstractSubobjectTreeNode>> SelectedNodes = GetSelectedNodes();
	HandleDeleteAction(SelectedNodes);
}

void SAbstractSubobjectTreeView::Construct(const FArguments& InArgs, const TSharedPtr<SAbstractSubobjectTreeEditor>& InOwnerTreeEditor) {
	OwnerTreeEditor = InOwnerTreeEditor;
	STreeView::Construct(InArgs);
}

FReply SAbstractSubobjectTreeView::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) {
	const TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

	// Allow dragging over assets of the supported class and component types
	if (Operation.IsValid() && (Operation->IsOfType<FExternalDragOperation>() || Operation->IsOfType<FAssetDragDropOp>())) {
		const FReply ParentReply = AssetUtil::CanHandleAssetDrag(DragDropEvent);
		if (!ParentReply.IsEventHandled() && Operation->IsOfType<FAssetDragDropOp>()) {
			const TSharedPtr<FAssetDragDropOp> AssetDragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);
			if (ContainsAnyClassAssets(AssetDragDropOp)) {
				return FReply::Handled();
			}
		}
		return ParentReply;
	}
	return FReply::Unhandled();
}

bool SAbstractSubobjectTreeView::ContainsAnyClassAssets(const TSharedPtr<FAssetDragDropOp>& AssetDragNDropOp) {
	for (const FAssetData& AssetData : AssetDragNDropOp->GetAssets()) {
		if (const UClass* AssetClass = AssetData.GetClass()) {
			if (AssetClass->IsChildOf(UClass::StaticClass())) {
				return true;
			}
		}
	}
	return false;
}

FReply SAbstractSubobjectTreeView::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) {
	if (const TSharedPtr<SAbstractSubobjectTreeEditor> OwnerEditor = OwnerTreeEditor.Pin()) {
		return OwnerEditor->TryHandleAssetDragNDropOperation(DragDropEvent);
	}
	return FReply::Unhandled();
}

void SAbstractSubobjectTreeRowWidget::Construct(const FArguments& Arguments, TSharedPtr<SAbstractSubobjectTreeEditor> InOwnerEditor, const TSharedPtr<FAbstractSubobjectTreeNode> InNodePtr, TSharedPtr<STableViewBase> InOwnerTableView) {
	OwnerEditor = InOwnerEditor;
	SubobjectData = InNodePtr;

	const STableRow::FArguments Args = STableRow::FArguments()
		.Style(Arguments._TableRowStyle)
		.Padding(Arguments._Padding)
		.OnDragDetected(this, &SAbstractSubobjectTreeRowWidget::HandleOnDragDetected)
		.OnDragEnter(this, &SAbstractSubobjectTreeRowWidget::HandleOnDragEnter)
		.OnDragLeave(this, &SAbstractSubobjectTreeRowWidget::HandleOnDragLeave)
		.OnCanAcceptDrop(this, &SAbstractSubobjectTreeRowWidget::HandleOnCanAcceptDrop)
		.OnAcceptDrop(this, &SAbstractSubobjectTreeRowWidget::HandleOnAcceptDrop)
		.Content() [
			SNew(SHorizontalBox)
			.ToolTip(CreateTooltipWidget())
			+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [
				SNew(SExpanderArrow, SharedThis(this))
				.Visibility(EVisibility::Visible)
			]
			+SHorizontalBox::Slot().Padding(Arguments._IconPadding).AutoWidth().VAlign(VAlign_Center) [
				SAssignNew(IconImage, SImage)
				.Image(GetIconBrush())
				.ColorAndOpacity(FSlateColor::UseForeground())
				.ToolTip(CreateIconTooltipWidget())
				.Visibility(Arguments._ShowIconBrush ? EVisibility::Visible : EVisibility::Collapsed)
			]
			+SHorizontalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left).AutoWidth() [
				SAssignNew(EditableNameTextBlock, SInlineEditableTextBlock)
				.OnEnterEditingMode(this, &SAbstractSubobjectTreeRowWidget::OnEnterTextEditingMode)
				.OnExitEditingMode(this, &SAbstractSubobjectTreeRowWidget::OnExitTextEditingMode)
				.Text(this, &SAbstractSubobjectTreeRowWidget::GetNameLabel)
				.Font(this, &SAbstractSubobjectTreeRowWidget::GetNameFont)
				.ColorAndOpacity(SubobjectData->IsNodeEditable() ? FSlateColor(FLinearColor(0.25f, 0.72f, 0.1f)) : FSlateColor::UseForeground())
				.OnVerifyTextChanged(this, &SAbstractSubobjectTreeRowWidget::OnNameTextVerifyChanged)
				.OnTextCommitted(this, &SAbstractSubobjectTreeRowWidget::OnNameTextCommit)
				.IsSelected(this, &SAbstractSubobjectTreeRowWidget::IsSelectedExclusively)
				.IsReadOnly(!SubobjectData->IsNodeEditable())
			]
		];
	STableRow::Construct(Args, InOwnerTableView.ToSharedRef());
	
	ExpanderArrowWidget->SetVisibility(EVisibility::Collapsed);
}

void SAbstractSubobjectTreeRowWidget::StartEditingSubobjectName() {
	if (EditableNameTextBlock) {
		EditableNameTextBlock->EnterEditingMode();
	}
}

FText SAbstractSubobjectTreeRowWidget::GetNameLabel() const {
	return SubobjectData->GetNameLabel(bIsBeingEdited);
}

FSlateFontInfo SAbstractSubobjectTreeRowWidget::GetNameFont() const {
	return SubobjectData->GetNameFont(bIsBeingEdited);
}

void SAbstractSubobjectTreeRowWidget::OnEnterTextEditingMode() {
	bIsBeingEdited = true;
	InitialText = GetNameLabel();
}

void SAbstractSubobjectTreeRowWidget::OnExitTextEditingMode() {
	bIsBeingEdited = false;
}

bool SAbstractSubobjectTreeRowWidget::OnNameTextVerifyChanged(const FText& InNewText, FText& OutErrorMessage) {
	return SubobjectData->CheckValidRename(InNewText, OutErrorMessage);
}

void SAbstractSubobjectTreeRowWidget::OnNameTextCommit(const FText& InNewName, ETextCommit::Type) {
	if (InitialText.EqualToCaseIgnored(InNewName)) {
		return;
	}
	if (const TSharedPtr<SAbstractSubobjectTreeEditor> PinnedOwnerTreeEditor = OwnerEditor.Pin()) {
		PinnedOwnerTreeEditor->HandleRenameAction(SubobjectData, InNewName);	
	}
}

const FSlateBrush* SAbstractSubobjectTreeRowWidget::GetIconBrush() const {
	return FAppStyle::GetBrush("SCS.Component");
}

TSharedRef<SToolTip> SAbstractSubobjectTreeRowWidget::CreateTooltipWidget() const {
	return SNew(SToolTip);
}

TSharedRef<SToolTip> SAbstractSubobjectTreeRowWidget::CreateIconTooltipWidget() const {
	return CreateTooltipWidget();
}

void SAbstractSubobjectTreeRowWidget::HandleOnDragEnter(const FDragDropEvent& DragDropEvent) {
	const TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	const TSharedPtr<SAbstractSubobjectTreeEditor> PinnedOwnerTreeEditor = OwnerEditor.Pin();

	// Exit early if operation or our owner are not valid
	if (!Operation.IsValid() || !PinnedOwnerTreeEditor.IsValid()) {
		return;
	}

	// If this is a drag and drop action from another node, evaluate its payload to determine which action dropping it on this node would result in
	if (const TSharedPtr<FAbstractSubobjectTreeDragDropOp> DragRowOp = DragDropEvent.GetOperationAs<FAbstractSubobjectTreeDragDropOp>()) {
		// Ask the parent editor which action we should take with this drag n drop
		FText FeedbackMessage;
		PinnedOwnerTreeEditor->UpdateDragDropOntoNode(DragRowOp.ToSharedRef(), SubobjectData, FeedbackMessage);

		// If we have a pending operation, the drop can be performed, otherwise, it is not allowed
		const FSlateBrush* StatusSymbol = DragRowOp->IsValidDragDropTargetAction()
				? FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"))
				: FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
		if (FeedbackMessage.IsEmpty()) {
			DragRowOp->SetFeedbackMessage(nullptr);
		} else {
			DragRowOp->SetSimpleFeedbackMessage(StatusSymbol, FLinearColor::White, FeedbackMessage);
		}
	}
	// Let the tree handle the external drag operations and asset dragging
	else if (Operation->IsOfType<FExternalDragOperation>() || Operation->IsOfType<FAssetDragDropOp>()) {
		if (const TSharedPtr<SAbstractSubobjectTreeView>& SubobjectTree = PinnedOwnerTreeEditor->GetSubobjectTree()) {
			SubobjectTree->OnDragEnter(FGeometry(), DragDropEvent);
		}
	}
}

void SAbstractSubobjectTreeRowWidget::HandleOnDragLeave(const FDragDropEvent& DragDropEvent) {
	// Cleanup the pending drop action once the operation leaves this widget
	if (const TSharedPtr<FAbstractSubobjectTreeDragDropOp> DragRowOp = DragDropEvent.GetOperationAs<FAbstractSubobjectTreeDragDropOp>()) {
		const TSharedPtr<SWidget> NoWidget;
		DragRowOp->SetFeedbackMessage(NoWidget);
		DragRowOp->ResetDragDropTargetAction();
	}
}

FReply SAbstractSubobjectTreeRowWidget::HandleOnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) {
	const TSharedPtr<SAbstractSubobjectTreeEditor> PinnedEditor = OwnerEditor.Pin();
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && PinnedEditor.IsValid() && PinnedEditor->IsEditingAllowed()) {
		TArray<TSharedPtr<FAbstractSubobjectTreeNode>> SelectedNodes = PinnedEditor->GetSelectedNodes();
		if (SelectedNodes.Num() == 0) {
			SelectedNodes.Add(SubobjectData);
		}

		// Begin the drag and drop operation with no set action and selected nodes as payload
		const TSharedRef<FAbstractSubobjectTreeDragDropOp> Operation = PinnedEditor->CreateDragDropActionFromNodes(SelectedNodes);
		return FReply::Handled().BeginDragDrop(Operation);
    }
	return FReply::Unhandled();
}

TOptional<EItemDropZone> SAbstractSubobjectTreeRowWidget::HandleOnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, const TSharedPtr<FAbstractSubobjectTreeNode> TargetItem) {
	if (const TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation()) {
		// If we have a valid drag and drop operation set for this action, drop it onto this item
		if (Operation->IsOfType<FAbstractSubobjectTreeDragDropOp>()) {
			const TSharedPtr<FAbstractSubobjectTreeDragDropOp> DragRowOp = StaticCastSharedPtr<FAbstractSubobjectTreeDragDropOp>(Operation);
			if (DragRowOp->IsValidDragDropTargetAction()) {
				return EItemDropZone::OntoItem;
			}
		}
		// Assume that component overlay tree will handle all the external and asset drag operations
		else if (Operation->IsOfType<FExternalDragOperation>() || Operation->IsOfType<FAssetDragDropOp>()) {
			return EItemDropZone::OntoItem;
		}
	}
	return TOptional<EItemDropZone>();
}

FReply SAbstractSubobjectTreeRowWidget::HandleOnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FAbstractSubobjectTreeNode> TargetItem) {
	const TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	const TSharedPtr<SAbstractSubobjectTreeEditor> PinnedTreeEditor = OwnerEditor.Pin();
	if (!Operation.IsValid() || !PinnedTreeEditor.IsValid()) {
		return FReply::Handled();
	}

	// If this is a component drag and drop operation with a defined action, forward it to the tree overlay to perform the operation in question
	if (Operation->IsOfType<FAbstractSubobjectTreeDragDropOp>()) {
		const TSharedPtr<FAbstractSubobjectTreeDragDropOp> DragRowOp = StaticCastSharedPtr<FAbstractSubobjectTreeDragDropOp>(Operation);
		PinnedTreeEditor->HandleDragDropAction(DragRowOp.ToSharedRef(), SubobjectData);
	}
	// Let the component tree handle the external drag operations and asset dragging
	else if (Operation->IsOfType<FExternalDragOperation>() || Operation->IsOfType<FAssetDragDropOp>()) {
		if (const TSharedPtr<SAbstractSubobjectTreeView>& SubobjectTree = PinnedTreeEditor->GetSubobjectTree()) {
			SubobjectTree->OnDrop(FGeometry(), DragDropEvent);
		}
	}
	return FReply::Handled();
}
