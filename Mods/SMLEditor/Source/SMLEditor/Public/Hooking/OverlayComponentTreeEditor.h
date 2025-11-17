#pragma once

#include "CoreMinimal.h"
#include "Hooking/AbstractSubobjectTree.h"
#include "GraphEditorDragDropAction.h"
#include "SComponentClassCombo.h"
#include "Components/ActorComponent.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class UBlueprintMixinComponentNode;
class FHookBlueprintEditor;

/** node representing a SCS node */
class SMLEDITOR_API FImmutableSCSNodeComponentData : public FAbstractSubobjectTreeNode {
public:
	FImmutableSCSNodeComponentData(UBlueprintGeneratedClass* InActualOwnerClass, const USCS_Node* InDataNode);

	FORCEINLINE static FName StaticTypeName() { return TEXT("SCSNodeComponentData"); }
	
	virtual FName GetTypeName() const override { return StaticTypeName(); }
	virtual FText GetNameLabel(bool bIsEditingName) const override;
	virtual UClass* GetOwnerClass() const override;
	virtual FName GetSubobjectVariableName() const override;
	virtual const UObject* GetImmutableObject() const override;
	const USCS_Node* GetSCSNode() const;
protected:
	TWeakObjectPtr<UBlueprintGeneratedClass> ActualOwnerClass;
	TWeakObjectPtr<const USCS_Node> DataNode;
};

/** node representing a native component added in C++ constructor */
class SMLEDITOR_API FImmutableNativeComponentData : public FAbstractSubobjectTreeNode {
public:
	FImmutableNativeComponentData(UClass* InOwnerClass, const UActorComponent* InActorComponentTemplate);

	FORCEINLINE static FName StaticTypeName() { return TEXT("NativeComponentData"); }
	
	virtual FName GetTypeName() const override { return StaticTypeName(); }
	virtual FText GetNameLabel(bool bIsEditingName) const override;
	virtual UClass* GetOwnerClass() const override;
	virtual FName GetSubobjectVariableName() const override;
	virtual const UObject* GetImmutableObject() const override;
protected:
	TWeakObjectPtr<UClass> OwnerClass;
	TWeakObjectPtr<const UActorComponent> ActorComponentTemplate;
};

/** node representing a mutable component tree node on the hook blueprint */
class SMLEDITOR_API FMutableMixinComponentNodeData : public FAbstractSubobjectTreeNode {
public:
	explicit FMutableMixinComponentNodeData(const UBlueprintMixinComponentNode* InDataNode);

	FORCEINLINE static FName StaticTypeName() { return TEXT("MixinComponentData"); }
	
	virtual FName GetTypeName() const override { return StaticTypeName(); }
	virtual bool IsNodeEditable() const override { return true; }
	virtual FText GetNameLabel(bool bIsEditingName) const override;
	virtual UClass* GetOwnerClass() const override;
	virtual FName GetSubobjectVariableName() const override;
	virtual const UObject* GetImmutableObject() const override;
	virtual bool CheckValidRename(const FText& InNewComponentName, FText& OutErrorMessage) const override;
	virtual UObject* GetMutableObject() const override;
	const UBlueprintMixinComponentNode* GetComponentNode() const;
protected:
	TWeakObjectPtr<const UBlueprintMixinComponentNode> DataNode;
};

class SMLEDITOR_API SOverlayComponentTreeEditor : public SAbstractSubobjectTreeEditor {
public:
	SLATE_BEGIN_ARGS(SOverlayComponentTreeEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<FHookBlueprintEditor>& InOwnerBlueprintEditor);

	// Begin SAbstractSubobjectTreeEditor interface
	virtual void RegisterEditorCommands() override;
	virtual bool IsEditingAllowed() const override;
	virtual FReply TryHandleAssetDragNDropOperation(const FDragDropEvent& DragDropEvent) override;
	virtual void HandleDeleteAction(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& NodesToDelete) override;
	virtual void HandleRenameAction(const TSharedPtr<FAbstractSubobjectTreeNode>& SubobjectData, const FText& InNewsubobjectName) override;
	virtual void HandleDragDropAction(const TSharedRef<FAbstractSubobjectTreeDragDropOp>& DragDropAction, const TSharedPtr<FAbstractSubobjectTreeNode>& DropOntoNode) override;
	virtual TSharedRef<FAbstractSubobjectTreeDragDropOp> CreateDragDropActionFromNodes(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& SelectedNodes) const override;
	virtual void UpdateDragDropOntoNode(const TSharedRef<FAbstractSubobjectTreeDragDropOp>& DragDropAction, const TSharedPtr<FAbstractSubobjectTreeNode>& DropOntoNode, FText& OutFeedbackMessage) const override;
	virtual void RefreshSubobjectTreeFromDataSource() override;
	virtual TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FAbstractSubobjectTreeNode> InNodePtr, const TSharedRef<STableViewBase>& OwnerTable) override;
	virtual void UpdateSelectionFromNodes(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& SelectedNodes) override;
	// End SAbstractSubobjectTreeEditor interface
	
	/** Attempts to find the node mapping to the provided preview component instance */
	TSharedPtr<FAbstractSubobjectTreeNode> FindComponentDataForComponentInstance(const UActorComponent* InPreviewComponent) const;
	/** Attempts to find the component instance for the provided node in the given actor */
	UActorComponent* FindComponentInstanceInActor(AActor* InActor, const TSharedPtr<FAbstractSubobjectTreeNode>& InComponentData) const;
	/** Sets up selection override for the provided actor component to be shown as selected when it is selected in the overlay component tree */
	void SetSelectionOverrideForComponent(UPrimitiveComponent* InPreviewComponent);
	/** Selects the tree node corresponding to the provided actor template component */
	void SelectNodeForComponent(const UActorComponent* InPreviewComponent, bool bIsMultiSelectActive);
