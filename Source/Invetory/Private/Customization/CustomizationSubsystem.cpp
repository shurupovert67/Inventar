#include "Customization/CustomizationSubsystem.h"

#include "Customization/CharacterCustomizationComponent.h"
#include "Customization/CustomizationSettings.h"
#include "Customization/CustomizationWidget.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCustomizationSubsystem, Log, All);

namespace CustomizationImport
{
	static FString RemapPath(const FString& InPath, const TMap<FString, FString>& Redirects)
	{
		if (InPath.IsEmpty())
		{
			return InPath;
		}

		for (const TPair<FString, FString>& Pair : Redirects)
		{
			if (!Pair.Key.IsEmpty() && InPath.StartsWith(Pair.Key, ESearchCase::IgnoreCase))
			{
				return Pair.Value + InPath.RightChop(Pair.Key.Len());
			}
		}

		return InPath;
	}

	/**
	 * Blueprint user defined structs name their properties after the variable (Name_Index_Guid),
	 * so both the exact name and the authored name are accepted.
	 */
	static FProperty* FindPropertyByAuthorName(const UScriptStruct* Struct, FName PropertyName)
	{
		if (!Struct || PropertyName.IsNone())
		{
			return nullptr;
		}

		if (FProperty* Exact = Struct->FindPropertyByName(PropertyName))
		{
			return Exact;
		}

		const FString Wanted = PropertyName.ToString();
		const FString Prefix = Wanted + TEXT("_");

		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property)
			{
				continue;
			}

			const FString Candidate = Property->GetName();
			if (Candidate.Equals(Wanted, ESearchCase::IgnoreCase))
			{
				return Property;
			}

			if (Candidate.StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				return Property;
			}
		}

		return nullptr;
	}

	static bool ReadText(const UScriptStruct* Struct, const uint8* Row, FName PropertyName, FText& OutText)
	{
		if (!Struct || !Row || PropertyName.IsNone())
		{
			return false;
		}

		FProperty* Property = FindPropertyByAuthorName(Struct, PropertyName);

		if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			OutText = TextProperty->GetPropertyValue_InContainer(Row);
			return true;
		}

		if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			OutText = FText::FromString(StringProperty->GetPropertyValue_InContainer(Row));
			return true;
		}

		if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			OutText = FText::FromName(NameProperty->GetPropertyValue_InContainer(Row));
			return true;
		}

		return false;
	}

	static bool ReadSoftPath(const UScriptStruct* Struct, const uint8* Row, FName PropertyName, FSoftObjectPath& OutPath)
	{
		if (!Struct || !Row || PropertyName.IsNone())
		{
			return false;
		}

		FProperty* Property = FindPropertyByAuthorName(Struct, PropertyName);

		if (const FSoftObjectProperty* SoftProperty = CastField<FSoftObjectProperty>(Property))
		{
			OutPath = SoftProperty->GetPropertyValue_InContainer(Row).ToSoftObjectPath();
			return true;
		}

		if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			if (UObject* Object = ObjectProperty->GetObjectPropertyValue_InContainer(Row))
			{
				OutPath = FSoftObjectPath(Object);
			}
			return true;
		}

		return false;
	}

	static bool ReadSlot(const UScriptStruct* Struct, const uint8* Row, FName PropertyName, FName& OutSlotId)
	{
		if (!Struct || !Row || PropertyName.IsNone())
		{
			return false;
		}

		FProperty* Property = FindPropertyByAuthorName(Struct, PropertyName);

		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			const uint8 Value = ByteProperty->GetPropertyValue_InContainer(Row);
			if (const UEnum* Enum = ByteProperty->Enum)
			{
				// User defined enums (like the one in this project) store the readable name in the display map.
				FString Name = Enum->GetDisplayNameTextByValue(Value).ToString();
				if (Name.IsEmpty())
				{
					Name = Enum->GetNameStringByValue(Value);
				}
				Name.TrimStartAndEndInline();
				OutSlotId = FName(*Name);
			}
			else
			{
				OutSlotId = FName(*FString::FromInt(Value));
			}
			return true;
		}

		if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			OutSlotId = NameProperty->GetPropertyValue_InContainer(Row);
			return true;
		}

		if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			OutSlotId = FName(*StringProperty->GetPropertyValue_InContainer(Row));
			return true;
		}

		return false;
	}

	static bool ReadTransform(const UScriptStruct* Struct, const uint8* Row, FName PropertyName, FTransform& OutTransform)
	{
		if (!Struct || !Row || PropertyName.IsNone())
		{
			return false;
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(FindPropertyByAuthorName(Struct, PropertyName)))
		{
			if (StructProperty->Struct == TBaseStructure<FTransform>::Get())
			{
				if (const FTransform* Value = StructProperty->ContainerPtrToValuePtr<FTransform>(Row))
				{
					OutTransform = *Value;
					return true;
				}
			}
		}

		return false;
	}
}

