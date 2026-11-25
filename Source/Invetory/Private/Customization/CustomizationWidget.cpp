#include "Customization/CustomizationWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Customization/CharacterCustomizationComponent.h"
#include "Customization/CustomizationSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Types/SlateEnums.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CustomizationWidget"

DEFINE_LOG_CATEGORY_STATIC(LogCustomizationWidget, Log, All);

namespace CustomizationUI
{
	static const FLinearColor TextColor(0.93f, 0.94f, 0.96f, 1.f);
	static const FLinearColor MutedColor(0.55f, 0.58f, 0.64f, 1.f);
	static const FLinearColor AccentColor(0.38f, 0.86f, 0.48f, 1.f);
	static const FLinearColor DimColor(0.f, 0.f, 0.f, 0.55f);
	static const FLinearColor PanelColor(0.035f, 0.038f, 0.046f, 0.98f);
	static const FLinearColor BlockColor(0.075f, 0.08f, 0.095f, 1.f);
	static const FLinearColor RowColor(0.11f, 0.115f, 0.135f, 1.f);
	static const FLinearColor DetailsColor(0.095f, 0.10f, 0.12f, 1.f);
	static const FLinearColor EquippedTint(0.10f, 0.20f, 0.13f, 1.f);
	static const FLinearColor CardTint(0.10f, 0.105f, 0.125f, 1.f);

	FORCEINLINE FSlateFontInfo Font(int32 Size, const TCHAR* Typeface = TEXT("Regular"))
	{
		return FCoreStyle::GetDefaultFontStyle(FName(Typeface), Size);
	}

	FORCEINLINE const FSlateBrush* SolidBrush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	FORCEINLINE TSharedRef<STextBlock> Label(const FText& Text, int32 Size, const FLinearColor& Color, const TCHAR* Typeface = TEXT("Regular"))
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(Font(Size, Typeface))
			.ColorAndOpacity(FSlateColor(Color));
	}

	/** Transparent button used for the inventory cards (the card background is drawn by a border). */
	static const FButtonStyle& CardButtonStyle()
	{
		static FButtonStyle Style = []()
		{
			FButtonStyle Result;

			FSlateBrush NoDraw;
			NoDraw.DrawAs = ESlateBrushDrawType::NoDrawType;

			Result.SetNormal(NoDraw);
			Result.SetDisabled(NoDraw);
			Result.SetHovered(FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.08f)));
			Result.SetPressed(FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.14f)));
			Result.SetNormalPadding(FMargin(0.f));
			Result.SetPressedPadding(FMargin(0.f));
			return Result;
		}();

		return Style;
	}
}

void UCustomizationWidget::SetupFor(UCharacterCustomizationComponent* InComponent)
{
	if (Component.Get() == InComponent)
	{
		Refresh();
		return;
	}

	if (UCharacterCustomizationComponent* Previous = Component.Get())
	{
		Previous->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
		Previous->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
		Previous->OnItemVisualReady.RemoveDynamic(this, &ThisClass::HandleItemVisualReady);
	}

	Component = InComponent;

	if (InComponent)
	{
		InComponent->OnEquipmentChanged.AddUniqueDynamic(this, &ThisClass::HandleEquipmentChanged);
		InComponent->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);
		InComponent->OnItemVisualReady.AddUniqueDynamic(this, &ThisClass::HandleItemVisualReady);
	}

	Refresh();
}

TSharedRef<SWidget> UCustomizationWidget::RebuildWidget()
{
	return BuildLayout();
}

void UCustomizationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

void UCustomizationWidget::NativeDestruct()
{
	if (UCharacterCustomizationComponent* Previous = Component.Get())
	{
		Previous->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
		Previous->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
		Previous->OnItemVisualReady.RemoveDynamic(this, &ThisClass::HandleItemVisualReady);
	}

	ReleaseIconHandles();

	Super::NativeDestruct();
}

void UCustomizationWidget::Refresh()
{
	if (InventoryHost.IsValid())
	{
		InventoryHost->SetContent(BuildInventoryGrid());
	}

	if (EquipmentHost.IsValid())
	{
		EquipmentHost->SetContent(BuildEquipmentList());
	}

	if (DetailsHost.IsValid())
	{
		DetailsHost->SetContent(BuildDetailsContent());
	}
}

