// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"

class AActor;
class UBodySetup;
class UEnum;
class UInstancedStaticMeshComponent;
class UObject;
class UStaticMesh;
class UStaticMeshComponent;
class UWorld;
struct FProjectWorldCanonicalBundle;

/**
 * Producer verify mechanics: an output projection observed after save and reload, a verification
 * identity, and a tracked baseline that ordinary runs only read. Baselines live in the producer's
 * owning plugin at Data/TestFixtures/Verify/<generator_id>.verify.json; production identity never
 * reads them.
 */

/** One observed output object. Kind and Key identify it; Fields hold its owned state as text. */
struct FProjectWorldProjectionRecord
{
	FString Kind;
	FString Key;
	TMap<FString, FString> Fields;
};

/** Records in any insertion order; serialization sorts records by (Kind, Key) and fields by name. */
class PROJECTWORLDEDITOR_API FProjectWorldProjection
{
public:
	/** The returned record stays valid for the projection's lifetime. A duplicate key is an error. */
	FProjectWorldProjectionRecord& Add(const FString& Kind, const FString& Key);
	TArray<const FProjectWorldProjectionRecord*> Sorted() const;
	bool IsValid(FString& OutError) const;
	bool IsEmpty() const { return Records.Num() == 0; }
	/** Compact canonical text of the sorted records; projection_sha256 hashes exactly this text. */
	FString ToCanonicalJson() const;
	FString Sha256() const;

private:
	TIndirectArray<FProjectWorldProjectionRecord> Records;
	TArray<FString> Errors;
};

/** Shared digests and field writers. Quantization is comparison contract v1. */
namespace ProjectWorldProjection
{
	PROJECTWORLDEDITOR_API FString HashText(const FString& Text);
	/** Translations in centimetres to 0.1 mm, quaternion with w >= 0 to 1e-6, scale to 1e-5. */
	PROJECTWORLDEDITOR_API FString Transform(const FTransform& Value);
	/** Sorted, comma-joined tags without the excluded prefixes. */
	PROJECTWORLDEDITOR_API FString Tags(const TArray<FName>& ActorTags, TConstArrayView<FString> ExcludedPrefixes = {});
	/** Object path relative to ArtifactRoot when it lies under it, the full path otherwise, "none" for null. */
	PROJECTWORLDEDITOR_API FString ObjectPath(const UObject* Object, const FString& ArtifactRoot);
	/** Package name relative to ArtifactRoot when it lies under it. */
	PROJECTWORLDEDITOR_API FString RootRelative(const FString& PackageName, const FString& ArtifactRoot);
	PROJECTWORLDEDITOR_API FString MeshDescription(const UStaticMesh& Mesh);
	PROJECTWORLDEDITOR_API FString RenderLod0(const UStaticMesh& Mesh);
	PROJECTWORLDEDITOR_API FString Instances(const UInstancedStaticMeshComponent& Component);
	/** Collision triangles: quantized vertex positions and triangle indices, both in stored order. */
	PROJECTWORLDEDITOR_API FString TriMesh(const TArray<FVector3f>& Vertices, const TArray<int32>& Indices);
	PROJECTWORLDEDITOR_API FString EnumName(const UEnum* Enum, int64 Value);
	PROJECTWORLDEDITOR_API void AddActorCommon(FProjectWorldProjectionRecord& Record, const AActor& Actor);
	/** Mesh reference, mobility, collision, navigation, overlap, relative transform, override materials. */
	PROJECTWORLDEDITOR_API void AddStaticMeshComponent(
		FProjectWorldProjectionRecord& Record,
		const FString& Prefix,
		const UStaticMeshComponent& Component,
		const FString& ArtifactRoot);
	/** Source and render geometry, static materials, the producer-set build settings, and collision. */
	PROJECTWORLDEDITOR_API void AddStaticMesh(
		FProjectWorldProjectionRecord& Record,
		const UStaticMesh& Mesh,
		const FString& ArtifactRoot);
	PROJECTWORLDEDITOR_API void AddBodySetup(FProjectWorldProjectionRecord& Record, const UBodySetup* BodySetup);
}

/**
 * Everything a baseline was recorded against. A difference here permits a re-record; the same
 * identity with different output is a failure that record mode refuses.
 */
struct PROJECTWORLDEDITOR_API FProjectWorldVerifyIdentity
{
	FString Verify;
	FString GeneratorId;
	int32 GeneratorVersion = 0;
	int32 OutputRevision = 0;
	int32 PipelineRevision = 0;
	int32 ComparisonContractVersion = 0;
	FString EngineIdentity;
	TArray<TPair<FString, FString>> FixtureInputs;
	TArray<FString> Embedded;
	/** Absolute baseline file; where the record lives, not part of the identity. */
	FString BaselinePath;
	/** Repository-relative descriptor that owns OutputRevision; named in failure messages. */
	FString DescriptorPath;

