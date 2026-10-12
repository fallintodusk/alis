// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTextureFixtureDomain.h"
#include "AssetReferencingDomains.h"
#include "AssetReferencingPolicySettings.h"
#include "AssetReferencingPolicySubsystem.h"
#include "Editor.h"
#include "Engine/EngineTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectTextureFixtureDomain, Log, All);

namespace ProjectTextureFixtureDomain
{
namespace
{
constexpr TCHAR DomainName[] = TEXT("ProjectTextureFixture");
constexpr TCHAR Root[] = TEXT("/ProjectTextureTest/");
bool bRegistered = false;
FARPDomainDefinitionByContentRoot OwnedRule;
TWeakObjectPtr<UAssetReferencingPolicySettings> PolicySettings;

void Refresh()
{
	if (GEditor)
	{
		if (auto* Policy = GEditor->GetEditorSubsystem<UAssetReferencingPolicySubsystem>())
		{
			Policy->GetDomainDB()->MarkDirty();
			Policy->GetDomainDB()->UpdateIfNecessary();
		}
	}
}
}

bool Register(FString& OutError)
{
	auto* Settings = GetMutableDefault<UAssetReferencingPolicySettings>();
	if (bRegistered)
	{
		if (Settings->AdditionalDomains.ContainsByPredicate([](const auto& Rule)
			{ return FARPDomainDefinitionByContentRoot::StaticStruct()->CompareScriptStruct(&Rule, &OwnedRule, 0); })) return true;
		OutError = TEXT("Fixture reference policy changed externally; generation refused.");
		return false;
	}
	for (const auto& Existing : Settings->AdditionalDomains)
	{
		if (Existing.DomainName == DomainName || Existing.ContentRoots.ContainsByPredicate(
			[](const FDirectoryPath& Path) { return !Path.Path.IsEmpty() &&
				(Path.Path.StartsWith(Root) || FString(Root).StartsWith(Path.Path)); }) ||
			Existing.SpecificAssets.ContainsByPredicate([](FName Asset) { return Asset.ToString().StartsWith(Root); }))
		{
			OutError = TEXT("Fixture reference domain conflicts with existing policy; no policy was changed.");
			return false;
		}
	}
	FARPDomainDefinitionByContentRoot Rule;
	Rule.DomainName = DomainName;
	Rule.DomainDisplayName = FText::AsCultureInvariant(TEXT("ProjectTexture disposable fixtures"));
	Rule.ErrorMessageIfUsedElsewhere = FText::AsCultureInvariant(TEXT("Production assets must not reference disposable texture fixtures."));
	FDirectoryPath Path;
	Path.Path = Root;
	Rule.ContentRoots.Add(Path);
	// This process-only rule never saves config or grants production domains access.
	OwnedRule = Rule;
	PolicySettings = Settings;
	Settings->AdditionalDomains.Add(MoveTemp(Rule));
	bRegistered = true;
	Refresh();
	return true;
}

void Shutdown()
{
	if (!bRegistered) return;
	// Module teardown can follow UObject destruction; never recreate a CDO there.
	auto* Settings = PolicySettings.Get();
	if (!Settings)
	{
		bRegistered = false;
		return;
	}
	const int32 Removed = Settings->AdditionalDomains.RemoveAll(
		[](const auto& Rule) { return FARPDomainDefinitionByContentRoot::StaticStruct()->CompareScriptStruct(&Rule, &OwnedRule, 0); });
	if (Removed != 1)
	{
		UE_LOG(LogProjectTextureFixtureDomain, Warning, TEXT("Fixture domain changed externally; shutdown preserves that policy."));
	}
	bRegistered = false;
	if (!IsEngineExitRequested()) Refresh();
}
}
