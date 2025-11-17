#pragma once

#include "CoreMinimal.h"
#include "GraphEditorDragDropAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Views/TreeFilterHandler.h"
#include "Misc/TextFilter.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Views/STreeView.h"

/** Generic class to represent a node in the tree */
class SMLEDITOR_API FAbstractSubobjectTreeNode : public TSharedFromThis<FAbstractSubobjectTreeNode> {
public:
	virtual ~FAbstractSubobjectTreeNode() = default;

	/** Returns the name of this node type */
	virtual FName GetTypeName() const = 0;
	/** Returns true if this node can be cast to another node type */
	virtual bool IsOfType(const FName& InTypeName) const { return InTypeName == GetTypeName(); }

	/** Templated version of IsOfType that checks if the node is of the provided type */
	template<typename T>
	FORCEINLINE bool IsOfType() const { return IsOfType(T::StaticTypeName()); }
	/** Attempts to dynamically cast this overlay data to the provided type. Returns nullptr if the types are not compatible */
	template<typename T>
	FORCEINLINE TSharedPtr<const T> GetAs() const { return IsOfType<T>() ? StaticCastSharedPtr<const T>(SharedThis(this).ToSharedPtr()) : nullptr; }
	template<typename T>
	FORCEINLINE TSharedPtr<T> GetAs() { return IsOfType<T>() ? StaticCastSharedPtr<T>(SharedThis(this).ToSharedPtr()) : nullptr; }
	
	/** Returns the label of this node. bIsEditingName is true if the name is currently being edited and should not be prettified */
	virtual FText GetNameLabel(bool bIsEditingName) const = 0;
	/** Returns all strings that are considered to be contained by this node for filtering purposes */
	virtual void GetFilterStrings(TArray<FString>& OutStrings) const { OutStrings.Add(GetNameLabel(false).ToString()); }
	/** Returns the font that should be used for the name block */
	virtual FSlateFontInfo GetNameFont(bool bIsEditingName) const;
	/** Returns the class in which this subobject has been introduced (specifically introduced, this will return base class if this is a derived class node) */
	virtual UClass* GetOwnerClass() const { return nullptr; }
	/** Returns the name of the variable that contains a reference to this subobject in the owner class. May return NAME_None for native subobjects with no associated property */
	virtual FName GetSubobjectVariableName() const { return NAME_None; }
	/** Returns the immutable object for this node. The provided object will not be cached. */
	virtual const UObject* GetImmutableObject() const = 0;
	/** Returns true if this node can be edited or false if it is read only representation */
	virtual bool IsNodeEditable() const { return false; }

	/** Checks if the provided name is a valid name for this subobject. Only needs to be implemented if the node is editable */
	virtual bool CheckValidRename(const FText& InNewSubobjectName, FText& OutErrorMessage) const { return false; }
	/** Returns the mutable object for this node */
	virtual UObject* GetMutableObject() const { return nullptr; }

	/** Returns true if this subobject is a direct child of the parent node */
	bool IsDirectlyAttachedToParent(const TSharedPtr<FAbstractSubobjectTreeNode>& InParentNode) const;
	/** Returns true if this subobject is a child node attached to the provided parent node */
	bool IsAttachedToParent(const TSharedPtr<FAbstractSubobjectTreeNode>& InParentNode) const;
	/** Populate the list with the children nodes of this subobject */
	void GetChildrenNodes(TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& OutChildrenNodes) const;

	/** Clears list of children and the parent node from this node */
	void ClearHierarchyData();
	/** Adds new child node to this node. Assumes that child node does not have a parent set currently */
	void AddChildNode(const TSharedPtr<FAbstractSubobjectTreeNode>& InNewChildNode);
protected:
	// Transient hierarchy information, will be rebuilt by the tree on demand
	TWeakPtr<FAbstractSubobjectTreeNode> ParentNode;
	TArray<TSharedPtr<FAbstractSubobjectTreeNode>> ChildNodes;
};

class SMLEDITOR_API FAbstractSubobjectTreeDragDropOp : public FGraphSchemaActionDragDropAction {
public:
	DRAG_DROP_OPERATOR_TYPE(FAbstractSubobjectTreeDragDropOp, FGraphSchemaActionDragDropAction);

	virtual bool IsValidDragDropTargetAction() const = 0;
	virtual void ResetDragDropTargetAction() = 0;

