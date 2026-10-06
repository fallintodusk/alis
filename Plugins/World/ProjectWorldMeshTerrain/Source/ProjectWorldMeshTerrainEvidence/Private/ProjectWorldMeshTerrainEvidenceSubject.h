#pragma once

#include "ProjectWorldEvidenceSubject.h"

/**
 * Mesh Terrain as an editor evidence subject. An editor world never holds the runtime-only
 * compiled sections; it draws transient preview sections that MeshPartition builds asynchronously
 * after the base modifiers load and register. Each loaded base modifier aimed at a Mesh Terrain
 * partition is one expected unit, drawn once its preview section exists and is not hidden in the
 * editor; a partition without any loaded base modifier counts as one undrawn unit.
 */
class FProjectWorldMeshTerrainEvidenceSubject final : public IProjectWorldEvidenceSubject
{
public:
	static FProjectWorldMeshTerrainEvidenceSubject& Get();

	virtual FProjectWorldEvidenceSubjectReport Inspect(const UWorld& World) const override;
};