	FString ProducerId() const;

	/** Reads the producer and pipeline descriptors, declared data inputs, and the running engine. */
	static bool ForProducer(
		const FString& Verify,
		const FString& GeneratorId,
		int32 GeneratorVersion,
		int32 ComparisonContractVersion,
		FProjectWorldVerifyIdentity& OutIdentity,
		FString& OutError);
	static bool ForPipeline(FProjectWorldVerifyIdentity& OutIdentity, FString& OutError);

	/** Tracked file: text hashed with normalized line endings, .uasset/.umap/.zip as raw bytes. */
	bool AddFixtureFile(const FString& RepoRelativePath, FString& OutError);
	/** Package bytes of an engine or project asset the fixture binds. */
	bool AddFixtureObject(const FString& ObjectPath, FString& OutError);
	void AddFixtureText(const FString& Name, const FString& Text);
	/** Canonical serialization of every bundle field, so a fixture value change moves identity. */
	void AddFixtureBundle(const FString& Name, const FProjectWorldCanonicalBundle& Bundle);
};

/** What a projection function observes: one world and the layer artifact root it reads. */
struct FProjectWorldVerifyScope
{
	UWorld* World = nullptr;
	FString ArtifactRoot;
	const FProjectWorldCanonicalBundle* CanonicalBundle = nullptr;
};

using FProjectWorldProjectFn = TFunction<bool(const FProjectWorldVerifyScope&, FProjectWorldProjection&, FString&)>;

namespace ProjectWorldVerify
{
	/** Projections are keyed by generator id and version; a key registers once. */
	PROJECTWORLDEDITOR_API void RegisterProjection(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		int32 ComparisonContractVersion,
		FProjectWorldProjectFn Projection);
	PROJECTWORLDEDITOR_API bool FindProjection(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		int32& OutComparisonContractVersion,
		FProjectWorldProjectFn& OutProjection);
	PROJECTWORLDEDITOR_API void ForEachProjection(
		TFunctionRef<void(const FString&, int32, int32, const FProjectWorldProjectFn&)> Visitor);

	enum class EMode : uint8
	{
		Compare,
		Record,
		Probe
	};

	/** -ProjectWorldVerifyRecord or -ProjectWorldVerifyProbe=<dir> on the editor command line. */
	PROJECTWORLDEDITOR_API EMode ModeFromCommandLine(FString& OutProbeDir);

	struct FResult
	{
		bool bPassed = false;
		FString Message;
	};

	/** Compare never writes; record writes only for a missing baseline or a changed identity. */
	PROJECTWORLDEDITOR_API FResult CompareOrRecord(
		const FProjectWorldVerifyIdentity& Identity,
		const FProjectWorldProjection& Projection,
		EMode Mode);

	/** Probe writes the projection and package digests for a determinism diff and never a baseline. */
	PROJECTWORLDEDITOR_API FResult WriteProbe(
		const FProjectWorldVerifyIdentity& Identity,
		const FProjectWorldProjection& Projection,
		const TMap<FString, FString>& PackageDigests,
		const FString& ProbeDir);
}

/** Static registration beside each projection function. */
struct PROJECTWORLDEDITOR_API FProjectWorldVerifyProjectionRegistrar
{
	FProjectWorldVerifyProjectionRegistrar(
		const TCHAR* GeneratorId,
		int32 GeneratorVersion,
		int32 ComparisonContractVersion,
		FProjectWorldProjectFn Projection);
};

/**
 * Test-only World Partition map under the /ProjectWorldVerify/ mount (<repo>/tmp/world/realization_verify/Content/).
 * The empty map is saved first so producers take their persistent self-save path; Reload drops every
 * package under the root and loads the map and its actors back from disk.
 */
class PROJECTWORLDEDITOR_API FProjectWorldVerifyWorld
{
public:
	explicit FProjectWorldVerifyWorld(const FString& Producer);
	~FProjectWorldVerifyWorld();

	FProjectWorldVerifyWorld(const FProjectWorldVerifyWorld&) = delete;
	FProjectWorldVerifyWorld& operator=(const FProjectWorldVerifyWorld&) = delete;

	bool IsReady(FString& OutError) const;
	UWorld* GetWorld() const;
	/** /ProjectWorldVerify/<Producer>/, used as the layer artifact root. */
	const FString& GetRoot() const;
	bool SaveOwned(FString& OutError);
	bool Reload(FString& OutError);
	/** Root-relative file path to SHA-256 for every file under the root. */
	TMap<FString, FString> PackageDigests() const;

private:
	struct FImpl;
	TUniquePtr<FImpl> Impl;
};