protected:
	void OnAttachToDropAction(const TSharedPtr<FAbstractSubobjectTreeNode>& AttachToComponentData, const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& NodesToAttach);
	/** Called to request the provided components to be detached from their parent component and moved to the same level as it */
	void OnDetachFromDropAction(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& NodesToAttach);
	
	/** Called to create a new component of the defined class after selecting it from the combo box */
	FSubobjectDataHandle PerformComboAddClass(TSubclassOf<UActorComponent> ComponentClass, EComponentCreateAction::Type ComponentCreateAction, UObject* AssetOverride);
	/** Called to find references to the selected nodes */
	void OnFindReferences();

	/** Called to request a component to be created in this class. This is generic function called for both pasting and new component creation. This does not start a transaction. */
	void CreateComponentFromClass(UClass* InComponentClass, UObject* InComponentAsset = nullptr, UObject* InComponentTemplate = nullptr);
	/** Attaches node to the provided node. This is a generic function. This does not start a transaction */
	static bool AttachComponentNodeToComponentData(UBlueprintMixinComponentNode* InComponentNode, const TSharedPtr<FAbstractSubobjectTreeNode>& InAttachToComponentData);
	/** Returns true if overlay tree node associated with this instance is selected */
	bool IsComponentInstanceSelected(const UPrimitiveComponent* InPreviewComponent) const;

	TWeakPtr<FHookBlueprintEditor> OwnerBlueprintEditor;
	TMap<TWeakObjectPtr<const UObject>, TSharedPtr<FAbstractSubobjectTreeNode>> ObjectToComponentDataCache;
};

class SMLEDITOR_API FOverlayComponentTreeRowDragDropOp : public FAbstractSubobjectTreeDragDropOp {
public:
	DRAG_DROP_OPERATOR_TYPE(FOverlayComponentTreeRowDragDropOp, FAbstractSubobjectTreeDragDropOp);

	/** Available drop actions */
	enum EDropActionType {
		DropAction_None,
		DropAction_AttachTo,
		DropAction_DetachFrom
	};

	// Begin FGraphEditorDragDropAction interface
	virtual void HoverTargetChanged() override;
	virtual FReply DroppedOnPanel(const TSharedRef<SWidget>& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;
	virtual FReply DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	// End FGraphEditorDragDropAction interface

	// Begin FAbstractSubobjectTreeDragDropOp interface
	virtual bool IsValidDragDropTargetAction() const override { return PendingDropAction != DropAction_None; }
	virtual void ResetDragDropTargetAction() override { PendingDropAction = DropAction_None; }
	// End FAbstractSubobjectTreeDragDropOp interface

	/** The type of drop action that's pending while dragging */
	EDropActionType PendingDropAction{DropAction_None};

	static TSharedRef<FOverlayComponentTreeRowDragDropOp> Create(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& InSourceNodes);

	static EDropActionType CheckCanAttachSelectedNodeToNode(const TSharedPtr<FAbstractSubobjectTreeNode>& SelectedNode, const TSharedPtr<FAbstractSubobjectTreeNode>& DraggedOverNode, FText& OutDescriptionMessage);
protected:
	/** Creates a variable getter node for the provided node */
	static class UK2Node_VariableGet* SpawnVariableGetNodeForComponentData(UEdGraph* InTargetGraph, const TSharedPtr<FAbstractSubobjectTreeNode>& InComponentData, const FVector2D& InScreenPosition);
};

class SOverlayComponentTreeViewRowWidget : public SAbstractSubobjectTreeRowWidget {
public:
	SLATE_BEGIN_ARGS(SOverlayComponentTreeViewRowWidget) {}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, TSharedPtr<SOverlayComponentTreeEditor> InOwnerEditor, const TSharedPtr<FAbstractSubobjectTreeNode> InNodePtr, TSharedPtr<STableViewBase> InOwnerTableView);
protected:
	virtual TSharedRef<SToolTip> CreateTooltipWidget() const override;
	virtual const FSlateBrush* GetIconBrush() const override;

	// Accessors to read the information about the asset that this component is created from
	EVisibility GetAssetVisibility() const;
	FText GetAssetName() const;
	FText GetAssetPath() const;

	// Accessors for the mobility settings of this component. Only relevant for components
	FText GetMobilityToolTipText() const;
	FSlateBrush const* GetMobilityIconImage() const;
	FText GetComponentEditorOnlyTooltipText() const;
	FText GetComponentEditableTooltipText() const;

	// Creates the tooltip widget for this component node
	FText GetTooltipText() const;
};

class SMLEDITOR_API SOverlayComponentTooltipBlock : public SCompoundWidget {
public:
	SLATE_BEGIN_ARGS(SOverlayComponentTooltipBlock) : _Important(false) {}
		SLATE_ARGUMENT(FText, Key);
		SLATE_ATTRIBUTE(FText, Value);
		SLATE_NAMED_SLOT(FArguments, ValueIcon);
		SLATE_ARGUMENT(bool, Important);
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
};