void UCustomizationWidget::HandleEquipmentChanged(FName SlotId, FName ItemId)
{
	Refresh();
}

void UCustomizationWidget::HandleInventoryChanged()
{
	Refresh();
}

void UCustomizationWidget::HandleItemVisualReady(FName ItemId)
{
	Refresh();
}

TSharedRef<SWidget> UCustomizationWidget::BuildLayout()
{
	SAssignNew(InventoryHost, SBox);
	SAssignNew(EquipmentHost, SBox);
	SAssignNew(DetailsHost, SBox);

	TWeakObjectPtr<UCustomizationWidget> WeakThis(this);

	return SNew(SBorder)
		.BorderImage(CustomizationUI::SolidBrush())
		.BorderBackgroundColor(CustomizationUI::DimColor)
		.Padding(FMargin(0.f))
		[
			// Keeps the panel readable on small windows: it is only ever scaled down.
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::DownOnly)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(1180.f)
				.HeightOverride(730.f)
				[
					SNew(SBorder)
					.BorderImage(CustomizationUI::SolidBrush())
					.BorderBackgroundColor(CustomizationUI::PanelColor)
					.Padding(FMargin(22.f))
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 0.f, 0.f, 16.f)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								CustomizationUI::Label(LOCTEXT("Title", "Character Customization"), 22, CustomizationUI::TextColor, TEXT("Bold"))
							]

							+ SHorizontalBox::Slot()
							.FillWidth(1.f)
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0.f, 0.f, 10.f, 0.f)
								[
									CustomizationUI::Label(LOCTEXT("CloseHint", "[TAB] close"), 10, CustomizationUI::MutedColor)
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SButton)
									.OnClicked_Lambda([WeakThis]()
									{
										if (UCustomizationWidget* Self = WeakThis.Get())
										{
											if (UGameInstance* GameInstance = Self->GetGameInstance())
											{
												if (UCustomizationSubsystem* Subsystem = GameInstance->GetSubsystem<UCustomizationSubsystem>())
												{
													Subsystem->HideUI();
												}
											}
										}
										return FReply::Handled();
									})
									[
										CustomizationUI::Label(LOCTEXT("Close", "Close"), 11, CustomizationUI::TextColor)
									]
								]
							]
						]

						+ SVerticalBox::Slot()
						.FillHeight(1.f)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.FillWidth(0.63f)
							.Padding(0.f, 0.f, 16.f, 0.f)
							[
								BuildInventoryPanel()
							]

							+ SHorizontalBox::Slot()
							.FillWidth(0.37f)
							[
								BuildEquipmentPanel()
							]
						]
					]
				]
			]
		];
}

TSharedRef<SWidget> UCustomizationWidget::BuildInventoryPanel()
{
	return SNew(SBorder)
		.BorderImage(CustomizationUI::SolidBrush())
		.BorderBackgroundColor(CustomizationUI::BlockColor)
		.Padding(FMargin(14.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 10.f)
			[
				CustomizationUI::Label(LOCTEXT("InventoryTitle", "INVENTORY"), 13, CustomizationUI::MutedColor, TEXT("Bold"))
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SScrollBox)

				+ SScrollBox::Slot()
				[
					InventoryHost.ToSharedRef()
				]
			]
		];
}

TSharedRef<SWidget> UCustomizationWidget::BuildInventoryGrid()
{
	TSharedRef<SWrapBox> Wrap = SNew(SWrapBox)
		.InnerSlotPadding(FVector2D(8.f, 8.f))
		.UseAllottedSize(true);

	const UCharacterCustomizationComponent* Comp = Component.Get();
	if (!Comp || Comp->GetInventoryRef().Num() == 0)
	{
		Wrap->AddSlot()
		[
			CustomizationUI::Label(LOCTEXT("NoItems", "No items available."), 12, CustomizationUI::MutedColor)
		];
		return Wrap;
	}

	for (const FClothingItemData& Item : Comp->GetInventoryRef())
	{
		RequestIcon(Item.Icon);
		Wrap->AddSlot()
		[
			BuildItemCard(Item)
		];
	}

	return Wrap;
}

