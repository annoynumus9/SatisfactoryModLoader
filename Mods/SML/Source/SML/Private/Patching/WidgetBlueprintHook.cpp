#include "Patching/WidgetBlueprintHook.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"

void UUserWidgetMixin::DispatchInitialize() {
	Initialize();
	ReceiveInitialize();
}

void UUserWidgetMixin::DispatchConstruct() {
	Construct();
	DispatchConstruct();
}

void UUserWidgetMixin::DispatchDestruct() {
	DispatchDestruct();
	Destruct();
}

void UUserWidgetMixin::DispatchTick(const FGeometry& MyGeometry, float InDeltaTime) {
	Tick(MyGeometry, InDeltaTime);
	ReceiveTick(MyGeometry, InDeltaTime);
}

void UWidgetMixinHostExtension::Initialize() {
	UUserWidget* UserWidget = GetOuterUUserWidget();
	const UWorld* OwnerWorld = GetWorld();

	// Do not create mixin instances for non-game world actors. We do not want to modify editor worlds
	// since they will be saved, and we only want active mixins to be saved in game and not within the editor assets.
	if (UserWidget && OwnerWorld && OwnerWorld->IsGameWorld()) {

		// Check which mixins we might already have to avoid creating them multiple times
		TSet<UHookBlueprintGeneratedClass*> ExistingMixinInstanceClasses;
		for (const UUserWidgetMixin* ActorMixin : MixinInstances) {
			if (ActorMixin) {
				ExistingMixinInstanceClasses.Add(Cast<UHookBlueprintGeneratedClass>(ActorMixin->GetClass()));
			}
		}
		
		// Create mixin instances or find existing ones. Skip nulls since editor can null out references to deleted assets
		for (UHookBlueprintGeneratedClass* MixinClass : MixinClasses) {
			if (MixinClass && !ExistingMixinInstanceClasses.Contains(MixinClass) && MixinClass->IsChildOf<UUserWidgetMixin>() && !MixinClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_Abstract)) {

				// Attempt to find an existing mixin object. If we failed, create a new one
				const FName MixinInstanceName = *FString::Printf(TEXT("Mixin_%s"), *MixinClass->GetName());
				UUserWidgetMixin* WidgetMixinObject = Cast<UUserWidgetMixin>(StaticFindObjectFast(MixinClass, UserWidget, MixinInstanceName, true));
				if (WidgetMixinObject == nullptr) {
					WidgetMixinObject = NewObject<UUserWidgetMixin>(UserWidget, MixinClass, MixinInstanceName);
					check(WidgetMixinObject);
				}

				// Add mixin object to the list
				MixinInstances.Add(WidgetMixinObject);
			}
		}
	}

	// Evaluate overlay component tree for each mixin
	for (UUserWidgetMixin* WidgetMixin : MixinInstances) {
		if (WidgetMixin) {
			const UWidgetMixinBlueprintGeneratedClass* MixinGeneratedClass = Cast<UWidgetMixinBlueprintGeneratedClass>(WidgetMixin->GetClass());
			if (MixinGeneratedClass && MixinGeneratedClass->OverlayWidgetTree) {
				MixinGeneratedClass->OverlayWidgetTree->ExecuteOnUserWidget(UserWidget, WidgetMixin);
			}
		}
	}

	// Dispatch Initialize to all the mixins now that we have created widgets for all of them
	bCachedReceiveTick = false;
	for (UUserWidgetMixin* WidgetMixin : MixinInstances) {
		if (WidgetMixin) {
			WidgetMixin->DispatchInitialize();
			bCachedReceiveTick |= WidgetMixin->bEnableMixinTick;
		}
	}
}

void UWidgetMixinHostExtension::Construct() {
	for (UUserWidgetMixin* WidgetMixin : MixinInstances) {
		if (WidgetMixin) {
			WidgetMixin->DispatchConstruct();
		}
	}
}

