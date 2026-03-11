#pragma once

#include "CoreMinimal.h"
#include "ContentBrowserFrontEndFilterExtension.h"
#include "ComponentFrontendFilter.generated.h"

/**
 * Extends Content Browser with component-based filters.
 * Creates filters from FComponentFilterRegistry entries.
 */
UCLASS()
class PROJECTCOREEDITOR_API UComponentFilterExtension : public UContentBrowserFrontEndFilterExtension
{
	GENERATED_BODY()

public:
	virtual void AddFrontEndFilterExtensions(
		TSharedPtr<class FFrontendFilterCategory> DefaultCategory,
		TArray<TSharedRef<class FFrontendFilter>>& InOutFilterList) const override;
};