TSharedRef<SWidget> UCustomizationWidget::BuildItemCard(const FClothingItemData& Item)
{
	const UCharacterCustomizationComponent* Comp = Component.Get();
	const bool bEquipped = Comp && Comp->IsEquipped(Item.ItemId);

	TWeakObjectPtr<UCharacterCustomizationComponent> WeakComp = Component;
	TWeakObjectPtr<UCustomizationWidget> WeakThis(this);

	return SNew(SBox)
		.WidthOverride(160.f)
		.HeightOverride(140.f)
		[
			SNew(SBorder)
			.BorderImage(CustomizationUI::SolidBrush())
			.BorderBackgroundColor(bEquipped ? CustomizationUI::EquippedTint : CustomizationUI::CardTint)
			.Padding(FMargin(2.f))
			[
				SNew(SButton)
				.ButtonStyle(&CustomizationUI::CardButtonStyle())
				.ContentPadding(FMargin(8.f))
				.OnClicked_Lambda([WeakComp, Item]()
				{
					if (UCharacterCustomizationComponent* Target = WeakComp.Get())
					{
						Target->EquipItem(Item);
					}
					return FReply::Handled();
				})
				.OnHovered_Lambda([WeakThis, Item]()
				{
					if (UCustomizationWidget* Self = WeakThis.Get())
					{
						Self->ShowDetails(Item);
					}
				})
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(64.f)
						.HeightOverride(64.f)
						[
							SNew(SImage)
							.Image(MakeIconAttribute(Item.Icon))
							.DesiredSizeOverride(FVector2D(64.f, 64.f))
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 6.f, 0.f, 2.f)
					[
						SNew(STextBlock)
						.Text(Item.DisplayName)
						.Font(CustomizationUI::Font(12, TEXT("Bold")))
						.Justification(ETextJustify::Center)
						.AutoWrapText(true)
						.ColorAndOpacity(FSlateColor(CustomizationUI::TextColor))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(bEquipped ? LOCTEXT("EquippedTag", "EQUIPPED") : FText::FromName(Item.SlotId))
						.Font(CustomizationUI::Font(9, bEquipped ? TEXT("Bold") : TEXT("Regular")))
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FSlateColor(bEquipped ? CustomizationUI::AccentColor : CustomizationUI::MutedColor))
					]
				]
			]
		];
}

TSharedRef<SWidget> UCustomizationWidget::BuildEquipmentPanel()
{
	return SNew(SBorder)
		.BorderImage(CustomizationUI::SolidBrush())
		.BorderBackgroundColor(CustomizationUI::BlockColor)
		.Padding(FMargin(14.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 10.f)
			[
				CustomizationUI::Label(LOCTEXT("EquipmentTitle", "EQUIPMENT"), 13, CustomizationUI::MutedColor, TEXT("Bold"))
			]

			+ SVerticalBox::Slot()
			.FillHeight(0.6f)
			[
				SNew(SScrollBox)

				+ SScrollBox::Slot()
				[
					EquipmentHost.ToSharedRef()
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(0.4f)
			.Padding(0.f, 12.f, 0.f, 0.f)
			[
				BuildDetailsPanel()
			]
		];
}

TSharedRef<SWidget> UCustomizationWidget::BuildEquipmentList()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	const UCharacterCustomizationComponent* Comp = Component.Get();
	if (!Comp)
	{
		Box->AddSlot()
		.AutoHeight()
		[
			CustomizationUI::Label(LOCTEXT("NoCharacter", "No character."), 11, CustomizationUI::MutedColor)
		];
		return Box;
	}

	const TArray<FCustomizationSlotDefinition> Slots = Comp->GetSlotDefinitions();
	if (Slots.Num() == 0)
	{
		Box->AddSlot()
		.AutoHeight()
		[
			CustomizationUI::Label(LOCTEXT("NoSlots", "No equipment slots configured."), 11, CustomizationUI::MutedColor)
		];
		return Box;
	}

	for (const FCustomizationSlotDefinition& SlotDefinition : Slots)
	{
		FClothingItemData EquippedItem;
		if (Comp->GetEquippedItem(SlotDefinition.SlotId, EquippedItem))
		{
			RequestIcon(EquippedItem.Icon);
		}

		Box->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			BuildSlotRow(SlotDefinition)
		];
	}

	return Box;
}

