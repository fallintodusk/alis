#include "ProjectWorldLayerProducerRegistry.h"

#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Features/IModularFeatures.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

namespace
{
	FString ProducerKey(const FProjectWorldLayerProducerDeclaration& Declaration)
	{
		return FString::Printf(TEXT("%s:v%d"), *Declaration.GeneratorId, Declaration.GeneratorVersion);
	}

	bool IsProducerId(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!((Character >= TEXT('a') && Character <= TEXT('z')) ||
				(Character >= TEXT('0') && Character <= TEXT('9')) || Character == TEXT('_')))
			{
				return false;
			}
		}
		return true;
	}

	bool IsSafeBuilderToken(const FString& Value, bool bArgument)
	{
		if (Value.IsEmpty() || (bArgument && Value[0] != TEXT('-')))
		{
			return false;
		}
		const int32 Start = bArgument ? 1 : 0;
		if (Start >= Value.Len() || !((Value[Start] >= TEXT('A') && Value[Start] <= TEXT('Z')) ||
			(Value[Start] >= TEXT('a') && Value[Start] <= TEXT('z'))))
		{
			return false;
		}
		for (int32 Index = Start + 1; Index < Value.Len(); ++Index)
		{
			const TCHAR Character = Value[Index];
			const bool bAlphanumeric =
				(Character >= TEXT('A') && Character <= TEXT('Z')) ||
				(Character >= TEXT('a') && Character <= TEXT('z')) ||
				(Character >= TEXT('0') && Character <= TEXT('9'));
			const bool bPunctuation = bArgument &&
				(Character == TEXT('_') || Character == TEXT('.') || Character == TEXT(':') ||
				 Character == TEXT('=') || Character == TEXT('[') || Character == TEXT(']') ||
				 Character == TEXT('/') || Character == TEXT('-'));
			if (!bAlphanumeric && Character != TEXT('_') && !bPunctuation)
			{
				return false;
			}
		}
		return true;
	}

	bool IsSafeBuilderSchema(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			const bool bAllowed =
				(Character >= TEXT('A') && Character <= TEXT('Z')) ||
				(Character >= TEXT('a') && Character <= TEXT('z')) ||
				(Character >= TEXT('0') && Character <= TEXT('9')) ||
				Character == TEXT('_') || Character == TEXT('.') ||
				Character == TEXT(':') || Character == TEXT('-');
			if (!bAllowed)
			{
				return false;
			}
		}
		return true;
	}

	bool ValidateDeclaration(const FProjectWorldLayerProducerDeclaration& Declaration, FString& OutError)
	{
		if (!IsProducerId(Declaration.GeneratorId) || Declaration.GeneratorVersion < 1 ||
			(Declaration.LayerKind != EProjectWorldLayerKind::GeneratedGeography &&
			 Declaration.LayerKind != EProjectWorldLayerKind::GeneratedGameplayPlacement) ||
			Declaration.CanonicalSelectors.IsEmpty() || Declaration.SpatialOwnership.IsEmpty() ||
			Declaration.RuntimeMapping.IsEmpty() || Declaration.OwnedActors.OwnedTags.IsEmpty() ||
			(Declaration.DependencyHaloCells.IsSet() &&
				(Declaration.DependencyHaloCells.GetValue() < 0 || Declaration.DependencyHaloCells.GetValue() > 16)))
		{
			OutError = FString::Printf(TEXT("Malformed layer producer declaration: %s"), *ProducerKey(Declaration));
			return false;
		}
		TSet<FString> Selectors;
		for (const FString& Selector : Declaration.CanonicalSelectors)
		{
			if (!IsProducerId(Selector) || Selectors.Contains(Selector))
			{
				OutError = FString::Printf(TEXT("Malformed selector in layer producer declaration: %s"), *ProducerKey(Declaration));
				return false;
			}
			Selectors.Add(Selector);
		}
		TSet<FName> Tags;
		for (const FName Tag : Declaration.OwnedActors.OwnedTags)
		{
			if (Tag.IsNone() || Tags.Contains(Tag))
			{
				OutError = FString::Printf(TEXT("Malformed owned tag in layer producer declaration: %s"), *ProducerKey(Declaration));
				return false;
			}
			Tags.Add(Tag);
		}
		if (Declaration.PostApplyBuilder.IsSet())
		{
			const FProjectWorldPostApplyBuilder& Builder = Declaration.PostApplyBuilder.GetValue();
			bool bSafe = IsSafeBuilderToken(Builder.InventoryCommandlet, false) &&
				IsSafeBuilderToken(Builder.BuilderCommandlet, false) &&
				IsSafeBuilderSchema(Builder.InventorySchema);
			for (const FString& Argument : Builder.InventoryArguments)
			{
				bSafe &= IsSafeBuilderToken(Argument, true);
			}
			for (const FString& Argument : Builder.BuilderArguments)
			{
				bSafe &= IsSafeBuilderToken(Argument, true);
			}
			if (!bSafe)
			{
				OutError = FString::Printf(TEXT("Malformed post-apply builder declaration: %s"), *ProducerKey(Declaration));
				return false;
			}
		}
		return true;
	}
}