void UWidgetMixinHostExtension::Destruct() {
	for (UUserWidgetMixin* WidgetMixin : MixinInstances) {
		if (WidgetMixin) {
			WidgetMixin->DispatchDestruct();
		}
	}
}

void UWidgetMixinHostExtension::Tick(const FGeometry& MyGeometry, float InDeltaTime) {
	for (UUserWidgetMixin* WidgetMixin : MixinInstances) {
		if (WidgetMixin) {
			WidgetMixin->DispatchTick(MyGeometry, InDeltaTime);
		}
	}
}

UUserWidgetMixin* UWidgetMixinHostExtension::FindMixinByClass(TSubclassOf<UUserWidgetMixin> MixinClass) const {
	for (UUserWidgetMixin* WidgetMixin : MixinInstances) {
		if (WidgetMixin && WidgetMixin->GetClass() == MixinClass) {
			return WidgetMixin;
		}
	}
	return nullptr;
}

UUserWidgetMixin* UUserWidgetMixin::GetWidgetMixin(const UUserWidget* InWidget, TSubclassOf<UUserWidgetMixin> InMixinClass) {
	if (InWidget) {
		if (const UWidgetMixinHostExtension* HostExtension = InWidget->GetExtension<UWidgetMixinHostExtension>()) {
			return HostExtension->FindMixinByClass(InMixinClass);
		}
	}
	return nullptr;
}

UObject* UUserWidgetMixin::GetHookObjectInstanceFromTargetMethodInstance(UObject* InObjectInstance, UClass* InHookObjectClass) {
	if (const UUserWidget* UserWidgetInstance = Cast<UUserWidget>( InObjectInstance )) {
		return GetWidgetMixin(UserWidgetInstance, InHookObjectClass);
	}
	return nullptr;
}

void UWidgetMixinPanelSlotTemplate_Generic::ApplyToPanelSlot( UPanelSlot* InPanelSlot )
{
	Super::ApplyToPanelSlot( InPanelSlot );
}

void UWidgetMixinPanelSlotTemplate_Canvas::ApplyToPanelSlot( UPanelSlot* InPanelSlot )
{
	Super::ApplyToPanelSlot( InPanelSlot );
}

UWidgetTreeExtension::UWidgetTreeExtension() {
	WidgetSubTree = CreateDefaultSubobject<UWidgetTree>(TEXT("WidgetSubTree"));
}

UPanelWidget* UWidgetTreeExtension::ResolveAttachmentParent(const UWidgetTree* HostWidgetTree) const {
	if (HostWidgetTree && ContainerWidgetName != NAME_None) {
		return Cast<UPanelWidget>(HostWidgetTree->FindWidget(ContainerWidgetName));
	}
	return nullptr;
}

void UWidgetTreeExtension::ExecuteOnUserWidget(const UUserWidget* HostWidget, UUserWidgetMixin* MixinInstance) const {
	// Make sure we have a valid host widget with a valid tree
	if (HostWidget == nullptr || HostWidget->WidgetTree == nullptr) {
		return;
	}

	// Make sure we have a valid attachment parent
	UPanelWidget* AttachmentParent = ResolveAttachmentParent(HostWidget->WidgetTree);
	if (AttachmentParent == nullptr || !AttachmentParent->CanAddMoreChildren()) {
		return;
	}

	// Create a subtree host widget that we will insert into the host widget tree
	const UClass* OwnerClass = CastChecked<UWidgetMixinBlueprintGeneratedClass>(GetOuter()->GetOuter());
	const FName HostWidgetName = *FString::Printf(TEXT("MixinSubtreeHost_%s_%s"), *GetNameSafe(OwnerClass), *GetName());

	UWidgetMixinSubtreeHostWidget* SubtreeHostWidget = NewObject<UWidgetMixinSubtreeHostWidget>(HostWidget->WidgetTree, HostWidgetName, RF_Transient);
	SubtreeHostWidget->OwnerWidgetTreeExtension = this;
	SubtreeHostWidget->OwnerWidgetMixin = MixinInstance;

	// Initialize the host widget now that it has a valid mixin and tree extension set up on it
	SubtreeHostWidget->Initialize();
	
	// Insert or add the subtree host widget into the parent panel
	if (ChildInsertIndex == INDEX_NONE) {
		AttachmentParent->AddChild(SubtreeHostWidget);
	} else {
		AttachmentParent->InsertChildAt(ChildInsertIndex, SubtreeHostWidget);
	}
}

