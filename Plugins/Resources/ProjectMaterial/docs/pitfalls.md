# ProjectMaterial pitfalls

## Launcher engine SHA-256 platform assertion

**Symptom:** Material generation asserts in `FPlatformMisc::GetSHA256Signature`.

**Root cause:** The installed UE 5.8 Windows launcher build exposes the API but has no
platform implementation for this call.

**Fix:** ProjectMaterial uses its small deterministic SHA-256 implementation for recipe,
package, manifest, and receipt identities.

**Regression test:** `Project.Material.Generation.EngineIdentity` checks the published
empty-input SHA-256 vector.

## Material instance setter reports false after writing

**Symptom:** A generated MIC contains the expected value, but
`UMaterialEditingLibrary` reports failure.

**Root cause:** In installed UE 5.8, the scalar and vector material-instance setter
helpers perform the write but return `false`.

**Fix:** The Editor compiler uses native Editor-only instance setters, then verifies
the parent and typed values after a fresh package load.

**Regression test:** `Project.Material.Generation.InstanceParameters`.

## Existing material reload keeps stale expressions

**Symptom:** Replacement saves, but an in-place `ReloadPackages` verification sees the
old or mixed material expression graph.

**Root cause:** Rebuilding expressions on an already loaded material leaves stale
UObject lifecycle state during same-process package reload.

**Fix:** A host-owned replacement removes only the exact prior output after snapshot,
builds directly at final identity, saves, releases the package, garbage collects, and
loads a fresh object for verification. Failure restores the exact host snapshot.

**Regression tests:** `Project.Material.Generation.ReloadAfterRollback` and the
fresh-process wrapper transaction integration.

## Schema edit leaves the embedded compiler fingerprint stale

**Symptom:** A material schema changes, but the next standard launcher build reports
the target is up to date and the embedded compiler fingerprint does not change.

**Root cause:** The fingerprint implementation reads schemas from `Data/Schemas`, but
UBT does not infer those files as C++ makefile inputs. Reading a file from `Build.cs`
does not by itself make later edits invalidate an existing makefile.

**Fix:** `ProjectMaterialEditor.Build.cs` builds one sorted fingerprint input set,
registers every member through `ExternalDependencies`, and hashes that same set.
Do not maintain separate dependency and hash lists.

**Regression proof:** A schema-only edit invalidates the standard launcher makefile
and changes the fingerprint; restoring it invalidates again and restores the exact
fingerprint. A test-only source edit rebuilds tests without changing the fingerprint.
