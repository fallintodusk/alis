// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Utilities/ProjectSha256.h"

#include "Misc/FileHelper.h"

namespace ProjectSha256
{
	uint32 RotateRight(uint32 Value, uint32 Count)
	{
		return (Value >> Count) | (Value << (32 - Count));
	}

	uint32 ReadBigEndian32(const uint8* Data)
	{
		return (uint32(Data[0]) << 24) |
			(uint32(Data[1]) << 16) |
			(uint32(Data[2]) << 8) |
			uint32(Data[3]);
	}

	constexpr uint32 RoundConstants[64] = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
		0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
		0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
		0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
		0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
		0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
		0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
		0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
		0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	};
}

bool FProjectSha256::HashFile(const FString& FilePath, FString& OutHash)
{
	TArray<uint8> Data;
	return FFileHelper::LoadFileToArray(Data, *FilePath) && HashBuffer(Data, OutHash);
}

bool FProjectSha256::HashBuffer(const TArray<uint8>& Data, FString& OutHash)
{
	using namespace ProjectSha256;
	uint32 HashState[8] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	};

	TArray<uint8> Message = Data;
	const uint64 BitLength = uint64(Data.Num()) * 8ull;
	Message.Add(0x80);
	while ((Message.Num() % 64) != 56)
	{
		Message.Add(0);
	}
	for (int32 Shift = 56; Shift >= 0; Shift -= 8)
	{
		Message.Add(uint8((BitLength >> Shift) & 0xff));
	}

	for (int32 Offset = 0; Offset < Message.Num(); Offset += 64)
	{
		uint32 Words[64];
		for (int32 Index = 0; Index < 16; ++Index)
		{
			Words[Index] = ReadBigEndian32(Message.GetData() + Offset + Index * 4);
		}
		for (int32 Index = 16; Index < 64; ++Index)
		{
			const uint32 S0 = RotateRight(Words[Index - 15], 7) ^ RotateRight(Words[Index - 15], 18) ^ (Words[Index - 15] >> 3);
			const uint32 S1 = RotateRight(Words[Index - 2], 17) ^ RotateRight(Words[Index - 2], 19) ^ (Words[Index - 2] >> 10);
			Words[Index] = Words[Index - 16] + S0 + Words[Index - 7] + S1;
		}

		uint32 A = HashState[0];
		uint32 B = HashState[1];
		uint32 C = HashState[2];
		uint32 D = HashState[3];
		uint32 E = HashState[4];
		uint32 F = HashState[5];
		uint32 G = HashState[6];
		uint32 H = HashState[7];
		for (int32 Index = 0; Index < 64; ++Index)
		{
			const uint32 S1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
			const uint32 Choice = (E & F) ^ ((~E) & G);
			const uint32 Temp1 = H + S1 + Choice + RoundConstants[Index] + Words[Index];
			const uint32 S0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
			const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
			const uint32 Temp2 = S0 + Majority;
			H = G;
			G = F;
			F = E;
			E = D + Temp1;
			D = C;
			C = B;
			B = A;
			A = Temp1 + Temp2;
		}
		HashState[0] += A;
		HashState[1] += B;
		HashState[2] += C;
		HashState[3] += D;
		HashState[4] += E;
		HashState[5] += F;
		HashState[6] += G;
		HashState[7] += H;
	}

	OutHash.Empty(64);
	for (uint32 Word : HashState)
	{
		OutHash += FString::Printf(TEXT("%08x"), Word);
	}
	return true;
}
