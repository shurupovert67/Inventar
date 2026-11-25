#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Customization/ClothingItemTypes.h"
#include "CustomizationSettings.generated.h"

class UDataTable;

/**
 * Project wide configuration of the customization system.
 * Everything here is data: new items are added to the data tables, new slots only need a new
 * entry in the Slots map (no code changes required).
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Character Customization"))
class INVETORY_API UCustomizationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCustomizationSettings();

	/** Data tables that describe the available items. Any row struct that has the configured field names works. */
	UPROPERTY(config, EditAnywhere, Category = "Data")
	TArray<TSoftObjectPtr<UDataTable>> ItemTables;

	/** Names of the row struct fields to read. */
	UPROPERTY(config, EditAnywhere, Category = "Data")
	FCustomizationFieldNames FieldNames;

	/** Optional path remapping, e.g. /Game/OldFolder/ -> /Game/NewFolder/. Applied to icon and mesh paths. */
	UPROPERTY(config, EditAnywhere, Category = "Data")
	TMap<FString, FString> FolderRedirects;

	/** Equipment slots. Adding a new entry here is enough to support a new slot. */
	UPROPERTY(config, EditAnywhere, Category = "Slots")
	TMap<FName, FCustomizationSlotDefinition> Slots;

	/** Automatically create the component, the widget and bind the toggle key for the local player. */
	UPROPERTY(config, EditAnywhere, Category = "Runtime")
	bool bAutoSetup = true;

	/** Key that opens / closes the customization UI. */
	UPROPERTY(config, EditAnywhere, Category = "Runtime")
	FKey ToggleKey = EKeys::Tab;

	/** Key that closes the customization UI. */
	UPROPERTY(config, EditAnywhere, Category = "Runtime")
	FKey CloseKey = EKeys::Escape;

	/** Give every catalog item to the player inventory when the component is created. */
	UPROPERTY(config, EditAnywhere, Category = "Runtime")
	bool bStartWithAllItems = true;
};
