#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Customization/ClothingItemTypes.h"
#include "CharacterCustomizationComponent.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
struct FStreamableHandle;

/** Broadcast when the equipment of a slot changes. Empty ItemId means the slot was cleared. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEquipmentChanged, FName, SlotId, FName, ItemId);

/** Broadcast when the set of items owned by the player changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

/** Broadcast when the visual asset of an item finished loading and was applied to the character. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemVisualReady, FName, ItemId);

/**
 * Holds the inventory and the equipped items of one character.
 *
 * Meshes are never loaded up front: equipping an item starts an async load of its static mesh,
 * and the mesh is attached to the static mesh component that matches the slot.
 */
UCLASS(ClassGroup = (Customization), meta = (BlueprintSpawnableComponent))
class INVETORY_API UCharacterCustomizationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterCustomizationComponent();

	//~ Begin UActorComponent interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

	/** Sets up the slot registry. Slots that are missing get an automatic definition (component name == slot id). */
	UFUNCTION(BlueprintCallable, Category = "Customization")
	void ConfigureSlots(const TMap<FName, FCustomizationSlotDefinition>& InSlots);

	/** Replaces the inventory contents. */
	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SetInventory(const TArray<FClothingItemData>& InItems);

	/** Adds one item to the inventory. */
	UFUNCTION(BlueprintCallable, Category = "Customization")
	void AddInventoryItem(const FClothingItemData& Item);

	/** Equips the item with the given id. The previously equipped item of the same slot is replaced. */
	UFUNCTION(BlueprintCallable, Category = "Customization")
	bool EquipItemById(FName ItemId);

	/** Equips an item directly. */
	bool EquipItem(const FClothingItemData& Item);

	/** Clears a slot. */
	UFUNCTION(BlueprintCallable, Category = "Customization")
	bool UnequipSlot(FName SlotId);

	/** Returns the item equipped in the slot. */
	UFUNCTION(BlueprintPure, Category = "Customization")
	bool GetEquippedItem(FName SlotId, FClothingItemData& OutItem) const;

	/** Ids of all slots that currently hold an item. */
	UFUNCTION(BlueprintPure, Category = "Customization")
	TArray<FName> GetEquippedSlotIds() const;

	/** True when the item is currently equipped. */
	UFUNCTION(BlueprintPure, Category = "Customization")
	bool IsEquipped(FName ItemId) const;

	/** True when the visual asset of the item is loaded and applied. */
	UFUNCTION(BlueprintPure, Category = "Customization")
	bool IsItemVisualReady(FName ItemId) const;

	/** Copy of the inventory. */
	UFUNCTION(BlueprintPure, Category = "Customization")
	TArray<FClothingItemData> GetInventory() const { return Inventory; }

	/** Copy of the slot definitions. */
	UFUNCTION(BlueprintPure, Category = "Customization")
	TArray<FCustomizationSlotDefinition> GetSlotDefinitions() const;

	/** Read only access for native code. */
	const TArray<FClothingItemData>& GetInventoryRef() const { return Inventory; }
	const TMap<FName, FClothingItemData>& GetEquippedItemsRef() const { return EquippedItems; }
	const TMap<FName, FCustomizationSlotDefinition>& GetSlotDefinitionsRef() const { return SlotDefinitions; }

	UPROPERTY(BlueprintAssignable, Category = "Customization")
	FOnEquipmentChanged OnEquipmentChanged;

	UPROPERTY(BlueprintAssignable, Category = "Customization")
	FOnInventoryChanged OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Customization")
	FOnItemVisualReady OnItemVisualReady;

protected:
	/** Clears a slot and broadcasts the change. */
	bool UnequipSlotInternal(FName SlotId);

	/** Registers a slot definition if the slot is unknown. Keeps the system data driven. */
	void EnsureSlotDefinition(FName SlotId);

	/** Finds the static mesh component that should receive the mesh of the given slot. */
	UStaticMeshComponent* FindTargetComponent(FName SlotId);

	/** Starts the async load of the item mesh (nothing is loaded if the item has no mesh). */
	void RequestItemVisual(const FClothingItemData& Item);

	/** Async load callback. */
	void HandleItemVisualLoaded(FName ItemId);

	/** Applies a loaded mesh to the character. */
	void ApplyItemVisual(const FClothingItemData& Item, UStaticMesh* Mesh);

	/** Frees the async load handle of the item. */
	void ReleaseItemVisualHandle(FName ItemId);

	const FClothingItemData* FindEquippedItemById(FName ItemId) const;

	/** Baseline (design time) relative transform of each target component, captured on first use. */
	UPROPERTY(Transient)
	TMap<FName, FTransform> BaselineTransforms;

	UPROPERTY(Transient)
	TArray<FClothingItemData> Inventory;

	UPROPERTY(Transient)
	TMap<FName, FClothingItemData> EquippedItems;

	UPROPERTY(Transient)
	TMap<FName, FCustomizationSlotDefinition> SlotDefinitions;

	UPROPERTY(Transient)
	TSet<FName> ReadyItems;

	/** Active async load handles, keyed by item id. */
	TMap<FName, TSharedPtr<FStreamableHandle>> VisualHandles;

	/** Components we already warned about. */
	TSet<FName> WarnedMissingComponents;
};
