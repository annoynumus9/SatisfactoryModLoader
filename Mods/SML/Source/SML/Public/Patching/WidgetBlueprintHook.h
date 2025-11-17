#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Extensions/UserWidgetExtension.h"
#include "Patching/BlueprintHookBlueprint.h"
#include "WidgetBlueprintHook.generated.h"

class UUserWidget;

/** Parent class for blueprint hooks that represent widget mixins */
UCLASS(Abstract, Blueprintable, BlueprintInternalUseOnly, Within = UserWidget)
class SML_API UUserWidgetMixin : public UBlueprintHook {
	GENERATED_BODY()
public:
	// Begin UObject interface
#if WITH_EDITOR
	virtual bool ImplementsGetWorld() const override { return true; }
#endif
	// End UObject interface

	/** Retrieves widget mixin instance of the provided type from the widget instance */
	UFUNCTION(BlueprintPure, Category = "Widget Mixin", meta = (DeterminesOutputType = "InMixinClass", CallableWithoutWorldContext))
	static UUserWidgetMixin* GetWidgetMixin( const UUserWidget* InWidget, TSubclassOf<UUserWidgetMixin> InMixinClass);

	/** Internal function used by generated hooking code */
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static UObject* GetHookObjectInstanceFromTargetMethodInstance(UObject* InObjectInstance, UClass* InHookObjectClass);

	/** Called once the owning User Widget has been Initialized. Will only be called once per widget instance */
	void DispatchInitialize();
	
	/** Called when the slate widget is constructed */
	void DispatchConstruct();
	
	/** Called when the slate widget is destructed */
	void DispatchDestruct();

	/** Called when the owning widget ticks */
	void DispatchTick(const FGeometry& MyGeometry, float InDeltaTime);
protected:
	/** Called once the owning User Widget has been Initialized */
	virtual void Initialize() {}
	
	/** Called when the slate widget is constructed */
	virtual void Construct() {}

	/** Called when the slate widget is destructed */
	virtual void Destruct() {}

	/** Called when the owning widget ticks */
	virtual void Tick(const FGeometry& MyGeometry, float InDeltaTime) {}

	/** Called once the owning User Widget has been Initialized */
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Initialize"))
	void ReceiveInitialize();
	
	/** Called when the slate widget is constructed */
	UFUNCTION(BlueprintImplementableEvent, Category = "Widget Mixin", meta = (DisplayName = "Construct"))
	void ReceiveConstruct();

	/** Called when the slate widget is destructed */
	UFUNCTION(BlueprintImplementableEvent, Category = "Widget Mixin", meta = (DisplayName = "Destruct"))
	void ReceiveDestruct();
	
	/** Called when the owning widget ticks */
	UFUNCTION(BlueprintImplementableEvent, Category = "Widget Mixin", meta = (DisplayName = "Tick"))
	void ReceiveTick(const FGeometry& MyGeometry, float InDeltaTime);
public:
	/** True to enable Tick to be dispatched for this mixin. Tick will not be called unless this is enabled */
	UPROPERTY(EditDefaultsOnly, Category = "Widget Mixin")
	bool bEnableMixinTick{false};
};

UCLASS()
class SML_API UWidgetMixinHostExtension : public UUserWidgetExtension {
	GENERATED_BODY()
public:
	// Begin UUserWidgetExtension interface
	virtual void Initialize() override;
	virtual void Construct() override;
	virtual void Destruct() override;
	virtual bool RequiresTick() const override { return bCachedReceiveTick; }
	virtual void Tick(const FGeometry& MyGeometry, float InDeltaTime) override;
	// End UUserWidgetExtension interface

	UUserWidgetMixin* FindMixinByClass(TSubclassOf<UUserWidgetMixin> MixinClass) const;
protected:
	/** Mixin classes installed on this host */
	UPROPERTY(VisibleAnywhere, Category = "Mixin Host")
	TArray<UHookBlueprintGeneratedClass*> MixinClasses;

	/** Constructed mixins for this actor instance */
	UPROPERTY()
	TArray<UUserWidgetMixin*> MixinInstances;