TSharedRef<SWidget> UCustomizationWidget::BuildSlotRow(const FCustomizationSlotDefinition& SlotDefinition)
{
	const UCharacterCustomizationComponent* Comp = Component.Get();

	FClothingItemData EquippedItem;
	const bool bHasItem = Comp && Comp->GetEquippedItem(SlotDefinition.SlotId, EquippedItem);

	TWeakObjectPtr<UCharacterCustomizationComponent> WeakComp = Component;
	const FName SlotId = SlotDefinition.SlotId;

	return SNew(SBorder)
		.BorderImage(CustomizationUI::SolidBrush())
		.BorderBackgroundColor(CustomizationUI::RowColor)
		.Padding(FMargin(8.f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(44.f)
				.HeightOverride(44.f)
				[
					SNew(SImage)
					.Image(MakeIconAttribute(bHasItem ? EquippedItem.Icon : TSoftObjectPtr<UTexture2D>()))
					.DesiredSizeOverride(FVector2D(44.f, 44.f))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(10.f, 0.f, 10.f, 0.f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					CustomizationUI::Label(SlotDefinition.DisplayName, 10, CustomizationUI::MutedColor, TEXT("Bold"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(bHasItem ? EquippedItem.DisplayName : LOCTEXT("SlotEmpty", "- empty -"))
					.Font(CustomizationUI::Font(13))
					.ColorAndOpacity(FSlateColor(bHasItem ? CustomizationUI::TextColor : CustomizationUI::MutedColor))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.IsEnabled(bHasItem)
				.OnClicked_Lambda([WeakComp, SlotId]()
				{
					if (UCharacterCustomizationComponent* Target = WeakComp.Get())
					{
						Target->UnequipSlot(SlotId);
					}
					return FReply::Handled();
				})
				[
					CustomizationUI::Label(LOCTEXT("Remove", "Remove"), 10, CustomizationUI::TextColor)
				]
			]
		];
}

TSharedRef<SWidget> UCustomizationWidget::BuildDetailsPanel()
{
	return SNew(SBorder)
		.BorderImage(CustomizationUI::SolidBrush())
		.BorderBackgroundColor(CustomizationUI::DetailsColor)
		.Padding(FMargin(12.f))
		[
			SNew(SBox)
			.MinDesiredHeight(150.f)
			[
				DetailsHost.ToSharedRef()
			]
		];
}

TSharedRef<SWidget> UCustomizationWidget::BuildDetailsContent()
{
	if (!bHasHoveredItem)
	{
		return CustomizationUI::Label(LOCTEXT("HoverHint", "Hover an item to see its name, description and icon."), 11, CustomizationUI::MutedColor);
	}

	const FClothingItemData Item = HoveredItem;
	RequestIcon(Item.Icon);

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.WidthOverride(96.f)
			.HeightOverride(96.f)
			[
				SNew(SImage)
				.Image(MakeIconAttribute(Item.Icon))
				.DesiredSizeOverride(FVector2D(96.f, 96.f))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Top)
		.Padding(12.f, 0.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(Item.DisplayName)
				.Font(CustomizationUI::Font(16, TEXT("Bold")))
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(CustomizationUI::TextColor))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 6.f)
			[
				CustomizationUI::Label(FText::FromName(Item.SlotId), 10, CustomizationUI::MutedColor)
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(STextBlock)
				.Text(Item.Description)
				.Font(CustomizationUI::Font(11))
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(CustomizationUI::MutedColor))
			]
		];
}

void UCustomizationWidget::ShowDetails(const FClothingItemData& Item)
{
	HoveredItem = Item;
	bHasHoveredItem = true;

	if (DetailsHost.IsValid())
	{
		DetailsHost->SetContent(BuildDetailsContent());
	}
}

void UCustomizationWidget::ClearDetails()
{
	bHasHoveredItem = false;
	HoveredItem = FClothingItemData();

	if (DetailsHost.IsValid())
	{
		DetailsHost->SetContent(BuildDetailsContent());
	}
}

const FClothingItemData* UCustomizationWidget::FindItemById(FName ItemId) const
{
	const UCharacterCustomizationComponent* Comp = Component.Get();
	if (!Comp)
	{
		return nullptr;
	}

	return Comp->GetInventoryRef().FindByPredicate([ItemId](const FClothingItemData& Item)
	{
		return Item.ItemId == ItemId;
	});
}

TAttribute<const FSlateBrush*> UCustomizationWidget::MakeIconAttribute(const TSoftObjectPtr<UTexture2D>& Icon)
{
	TWeakObjectPtr<UCustomizationWidget> WeakThis(this);
	const FSoftObjectPath IconPath = Icon.ToSoftObjectPath();

	return TAttribute<const FSlateBrush*>::CreateLambda([WeakThis, IconPath]() -> const FSlateBrush*
	{
		if (const UCustomizationWidget* Self = WeakThis.Get())
		{
			return Self->GetIconBrushByPath(IconPath);
		}
		return nullptr;
	});
}

const FSlateBrush* UCustomizationWidget::GetIconBrushByPath(const FSoftObjectPath& IconPath) const
{
	if (const TSharedPtr<FSlateBrush>* Found = IconBrushes.Find(IconPath))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}

	if (!PlaceholderBrush.IsValid())
	{
		PlaceholderBrush = MakeShared<FSlateColorBrush>(FLinearColor(0.16f, 0.17f, 0.20f, 1.f));
	}

	return PlaceholderBrush.Get();
}

