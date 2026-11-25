#include "Customization/CharacterCustomizationComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogCustomization, Log, All);

UCharacterCustomizationComponent::UCharacterCustomizationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCharacterCustomizationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TPair<FName, TSharedPtr<FStreamableHandle>>& Pair : VisualHandles)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->ReleaseHandle();
		}
	}
	VisualHandles.Reset();

	Super::EndPlay(EndPlayReason);
}

void UCharacterCustomizationComponent::ConfigureSlots(const TMap<FName, FCustomizationSlotDefinition>& InSlots)
{
	SlotDefinitions = InSlots;

	// Make sure the slot of every inventory item has a definition, so a new slot in the data table
	// works out of the box (the target component name defaults to the slot id).
	for (const FClothingItemData& Item : Inventory)
	{
		EnsureSlotDefinition(Item.SlotId);
	}
}

void UCharacterCustomizationComponent::EnsureSlotDefinition(FName SlotId)
{
	if (SlotId.IsNone() || SlotDefinitions.Contains(SlotId))
	{
		return;
	}

	FCustomizationSlotDefinition Definition;
	Definition.SlotId = SlotId;
	Definition.DisplayName = FText::FromName(SlotId);
	Definition.TargetComponentName = SlotId;
	Definition.SortOrder = SlotDefinitions.Num();
	SlotDefinitions.Add(SlotId, Definition);

	UE_LOG(LogCustomization, Log, TEXT("Auto-registered equipment slot '%s' (target component '%s')."), *SlotId.ToString(), *SlotId.ToString());
}

void UCharacterCustomizationComponent::SetInventory(const TArray<FClothingItemData>& InItems)
{
	Inventory = InItems;

	for (const FClothingItemData& Item : Inventory)
	{
		EnsureSlotDefinition(Item.SlotId);
	}

	OnInventoryChanged.Broadcast();
}

void UCharacterCustomizationComponent::AddInventoryItem(const FClothingItemData& Item)
{
	if (!Item.IsValidForEquipment())
	{
		return;
	}

	Inventory.Add(Item);
	EnsureSlotDefinition(Item.SlotId);
	OnInventoryChanged.Broadcast();
}

bool UCharacterCustomizationComponent::EquipItemById(FName ItemId)
{
	const FClothingItemData* Item = Inventory.FindByPredicate([ItemId](const FClothingItemData& Candidate)
	{
		return Candidate.ItemId == ItemId;
	});

	if (!Item)
	{
		UE_LOG(LogCustomization, Warning, TEXT("EquipItemById: item '%s' is not in the inventory."), *ItemId.ToString());
		return false;
	}

	return EquipItem(*Item);
}

bool UCharacterCustomizationComponent::EquipItem(const FClothingItemData& Item)
{
	if (!Item.IsValidForEquipment())
	{
		UE_LOG(LogCustomization, Warning, TEXT("EquipItem: item has no id or no slot."));
		return false;
	}

	if (const FClothingItemData* Current = EquippedItems.Find(Item.SlotId))
	{
		if (Current->ItemId == Item.ItemId)
		{
			// Already equipped, nothing to do.
			return true;
		}

		// Replace the previous item of the slot.
		UnequipSlotInternal(Item.SlotId);
	}

	EnsureSlotDefinition(Item.SlotId);
	EquippedItems.Add(Item.SlotId, Item);

	UE_LOG(LogCustomization, Log, TEXT("Equipped '%s' into slot '%s'."), *Item.ItemId.ToString(), *Item.SlotId.ToString());

	RequestItemVisual(Item);
	OnEquipmentChanged.Broadcast(Item.SlotId, Item.ItemId);
	return true;
}

bool UCharacterCustomizationComponent::UnequipSlot(FName SlotId)
{
	return UnequipSlotInternal(SlotId);
}