/** Catches the UI toggle keys before the game consumes them. Works with Enhanced Input and legacy input. */
class FCustomizationInputProcessor final : public IInputProcessor
{
public:
	FCustomizationInputProcessor(UCustomizationSubsystem* InSubsystem, FKey InToggleKey, FKey InCloseKey)
		: Subsystem(InSubsystem)
		, ToggleKey(InToggleKey)
		, CloseKey(InCloseKey)
	{
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		UCustomizationSubsystem* Owner = Subsystem.Get();
		if (!Owner)
		{
			return false;
		}

		const FKey Key = InKeyEvent.GetKey();

		if (Key == ToggleKey)
		{
			Owner->ToggleUI();
			return true;
		}

		if (Key == CloseKey && Owner->IsUIVisible())
		{
			Owner->HideUI();
			return true;
		}

		return false;
	}

	virtual const TCHAR* GetDebugName() const override
	{
		return TEXT("CustomizationInputProcessor");
	}

private:
	TWeakObjectPtr<UCustomizationSubsystem> Subsystem;
	FKey ToggleKey;
	FKey CloseKey;
};

void UCustomizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ReloadCatalog();

	const UCustomizationSettings* Settings = GetDefault<UCustomizationSettings>();
	if (!Settings->bAutoSetup)
	{
		return;
	}

	BindInputProcessor();

	FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &ThisClass::HandlePostWorldInitialization);

	// The world may already exist when the subsystem is created late.
	if (UWorld* World = GetWorld())
	{
		StartPlayerWatchdog(World);
	}
}

void UCustomizationSubsystem::Deinitialize()
{
	FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);

	if (UWorld* World = WatchedWorld.Get())
	{
		World->GetTimerManager().ClearTimer(WatchdogTimerHandle);
	}
	WatchedWorld.Reset();

	UnbindInputProcessor();

	Widget = nullptr;
	PlayerComponent.Reset();

	Super::Deinitialize();
}