void UCustomizationWidget::RequestIcon(const TSoftObjectPtr<UTexture2D>& Icon)
{
	const FSoftObjectPath IconPath = Icon.ToSoftObjectPath();
	if (!IconPath.IsValid() || IconBrushes.Contains(IconPath) || PendingIcons.Contains(IconPath))
	{
		return;
	}

	PendingIcons.Add(IconPath);

	FStreamableDelegate OnLoaded = FStreamableDelegate::CreateUObject(this, &ThisClass::HandleIconLoaded, IconPath);
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(IconPath, OnLoaded);
	if (Handle.IsValid())
	{
		IconHandles.Add(IconPath, Handle);
	}
	else
	{
		UE_LOG(LogCustomizationWidget, Warning, TEXT("Failed to start async load of the icon '%s'."), *IconPath.ToString());
	}
}

void UCustomizationWidget::HandleIconLoaded(FSoftObjectPath IconPath)
{
	PendingIcons.Remove(IconPath);

	UTexture2D* Texture = Cast<UTexture2D>(IconPath.ResolveObject());
	if (!Texture)
	{
		if (const TSharedPtr<FStreamableHandle>* Handle = IconHandles.Find(IconPath))
		{
			if (Handle->IsValid())
			{
				Texture = Cast<UTexture2D>((*Handle)->GetLoadedAsset());
			}
		}
	}

	if (!Texture)
	{
		UE_LOG(LogCustomizationWidget, Warning, TEXT("Icon '%s' could not be loaded."), *IconPath.ToString());
		return;
	}

	TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
	Brush->SetResourceObject(Texture);
	Brush->ImageSize = FVector2D(128.f, 128.f);
	Brush->DrawAs = ESlateBrushDrawType::Image;
	Brush->Tiling = ESlateBrushTileType::NoTile;

	IconBrushes.Add(IconPath, Brush);
}

void UCustomizationWidget::ReleaseIconHandles()
{
	for (const TPair<FSoftObjectPath, TSharedPtr<FStreamableHandle>>& Pair : IconHandles)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->ReleaseHandle();
		}
	}

	IconHandles.Reset();
	PendingIcons.Reset();
}

#undef LOCTEXT_NAMESPACE
