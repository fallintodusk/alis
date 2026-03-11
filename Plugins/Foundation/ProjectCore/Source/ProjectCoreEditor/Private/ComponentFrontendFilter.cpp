#include "ComponentFrontendFilter.h"
#include "FrontendFilterBase.h"
#include "ContentBrowserDataSource.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "UObject/UObjectIterator.h"

/**
 * Filter that matches Blueprints containing a specific component class.
 */
class FFrontendFilter_Component : public FFrontendFilter
{
public:
	FFrontendFilter_Component(
		TSharedPtr<FFrontendFilterCategory> InCategory,
		UClass* InComponentClass)
		: FFrontendFilter(InCategory)
		, ComponentClass(InComponentClass)
	{
		// Extract display name from metadata or class name
		const FString* MetaName = InComponentClass->FindMetaData(TEXT("ContentBrowserFilterName"));
		DisplayName = MetaName ? *MetaName : InComponentClass->GetName().Replace(TEXT("Component"), TEXT(""));

		const FString* MetaTooltip = InComponentClass->FindMetaData(TEXT("ContentBrowserFilterTooltip"));
		Tooltip = MetaTooltip ? *MetaTooltip : FString::Printf(TEXT("Blueprints with %s"), *InComponentClass->GetName());
	}

	virtual FString GetName() const override
	{
		return FString::Printf(TEXT("Component_%s"), *ComponentClass->GetName());
	}

	virtual FText GetDisplayName() const override
	{
		return FText::FromString(DisplayName);
	}

	virtual FText GetToolTipText() const override
	{
		return FText::FromString(Tooltip);
	}

	virtual bool PassesFilter(const FContentBrowserItem& InItem) const override
	{
		FAssetData AssetData;
		if (!InItem.Legacy_TryGetAssetData(AssetData))
		{
			return false;
		}

		if (AssetData.AssetClassPath != UBlueprint::StaticClass()->GetClassPathName())
		{
			return false;
		}

		UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
		if (!Blueprint)
		{
			return false;
		}

		// Check this Blueprint and all parent Blueprints
		UBlueprint* CurrentBP = Blueprint;
		while (CurrentBP)
		{
			if (CurrentBP->SimpleConstructionScript)
			{
				for (USCS_Node* Node : CurrentBP->SimpleConstructionScript->GetAllNodes())
				{
					if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(ComponentClass))
					{
						return true;
					}
				}
			}

			// Move to parent Blueprint
			CurrentBP = CurrentBP->ParentClass ? Cast<UBlueprint>(CurrentBP->ParentClass->ClassGeneratedBy) : nullptr;
		}

		return false;
	}

private:
	FString DisplayName;
	FString Tooltip;
	UClass* ComponentClass;
};

void UComponentFilterExtension::AddFrontEndFilterExtensions(
	TSharedPtr<FFrontendFilterCategory> DefaultCategory,
	TArray<TSharedRef<FFrontendFilter>>& InOutFilterList) const
{
	TSharedPtr<FFrontendFilterCategory> Category = MakeShared<FFrontendFilterCategory>(
		FText::FromString(TEXT("Project Components")),
		FText::FromString(TEXT("Filter by project component types")));

	// Auto-discover components with ContentBrowserFilter metadata
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UActorComponent::StaticClass()) &&
			Class->HasMetaData(TEXT("ContentBrowserFilter")))
		{
			InOutFilterList.Add(MakeShared<FFrontendFilter_Component>(Category, Class));
		}
	}
}
