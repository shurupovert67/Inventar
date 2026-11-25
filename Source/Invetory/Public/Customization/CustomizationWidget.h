#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Customization/ClothingItemTypes.h"
#include "CustomizationWidget.generated.h"

class UCharacterCustomizationComponent;
class UTexture2D;
struct FStreamableHandle;
struct FSlateColorBrush;
class SBox;

/**
 * Inventory + equipment UI of the customization system.
 *
 * The widget is built from C++ Slate (no widget blueprint asset required), so it can be created
 * directly by UCustomizationSubsystem. Icons are streamed in on demand, only when they are shown.
 */
UCLASS()
class INVETORY_API UCustomizationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Binds the widget to a component (safe to call multiple times). */
	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SetupFor(UCharacterCustomizationComponent* InComponent);

	/** Rebuilds the inventory and the equipment panel. */
	UFUNCTION(BlueprintCallable, Category = "Customization")
	void Refresh();

protected:
	//~ Begin UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget interface

	UFUNCTION()
	void HandleEquipmentChanged(FName SlotId, FName ItemId);

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleItemVisualReady(FName ItemId);

private:
	TSharedRef<SWidget> BuildLayout();
	TSharedRef<SWidget> BuildInventoryPanel();
	TSharedRef<SWidget> BuildInventoryGrid();
	TSharedRef<SWidget> BuildEquipmentPanel();
	TSharedRef<SWidget> BuildEquipmentList();
	TSharedRef<SWidget> BuildDetailsPanel();
	TSharedRef<SWidget> BuildDetailsContent();

	TSharedRef<SWidget> BuildItemCard(const FClothingItemData& Item);
	TSharedRef<SWidget> BuildSlotRow(const FCustomizationSlotDefinition& SlotDefinition);

	void ShowDetails(const FClothingItemData& Item);
	void ClearDetails();

	const FClothingItemData* FindItemById(FName ItemId) const;

	/** Slate attribute that resolves the icon brush once the texture finished loading. */
	TAttribute<const FSlateBrush*> MakeIconAttribute(const TSoftObjectPtr<UTexture2D>& Icon);

	const FSlateBrush* GetIconBrushByPath(const FSoftObjectPath& IconPath) const;
	void RequestIcon(const TSoftObjectPtr<UTexture2D>& Icon);
	void HandleIconLoaded(FSoftObjectPath IconPath);

	void ReleaseIconHandles();

	TWeakObjectPtr<UCharacterCustomizationComponent> Component;

	TSharedPtr<SBox> InventoryHost;
	TSharedPtr<SBox> EquipmentHost;
	TSharedPtr<SBox> DetailsHost;

	FClothingItemData HoveredItem;
	bool bHasHoveredItem = false;

	TMap<FSoftObjectPath, TSharedPtr<FSlateBrush>> IconBrushes;
	TMap<FSoftObjectPath, TSharedPtr<FStreamableHandle>> IconHandles;
	TSet<FSoftObjectPath> PendingIcons;

	mutable TSharedPtr<FSlateColorBrush> PlaceholderBrush;
};
