// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "MeshDescription.h"

namespace ProjectWorldWaterHorizontalComposition
{
    bool Compose(FMeshDescription& Mesh, bool& OutChanged, FString& OutGeometryHash, FString& OutError);
}