void UWidgetTreeExtension::InitializeHostWidgetStatic(UWidgetMixinSubtreeHostWidget* HostWidget) const {
	// Set up the class that resulted in generation of that widget
	UClass* OwnerClass = CastChecked<UWidgetMixinBlueprintGeneratedClass>(GetOuter()->GetOuter());
#if UE_HAS_WIDGET_GENERATED_BY_CLASS
	HostWidget->WidgetGeneratedByClass = MakeWeakObjectPtr(OwnerClass);
#endif

	// Duplicate our widget subtree and initialize the widget from it. We do not support named slots merging so provide an empty map
	const TMap<FName, UWidget*> NamedSlotContentToMerge;
	HostWidget->DuplicateAndInitializeFromWidgetTree(WidgetSubTree, NamedSlotContentToMerge);

	// If we have an owner mixin, set the widget property on it for all variable widgets
	if (HostWidget->WidgetTree && HostWidget->OwnerWidgetMixin) {
		HostWidget->WidgetTree->ForEachWidget([&](UWidget* Widget) {
			// Check that the widget is valid and represents a variable
			if (Widget != nullptr && Widget->bIsVariable) {
				const FObjectProperty* WidgetProperty = FindFProperty<FObjectProperty>(HostWidget->OwnerWidgetMixin->GetClass(), Widget->GetFName());

				// Make sure that the property exists and the type is compatible, and then set its value
				if (WidgetProperty && WidgetProperty->PropertyClass && Widget->GetClass()->IsChildOf(WidgetProperty->PropertyClass)) {
					WidgetProperty->SetObjectPropertyValue_InContainer(HostWidget->OwnerWidgetMixin, Widget);
				}
			}
		});
	}

	// Copy WidgetGeneratedBy from our owner class
#if WITH_EDITOR
	HostWidget->WidgetGeneratedBy = OwnerClass->ClassGeneratedBy;
#endif
}

void UWidgetMixinSubtreeHostWidget::InitializeNativeClassData() {
	// Forward back to the widget subtree
	if (OwnerWidgetTreeExtension) {
		OwnerWidgetTreeExtension->InitializeHostWidgetStatic(this);
	}
}

#if WITH_EDITOR

void UWidgetTreeExtension::AttachSubtreeToWidget(const UPanelWidget* AttachmentParent, const int32 AttachmentIndex) {
	if (AttachmentParent && AttachmentParent->CanAddMoreChildren()) {
		Modify();
		ContainerWidgetName = AttachmentParent->GetFName();
		ChildInsertIndex = AttachmentIndex;
	}
}

void UWidgetTreeExtension::CopyAttachmentDataFromSubtree(const UWidgetTreeExtension* InOtherSubtree) {
	if (InOtherSubtree) {
		Modify();
		ContainerWidgetName = InOtherSubtree->ContainerWidgetName;
		ChildInsertIndex = InOtherSubtree->ChildInsertIndex;
	}
}