	/** True if any of the mixin instances want to receive tick. Set during Initialize */
	UPROPERTY()
	bool bCachedReceiveTick{false};

	friend class UBlueprintHookManager;
};

UCLASS(NotBlueprintable, BlueprintInternalUseOnly)
class SML_API UWidgetMixinSubtreeHostWidget : public UUserWidget {
	GENERATED_BODY()
public:
	// Begin UUserWidget interface
	virtual void InitializeNativeClassData() override;
	// End UUserWidget interface
	
	/** Widget mixin that owns this subtree host widget */
	FORCEINLINE UUserWidgetMixin* GetOwnerWidgetMixin() const { return OwnerWidgetMixin; }
protected:
	friend class UWidgetTreeExtension;

	UPROPERTY()
	const UWidgetTreeExtension* OwnerWidgetTreeExtension{};
	
	UPROPERTY()
	UUserWidgetMixin* OwnerWidgetMixin{};
};

UCLASS(Abstract)
class SML_API UWidgetMixinPanelSlotTemplate : public UObject {
	GENERATED_BODY()
public:
	virtual void ApplyToPanelSlot(UPanelSlot* InPanelSlot) {};
};

UCLASS()
class SML_API UWidgetMixinPanelSlotTemplate_Generic : public UWidgetMixinPanelSlotTemplate {
	GENERATED_BODY()
public:
	/** How much space this slot should occupy in the direction of the panel. */
	UPROPERTY(EditAnywhere, Category="Layout", meta = (DisplayAfter = "Padding"))
	FSlateChildSize Size{};

	/** The padding area between the slot and the content it contains. */
	UPROPERTY(EditAnywhere, Category="Layout")
	FMargin Padding{};

	/** The alignment of the object horizontally. */
	UPROPERTY(EditAnywhere, Category="Layout")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment{HAlign_Fill};

	/** The alignment of the object vertically. */
	UPROPERTY(EditAnywhere, Category="Layout")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment{HAlign_Fill};

	// Begin UWidgetMixinPanelSlotTemplate interface
	virtual void ApplyToPanelSlot(UPanelSlot* InPanelSlot) override;
	// End UWidgetMixinPanelSlotTemplate interface
};

UCLASS()
class SML_API UWidgetMixinPanelSlotTemplate_Canvas : public UWidgetMixinPanelSlotTemplate {
	GENERATED_BODY()
public:
	/** The anchoring information for the slot */
	UPROPERTY(EditAnywhere, Category = "Layout")
	FAnchorData LayoutData{};

	/** When AutoSize is true we use the widget's desired size */
	UPROPERTY(EditAnywhere, Category = "Layout")
	bool bAutoSize{false};

	/** The order priority this widget is rendered in.  Higher values are rendered last (and so they will appear to be on top). */
	UPROPERTY(EditAnywhere, Category = "Layout")
	int32 ZOrder{0};

	// Begin UWidgetMixinPanelSlotTemplate interface
	virtual void ApplyToPanelSlot(UPanelSlot* InPanelSlot) override;
	// End UWidgetMixinPanelSlotTemplate interface
};

UCLASS(Within = OverlayWidgetTree)
class SML_API UWidgetTreeExtension : public UObject {
	GENERATED_BODY()
public:
	UWidgetTreeExtension();

	/** Returns the widget subtree this extension contains */
	FORCEINLINE UWidgetTree* GetWidgetSubTree() const { return WidgetSubTree; }

	/** Returns the slot template for this widget subtree */
	FORCEINLINE UWidgetMixinPanelSlotTemplate* GetSlotTemplate() const { return SlotTemplate; }

	/** Returns the index at which this subtree should be inserted into the attachment parent, or INDEX_NONE if it should just be added as the last element */
	FORCEINLINE int32 GetChildInsertIndex() const { return ChildInsertIndex; }

	/** Resolves attachment parent panel in the host widget tree for this subtree */
	UPanelWidget* ResolveAttachmentParent( const UWidgetTree* HostWidgetTree) const;