void UCustomizationSubsystem::HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS)
{
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	if (World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	if (!GetDefault<UCustomizationSettings>()->bAutoSetup)
	{
		return;
	}

	StartPlayerWatchdog(World);
}

void UCustomizationSubsystem::StartPlayerWatchdog(UWorld* World)
{
	if (!World)
	{
		return;
	}

	if (UWorld* PreviousWorld = WatchedWorld.Get())
	{
		PreviousWorld->GetTimerManager().ClearTimer(WatchdogTimerHandle);
	}

	WatchedWorld = World;
	Widget = nullptr;
	PlayerComponent.Reset();

	World->GetTimerManager().SetTimer(WatchdogTimerHandle, FTimerDelegate::CreateUObject(this, &ThisClass::WatchPlayer), 0.25f, true, 0.25f);
}

void UCustomizationSubsystem::WatchPlayer()
{
	UWorld* World = WatchedWorld.Get();
	if (!World)
	{
		return;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	APawn* Pawn = PlayerController->GetPawn();
	if (!Pawn)
	{
		return;
	}

	UCharacterCustomizationComponent* Component = Pawn->FindComponentByClass<UCharacterCustomizationComponent>();
	const bool bComponentIsNew = (Component == nullptr);

	if (!Component)
	{
		Component = NewObject<UCharacterCustomizationComponent>(Pawn, TEXT("CharacterCustomizationComponent"));
		if (!Component)
		{
			return;
		}

		Pawn->AddInstanceComponent(Component);
		Component->RegisterComponent();
	}

	if (Component != PlayerComponent.Get())
	{
		Component->OnEquipmentChanged.AddUniqueDynamic(this, &ThisClass::HandlePlayerEquipmentChanged);
		Component->ConfigureSlots(SlotDefinitions);

		const UCustomizationSettings* Settings = GetDefault<UCustomizationSettings>();
		if (bComponentIsNew && Settings->bStartWithAllItems)
		{
			Component->SetInventory(CatalogItems);
		}

		PlayerComponent = Component;

		if (Widget)
		{
			Widget->SetupFor(Component);
		}
	}

	if (!Widget)
	{
		Widget = CreateWidget<UCustomizationWidget>(PlayerController, UCustomizationWidget::StaticClass());
		if (Widget)
		{
			Widget->SetupFor(Component);
			Widget->SetVisibility(ESlateVisibility::Collapsed);
			Widget->AddToViewport(100);

			UE_LOG(LogCustomizationSubsystem, Log, TEXT("Customization ready for '%s': %d items in the inventory, %d slots."),
				*Pawn->GetName(), Component->GetInventoryRef().Num(), Component->GetSlotDefinitionsRef().Num());
		}
		else
		{
			UE_LOG(LogCustomizationSubsystem, Warning, TEXT("Failed to create the customization widget."));
		}
	}
	else if (Widget->GetOwningPlayer() != PlayerController)
	{
		Widget->SetOwningPlayer(PlayerController);
		Widget->SetupFor(Component);
	}
}

void UCustomizationSubsystem::HandlePlayerEquipmentChanged(FName SlotId, FName ItemId)
{
	UE_LOG(LogCustomizationSubsystem, Verbose, TEXT("Slot '%s' now holds '%s'."), *SlotId.ToString(), *ItemId.ToString());
}

bool UCustomizationSubsystem::ReloadCatalog()
{
	CatalogItems.Reset();
	SlotDefinitions.Reset();

	const UCustomizationSettings* Settings = GetDefault<UCustomizationSettings>();

	for (const TPair<FName, FCustomizationSlotDefinition>& Pair : Settings->Slots)
	{
		FCustomizationSlotDefinition Definition = Pair.Value;
		if (Definition.SlotId.IsNone())
		{
			Definition.SlotId = Pair.Key;
		}
		if (Definition.TargetComponentName.IsNone())
		{
			Definition.TargetComponentName = Definition.SlotId;
		}
		if (Definition.DisplayName.IsEmpty())
		{
			Definition.DisplayName = FText::FromName(Definition.SlotId);
		}

		SlotDefinitions.Add(Definition.SlotId, Definition);
	}

	for (const TSoftObjectPtr<UDataTable>& TableReference : Settings->ItemTables)
	{
		UDataTable* Table = TableReference.LoadSynchronous();
		if (!Table)
		{
			UE_LOG(LogCustomizationSubsystem, Error, TEXT("Customization data table '%s' could not be loaded."), *TableReference.ToString());
			continue;
		}

		ParseTable(Table);
	}

	UE_LOG(LogCustomizationSubsystem, Log, TEXT("Customization catalog loaded: %d items in %d slots."), CatalogItems.Num(), SlotDefinitions.Num());

	if (CatalogItems.Num() == 0)
	{
		UE_LOG(LogCustomizationSubsystem, Warning, TEXT("Customization catalog is empty. Check the item tables in Project Settings > Game > Character Customization."));
	}

	// Refresh every player that is already alive (after a manual reload).
	if (UCharacterCustomizationComponent* Component = PlayerComponent.Get())
	{
		Component->ConfigureSlots(SlotDefinitions);

		if (Component->GetInventoryRef().Num() == 0 && Settings->bStartWithAllItems)
		{
			Component->SetInventory(CatalogItems);
		}
	}

	return CatalogItems.Num() > 0;
}

void UCustomizationSubsystem::ParseTable(UDataTable* Table)
{
	const UScriptStruct* RowStruct = Table->GetRowStruct();
	if (!RowStruct)
	{
		UE_LOG(LogCustomizationSubsystem, Error, TEXT("Data table '%s' has no row structure."), *Table->GetName());
		return;
	}

	const UCustomizationSettings* Settings = GetDefault<UCustomizationSettings>();
	const FCustomizationFieldNames& Fields = Settings->FieldNames;

	if (!CustomizationImport::FindPropertyByAuthorName(RowStruct, Fields.Slot))
	{
		UE_LOG(LogCustomizationSubsystem, Error,
			TEXT("Row struct '%s' has no slot field '%s' (configured in project settings)."), *RowStruct->GetName(), *Fields.Slot.ToString());
	}

	for (const TPair<FName, uint8*>& RowPair : Table->GetRowMap())
	{
		const uint8* RowData = RowPair.Value;
		if (!RowData)
		{
			continue;
		}

		FClothingItemData Item;
		Item.ItemId = RowPair.Key;

		CustomizationImport::ReadText(RowStruct, RowData, Fields.ItemName, Item.DisplayName);
		CustomizationImport::ReadText(RowStruct, RowData, Fields.Description, Item.Description);
		CustomizationImport::ReadSlot(RowStruct, RowData, Fields.Slot, Item.SlotId);
		CustomizationImport::ReadTransform(RowStruct, RowData, Fields.MeshOffset, Item.MeshOffset);

		if (Item.ItemId.IsNone() || Item.SlotId.IsNone())
		{
			UE_LOG(LogCustomizationSubsystem, Warning, TEXT("Skipping row '%s': no item id or no slot."), *RowPair.Key.ToString());
			continue;
		}

		if (CatalogItems.ContainsByPredicate([&Item](const FClothingItemData& Existing) { return Existing.ItemId == Item.ItemId; }))
		{
			UE_LOG(LogCustomizationSubsystem, Warning, TEXT("Duplicate item id '%s' in table '%s', skipped."), *Item.ItemId.ToString(), *Table->GetName());
			continue;
		}

		FSoftObjectPath IconPath;
		if (CustomizationImport::ReadSoftPath(RowStruct, RowData, Fields.Icon, IconPath) && IconPath.IsValid())
		{
			IconPath = FSoftObjectPath(CustomizationImport::RemapPath(IconPath.ToString(), Settings->FolderRedirects));
			if (!FPackageName::DoesPackageExist(IconPath.GetLongPackageName()))
			{
				UE_LOG(LogCustomizationSubsystem, Warning, TEXT("Icon '%s' of item '%s' does not exist."), *IconPath.ToString(), *Item.ItemId.ToString());
			}
			Item.Icon = TSoftObjectPtr<UTexture2D>(IconPath);
		}

		FSoftObjectPath MeshPath;
		if (CustomizationImport::ReadSoftPath(RowStruct, RowData, Fields.Mesh, MeshPath) && MeshPath.IsValid())
		{
			MeshPath = FSoftObjectPath(CustomizationImport::RemapPath(MeshPath.ToString(), Settings->FolderRedirects));
			if (!FPackageName::DoesPackageExist(MeshPath.GetLongPackageName()))
			{
				UE_LOG(LogCustomizationSubsystem, Warning, TEXT("Mesh '%s' of item '%s' does not exist."), *MeshPath.ToString(), *Item.ItemId.ToString());
			}
			Item.Mesh = TSoftObjectPtr<UStaticMesh>(MeshPath);
		}

		RegisterSlotIfMissing(Item.SlotId);
		CatalogItems.Add(Item);

		UE_LOG(LogCustomizationSubsystem, Verbose, TEXT("Item '%s': name '%s', slot '%s', icon '%s', mesh '%s'."),
			*Item.ItemId.ToString(),
			*Item.DisplayName.ToString(),
			*Item.SlotId.ToString(),
			*Item.Icon.ToString(),
			*Item.Mesh.ToString());
	}
}

void UCustomizationSubsystem::RegisterSlotIfMissing(FName SlotId)
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

	UE_LOG(LogCustomizationSubsystem, Log, TEXT("Auto-registered equipment slot '%s'."), *SlotId.ToString());
}

TArray<FClothingItemData> UCustomizationSubsystem::GetItemsForSlot(FName SlotId) const
{
	TArray<FClothingItemData> Result;
	for (const FClothingItemData& Item : CatalogItems)
	{
		if (Item.SlotId == SlotId)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<FCustomizationSlotDefinition> UCustomizationSubsystem::GetSlotDefinitions() const
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

UCharacterCustomizationComponent* UCustomizationSubsystem::GetPlayerCustomizationComponent() const
{
	return PlayerComponent.Get();
}

void UCustomizationSubsystem::ToggleUI()
{
	if (IsUIVisible())
	{
		HideUI();
	}
	else
	{
		ShowUI();
	}
}

void UCustomizationSubsystem::ShowUI()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	if (!Widget)
	{
		WatchPlayer();
	}

	if (!Widget)
	{
		UE_LOG(LogCustomizationSubsystem, Warning, TEXT("Customization UI cannot be opened: the widget is not created yet."));
		return;
	}

	Widget->SetupFor(GetPlayerCustomizationComponent());
	Widget->Refresh();
	Widget->SetVisibility(ESlateVisibility::Visible);

	PlayerController->SetShowMouseCursor(true);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
}

void UCustomizationSubsystem::HideUI()
{
	if (!Widget)
	{
		return;
	}

	Widget->SetVisibility(ESlateVisibility::Collapsed);

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			PlayerController->SetInputMode(FInputModeGameOnly());
			PlayerController->SetShowMouseCursor(false);
		}
	}
}

bool UCustomizationSubsystem::IsUIVisible() const
{
	return Widget && Widget->IsVisible();
}

void UCustomizationSubsystem::BindInputProcessor()
{
	if (!FSlateApplication::IsInitialized() || InputProcessor.IsValid())
	{
		return;
	}

	const UCustomizationSettings* Settings = GetDefault<UCustomizationSettings>();
	InputProcessor = MakeShared<FCustomizationInputProcessor>(this, Settings->ToggleKey, Settings->CloseKey);
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
}

void UCustomizationSubsystem::UnbindInputProcessor()
{
	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();
}
