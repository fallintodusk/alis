// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVerify.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldVerifyInternal.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldVerifyIdentityDetail
{
	using ProjectWorldVerifyJson::Quote;
	using ProjectWorldVerifyJson::ReadPositiveInteger;
	using ProjectWorldVerifyPaths::ProjectDirectory;
	using ProjectWorldVerifyPaths::RepoRelative;

	struct FDescriptor
	{
		FString Path;
		FString PluginDirectory;
		TSharedPtr<FJsonObject> Root;
	};

	bool LoadDescriptors(TArray<FDescriptor>& OutDescriptors, FString& OutError)
	{
		for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
		{
			if (Plugin->GetType() != EPluginType::Project)
			{
				continue;
			}
			const FString PluginDirectory = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir());
			const FString ProducerDirectory = FPaths::Combine(PluginDirectory, TEXT("Data/Producers"));
			TArray<FString> Files;
			IFileManager::Get().FindFiles(Files, *FPaths::Combine(ProducerDirectory, TEXT("*.json")), true, false);
			Files.Sort();
			for (const FString& File : Files)
			{
				FDescriptor& Descriptor = OutDescriptors.AddDefaulted_GetRef();
				Descriptor.Path = FPaths::Combine(ProducerDirectory, File);
				Descriptor.PluginDirectory = PluginDirectory;
				FString Text;
				if (!FFileHelper::LoadFileToString(Text, *Descriptor.Path) ||
					!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Descriptor.Root) ||
					!Descriptor.Root.IsValid())
				{
					OutError = FString::Printf(TEXT("Producer descriptor is unreadable: %s"), *RepoRelative(Descriptor.Path));
					return false;
				}
			}
		}
		return true;
	}

	FString Number(double Value)
	{
		return FString::Printf(TEXT("%.17g"), Value);
	}

	void Append(FString& Target, const FString& Name, const FString& Value)
	{
		Target += Name + TEXT("=") + Quote(Value) + TEXT("\n");
	}

	void AppendPoints(FString& Target, const FString& Name, const TArray<FVector2D>& Points)
	{
		FString Value = FString::FromInt(Points.Num());
		for (const FVector2D& Point : Points)
		{
			Value += TEXT(";") + Number(Point.X) + TEXT(",") + Number(Point.Y);
		}
		Append(Target, Name, Value);
	}

	void AppendParts(FString& Target, const FString& Name, const TArray<TArray<FVector2D>>& Parts)
	{
		Append(Target, Name + TEXT(".count"), FString::FromInt(Parts.Num()));
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			AppendPoints(Target, FString::Printf(TEXT("%s[%d]"), *Name, Index), Parts[Index]);
		}
	}

	void AppendPolygons(FString& Target, const FString& Name, const TArray<FProjectWorldCanonicalPolygon>& Polygons)
	{
		Append(Target, Name + TEXT(".count"), FString::FromInt(Polygons.Num()));
		for (int32 Index = 0; Index < Polygons.Num(); ++Index)
		{
			const FString Prefix = FString::Printf(TEXT("%s[%d]"), *Name, Index);
			AppendPoints(Target, Prefix + TEXT(".outer"), Polygons[Index].Outer);
			AppendParts(Target, Prefix + TEXT(".holes"), Polygons[Index].Holes);
		}
	}

	void AppendStrings(FString& Target, const FString& Name, const TArray<FString>& Values)
	{
		Append(Target, Name, FString::FromInt(Values.Num()) + TEXT(";") + FString::Join(Values, TEXT(";")));
	}

	// Every field of ProjectWorldCanonicalBundle.h in declaration order; map entries sorted by key.
	FString SerializeBundle(const FProjectWorldCanonicalBundle& Bundle)
	{
		FString Text;
		Append(Text, TEXT("compile_result_path"), Bundle.CompileResultPath);
		Append(Text, TEXT("compile_result_hash"), Bundle.CompileResultHash);
		Append(Text, TEXT("inputs_hash"), Bundle.InputsHash);
		Append(Text, TEXT("profile_id"), Bundle.ProfileId);
		Append(Text, TEXT("world_data_plugin"), Bundle.WorldDataPluginName);
		Append(Text, TEXT("grid_id"), Bundle.GridId);
		Append(Text, TEXT("canonical_crs"), Bundle.CanonicalCrs);
		Append(Text, TEXT("vertical_datum"), Bundle.VerticalDatum);
		Append(Text, TEXT("coordinate_transform"), Bundle.CoordinateTransform);
		AppendPoints(Text, TEXT("lattice_origin"), {Bundle.LatticeOriginMeters});
		AppendPoints(Text, TEXT("engine_georeference_origin"), {Bundle.EngineGeoreferenceOriginMeters});
		AppendPoints(Text, TEXT("sample_spacing"), {Bundle.SampleSpacingMeters});
		Append(Text, TEXT("cell_quads"), FString::Printf(TEXT("%d,%d"), Bundle.CellQuads.X, Bundle.CellQuads.Y));
		Append(Text, TEXT("coordinate_quantization"), Number(Bundle.CoordinateQuantizationMeters));
		Append(Text, TEXT("height_quantization"), Number(Bundle.HeightQuantizationMeters));
		Append(Text, TEXT("height_origin"), Number(Bundle.HeightOriginMeters));
		Append(Text, TEXT("cells.count"), FString::FromInt(Bundle.Cells.Num()));
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			const FString P = TEXT("cell[") + Cell.CellId + TEXT("].");
			const FProjectWorldCanonicalTerrain& Terrain = Cell.Terrain;
			Append(Text, P + TEXT("feature_artifact_hash"), Cell.FeatureArtifactHash);
			Append(Text, P + TEXT("bounds"), Number(Cell.Bounds.X) + TEXT(",") + Number(Cell.Bounds.Y) + TEXT(",") +
				Number(Cell.Bounds.Z) + TEXT(",") + Number(Cell.Bounds.W));
			Append(Text, P + TEXT("xy"), FString::Printf(TEXT("%d,%d"), Cell.CellX, Cell.CellY));
			AppendStrings(Text, P + TEXT("owned"), Cell.OwnedFeatureIds);
			AppendStrings(Text, P + TEXT("referenced"), Cell.ReferencedFeatureIds);
			Append(Text, P + TEXT("terrain.artifact_hash"), Terrain.ArtifactHash);
			Append(Text, P + TEXT("terrain.vertical"), Terrain.VerticalProvenanceId + TEXT(";") + Terrain.VerticalDatum +
				TEXT(";") + Terrain.VerticalConfidence + TEXT(";") + Number(Terrain.VerticalSourceAccuracyMeters) +
				TEXT(";") + Number(Terrain.SamplingQuantizationResidualMeters));
			Append(Text, P + TEXT("terrain.bounds"), Number(Terrain.Bounds.X) + TEXT(",") + Number(Terrain.Bounds.Y) +
				TEXT(",") + Number(Terrain.Bounds.Z) + TEXT(",") + Number(Terrain.Bounds.W));
			AppendPoints(Text, P + TEXT("terrain.spacing"), {Terrain.SampleSpacing});
			Append(Text, P + TEXT("terrain.samples"), FString::Printf(TEXT("%d,%d"), Terrain.SamplesX, Terrain.SamplesY));
			FString Heights = FString::FromInt(Terrain.HeightsMeters.Num());
			for (const double Height : Terrain.HeightsMeters)
			{
				Heights += TEXT(";") + Number(Height);
			}
			Append(Text, P + TEXT("terrain.heights"), Heights);
			Append(Text, P + TEXT("terrain.surface_contract"), Terrain.SurfaceContractId + TEXT(";") +
				FString::FromInt(Terrain.SurfaceContractVersion) + TEXT(";") + Terrain.SurfaceContractHash);
			TArray<FString> Roles;
			for (const FName& Role : Terrain.SurfaceRoles)
			{
				Roles.Add(Role.ToString());
			}
			AppendStrings(Text, P + TEXT("terrain.surface_roles"), Roles);
			TArray<FName> WeightNames;
			Terrain.SurfaceWeights.GetKeys(WeightNames);
			WeightNames.Sort([](const FName& Left, const FName& Right) { return Left.LexicalLess(Right); });
			for (const FName& WeightName : WeightNames)
			{
				FString Weights;
				for (const float Weight : Terrain.SurfaceWeights.FindChecked(WeightName))
				{
					Weights += Number(Weight) + TEXT(";");
				}
				Append(Text, P + TEXT("terrain.weights.") + WeightName.ToString(), Weights);
			}
		}
		TArray<FString> FeatureIds;
		Bundle.Features.GetKeys(FeatureIds);
		FeatureIds.Sort([](const FString& Left, const FString& Right) { return ProjectWorldVerifyJson::Less(Left, Right); });
		Append(Text, TEXT("features.count"), FString::FromInt(FeatureIds.Num()));
		for (const FString& FeatureId : FeatureIds)
		{
			const FProjectWorldCanonicalFeature& Feature = Bundle.Features.FindChecked(FeatureId);
			const FProjectWorldCanonicalWaterSurface& Water = Feature.WaterSurface;
			const FString P = TEXT("feature[") + FeatureId + TEXT("].");
			Append(Text, P + TEXT("id"), Feature.FeatureId);
			Append(Text, P + TEXT("class"), Feature.FeatureClass);
			Append(Text, P + TEXT("owner"), Feature.OwnerCellId);
			Append(Text, P + TEXT("geometry_type"), Feature.GeometryType);
			AppendPoints(Text, P + TEXT("points"), Feature.GeometryPoints);
			AppendParts(Text, P + TEXT("parts"), Feature.GeometryParts);
			AppendPolygons(Text, P + TEXT("polygons"), Feature.GeometryPolygons);
			Append(Text, P + TEXT("representations.count"), FString::FromInt(Feature.Representations.Num()));
			for (int32 Index = 0; Index < Feature.Representations.Num(); ++Index)
			{
				const FProjectWorldCanonicalRepresentation& Representation = Feature.Representations[Index];
				const FString R = FString::Printf(TEXT("%srepresentation[%d]."), *P, Index);
				Append(Text, R + TEXT("cell"), Representation.CellId);
				Append(Text, R + TEXT("kind"), Representation.Kind);
				AppendPoints(Text, R + TEXT("points"), Representation.Points);
				AppendParts(Text, R + TEXT("parts"), Representation.Parts);
				AppendPolygons(Text, R + TEXT("polygons"), Representation.Polygons);
			}
			FString Knots = FString::FromInt(Water.Knots.Num());
			for (const FVector& Knot : Water.Knots)
			{
				Knots += TEXT(";") + Number(Knot.X) + TEXT(",") + Number(Knot.Y) + TEXT(",") + Number(Knot.Z);
			}
			Append(Text, P + TEXT("water"), Water.SurfaceGroupId + TEXT(";") + FString::Join(Water.SurfaceGroupMembers, TEXT(",")) +
				TEXT(";") + Water.Geometry + TEXT(";") + Water.Behavior + TEXT(";") + Water.Role + TEXT(";") +
				Water.FunctionId + TEXT(";") + FString::FromInt(Water.FunctionVersion) + TEXT(";") +
				Number(Water.LevelMeters) + TEXT(";") + Knots + TEXT(";") + (Water.bValid ? TEXT("1") : TEXT("0")));
			Append(Text, P + TEXT("classes"), Feature.RoadClass + TEXT(";") + Feature.VegetationClass + TEXT(";") +
				Feature.FoliageClass + TEXT(";") + Feature.LeafType + TEXT(";") + Feature.LeafCycle + TEXT(";") + Feature.Species);
			Append(Text, P + TEXT("dimensions"), Number(Feature.WidthMeters) + TEXT(";") + Number(Feature.HeightMeters));
			Append(Text, P + TEXT("volumes.count"), FString::FromInt(Feature.BuildingVolumes.Num()));
			for (int32 Index = 0; Index < Feature.BuildingVolumes.Num(); ++Index)
			{
				const FProjectWorldCanonicalBuildingVolume& Volume = Feature.BuildingVolumes[Index];
				const FString V = FString::Printf(TEXT("%svolume[%d]."), *P, Index);
				Append(Text, V + TEXT("id"), Volume.VolumeId + TEXT(";") + Volume.SourceFeatureId + TEXT(";") + Volume.GeometryType);
				AppendPolygons(Text, V + TEXT("polygons"), Volume.GeometryPolygons);
				Append(Text, V + TEXT("heights"), Number(Volume.MinHeightMeters) + TEXT(";") + Number(Volume.HeightMeters));
			}
		}
		Append(Text, TEXT("verified_output_count"), FString::FromInt(Bundle.VerifiedOutputCount));
		return Text;
	}
}

