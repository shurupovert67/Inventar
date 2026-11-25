#include "Customization/CustomizationSettings.h"

#define LOCTEXT_NAMESPACE "CustomizationSettings"

namespace
{
	FCustomizationSlotDefinition MakeSlot(const TCHAR* Id, const TCHAR* DisplayName, const TCHAR* ComponentName, int32 Order)
	{
		FCustomizationSlotDefinition Slot;
		Slot.SlotId = Id;
		Slot.DisplayName = FText::FromString(DisplayName);
		Slot.TargetComponentName = ComponentName;
		Slot.MeshOffset = FTransform::Identity;
		Slot.SortOrder = Order;
		return Slot;
	}
}

UCustomizationSettings::UCustomizationSettings()
{
	// Default catalog: the data table that already exists in the project.
	ItemTables.Add(TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/BPs/DT_ClothingItems.DT_ClothingItems"))));

	// The table was authored when the assets lived in /Game/Customization, they were moved since then.
	FolderRedirects.Add(TEXT("/Game/Customization/"), TEXT("/Game/BPs/Data/"));

	Slots.Add(TEXT("Head"), MakeSlot(TEXT("Head"), TEXT("Head"), TEXT("Head"), 0));
	Slots.Add(TEXT("Body"), MakeSlot(TEXT("Body"), TEXT("Body"), TEXT("Body"), 1));
	Slots.Add(TEXT("Legs"), MakeSlot(TEXT("Legs"), TEXT("Legs"), TEXT("Legs"), 2));
	Slots.Add(TEXT("Shoes"), MakeSlot(TEXT("Shoes"), TEXT("Shoes"), TEXT("Shoes"), 3));
	Slots.Add(TEXT("Accessory"), MakeSlot(TEXT("Accessory"), TEXT("Accessory"), TEXT("Backpack"), 4));
}

#undef LOCTEXT_NAMESPACE
