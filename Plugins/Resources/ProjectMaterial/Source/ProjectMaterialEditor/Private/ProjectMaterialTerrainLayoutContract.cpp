// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialTerrainLayoutContract.h"

#include "ProjectMaterialCompilerIdentity.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool ReadString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		FString& OutValue,
		FString& OutError)
	{
		if (!Object->TryGetStringField(Field, OutValue) || OutValue.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Terrain layout receipt is missing %s."), Field);
			return false;
		}
		return true;
	}

	bool IsSha256(const FString& Value)
	{
		if (Value.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsHexDigit(Character) || FChar::IsUpper(Character))
			{
				return false;
			}
		}
		return true;
	}

	FString LayoutPayload()
	{
		return TEXT("project_mesh_terrain_layout_v1|channels=ground:0,hydro_transition:1|")
			TEXT("source=float32_vertex_weight|compiled=unorm8_texture2d_array|")
			TEXT("texel_cm=3000|max_dimension=4096|uv_set=0");
	}
}

bool FProjectMaterialTerrainLayoutCompiler::CompileFile(
	const FString& ReceiptPath,
	const FString& ExpectedLayoutSha256,
	FProjectMaterialTerrainLayoutContract& OutContract,
	FString& OutError)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *ReceiptPath))
	{
		OutError = FString::Printf(TEXT("Cannot read terrain layout receipt: %s"), *ReceiptPath);
		return false;
	}
	return CompileJson(Json, ExpectedLayoutSha256, OutContract, OutError);
}

bool FProjectMaterialTerrainLayoutCompiler::CompileJson(
	const FString& Json,
	const FString& ExpectedLayoutSha256,
	FProjectMaterialTerrainLayoutContract& OutContract,
	FString& OutError)
{
	OutContract = {};
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Terrain layout receipt is malformed.");
		return false;
	}
	double LayoutVersion = 0.0;
	double TexelSize = 0.0;
	double MaxDimension = 0.0;
	double UvSet = -1.0;
	FString ReceiptId;
	FString LayoutId;
	FString SourceEncoding;
	FString CompiledEncoding;
	FString ReceiptPayload;
	if (!ReadString(Root, TEXT("receipt_id"), ReceiptId, OutError) ||
		ReceiptId != TEXT("project_mesh_terrain_layout") ||
		!ReadString(Root, TEXT("layout_id"), LayoutId, OutError) ||
		LayoutId != TEXT("project_mesh_terrain_channels") ||
		!Root->TryGetNumberField(TEXT("layout_version"), LayoutVersion) || LayoutVersion != 1.0 ||
		!ReadString(Root, TEXT("layout_sha256"), OutContract.LayoutSha256, OutError) ||
		!ReadString(Root, TEXT("source_encoding"), SourceEncoding, OutError) ||
		SourceEncoding != TEXT("float32_vertex_weight") ||
		!ReadString(Root, TEXT("compiled_encoding"), CompiledEncoding, OutError) ||
		CompiledEncoding != TEXT("unorm8_texture2d_array") ||
		!Root->TryGetNumberField(TEXT("channel_texel_size_cm"), TexelSize) || TexelSize != 3000.0 ||
		!Root->TryGetNumberField(TEXT("channel_texture_max_dimension"), MaxDimension) || MaxDimension != 4096.0 ||
		!Root->TryGetNumberField(TEXT("uv_set"), UvSet) || UvSet != 0.0 ||
		!ReadString(Root, TEXT("receipt_payload"), ReceiptPayload, OutError) ||
		!ReadString(Root, TEXT("receipt_sha256"), OutContract.ReceiptSha256, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Terrain layout receipt has an unsupported contract value.");
		}
		return false;
	}
	OutContract.LayoutId = LayoutId;
	OutContract.LayoutVersion = static_cast<int32>(LayoutVersion);

	const TArray<TSharedPtr<FJsonValue>>* Channels = nullptr;
	if (!Root->TryGetArrayField(TEXT("channels"), Channels) || Channels == nullptr || Channels->Num() != 2)
	{
		OutError = TEXT("Terrain layout receipt must contain exactly two semantic channels.");
		return false;
	}
	const TArray<TPair<FName, int32>> ExpectedChannels = {
		{TEXT("ground"), 0}, {TEXT("hydro_transition"), 1}};
	for (int32 Index = 0; Index < ExpectedChannels.Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Channel = (*Channels)[Index]->AsObject();
		FString SemanticName;
		double PrivateIndex = -1.0;
		if (!Channel.IsValid() ||
			!Channel->TryGetStringField(TEXT("semantic_name"), SemanticName) ||
			!Channel->TryGetNumberField(TEXT("private_channel_index"), PrivateIndex) ||
			SemanticName != ExpectedChannels[Index].Key.ToString() ||
			PrivateIndex != ExpectedChannels[Index].Value)
		{
			OutError = TEXT("Terrain layout receipt channel mapping is invalid.");
			return false;
		}
		OutContract.SemanticChannels.Add(ExpectedChannels[Index]);
	}

	const FString ComputedLayoutSha256 = FProjectMaterialCompilerIdentity::ComputeStringSha256(LayoutPayload());
	if (!IsSha256(OutContract.LayoutSha256) ||
		!IsSha256(OutContract.ReceiptSha256) ||
		OutContract.LayoutSha256 != ComputedLayoutSha256 ||
		(!ExpectedLayoutSha256.IsEmpty() && OutContract.LayoutSha256 != ExpectedLayoutSha256) ||
		OutContract.ReceiptSha256 != FProjectMaterialCompilerIdentity::ComputeStringSha256(ReceiptPayload))
	{
		OutError = TEXT("Terrain layout receipt hash or payload mismatch.");
		return false;
	}
	return true;
}