bool UCharacterCustomizationComponent::UnequipSlotInternal(FName SlotId)
{
	const FClothingItemData* Item = EquippedItems.Find(SlotId);
	if (!Item)
	{
		return false;
	}

	const FName ItemId = Item->ItemId;

	ReleaseItemVisualHandle(ItemId);
	ReadyItems.Remove(ItemId);

	if (UStaticMeshComponent* Target = FindTargetComponent(SlotId))
	{
		Target->SetStaticMesh(nullptr);
	}

	EquippedItems.Remove(SlotId);

	UE_LOG(LogCustomization, Log, TEXT("Unequipped '%s' from slot '%s'."), *ItemId.ToString(), *SlotId.ToString());

	OnEquipmentChanged.Broadcast(SlotId, NAME_None);
	return true;
}

bool UCharacterCustomizationComponent::GetEquippedItem(FName SlotId, FClothingItemData& OutItem) const
{
	if (const FClothingItemData* Item = EquippedItems.Find(SlotId))
	{
		OutItem = *Item;
		return true;
	}

	OutItem = FClothingItemData();
	return false;
}

TArray<FName> UCharacterCustomizationComponent::GetEquippedSlotIds() const
{
	TArray<FName> Result;
	EquippedItems.GetKeys(Result);
	return Result;
}

bool UCharacterCustomizationComponent::IsEquipped(FName ItemId) const
{
	return FindEquippedItemById(ItemId) != nullptr;
}

bool UCharacterCustomizationComponent::IsItemVisualReady(FName ItemId) const
{
	return ReadyItems.Contains(ItemId);
}

TArray<FCustomizationSlotDefinition> UCharacterCustomizationComponent::GetSlotDefinitions() const
{
	TArray<FCustomizationSlotDefinition> Result;
	SlotDefinitions.GenerateValueArray(Result);
	Result.Sort([](const FCustomizationSlotDefinition& A, const FCustomizationSlotDefinition& B)
	{
		if (A.SortOrder != B.SortOrder)
		{
			return A.SortOrder < B.SortOrder;
		}
		return A.SlotId.LexicalLess(B.SlotId);
	});
	return Result;
}

UStaticMeshComponent* UCharacterCustomizationComponent::FindTargetComponent(FName SlotId)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	FName DesiredName = SlotId;
	if (const FCustomizationSlotDefinition* Definition = SlotDefinitions.Find(SlotId))
	{
		if (!Definition->TargetComponentName.IsNone())
		{
			DesiredName = Definition->TargetComponentName;
		}
	}

	for (UActorComponent* Component : Owner->GetComponents())
	{
		if (Component && Component->GetFName() == DesiredName)
		{
			if (UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component))
			{
				return MeshComponent;
			}
		}
	}

	// Fallback: case insensitive name match.
	for (UActorComponent* Component : Owner->GetComponents())
	{
		if (Component && Component->GetName().Equals(DesiredName.ToString(), ESearchCase::IgnoreCase))
		{
			if (UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component))
			{
				return MeshComponent;
			}
		}
	}

	if (!WarnedMissingComponents.Contains(DesiredName))
	{
		WarnedMissingComponents.Add(DesiredName);

		TArray<FString> Available;
		for (UActorComponent* Component : Owner->GetComponents())
		{
			if (UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component))
			{
				Available.Add(MeshComponent->GetName());
			}
		}

		UE_LOG(LogCustomization, Warning,
			TEXT("Slot '%s' targets static mesh component '%s', which was not found on '%s'. Available static mesh components: %s"),
			*SlotId.ToString(), *DesiredName.ToString(), *Owner->GetName(), *FString::Join(Available, TEXT(", ")));
	}

	return nullptr;
}

const FClothingItemData* UCharacterCustomizationComponent::FindEquippedItemById(FName ItemId) const
{
	for (const TPair<FName, FClothingItemData>& Pair : EquippedItems)
	{
		if (Pair.Value.ItemId == ItemId)
		{
			return &Pair.Value;
		}
	}
	return nullptr;
}