	/** Nodes that we started the drag from */
	TArray<TSharedPtr<FAbstractSubobjectTreeNode>> SourceNodes;
};

class SMLEDITOR_API SAbstractSubobjectTreeView : public STreeView<TSharedPtr<FAbstractSubobjectTreeNode>> {
public:
	void Construct(const FArguments& InArgs, const TSharedPtr<class SAbstractSubobjectTreeEditor>& InOwnerTreeEditor);

	// Begin SWidget interface
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	// End SWidget interface
protected:
	/** Check if asset drag and drop operation contains any class assets (e.g. blueprints) */
	static bool ContainsAnyClassAssets(const TSharedPtr<class FAssetDragDropOp>& AssetDragNDropOp);
	
	TWeakPtr<SAbstractSubobjectTreeEditor> OwnerTreeEditor;
};

class SMLEDITOR_API SAbstractSubobjectTreeEditor : public SCompoundWidget {
public:
	SLATE_BEGIN_ARGS(SAbstractSubobjectTreeEditor) : _ItemHeight(20.0f) {}
		SLATE_ARGUMENT(float, ItemHeight)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Returns true if editing of the tree structure is currently allowed */
	virtual bool IsEditingAllowed() const { return true; }

	/** Returns the subobject tree for the overlay tree editor */
	TSharedPtr<SAbstractSubobjectTreeView> GetSubobjectTree() const { return SubobjectTreeView; }
	/** Returns all nodes currently selected in the subobject tree, sorted by their depth */
	TArray<TSharedPtr<FAbstractSubobjectTreeNode>> GetSelectedNodes() const;

	/** Handles asset being dropped into the subobject tree. Attempts to create an subobject from the asset class */
	virtual FReply TryHandleAssetDragNDropOperation(const FDragDropEvent& DragDropEvent) = 0;
	/** Called to allocate a editor-specific drag and drop operation for the selected nodes */
	virtual TSharedRef<FAbstractSubobjectTreeDragDropOp> CreateDragDropActionFromNodes(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& SelectedNodes) const = 0;
	/** Called to update the operation based on which action should be taken with drag drop */
	virtual void UpdateDragDropOntoNode(const TSharedRef<FAbstractSubobjectTreeDragDropOp>& DragDropAction, const TSharedPtr<FAbstractSubobjectTreeNode>& DropOntoNode, FText& OutFeedbackMessage) const = 0;
	/** Called to request the provided drag and drop action to be handled */
	virtual void HandleDragDropAction(const TSharedRef<FAbstractSubobjectTreeDragDropOp>& DragDropAction, const TSharedPtr<FAbstractSubobjectTreeNode>& DropOntoNode) = 0;
	/** Called to request the provided subobjects to be removed */
	virtual void HandleDeleteAction(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& NodesToDelete) = 0;
	/** Called to request the provided subobjects to be renamed */
	virtual void HandleRenameAction(const TSharedPtr<FAbstractSubobjectTreeNode>& SubobjectData, const FText& InNewsubobjectName) = 0;

	/** Requests the refresh of the subobject tree */
	void UpdateTree();
protected:
	/** Called when the filter text is changed */
	void OnSearchChanged(const FText& InFilterText);
	/** Gets the search text currently being used to filter the list */
	FText GetSearchText() const;
	
	// Utility functions for modification or deletion of existing nodes
	bool CanRenameSubobject() const;
	void OnRenameSubobject();
	bool CanDeleteNodes() const;
	void OnDeleteNodes();

	/** Refreshes the subobject tree from the data source. Attempts to preserve as many existing nodes as possible while rebuilding the node hierarchy from scratch */
	virtual void RefreshSubobjectTreeFromDataSource() = 0;
	/** Update the details panel to point to the selected editable nodes */
	virtual void UpdateSelectionFromNodes(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& SelectedNodes) = 0;
	/** Register default commands implementation for the editor (such as copying, pasting, deleting or searching) */
	virtual void RegisterEditorCommands();
	