bool IProjectWorldLayerProducer::Delete(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldRealizationResult& OutResult, FString& OutError) const
{
	return ProjectWorldLayerProducerRegistry::SweepOwnedActors(
		Context.World, GetDeclaration().OwnedActors, OutResult, OutError);
}

namespace ProjectWorldLayerProducerRegistry
{
	bool GetAll(TArray<const IProjectWorldLayerProducer*>& OutProducers, FString& OutError)
	{
		OutProducers.Reset();
		OutError.Reset();
		IModularFeatures::FScopedLockModularFeatureList Lock;
		TSet<FString> Keys;
		for (IProjectWorldLayerProducer* Producer :
			IModularFeatures::Get().GetModularFeatureImplementations<IProjectWorldLayerProducer>(
				IProjectWorldLayerProducer::GetModularFeatureName()))
		{
			if (Producer == nullptr || !ValidateDeclaration(Producer->GetDeclaration(), OutError))
			{
				if (OutError.IsEmpty())
				{
					OutError = TEXT("A null layer producer was registered.");
				}
				OutProducers.Reset();
				return false;
			}
			const FString Key = ProducerKey(Producer->GetDeclaration());
			if (Keys.Contains(Key))
			{
				OutError = FString::Printf(TEXT("Duplicate layer producer registration: %s"), *Key);
				OutProducers.Reset();
				return false;
			}
			Keys.Add(Key);
			OutProducers.Add(Producer);
		}
		OutProducers.Sort([](const IProjectWorldLayerProducer& Left, const IProjectWorldLayerProducer& Right)
		{
			return ProducerKey(Left.GetDeclaration()) < ProducerKey(Right.GetDeclaration());
		});
		return true;
	}

	const IProjectWorldLayerProducer* Find(const FString& GeneratorId, int32 GeneratorVersion, FString& OutError)
	{
		TArray<const IProjectWorldLayerProducer*> Producers;
		if (!GetAll(Producers, OutError))
		{
			return nullptr;
		}
		for (const IProjectWorldLayerProducer* Producer : Producers)
		{
			const FProjectWorldLayerProducerDeclaration& Declaration = Producer->GetDeclaration();
			if (Declaration.GeneratorId == GeneratorId && Declaration.GeneratorVersion == GeneratorVersion)
			{
				return Producer;
			}
		}
		OutError = FString::Printf(TEXT("Generator pair is not registered: %s:v%d"), *GeneratorId, GeneratorVersion);
		return nullptr;
	}