void UCharacterCustomizationComponent::RequestItemVisual(const FClothingItemData& Item)
{
	const FSoftObjectPath MeshPath = Item.Mesh.ToSoftObjectPath();
	if (!MeshPath.IsValid())
	{
		UE_LOG(LogCustomization, Log, TEXT("Item '%s' has no mesh, only the UI data will be used."), *Item.ItemId.ToString());
		return;
	}

	// Already in memory (e.g. game cooked a reference): no need to stream.
	if (UObject* Resolved = MeshPath.ResolveObject())
	{
		if (UStaticMesh* LoadedMesh = Cast<UStaticMesh>(Resolved))
		{
			ApplyItemVisual(Item, LoadedMesh);
			OnItemVisualReady.Broadcast(Item.ItemId);
			return;
		}
	}

	UE_LOG(LogCustomization, Log, TEXT("Requesting async load of '%s' for item '%s'."), *MeshPath.ToString(), *Item.ItemId.ToString());

	ReleaseItemVisualHandle(Item.ItemId);

	FStreamableDelegate OnLoaded = FStreamableDelegate::CreateUObject(this, &ThisClass::HandleItemVisualLoaded, Item.ItemId);
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MeshPath, OnLoaded);
	if (Handle.IsValid())
	{
		VisualHandles.Add(Item.ItemId, Handle);
	}
	else
	{
		UE_LOG(LogCustomization, Warning, TEXT("Failed to start async load of '%s'."), *MeshPath.ToString());
	}
}

void UCharacterCustomizationComponent::HandleItemVisualLoaded(FName ItemId)
{
	UStaticMesh* LoadedMesh = nullptr;
	if (const TSharedPtr<FStreamableHandle>* Handle = VisualHandles.Find(ItemId))
	{
		if (Handle->IsValid())
		{
			LoadedMesh = Cast<UStaticMesh>((*Handle)->GetLoadedAsset());
		}
	}

	if (!LoadedMesh)
	{
		UE_LOG(LogCustomization, Warning, TEXT("Async load for item '%s' finished without a valid static mesh."), *ItemId.ToString());
		return;
	}

	// The player might have unequipped the item while it was loading.
	const FClothingItemData* Item = FindEquippedItemById(ItemId);
	if (!Item)
	{
		ReleaseItemVisualHandle(ItemId);
		return;
	}

	ApplyItemVisual(*Item, LoadedMesh);
	ReadyItems.Add(ItemId);
	OnItemVisualReady.Broadcast(ItemId);
}

void UCharacterCustomizationComponent::ApplyItemVisual(const FClothingItemData& Item, UStaticMesh* Mesh)
{
	UStaticMeshComponent* Target = FindTargetComponent(Item.SlotId);
	if (!Target)
	{
		return;
	}

	// Remember the design time transform once, so offsets never accumulate.
	const FName ComponentKey = Target->GetFName();
	FTransform* Baseline = BaselineTransforms.Find(ComponentKey);
	if (!Baseline)
	{
		Baseline = &BaselineTransforms.Add(ComponentKey, Target->GetRelativeTransform());
	}

	FTransform Offset = Item.MeshOffset;
	if (const FCustomizationSlotDefinition* Definition = SlotDefinitions.Find(Item.SlotId))
	{
		Offset = Definition->MeshOffset * Item.MeshOffset;
	}

	Target->SetStaticMesh(Mesh);
	Target->SetRelativeTransform(*Baseline * Offset);
	Target->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Target->SetGenerateOverlapEvents(false);
	Target->SetVisibility(true, true);

	UE_LOG(LogCustomization, Log, TEXT("Applied mesh '%s' to component '%s' (slot '%s')."),
		*Mesh->GetName(), *Target->GetName(), *Item.SlotId.ToString());
}

void UCharacterCustomizationComponent::ReleaseItemVisualHandle(FName ItemId)
{
	if (TSharedPtr<FStreamableHandle>* Handle = VisualHandles.Find(ItemId))
	{
		if (Handle->IsValid())
		{
			(*Handle)->ReleaseHandle();
		}
		VisualHandles.Remove(ItemId);
	}
}