FString FProjectWorldVerifyIdentity::ProducerId() const
{
	return FString::Printf(TEXT("%s:v%d"), *GeneratorId, GeneratorVersion);
}

bool FProjectWorldVerifyIdentity::ForProducer(
	const FString& Verify,
	const FString& GeneratorId,
	int32 GeneratorVersion,
	int32 ComparisonContractVersion,
	FProjectWorldVerifyIdentity& OutIdentity,
	FString& OutError)
{
	using namespace ProjectWorldVerifyIdentityDetail;
	OutIdentity = FProjectWorldVerifyIdentity();
	OutIdentity.Verify = Verify;
	OutIdentity.GeneratorId = GeneratorId;
	OutIdentity.GeneratorVersion = GeneratorVersion;
	OutIdentity.ComparisonContractVersion = ComparisonContractVersion;
	TArray<FDescriptor> Descriptors;
	if (!LoadDescriptors(Descriptors, OutError))
	{
		return false;
	}
	const FDescriptor* Producer = nullptr;
	const FDescriptor* Pipeline = nullptr;
	for (const FDescriptor& Descriptor : Descriptors)
	{
		FString Kind;
		FString Id;
		int32 Version = 0;
		if (!Descriptor.Root->TryGetStringField(TEXT("kind"), Kind))
		{
			OutError = FString::Printf(TEXT("Producer descriptor has no kind: %s"), *RepoRelative(Descriptor.Path));
			return false;
		}
		if (Kind == TEXT("pipeline"))
		{
			if (Pipeline != nullptr)
			{
				OutError = TEXT("More than one realization pipeline descriptor exists.");
				return false;
			}
			Pipeline = &Descriptor;
		}
		else if (Kind == TEXT("producer") && Descriptor.Root->TryGetStringField(TEXT("generator_id"), Id) &&
			Id == GeneratorId && ReadPositiveInteger(Descriptor.Root, TEXT("generator_version"), Version) &&
			Version == GeneratorVersion)
		{
			if (Producer != nullptr)
			{
				OutError = FString::Printf(TEXT("Producer descriptor is duplicated: %s"), *OutIdentity.ProducerId());
				return false;
			}
			Producer = &Descriptor;
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* DataInputs = nullptr;
	if (Producer == nullptr || Pipeline == nullptr ||
		!ReadPositiveInteger(Producer->Root, TEXT("output_revision"), OutIdentity.OutputRevision) ||
		!Producer->Root->TryGetArrayField(TEXT("data_inputs"), DataInputs) || DataInputs == nullptr ||
		!ReadPositiveInteger(Pipeline->Root, TEXT("pipeline_revision"), OutIdentity.PipelineRevision))
	{
		OutError = FString::Printf(
			TEXT("No valid producer descriptor for %s with a realization pipeline descriptor under Plugins/*/*/Data/Producers/."),
			*OutIdentity.ProducerId());
		return false;
	}
	OutIdentity.DescriptorPath = RepoRelative(Producer->Path);
	OutIdentity.BaselinePath = FPaths::Combine(
		Producer->PluginDirectory, TEXT("Data/TestFixtures/Verify"), GeneratorId + TEXT(".verify.json"));
	for (const TSharedPtr<FJsonValue>& Value : *DataInputs)
	{
		FString Path;
		FString Digest;
		if (!Value.IsValid() || !Value->TryGetString(Path) ||
			!FProjectSha256::HashFile(FPaths::Combine(ProjectDirectory(), Path), Digest))
		{
			OutError = FString::Printf(TEXT("Declared data input is unreadable for %s: %s"), *OutIdentity.ProducerId(), *Path);
			return false;
		}
		OutIdentity.FixtureInputs.Emplace(TEXT("data:") + Path, Digest);
	}
	// The same file and raw-byte rule as the production fingerprint; an unreadable engine fails closed.
	const FString BuildVersion = FPaths::Combine(
		FPaths::ConvertRelativePathToFull(FPaths::EngineDir()), TEXT("Build/Build.version"));
	if (!FProjectSha256::HashFile(BuildVersion, OutIdentity.EngineIdentity))
	{
		OutError = FString::Printf(TEXT("Engine build identity is unreadable: %s"), *BuildVersion);
		return false;
	}
	return true;
}

bool FProjectWorldVerifyIdentity::ForPipeline(FProjectWorldVerifyIdentity& OutIdentity, FString& OutError)
{
	using namespace ProjectWorldVerifyIdentityDetail;
	TArray<FDescriptor> Descriptors;
	if (!LoadDescriptors(Descriptors, OutError))
	{
		return false;
	}
	const FDescriptor* Pipeline = nullptr;
	for (const FDescriptor& Descriptor : Descriptors)
	{
		FString Kind;
		if (Descriptor.Root->TryGetStringField(TEXT("kind"), Kind) && Kind == TEXT("pipeline"))
		{
			if (Pipeline != nullptr)
			{
				OutError = TEXT("More than one realization pipeline descriptor exists.");
				return false;
			}
			Pipeline = &Descriptor;
		}
	}
	OutIdentity = FProjectWorldVerifyIdentity();
	if (Pipeline == nullptr || !ReadPositiveInteger(Pipeline->Root, TEXT("pipeline_revision"),
		OutIdentity.PipelineRevision))
	{
		OutError = TEXT("No valid realization pipeline descriptor exists.");
		return false;
	}
	OutIdentity.Verify = TEXT("Project.World.Realization.Verify.Pipeline");
	OutIdentity.GeneratorId = TEXT("realization_pipeline");
	OutIdentity.GeneratorVersion = 1;
	OutIdentity.OutputRevision = 1;
	OutIdentity.ComparisonContractVersion = 1;
	OutIdentity.DescriptorPath = RepoRelative(Pipeline->Path);
	OutIdentity.BaselinePath = FPaths::Combine(Pipeline->PluginDirectory,
		TEXT("Data/TestFixtures/Verify/realization_pipeline.verify.json"));
	const FString BuildVersion = FPaths::Combine(
		FPaths::ConvertRelativePathToFull(FPaths::EngineDir()), TEXT("Build/Build.version"));
	if (!FProjectSha256::HashFile(BuildVersion, OutIdentity.EngineIdentity))
	{
		OutError = TEXT("Engine build identity is unreadable.");
		return false;
	}
	return true;
}

bool FProjectWorldVerifyIdentity::AddFixtureFile(const FString& RepoRelativePath, FString& OutError)
{
	const FString Path = FPaths::Combine(ProjectWorldVerifyIdentityDetail::ProjectDirectory(), RepoRelativePath);
	const FString Extension = FPaths::GetExtension(Path).ToLower();
	FString Digest;
	FString Text;
	const bool bHashed = Extension == TEXT("uasset") || Extension == TEXT("umap") || Extension == TEXT("zip")
		? FProjectSha256::HashFile(Path, Digest)
		: FFileHelper::LoadFileToString(Text, *Path) && FProjectSha256::HashNormalizedText(Text, Digest);
	if (!bHashed)
	{
		OutError = FString::Printf(TEXT("Fixture file is unreadable: %s"), *RepoRelativePath);
		return false;
	}
	FixtureInputs.Emplace(TEXT("file:") + RepoRelativePath, Digest);
	return true;
}

bool FProjectWorldVerifyIdentity::AddFixtureObject(const FString& ObjectPath, FString& OutError)
{
	FString Filename;
	FString Digest;
	if (!FPackageName::TryConvertLongPackageNameToFilename(
			FPackageName::ObjectPathToPackageName(ObjectPath), Filename, FPackageName::GetAssetPackageExtension()) ||
		!FProjectSha256::HashFile(Filename, Digest))
	{
		OutError = FString::Printf(TEXT("Fixture object package is unreadable: %s"), *ObjectPath);
		return false;
	}
	FixtureInputs.Emplace(TEXT("object:") + ObjectPath, Digest);
	return true;
}

void FProjectWorldVerifyIdentity::AddFixtureText(const FString& Name, const FString& Text)
{
	FixtureInputs.Emplace(Name, ProjectWorldProjection::HashText(Text));
}

void FProjectWorldVerifyIdentity::AddFixtureBundle(const FString& Name, const FProjectWorldCanonicalBundle& Bundle)
{
	AddFixtureText(Name, ProjectWorldVerifyIdentityDetail::SerializeBundle(Bundle));
}
