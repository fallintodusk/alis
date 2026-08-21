// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

enum class EProjectWorldLayerKind : uint8;
struct FProjectWorldRealizationLayer;

namespace ProjectWorldRealizationGeneratorRegistry
{
	bool IsRegistered(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		EProjectWorldLayerKind LayerKind);

	bool ValidateSettings(
		const FProjectWorldRealizationLayer& Layer,
		FString& OutError);
}
