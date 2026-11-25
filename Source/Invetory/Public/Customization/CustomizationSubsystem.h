#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Customization/ClothingItemTypes.h"
#include "CustomizationSubsystem.generated.h"

class UCharacterCustomizationComponent;
class UCustomizationWidget;
class UDataTable;
class FCustomizationInputProcessor;

/**
 * Entry point of the customization system.
 *
 * Owns the runtime catalog (parsed from data tables), creates the customization component and the
 * inventory widget for the local player and exposes everything to Blueprints.
 */
UCLASS()
class INVETORY_API UCustomizationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/** Re-reads the data tables. */
	UFUNCTION(BlueprintCallable, Category = "Customization")
	bool ReloadCatalog();

	/** All items of the catalog. */
	UFUNCTION(BlueprintPure, Category = "Customization")
	TArray<FClothingItemData> GetCatalogItems() const { return CatalogItems; }

	/** Items that belong to the given slot. */
	UFUNCTION(BlueprintPure, Category = "Customization")
	TArray<FClothingItemData> GetItemsForSlot(FName SlotId) const;

	/** All known equipment slots, sorted for the UI. */
	UFUNCTION(BlueprintPure, Category = "Customization")
	TArray<FCustomizationSlotDefinition> GetSlotDefinitions() const;

	/** Customization component of the local player character. */
	UFUNCTION(BlueprintPure, Category = "Customization")
	UCharacterCustomizationComponent* GetPlayerCustomizationComponent() const;

	/** Opens the UI if it is closed, closes it otherwise. */
	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	void ToggleUI();

	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	void ShowUI();

	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	void HideUI();

	UFUNCTION(BlueprintPure, Category = "Customization|UI")
	bool IsUIVisible() const;

	const TArray<FClothingItemData>& GetCatalogItemsRef() const { return CatalogItems; }

protected:
	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);
	void StartPlayerWatchdog(UWorld* World);
	void WatchPlayer();

	UFUNCTION()
	void HandlePlayerEquipmentChanged(FName SlotId, FName ItemId);

	void ParseTable(UDataTable* Table);
	void RegisterSlotIfMissing(FName SlotId);

	void BindInputProcessor();
	void UnbindInputProcessor();

	UPROPERTY(Transient)
	TArray<FClothingItemData> CatalogItems;

	UPROPERTY(Transient)
	TMap<FName, FCustomizationSlotDefinition> SlotDefinitions;

	UPROPERTY(Transient)
	TObjectPtr<UCustomizationWidget> Widget;

	TWeakObjectPtr<UCharacterCustomizationComponent> PlayerComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UWorld> WatchedWorld;

	FTimerHandle WatchdogTimerHandle;

	TSharedPtr<FCustomizationInputProcessor> InputProcessor;
};
