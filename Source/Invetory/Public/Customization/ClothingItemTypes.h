#pragma once

#include "CoreMinimal.h"
#include "ClothingItemTypes.generated.h"

class UStaticMesh;
class UTexture2D;

/**
 * Runtime representation of a single customization item.
 * Only soft references are stored here, so no visual asset is loaded until it is actually needed.
 */
USTRUCT(BlueprintType)
struct INVETORY_API FClothingItemData
{
	GENERATED_BODY()

	/** Unique identifier (row name of the source data table row). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName ItemId;

	/** Name shown in the UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/** Description shown in the UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText Description;

	/** Id of the equipment slot this item belongs to (Head / Body / Legs / Shoes / Accessory by default). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName SlotId;

	/** Icon, loaded on demand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Visual mesh, loaded on demand (async) when the item gets equipped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Optional additional local offset applied on top of the slot offset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FTransform MeshOffset = FTransform::Identity;

	bool IsValidForEquipment() const { return !ItemId.IsNone() && !SlotId.IsNone(); }
};

/**
 * Description of an equipment slot.
 * Slots are data driven: adding a new entry here (or a new slot id in the data table) is enough,
 * no code change is required.
 */
USTRUCT(BlueprintType)
struct INVETORY_API FCustomizationSlotDefinition
{
	GENERATED_BODY()

	/** Unique slot id, must match the slot id of the items. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	FName SlotId;

	/** Name shown in the UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	FText DisplayName;

	/** Name of the static mesh component on the character that receives the item mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	FName TargetComponentName;

	/** Local offset applied to the target component when an item is equipped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	FTransform MeshOffset = FTransform::Identity;

	/** UI ordering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	int32 SortOrder = 0;
};

/** Names of the row-struct fields that the DataTable importer looks for. */
USTRUCT()
struct INVETORY_API FCustomizationFieldNames
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Fields")
	FName ItemName = TEXT("ItemName");

	UPROPERTY(EditAnywhere, Category = "Fields")
	FName Description = TEXT("Description");

	UPROPERTY(EditAnywhere, Category = "Fields")
	FName Icon = TEXT("Icon");

	UPROPERTY(EditAnywhere, Category = "Fields")
	FName Slot = TEXT("SlotType");

	UPROPERTY(EditAnywhere, Category = "Fields")
	FName Mesh = TEXT("ClothingMesh");

	/** Optional field: extra local offset of the item mesh. */
	UPROPERTY(EditAnywhere, Category = "Fields")
	FName MeshOffset = TEXT("MeshOffset");

	/** Optional field: overrides the target mesh component of the slot for this item. */
	UPROPERTY(EditAnywhere, Category = "Fields")
	FName TargetComponent = TEXT("TargetComponent");
};
