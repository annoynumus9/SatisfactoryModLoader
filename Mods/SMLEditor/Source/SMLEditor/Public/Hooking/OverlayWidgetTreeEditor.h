#pragma once

#include "CoreMinimal.h"
#include "Hooking/AbstractSubobjectTree.h"
#include "GraphEditorDragDropAction.h"
#include "SComponentClassCombo.h"
#include "Components/ActorComponent.h"
#include "Widgets/Views/STreeView.h"

class FWidgetBlueprintMixinEditor;
class UWidget;
class UWidgetTree;

class SMLEDITOR_API FWidgetRootNode : public FAbstractSubobjectTreeNode {
public:
	explicit FWidgetRootNode(UBlueprintGeneratedClass* InWidgetMixinTargetClass);

	FORCEINLINE static FName StaticTypeName() { return TEXT("WidgetRootNode"); }
	
	virtual FName GetTypeName() const override { return StaticTypeName(); }
	virtual FText GetNameLabel(bool bIsEditingName) const override;
	virtual FSlateFontInfo GetNameFont(bool bIsEditingName) const override;
	virtual const UObject* GetImmutableObject() const override { return nullptr; }
protected:
	FText RootNodeName;
};

/** Node representing a widget in the widget tree of the target widget blueprint */
class SMLEDITOR_API FImmutableTemplateWidgetNode : public FAbstractSubobjectTreeNode {
public:
	explicit FImmutableTemplateWidgetNode(UWidget* InWidgetReference);

	FORCEINLINE static FName StaticTypeName() { return TEXT("TemplateWidget"); }
	
	virtual FName GetTypeName() const override { return StaticTypeName(); }
	virtual FText GetNameLabel(bool bIsEditingName) const override;
	virtual FSlateFontInfo GetNameFont(bool bIsEditingName) const override;
	virtual void GetFilterStrings(TArray<FString>& OutStrings) const override;
	virtual const UObject* GetImmutableObject() const override;
	UWidget* GetTemplateWidget() const;
protected:
	TWeakObjectPtr<UWidget> WidgetReference;
};

class SMLEDITOR_API SOverlayWidgetTreeEditor : public SAbstractSubobjectTreeEditor {
public:
	SLATE_BEGIN_ARGS(SOverlayWidgetTreeEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<FWidgetBlueprintMixinEditor>& InOwnerBlueprintEditor);

	// Begin SAbstractSubobjectTreeEditor interface
	virtual bool IsEditingAllowed() const override;
	virtual FReply TryHandleAssetDragNDropOperation(const FDragDropEvent& DragDropEvent) override;
	virtual void HandleDeleteAction(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& NodesToDelete) override;
	virtual void HandleRenameAction(const TSharedPtr<FAbstractSubobjectTreeNode>& SubobjectData, const FText& InNewsubobjectName) override;
	virtual void HandleDragDropAction(const TSharedRef<FAbstractSubobjectTreeDragDropOp>& DragDropAction, const TSharedPtr<FAbstractSubobjectTreeNode>& DropOntoNode) override;
	virtual TSharedRef<FAbstractSubobjectTreeDragDropOp> CreateDragDropActionFromNodes(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& SelectedNodes) const override;
	virtual void UpdateDragDropOntoNode(const TSharedRef<FAbstractSubobjectTreeDragDropOp>& DragDropAction, const TSharedPtr<FAbstractSubobjectTreeNode>& DropOntoNode, FText& OutFeedbackMessage) const override;
	virtual void RefreshSubobjectTreeFromDataSource() override;
	virtual void UpdateSelectionFromNodes(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& SelectedNodes) override;
	virtual TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FAbstractSubobjectTreeNode> InNodePtr, const TSharedRef<STableViewBase>& OwnerTable) override;
	// End SAbstractSubobjectTreeEditor interface

	/** Creates a tooltip for the widget class */
	static FText CreateTooltipForWidgetClass( const UClass* WidgetClass);
protected:
	/** Called to request a component to be created in this class. This is generic function called for both pasting and new component creation. This does not start a transaction. */
	void CreateComponentFromClass(UClass* InComponentClass, UObject* InComponentAsset = nullptr, UObject* InComponentTemplate = nullptr);

	TWeakPtr<FWidgetBlueprintMixinEditor> OwnerBlueprintEditor;
	TMap<TWeakObjectPtr<const UObject>, TSharedPtr<FAbstractSubobjectTreeNode>> ObjectToComponentDataCache;
	TSharedPtr<FAbstractSubobjectTreeNode> RootNode;
};

class SMLEDITOR_API SOverlayWidgetTreeRowWidget : public SAbstractSubobjectTreeRowWidget
{
	SLATE_BEGIN_ARGS(SOverlayWidgetTreeRowWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<SAbstractSubobjectTreeEditor> InOwnerEditor, const TSharedPtr<FAbstractSubobjectTreeNode> InNodePtr, TSharedPtr<STableViewBase> InOwnerTableView);

	// Begin SAbstractSubobjectTreeRowWidget interface
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual TSharedRef<SToolTip> CreateIconTooltipWidget() const override;
	virtual TSharedRef<SToolTip> CreateTooltipWidget() const override;
	// End SAbstractSubobjectTreeRowWidget interface
};

class SMLEDITOR_API FOverlayWidgetTreeRowDragDropOp : public FAbstractSubobjectTreeDragDropOp {
public:
	DRAG_DROP_OPERATOR_TYPE(FOverlayWidgetTreeRowDragDropOp, FAbstractSubobjectTreeDragDropOp);

	/** Available drop actions */
	enum EDropActionType { 
		DropAction_None,
		DropAction_AttachAbove,
		DropAction_AttachAsChild,
		DropAction_AttachBelow,
	};

	// Begin FAbstractSubobjectTreeDragDropOp interface
	virtual bool IsValidDragDropTargetAction() const override { return PendingDropAction != DropAction_None; }
	virtual void ResetDragDropTargetAction() override { PendingDropAction = DropAction_None; }
	// End FAbstractSubobjectTreeDragDropOp interface

	/** The type of drop action that's pending while dragging */
	EDropActionType PendingDropAction{DropAction_None};
	
	static TSharedRef<FOverlayWidgetTreeRowDragDropOp> Create(const TArray<TSharedPtr<FAbstractSubobjectTreeNode>>& InSourceNodes);
};
