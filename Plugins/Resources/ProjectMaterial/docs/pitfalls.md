# ProjectMaterial pitfalls

## Material instance setter reports false after writing

**Symptom:** A generated instance contains the expected value, but an
`UMaterialEditingLibrary` setter reports failure.

**Root cause:** Installed UE 5.8 scalar and vector instance helpers may perform the
write while returning `false`.

**Fix:** The compiler uses native Editor-only setters and verifies typed values after
a fresh package load.

**Regression test:** `Project.Material.Generation.SurfaceAssetCompilerContract`.

## Existing material reload keeps stale expressions

**Symptom:** Replacement saves, but same-process reload sees an old or mixed graph.

**Root cause:** Rebuilding expressions on a loaded material can retain stale UObject
lifecycle state.

**Fix:** The host snapshots the exact output, the compiler reconstructs at the final
identity, then saves, unloads, garbage collects, reloads, and verifies. Rejection
restores the host snapshot.

**Regression tests:** the surface generation reload test and host transaction rollback
test.

## Schema edit leaves the compiler fingerprint stale

**Symptom:** A schema changes but a standard launcher build remains up to date.

**Root cause:** UBT does not infer JSON schema files as C++ makefile inputs.

**Fix:** `ProjectMaterialEditor.Build.cs` registers and hashes one sorted compiler
source/schema set. Do not maintain separate dependency and fingerprint lists.

**Regression proof:** a schema-only edit invalidates the standard makefile and changes
the embedded fingerprint; test-only source does not.

## Producer provenance in surface identity forced cross-owner rebuilds

**Symptom:** A ProjectTexture change that kept the pattern contract, even one to its
manifest or cleanup code, regenerated every surface and recompiled their shaders; a Mesh
Terrain adapter change did the same for terrain surfaces.

**Root cause:** Surface semantic identity hashed the pattern's semantic identity, which
carries ProjectTexture's compiler fingerprint, its package bytes, and the whole layout
receipt identity, although the builder reads only the pattern path and contract and the
channel map.

**Fix:** `ComputeArtifactSemanticIdentity` binds the pattern object path and output
contract and the layout ID, version, and `layout_sha256`. The pattern hashes and receipt
identity stay in the record as authenticated provenance. Do not add producer provenance
back to the identity.

**Regression tests:** `Project.Material.Generation.SurfaceIdentityFirewall` and the host
case for a pattern regenerated under the same output contract.

## A failed rollback deleted its own snapshot

**Symptom:** After a rejected run whose restore also failed, for example on an output
file held open by another process, the next run removed the output root and then could
not find the snapshot to restore.

**Root cause:** The host's final cleanup deleted the operation folder holding the snapshot
while the journal that names it survived.

**Fix:** `run_material_generation.ps1` keeps the operation folder while the journal exists
and reports both the rejection and the rollback failure.

**Regression tests:** the host cases for a failed orphan deletion with recovery, in both
the surface and pattern tests.