	bool ValidateLayer(const FProjectWorldRealizationLayer& Layer, FString& OutError)
	{
		const IProjectWorldLayerProducer* Producer = Find(Layer.GeneratorId, Layer.GeneratorVersion, OutError);
		if (Producer == nullptr)
		{
			return false;
		}
		const FProjectWorldLayerProducerDeclaration& Declaration = Producer->GetDeclaration();
		if (Layer.LayerKind != Declaration.LayerKind ||
			Layer.DirtyGranularity != Declaration.DirtyGranularity ||
			Layer.CanonicalSelectors != Declaration.CanonicalSelectors ||
			Layer.SpatialOwnership != Declaration.SpatialOwnership ||
			Layer.RuntimeMapping != Declaration.RuntimeMapping ||
			(Declaration.DependencyHaloCells.IsSet() &&
				Layer.DependencyHaloCells != Declaration.DependencyHaloCells.GetValue()) ||
			(Declaration.PermittedDependencies.IsSet() &&
				Layer.DependsOn != Declaration.PermittedDependencies.GetValue()))
		{
			OutError = FString::Printf(TEXT("Layer tuple does not match producer declaration: %s"),
				*ProducerKey(Declaration));
			return false;
		}
		return Producer->ValidateSettings(Layer.NormalizedSettings, OutError);
	}

	bool FindActorOwner(const AActor& Actor, const IProjectWorldLayerProducer*& OutOwner, FString& OutError)
	{
		OutOwner = nullptr;
		TArray<const IProjectWorldLayerProducer*> Producers;
		if (!GetAll(Producers, OutError))
		{
			return false;
		}
		for (const IProjectWorldLayerProducer* Producer : Producers)
		{
			for (const FName Tag : Producer->GetDeclaration().OwnedActors.OwnedTags)
			{
				if (Actor.Tags.Contains(Tag))
				{
					if (OutOwner != nullptr && OutOwner != Producer)
					{
						OutError = FString::Printf(TEXT("Actor has two layer producer owners: %s"), *Actor.GetPathName());
						OutOwner = nullptr;
						return false;
					}
					OutOwner = Producer;
				}
			}
		}
		return true;
	}

	bool SweepOwnedActors(UWorld& World, const FProjectWorldLayerActorOwnership& OwnedActors,
		FProjectWorldRealizationResult& OutResult, FString& OutError)
	{
		TArray<AActor*> Actors;
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			for (const FName Tag : OwnedActors.OwnedTags)
			{
				if (It->Tags.Contains(Tag))
				{
					Actors.Add(*It);
					break;
				}
			}
		}
		for (AActor* Actor : Actors)
		{
			if (!World.EditorDestroyActor(Actor, true))
			{
				OutError = FString::Printf(TEXT("Cannot delete owned layer actor: %s"), *Actor->GetPathName());
				return false;
			}
			++OutResult.RemovedActorCount;
		}
		return true;
	}

	bool AddPackageArtifact(const FString& PackageName, const FString& Kind,
		const FString& SemanticHash, FProjectWorldLayerInventory& Inventory, FString& OutError)
	{
		const FString Filename = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(
			PackageName, FPackageName::GetAssetPackageExtension()));
		if (!FPaths::FileExists(Filename))
		{
			OutError = FString::Printf(TEXT("Layer package was not saved: %s"), *PackageName);
			return false;
		}
		FString Relative = Filename;
		const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		if (!FPaths::MakePathRelativeTo(Relative, *ProjectRoot) || Relative.StartsWith(TEXT("..")))
		{
			OutError = FString::Printf(TEXT("Layer package escapes the project: %s"), *Filename);
			return false;
		}
		Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
		FString Digest;
		if (!FProjectSha256::HashFile(Filename, Digest))
		{
			OutError = FString::Printf(TEXT("Cannot hash layer package: %s"), *Filename);
			return false;
		}
		Inventory.Artifacts.Add({Relative, Kind, Digest, SemanticHash});
		return true;
	}

	bool ParseExactSettings(const FString& NormalizedSettings,
		TConstArrayView<const TCHAR*> Fields, TSharedPtr<FJsonObject>& OutSettings, FString& OutError)
	{
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(NormalizedSettings), OutSettings) ||
			!OutSettings.IsValid() || OutSettings->Values.Num() != Fields.Num())
		{
			OutError = TEXT("Producer settings must be an object with the exact declared fields.");
			return false;
		}
		for (const TCHAR* Field : Fields)
		{
			if (!OutSettings->HasField(Field))
			{
				OutError = FString::Printf(TEXT("Producer setting is missing: %s"), Field);
				return false;
			}
		}
		return true;
	}
}
