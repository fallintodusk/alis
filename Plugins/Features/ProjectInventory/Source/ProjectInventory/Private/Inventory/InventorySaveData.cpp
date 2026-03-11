// Copyright ALIS. All Rights Reserved.

#include "Inventory/InventorySaveData.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
	const uint32 InventorySaveMagic = 0x53564E49; // "INVS"
	const uint16 InventorySaveVersion = 1;
	const int32 InventorySaveHeaderSize = sizeof(uint32) + sizeof(uint16);
}

bool SerializeInventorySaveData(const FInventorySaveData& InData, TArray<uint8>& OutBytes)
{
    OutBytes.Reset();

    FMemoryWriter Writer(OutBytes, true);
    FObjectAndNameAsStringProxyArchive Ar(Writer, true);
    Ar.ArIsSaveGame = true;

	uint32 Magic = InventorySaveMagic;
	uint16 Version = InventorySaveVersion;
	Ar << Magic;
	Ar << Version;

    FInventorySaveData Temp = InData;
    FInventorySaveData::StaticStruct()->SerializeItem(Ar, &Temp, nullptr);
    return !Ar.IsError();
}

bool DeserializeInventorySaveData(const TArray<uint8>& InBytes, FInventorySaveData& OutData)
{
    if (InBytes.Num() == 0)
    {
        OutData = FInventorySaveData();
        return true;
    }

	if (InBytes.Num() >= InventorySaveHeaderSize)
	{
		FMemoryReader Reader(InBytes, true);
		FObjectAndNameAsStringProxyArchive Ar(Reader, true);
		Ar.ArIsSaveGame = true;

		uint32 Magic = 0;
		uint16 Version = 0;
		Ar << Magic;
		Ar << Version;

		if (Ar.IsError())
		{
			return false;
		}

		if (Magic == InventorySaveMagic)
		{
			if (Version > InventorySaveVersion)
			{
				return false;
			}

			FInventorySaveData::StaticStruct()->SerializeItem(Ar, &OutData, nullptr);
			return !Ar.IsError();
		}
	}

	// Legacy fallback (no header)
	FMemoryReader LegacyReader(InBytes, true);
	FObjectAndNameAsStringProxyArchive LegacyAr(LegacyReader, true);
	LegacyAr.ArIsSaveGame = true;
	FInventorySaveData::StaticStruct()->SerializeItem(LegacyAr, &OutData, nullptr);
	return !LegacyAr.IsError();
}
