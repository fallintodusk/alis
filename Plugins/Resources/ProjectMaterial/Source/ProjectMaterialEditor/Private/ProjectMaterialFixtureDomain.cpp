// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialFixtureDomain.h"
#include "AssetReferencingDomains.h"
#include "AssetReferencingPolicySettings.h"
#include "AssetReferencingPolicySubsystem.h"
#include "Editor.h"
#include "Engine/EngineTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectMaterialFixtureDomain, Log, All);

namespace ProjectMaterialFixtureDomain
{
namespace
{
constexpr TCHAR DomainName[] = TEXT("ProjectMaterialFixture");
constexpr TCHAR Root[] = TEXT("/ProjectMaterialTest/");
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
		const int32 Index=Settings->AdditionalDomains.IndexOfByPredicate([](const auto& Rule)
			{ return FARPDomainDefinitionByContentRoot::StaticStruct()->CompareScriptStruct(&Rule, &OwnedRule, 0); });
		if (Index!=INDEX_NONE)
		{
			if (!OwnedRule.CanReferenceTheseDomains.Contains(TEXT("ProjectTextureFixture")) &&
				Settings->AdditionalDomains.ContainsByPredicate([](const auto& Rule){ return Rule.DomainName==TEXT("ProjectTextureFixture"); }))
			{
				OwnedRule.CanReferenceTheseDomains.Add(TEXT("ProjectTextureFixture"));
				Settings->AdditionalDomains[Index]=OwnedRule; Refresh();
			}
			return true;
		}
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
	Rule.DomainDisplayName = FText::AsCultureInvariant(TEXT("ProjectMaterial disposable fixtures"));
	Rule.ErrorMessageIfUsedElsewhere = FText::AsCultureInvariant(TEXT("Production assets must not reference disposable material fixtures."));
	FDirectoryPath Path;
	Path.Path = Root;
	Rule.ContentRoots.Add(Path);
	Rule.CanReferenceTheseDomains.Add(TEXT("ProjectTexture"));
	if (Settings->AdditionalDomains.ContainsByPredicate([](const auto& Existing){ return Existing.DomainName==TEXT("ProjectTextureFixture"); }))
		Rule.CanReferenceTheseDomains.Add(TEXT("ProjectTextureFixture"));
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
		UE_LOG(LogProjectMaterialFixtureDomain, Warning, TEXT("Fixture domain changed externally; shutdown preserves that policy."));
	}
	bRegistered = false;
	if (!IsEngineExitRequested()) Refresh();
}
}
