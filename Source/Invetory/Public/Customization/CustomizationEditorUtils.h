#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CustomizationEditorUtils.generated.h"

class UDataTable;

/**
 * Small editor helpers for the customization data tables.
 * Only compiled in editor builds, safe to call from the editor or from Python.
 */
UCLASS()
class INVETORY_API UCustomizationEditorUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/**
	 * Sets the slot value of one row of a customization data table.
	 * The slot id is matched against the display names of the slot enum ("Head", "Body", "Accessory", ...).
	 */
	UFUNCTION(BlueprintCallable, Category = "Customization|Editor", meta = (DevelopmentOnly))
	static bool SetDataTableItemSlot(UDataTable* Table, FName RowName, FName SlotId);
#endif
};
