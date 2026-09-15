// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "W_PreviewFlightHints.generated.h"

UCLASS()
class PROJECTSINGLEPLAYCLIENT_API UW_PreviewFlightHints final : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UW_PreviewFlightHints(const FObjectInitializer& ObjectInitializer);
};