void UWidgetTreeExtension::AttachWidgetToTree(UWidget* Widget, UPanelWidget* AttachmentParent, const int32 AttachIndex) const {
	// Check if widget is valid
	if (Widget == nullptr) {
		return;
	}
	// Modify the widget since we will alter its current slot
	Widget->Modify();
	
	// Remove this widget from its current parent panel
	UWidgetTree* CurrentOwnerWidgetTree = Widget->GetTypedOuter<UWidgetTree>();
	if (UPanelWidget* CurrentParentWidget = Widget->GetParent()) {
		CurrentParentWidget->Modify();
		CurrentParentWidget->RemoveChild(Widget);
	}
	// If this is a root widget of another widget tree, clear it
	else if (CurrentOwnerWidgetTree && CurrentOwnerWidgetTree->RootWidget == Widget) {
		CurrentOwnerWidgetTree->Modify();
		CurrentOwnerWidgetTree->RootWidget = nullptr;
	}
	
	// Move this widget under our widget tree
	if (CurrentOwnerWidgetTree != WidgetSubTree) {
		Widget->Rename(nullptr, WidgetSubTree, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	}
	
	// If attachment parent is null, set up current widget as a new root widget for this widget subtree
	if (AttachmentParent == nullptr) {
		WidgetSubTree->Modify();
		WidgetSubTree->RootWidget = Widget;
		return;
	}

	// Attachment parent must be owned by this widget subtree and be able to support more children
	const UWidgetTree* AttachmentParentWidgetTree = AttachmentParent->GetTypedOuter<UWidgetTree>();
	if (AttachmentParent->CanAddMoreChildren() && AttachmentParentWidgetTree == WidgetSubTree) {
		AttachmentParent->Modify();
		if (AttachIndex == INDEX_NONE) {
			AttachmentParent->AddChild(Widget);
		} else {
			AttachmentParent->InsertChildAt(AttachIndex, Widget);
		}
	}
}

#endif

void UOverlayWidgetTree::ExecuteOnUserWidget(const UUserWidget* HostWidget, UUserWidgetMixin* MixinInstance) {
	// Execute widget tree extensions in their natural order. Order here matters in case there are multiple extensions being attached to the same parent
	for (const UWidgetTreeExtension* WidgetTreeExtension : WidgetTreeExtensions) {
		if (WidgetTreeExtension) {
			WidgetTreeExtension->ExecuteOnUserWidget(HostWidget, MixinInstance);
		}
	}
}

#if WITH_EDITOR

void UOverlayWidgetTree::AttachAsChild(UWidget* TargetWidget, UPanelWidget* AttachmentParentWidget) {
	if (TargetWidget == nullptr || AttachmentParentWidget == nullptr) {
		return;
	}

	// Attachment parent must have a valid widget tree, otherwise it is an invalid attachment point
	const UWidgetTree* AttachmentParentWidgetTree = AttachmentParentWidget->GetTypedOuter<UWidgetTree>();
	if (AttachmentParentWidgetTree == nullptr) {
		return;
	}

	// If the attachment parent widget is owned by a widget tree extension, just forward the call to the extension
	if (const UWidgetTreeExtension* OwnerExtension = Cast<UWidgetTreeExtension>(AttachmentParentWidgetTree->GetOuter())) {
		OwnerExtension->AttachWidgetToTree(TargetWidget, AttachmentParentWidget, INDEX_NONE);
		return;
	}

	// Attachment parent widget is owned by the blueprint generated class otherwise. Create a new widget tree extension
	UWidgetTreeExtension* NewWidgetTreeExtension = NewObject<UWidgetTreeExtension>(this, NAME_None, RF_Transactional);
	Modify();
	WidgetTreeExtensions.Add(NewWidgetTreeExtension);

	// Attach tree extension to the attachment parent widget (as a child), and then attach the target widget to it as a root widget
	NewWidgetTreeExtension->AttachSubtreeToWidget(AttachmentParentWidget, INDEX_NONE);
	NewWidgetTreeExtension->AttachWidgetToTree(TargetWidget, nullptr, INDEX_NONE);
}

void UOverlayWidgetTree::AttachBelowWidget(UWidget* TargetWidget, const UWidget* SourceWidget) {
	if (TargetWidget == nullptr || SourceWidget == nullptr) {
		return;
	}

	// Source widget must have a valid widget tree, otherwise it is an invalid attachment point
	const UWidgetTree* SourceWidgetWidgetTree = SourceWidget->GetTypedOuter<UWidgetTree>();
	if (SourceWidgetWidgetTree == nullptr) {
		return;
	}

	// If source widget is owned by the extension, and is a root widget of the extension
	if (const UWidgetTreeExtension* OwnerExtension = Cast<UWidgetTreeExtension>(SourceWidgetWidgetTree->GetOuter())) {
		// Bail out if this extension somehow does not belong to this overlay widget tree
		if (OwnerExtension->GetOuter() != this) {
			return;
		}

		// Check if we are attaching adjacent to the root widget. This requires complex handling
		if (OwnerExtension->GetWidgetSubTree()->RootWidget == SourceWidget) {
			const int32 OwnerExtensionIndex = WidgetTreeExtensions.IndexOfByKey(OwnerExtension);
		
			// Create a new widget tree extension. Add it into the list in the correct order
			UWidgetTreeExtension* NewWidgetTreeExtension = NewObject<UWidgetTreeExtension>(this, NAME_None, RF_Transactional);

			// Since we are attaching relative to other extension, check if it is naturally ordered or inserted
			Modify();
			if (OwnerExtension->GetChildInsertIndex() == INDEX_NONE) {
				// If source extension is naturally ordered, attach ourselves right after it
				if (OwnerExtensionIndex != INDEX_NONE && WidgetTreeExtensions.IsValidIndex(OwnerExtensionIndex + 1)) {
					WidgetTreeExtensions.Insert(NewWidgetTreeExtension, OwnerExtensionIndex + 1);
				} else {
					WidgetTreeExtensions.Add(NewWidgetTreeExtension);
				}
			} else {
				// If source extension is inserted, we will copy the position from it.
				// That means that the extension that is executed later will be inserted above the extension that is executed earlier.
				// So to insert this widget below the source widget, we need to insert ourselves in front of the original widget.
				if (WidgetTreeExtensions.IsValidIndex(OwnerExtensionIndex)) {
					WidgetTreeExtensions.Insert(NewWidgetTreeExtension, OwnerExtensionIndex);
				} else {
					WidgetTreeExtensions.Add(NewWidgetTreeExtension);
				}
			}

			// Copy the attachment data from the original extension and attach the target widget as the root of the subtree
			NewWidgetTreeExtension->CopyAttachmentDataFromSubtree(OwnerExtension);
			NewWidgetTreeExtension->AttachWidgetToTree(TargetWidget, nullptr, INDEX_NONE);
		} else {
			// If we are not attaching alongside the root widget, determine the parent widget
			UPanelWidget* AttachmentParent = SourceWidget->GetParent();

			// We must always have a valid attachment parent at this point
			if (AttachmentParent == nullptr) {
				return;
			}
			const int32 SourceWidgetIndex = AttachmentParent->GetChildIndex(SourceWidget);

			// Attach the target widget below the source widget
			OwnerExtension->AttachWidgetToTree(TargetWidget, AttachmentParent, SourceWidgetIndex + 1);
		}
	} else {
		// Target widget is owned by the target widget tree otherwise. So create a new widget tree extension
		UWidgetTreeExtension* NewWidgetTreeExtension = NewObject<UWidgetTreeExtension>(this, NAME_None, RF_Transactional);

		// 
		Modify();
		
	}
}

void UOverlayWidgetTree::AttachAboveWidget( UWidget* TargetWidget, const UWidget* SourceWidget )
{
}

#endif

void UWidgetMixinBlueprintGeneratedClass::GetPreloadDependencies(TArray<UObject*>& OutDeps) {
	Super::GetPreloadDependencies(OutDeps);
}

UClass* UWidgetMixinBlueprintGeneratedClass::RegenerateClass(UClass* ClassToRegenerate, UObject* PreviousCDO) {
	return Super::RegenerateClass(ClassToRegenerate, PreviousCDO);
}

UClass* UWidgetMixinBlueprint::RegenerateClass(UClass* ClassToRegenerate, UObject* PreviousCDO ) {
	return Super::RegenerateClass(ClassToRegenerate, PreviousCDO);
}