	/** Creates a row widget for the tree overlay subobject */
	virtual TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FAbstractSubobjectTreeNode> InNodePtr, const TSharedRef<STableViewBase>& OwnerTable);
	/** Returns subobjects that are children of the provided subobject */
	void OnGetChildrenForTree(const TSharedPtr<FAbstractSubobjectTreeNode> InNodePtr, TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& OutChildren);
	/** Recursively expands the tree from the provided item */
	void SetItemExpansionRecursive(const TSharedPtr<FAbstractSubobjectTreeNode> InNodeToChange, bool bInExpansionState);
	/** Returns the strings that are relevant for the search of the node in the tree */
	void GetNodeFilterStrings(TSharedPtr<FAbstractSubobjectTreeNode> Item, TArray<FString>& OutStrings);
	
	/** Called when selection in the tree changes */
	void OnTreeSelectionChanged(TSharedPtr<FAbstractSubobjectTreeNode> InSelectedNodePtr, ESelectInfo::Type SelectInfo);
	/** Callback when a subobject item is scrolled into view */
	void OnItemScrolledIntoView(TSharedPtr<FAbstractSubobjectTreeNode> InItem, const TSharedPtr<ITableRow>& InWidget);

	TSharedPtr<SAbstractSubobjectTreeView> SubobjectTreeView;
	TSharedPtr<FUICommandList> CommandList;
	TSharedPtr<FAbstractSubobjectTreeNode> PendingRenameSubobjectData;
	TSharedPtr<TreeFilterHandler<TSharedPtr<FAbstractSubobjectTreeNode>>> FilterHandler;
	TSharedPtr<TTextFilter<TSharedPtr<FAbstractSubobjectTreeNode>>> SearchBoxTreeFilter;
	TSharedPtr<class SSearchBox> SearchBoxPtr;

	// Root node set, all nodes in the tree, and caches to reuse existing nodes to avoid completely regenerating the entire tree
	TArray<TSharedPtr<FAbstractSubobjectTreeNode>> RootNodes;
	TArray<TSharedPtr<FAbstractSubobjectTreeNode>> AllNodes;

	/** Filtered by the filter handler, not to be modified directly by the derived subclasses */
	TArray<TSharedPtr<FAbstractSubobjectTreeNode>> FilteredRootNodes;
};

class SAbstractSubobjectTreeRowWidget : public STableRow<TSharedPtr<FAbstractSubobjectTreeNode>> {
public:
	SLATE_BEGIN_ARGS(SAbstractSubobjectTreeRowWidget) : _ShowIconBrush(false), _TableRowStyle(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row")),
		_Padding(FMargin(0.0f)), _IconPadding(FMargin(0.0f, 0.0f, 2.0f, 0.0f)) {}
		SLATE_ARGUMENT(bool, ShowIconBrush)
		SLATE_ARGUMENT(const FTableRowStyle*, TableRowStyle)
		SLATE_ARGUMENT(FMargin, Padding)
		SLATE_ARGUMENT(FMargin, IconPadding)
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, TSharedPtr<SAbstractSubobjectTreeEditor> InOwnerEditor, const TSharedPtr<FAbstractSubobjectTreeNode> InNodePtr, TSharedPtr<STableViewBase> InOwnerTableView);

	/** Request the widget to start editing the subobject name box */
	void StartEditingSubobjectName();
protected:
	// Accessor and mutators for the name of the subobject */
	FText GetNameLabel() const;
	FSlateFontInfo GetNameFont() const;
	bool OnNameTextVerifyChanged(const FText& InNewText, FText& OutErrorMessage);
	void OnNameTextCommit(const FText& InNewName, ETextCommit::Type InTextCommit);

	void OnEnterTextEditingMode();
	void OnExitTextEditingMode();

	virtual const FSlateBrush* GetIconBrush() const;
	virtual TSharedRef<SToolTip> CreateTooltipWidget() const;
	virtual TSharedRef<SToolTip> CreateIconTooltipWidget() const;

	// Handlers for dragging and dropping the nodes onto the tree node
	void HandleOnDragEnter(const FDragDropEvent& DragDropEvent);
	void HandleOnDragLeave(const FDragDropEvent& DragDropEvent);
	FReply HandleOnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	TOptional<EItemDropZone> HandleOnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, const TSharedPtr<FAbstractSubobjectTreeNode> TargetItem);
	FReply HandleOnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FAbstractSubobjectTreeNode> TargetItem);

	TSharedPtr<SInlineEditableTextBlock> EditableNameTextBlock;
	TSharedPtr<SImage> IconImage;
	TWeakPtr<SAbstractSubobjectTreeEditor> OwnerEditor;
	TSharedPtr<FAbstractSubobjectTreeNode> SubobjectData;
	bool bIsBeingEdited{false};
	FText InitialText;
};