	/** Executes the extension on the provided widget. Mixin instance can be nullptr */
	void ExecuteOnUserWidget(const UUserWidget* HostWidget, UUserWidgetMixin* MixinInstance) const;

	/** Internal function to duplicate the subtree into the host widget on initialization */
	void InitializeHostWidgetStatic(UWidgetMixinSubtreeHostWidget* HostWidget) const;
#if WITH_EDITOR
	/** Attaches the subtree to the widget in the original widget tree */
	void AttachSubtreeToWidget(const UPanelWidget* AttachmentParent, int32 AttachmentIndex);

	/** Copies attachment data from another subtree */
	void CopyAttachmentDataFromSubtree(const UWidgetTreeExtension* InOtherSubtree);

	/** Attaches the provided widget to the attachment parent located in this widget tree */
	void AttachWidgetToTree(UWidget* Widget, UPanelWidget* AttachmentParent, int32 AttachIndex) const;
#endif
protected:
	/** Widget subtree that will be injected into the widget tree being extended */
	UPROPERTY()
	UWidgetTree* WidgetSubTree;

	/** Name of the container widget that this tree will be inserted as a child of */
	UPROPERTY()
	FName ContainerWidgetName;

	/** The index at which we want to insert this extension */
	UPROPERTY()
	int32 ChildInsertIndex{INDEX_NONE};

	/** Template for the configuration of the panel slot where this widget should be inserted */
	UPROPERTY()
	UWidgetMixinPanelSlotTemplate* SlotTemplate{};
};

UCLASS()
class SML_API UOverlayWidgetTree : public UObject {
	GENERATED_BODY()
public:
	/** Applies the subtrees contained in this overlay to the host widget and updates the mixin instance properties to point to instantiated widgets. Mixin instance can be nullptr */
	void ExecuteOnUserWidget( const UUserWidget* HostWidget, UUserWidgetMixin* MixinInstance);
	
#if WITH_EDITOR
	/** Attaches a target widget as a child to the provided source panel widget. Will automatically forward to the correct widget tree extension or create one */
	void AttachAsChild(UWidget* TargetWidget, UPanelWidget* AttachmentParentWidget);
	
	/** Attaches a target widget below the provided source widget. Will automatically forward to the correct widget tree extension or create one */
	void AttachBelowWidget(UWidget* TargetWidget, const UWidget* SourceWidget);

	/** Attaches a target widget above the provided source widget. Will automatically forward to the correct widget tree extension or create one */
	void AttachAboveWidget(UWidget* TargetWidget, const UWidget* SourceWidget);
#endif
protected:
	/** Overlay extensions applied on top of the base widget tree */
	UPROPERTY()
	TArray<UWidgetTreeExtension*> WidgetTreeExtensions;
};

UCLASS()
class SML_API UWidgetMixinBlueprintGeneratedClass : public UHookBlueprintGeneratedClass {
	GENERATED_BODY()
public:
	/** Overlay widget tree for this widget blueprint hook */
	UPROPERTY()
	UOverlayWidgetTree* OverlayWidgetTree{};

	// Begin UBlueprintGeneratedClass interface
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
#if WITH_EDITOR
	virtual UClass* RegenerateClass(UClass* ClassToRegenerate, UObject* PreviousCDO) override;
#endif
	// End UBlueprintGeneratedClass interface
};

UCLASS(NotBlueprintType)
class SML_API UWidgetMixinBlueprint : public UHookBlueprint {
	GENERATED_BODY()
public:
#if WITH_EDITOR
	// Begin UBlueprint interface
	virtual UClass* GetBlueprintClass() const override { return UWidgetMixinBlueprintGeneratedClass::StaticClass(); }
	virtual UClass* RegenerateClass(UClass* ClassToRegenerate, UObject* PreviousCDO) override;
	// End UBlueprint interface
#endif

#if WITH_EDITORONLY_DATA
	/** Overlay widget tree for this widget blueprint hook */
	UPROPERTY()
	UOverlayWidgetTree* OverlayWidgetTree{};
#endif
};
