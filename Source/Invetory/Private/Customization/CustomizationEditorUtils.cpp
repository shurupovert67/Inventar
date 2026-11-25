#include "Customization/CustomizationEditorUtils.h"

#if WITH_EDITOR
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCustomizationEditor, Log, All);

namespace
{
	FProperty* FindFieldByAuthorName(const UScriptStruct* Struct, const FName FieldName)
	{
		if (!Struct || FieldName.IsNone())
		{
			return nullptr;
		}

		if (FProperty* Exact = Struct->FindPropertyByName(FieldName))
		{
			return Exact;
		}

		const FString Wanted = FieldName.ToString();
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (*It && It->GetName().StartsWith(Wanted + TEXT("_")))
			{
				return *It;
			}
		}

		return nullptr;
	}
}

bool UCustomizationEditorUtils::SetDataTableItemSlot(UDataTable* Table, FName RowName, FName SlotId)
{
	if (!Table || RowName.IsNone() || SlotId.IsNone())
	{
		return false;
	}

	const UScriptStruct* RowStruct = Table->GetRowStruct();
	FByteProperty* SlotProperty = CastField<FByteProperty>(FindFieldByAuthorName(RowStruct, TEXT("SlotType")));
	if (!SlotProperty || !SlotProperty->Enum)
	{
		UE_LOG(LogCustomizationEditor, Error, TEXT("'%s' has no enum slot field 'SlotType'."), Table ? *Table->GetName() : TEXT("None"));
		return false;
	}

	uint8* RowData = Table->FindRowUnchecked(RowName);
	if (!RowData)
	{
		UE_LOG(LogCustomizationEditor, Error, TEXT("Row '%s' was not found in '%s'."), *RowName.ToString(), *Table->GetName());
		return false;
	}

	const UEnum* Enum = SlotProperty->Enum;
	const FString WantedSlot = SlotId.ToString();

	for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
	{
		const FString DisplayName = Enum->GetDisplayNameTextByIndex(Index).ToString();
		const FString EnumName = Enum->GetNameStringByIndex(Index);

		if (DisplayName.Equals(WantedSlot, ESearchCase::IgnoreCase) || EnumName.Equals(WantedSlot, ESearchCase::IgnoreCase))
		{
			const uint8 Value = static_cast<uint8>(Enum->GetValueByIndex(Index));
			SlotProperty->SetPropertyValue_InContainer(RowData, Value);
			Table->MarkPackageDirty();

			UE_LOG(LogCustomizationEditor, Log, TEXT("Row '%s' of '%s' now uses slot '%s' (value %d)."),
				*RowName.ToString(), *Table->GetName(), *WantedSlot, Value);
			return true;
		}
	}

	UE_LOG(LogCustomizationEditor, Error, TEXT("Slot '%s' was not found in enum '%s'."), *WantedSlot, *Enum->GetName());
	return false;
}
#endif // WITH_EDITOR
