# Package Scripts

Canonical release packaging entry points for ALIS.

## Release Transaction

The operator uses `make release` and `make mirror` for the GitHub release, then
may publish the same signed game to another authorized channel:

```powershell
make release 2.0.0 RELEASE_SIGN=0
# Review tmp/release/v2.0.0/game and tmp/release/v2.0.0/github.
make mirror RTAG=v2.0.0
make release 2.0.0
# Manually upload every file from tmp/release/v2.0.0/github.
make publish itch
```

`make release` owns the complete local release state machine. From the repository alone it
builds and machine-verifies a stale/missing player Candidate, prepares the
matching filtered public source and developer payload, verifies an isolated
payload install/build and both public maps, composes the combined release, and
resumes the same bytes for signing. It performs no GitHub write.

Unsigned mode runs the machine-owned gates and prepares one complete review
directory without approval, GPG, or a GitHub write. After human review and the
source commit, explicit `RTAG` selects the mirror-owned reviewed publication
route; bare `make mirror` remains generic. The final signed invocation is the
explicit Product/terms/rights approval action: it refuses a changed public Git
tree, records approval, signs the exact prepared bytes, and consumer-verifies
both projections. It adds verification sidecars to `game/`, rebuilds only the
Player transport archive in `github/`, then signs the flat GitHub inventory.
The only interactive prompt is GPG itself. See the
[mirror command contract](../../git/mirror/README.md#make-wrapper).

For game-only distribution from an already reviewed unsigned workspace:

```powershell
make release 2.0.0 TARGET=game
make publish itch
```

This path signs and verifies only `game/`. It does not build, resolve a GitHub
branch or tag, refresh the Player archives, or sign `github/`. The recorded
approval is scoped to game distribution and cannot be promoted into the full
GitHub route from that workspace.

`make publish itch` consumes only an existing signed `game/` projection. By
default it selects the highest stable SemVer workspace directly under
`tmp/release/`; if that selected workspace fails validation, it fails closed
instead of falling back to an older version. `PUBLISH_VERSION=X.Y.Z` selects an
exact version. The Makefile owns the ALIS itch destination;
`ITCH_TARGET=user/game` is an explicit override.

The unsigned mode stops at a hash-verified `pending_owner_approval` directory.
It never approves terms, invokes GPG, or creates signing outputs. A later
`make release 2.0.0 RELEASE_SIGN=0` or `make release 2.0.0` treats a valid
`tmp/release/v2.0.0/` as the candidate authority and reuses those exact files.
Later workspace changes do not rebuild or replace it. To intentionally prepare
a fresh candidate, remove that version directory and run the same unsigned
command; there is no separate rebuild flag.
When the exact default output path still contains a matching pending unsigned
manifest from an obsolete layout, unsigned mode removes it and prepares the
current `game/` plus flat `github/` workspace in the same command.
Signed, partially signed, mismatched, and custom-path legacy state remains
unchanged and fails closed.

Release automation stages the version-scoped inputs under
`tmp/release/inputs/v2.0.0/`:

```text
public-source/
developer/
reports/effective-component-manifest.json
reports/developer-dependency-report.json
reports/public-source-privacy.json
reports/public-world-map-load.json
```

The machine-accepted player Candidate is World-owned until release preparation
has validated the complete workspace. Release then recoverably adopts its
`Windows/` contents as `game/`; interrupted adoption is resumable and does not
leave a second reusable package authority. Human Product approval belongs to
the exact combined release workspace, not an intermediate package. No agent or
maintainer manually populates the input tree.

Signed finalization replaces the GitHub projection through one recognized
workspace-local backup. A retry restores that backup when `github/` is missing,
or removes it only after the replacement `github/` passes workspace and release
manifest verification. Multiple backups fail closed for operator inspection.

`prepare_release.ps1` is the one cross-owner preparation entry point for a
combined player and developer release. It consumes and then adopts the
machine-accepted player Candidate, public source/payload manifests, dependency and
privacy reports, component manifest, attribution, and Product terms. It emits
one versioned workspace under project `tmp/`:

```text
tmp/release/vX.Y.Z/
|-- game/                 # Directly runnable; Alis.exe is here.
|-- github/               # Exact flat GitHub Release upload inventory.
|   |-- README.txt
|   |-- ALIS_Win64_vX.Y.Z.zip.001
|   |-- ALIS_DeveloperProject_X.Y.Z_<id>.zip
|   |-- PRODUCT_TERMS.txt
|   |-- INSTALL_ALIS_PLAYER.bat
|   |-- INSTALL_ALIS_DEVELOPER.bat
|   |-- effective-component-manifest.json
|   `-- release_manifest.json
|-- package_summary.txt
`-- release-workspace.json
```

`README.txt` owns the version highlights and complete Player/Developer quick
starts. Manual 7-Zip setup is primary for both roles; the BAT/PowerShell pairs
are optional convenience. Their default installation destinations are siblings
of the download directory, so accepting the prompt leaves the release asset
inventory unchanged. Validation reports stay with the release inputs and are
not public assets. Tagged repository guides and licenses remain in their source
owners instead of being copied into the release.

Player package identity excludes only declared runtime-written state and orders
relative paths by ordinal UTF-8 bytes. The World gate, ProjectCinematic binding,
player archive, and release coordinator must agree byte-for-byte. The focused
cross-owner regression is `scripts/ue/world/test/package_identity.Tests.ps1`.

```powershell
.\scripts\ue\package\prepare_release.ps1 `
  -ReleaseDir tmp\release\v2.0.0 `
  -PublicSourceRoot <exact-public-tag-checkout> `
  -PlayerPackageRoot <accepted-shipping-candidate> `
  -PlayerEvidence <machine-acceptance-composite.json> `
  -DeveloperReleaseDir <developer-payload-directory> `
  -DeveloperPayloadManifest <developer-payload.json> `
  -ComponentManifest <effective-component-manifest.json> `
  -DependencyReport <developer-dependency-report.json> `
  -PrivacyReport <public-source-privacy.json> `
  -MapLoadReport <public-world-map-load.json> `
  -AttributionNotice <developer-payload-directory>\<payload>.notices.json `
  -ProductTerms PRODUCT_TERMS.txt
```

The attribution input is the exact composer-produced notices manifest whose
`payload_id` matches the developer payload. Preparation authenticates every
developer archive part, the joined logical archive, and the complete allowed
developer-directory inventory before copying anything.

The prepared state is intentionally not signable. After reviewing the exact
Product build, Product terms, and rights inputs, the release owner runs
`make release X.Y.Z`. That command boundary records the bounded non-personal
approval through the internal `prepare_release.py approve` transition. There
is no separate confirmation prompt or maintainer command. Only
`ready_for_signature` passes `sign_release.ps1`, and only GPG asks for private
key input. Developers and players run the bundled public verifier; they never
need the private key.

For a tagged public source release, generate
`effective-component-manifest.json` from the clean public tag before signing:

```powershell
python scripts/ue/check/governance/generate_component_manifest.py `
  --tag <tag> `
  --output <release-dir>/effective-component-manifest.json
```

`sign_release.ps1` then includes that root-level manifest in the signed
`SHA256SUMS.txt`.

Focused signing proof:

```powershell
.\scripts\ue\package\tests\test_component_manifest_signing.ps1
```

## Scripts

### `release.ps1`

Coordinates the complete local release state machine over existing owners. It
validates one `X.Y.Z` identity, creates missing owner outputs, prepares or
resumes the exact release directory, keeps unsigned rehearsal private-key-free,
gates approval, invokes the existing signer for `game/` and `github/` in one
finalization pass, and runs the existing consumer verifier against both. It
never writes to a remote.

### `publish_itch.ps1`

Verifies the selected release workspace and its signed `game/` inventory,
then invokes Butler for one exact `user/game:channel`. It refuses pending
remote work, downgrade, or changed same-version content; identical
same-version content is an idempotent success. A successful upload is read back
through Butler before the command reports acceptance. Butler owns credentials
outside the repository. This script never builds, signs, mirrors, archives, or
modifies the release workspace.

Focused publisher proof:

```powershell
.\scripts\ue\package\tests\test_publish_itch.ps1
```

### `prepare_release_inputs.ps1`

Internal release step. It regenerates the public-safe Kazan and Manhattan
projection from current ProjectWorldData canonical authority and the declared
public realization profiles inside a byte-restored generated-content
transaction. It never discovers an old projection under `tmp/release`.
It then runs the existing mirror/developer-payload owner, installs the payload
into an isolated exact-tag checkout, builds the Editor, loads Kazan and
Manhattan, runs the dependency audit, and promotes the complete input tree only
after every check passes. Failed work trees and isolated verification checkouts
are removed on success or failure, and the release entry point clears abandoned
scratch from interrupted runs. Starting a fresh automatic review replaces prior
automatic input trees, so only its promoted public-source repository remains.
That input tree is retained for reviewed mirror publication, then removed after
the signed release passes consumer verification.
`-PublicAssetRoot` is an explicit test/diagnostic seam, not normal release
discovery. Maintainers call `make release X.Y.Z`, not this script.

### `package_release.ps1`

Packages a Win64 release build through `RunUAT BuildCookRun`.

Defaults:

- reads `UE_PATH` from `scripts/config/ue_path.conf`
- uses `Shipping`
- uses `-nodebuginfo` so staged `.pdb` files do not bloat the distributable package
- uses `-skipencryption` for public release packaging
- disables Asset Registry cache reads for the cook so replaced World Partition
  external actors are discovered from the current content tree
- uses `1900 MiB` split threshold for GitHub-safe archive transport
- writes a `package_summary.txt` into the output directory
- accepts `-RequiredCookMap` for validation runs; this preserves the configured
  `MapsToCook` set and adds one explicit map without editing shipping config

### `inspect_iostore.ps1`

Lists every packaged IoStore container through the configured engine's
`UnrealPak`, proves one exact required package entry, and writes a structured
receipt with the largest listed entries. It does not infer payload presence
from Asset Registry text or a cook configuration mention. It rejects any
`ProjectWorldTestData` entry. Listed ProjectWorld production bytes are
observability, not a dependency-closure or without-world delta.

Examples:

```powershell
.\scripts\ue\package\package_release.ps1 `
  -OutputDir Saved\PackageRelease\Candidate `
  -RequiredCookMap /ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory
```

```bat
scripts\ue\package\package_release_source.bat -CreateReleaseArchive -SplitSizeMB 1900
```

The default command resolves the installed launcher engine and is the fast
candidate/package-iteration route. The source wrapper is the explicit public
release route. A non-installed engine is rejected unless that wrapper supplies
`-SourceRelease`, because source UAT also schedules editor and cook-tool targets
and may require a large one-time toolchain rebuild.

Do not alternate launcher and source roots for ordinary iteration. UBT stores
absolute engine paths in project target metadata and response files under the
shared project `Intermediate/` and `Binaries/` trees. Changing roots invalidates
that metadata in both directions. Source Shipping also enables logging through
a unique build environment, so a cold public-release gate can legitimately
compile engine code; that cost does not belong in Kazan candidate iteration.

Focused admission regression:

```powershell
.\scripts\ue\package\tests\test_engine_route_admission.ps1
```

Key parameters:

- `-EngineRoot` optional installed-engine override without changing `scripts/config/ue_path.conf`
- `-SourceRelease` explicit admission for a non-installed source engine; the
  public source wrapper supplies it
- `-OutputDir` explicit archive directory
- `-RequiredCookMap` extends the configured release map set for an exact
  profile-owned package proof; normal release packaging should omit it
- `-SkipBuild` skips the build step but still cooks/packages
- `-IncludeStagedDebugFiles` keeps `.pdb` files in the packaged output
- `-EncryptContent` opt-in override for encrypted containers
- `-CreateReleaseArchive` creates a zip, optionally split into parts
- when a created zip already fits under the requested split threshold, the script keeps a normal `.zip`
- `-SplitSizeMB` archive split size in MiB, default `1900`

### `package_release.bat`

Windows wrapper for `package_release.ps1`.

The packager fails closed unless every file under the UAT staged Windows tree
exists in the archive with the same size. This guards against UAT reporting a
successful archive after copy retries were exhausted. For an exact L0 check of
this verifier without cooking or packaging, run:

```powershell
python -m unittest scripts/ue/package/tests/test_verify_staged_archive.py
```

Example:

```bat
scripts\ue\package\package_release.bat -OutputDir Saved\PackageRelease\Candidate
```

### `sign_release.ps1`

Generates a signed checksum manifest for one release projection.

Defaults:

- discovers `gpg.exe` from PATH or common Windows install locations
- rejects the directory before key access unless `release_manifest.json` is
  hash-clean and `ready_for_signature`
- reuses the ALIS site trust fingerprint `3B9885F0C2D8D927C27FAB58F61A530034CFB5E7`
- `GitHub` mode signs every prepared flat release asset
- `Game` mode signs the recursive runnable tree with safe relative paths and
  writes its public verification material under `game/Verification/`; it first
  proves that `game/` and the approval-owning `github/` are exact siblings in
  one verified release workspace
- exports the public half of the selected signing key as `ALIS_PUBLIC_KEY.asc`
- includes the exported key in `SHA256SUMS.txt` so every distribution mirror carries the same key asset
- excludes `SHA256SUMS.txt` and `SHA256SUMS.txt.asc` from the manifest; the signature signs the manifest, and the manifest never hashes itself
- preserves the already-reviewed `README.txt` unchanged
- copies `VERIFY_RELEASE.ps1` and `VERIFY_RELEASE.bat` into the release directory before hashing so advanced users have a self-contained verifier next to the archives
- requires unique public filenames only for the flat GitHub projection
- verifies the detached signature after signing
- writes a summary only when an explicit path outside the upload directory is
  requested

This is an internal worker of signed `make release X.Y.Z`, not a separate
operator step.

Key parameters:

- `-ReleaseDir` projection root; Game mode accepts only the workspace's exact
  `game/` child
- `-Projection` selects `GitHub` or `Game`; release coordination owns this choice
- `-ApprovalDir` points Game mode at the same workspace's exact `github/` child
- `-GpgPath` optional explicit path to `gpg.exe`
- `-GpgHome` optional explicit signing keyring directory; it is mandatory when `-SigningKeyFingerprint` differs from the canonical ALIS key
- an explicit GPG home must already exist, must be owned by the calling operation, and is rejected if it resolves to the default user GPG directory or user profile
- the signing script never deletes an explicit GPG home because it contains caller-owned secret-key material; a throwaway test harness must clean only the disposable home it created
- `-SigningKeyFingerprint` override only if the ALIS public trust identity changes
- `-SummaryPath` optional signing receipt outside the upload directory
- `-SkipVerify` skips the post-sign `gpg --verify` step

### `sign_release.bat`

Windows wrapper for internal diagnostics around `sign_release.ps1`.

### `verify_release.ps1`

Verifies a signed checksum manifest and its listed files using the ALIS public
key bundled with the selected projection.

Defaults:

- uses explicit `-PublicKeyPath` first, then bundled `ALIS_PUBLIC_KEY.asc`
- falls back to `https://fall.is/assets/security/public-key.asc` only for older releases without a bundled key
- checks fingerprint `3B9885F0C2D8D927C27FAB58F61A530034CFB5E7`
- passes a temporary GPG home explicitly to all public-key operations, so it does not initialize or modify the user's main keyring
- verifies both the detached signature and every asset hash listed in
  `SHA256SUMS.txt` by default
- accepts an explicit required-asset subset for role installers without
  requiring unrelated downloads
- writes a summary only when an explicit path outside the download directory is
  requested
- when copied into a release directory, it can infer that directory automatically without `-ReleaseDir`

Examples:

```powershell
.\scripts\ue\package\verify_release.ps1 `
  -ReleaseDir tmp\release\v2.0.0\github
```

```powershell
.\scripts\ue\package\verify_release.ps1 `
  -ReleaseDir <build-dir> `
  -PublicKeyPath <site-root>\assets\security\public-key.asc `
  -GpgPath "C:\Program Files\Git\usr\bin\gpg.exe"
```

Key parameters:

- `-ReleaseDir` packaged release output directory that contains archive parts and the hash/signature files
- `-PublicKeyPath` optional local ALIS public key file
- `-BundledPublicKeyName` override only for a legacy/nonstandard release asset name
- `-ManifestRelativePath` and `-SignatureRelativePath` locate a projection's
  signed envelope under its root
- `-AllowRelativeAssetPaths` enables recursive game-manifest entries
- `-RequireExactInventory` rejects unsigned extra files in a game projection
- `-PublicKeyUrl` override only if the site public key URL changes
- `-ExpectedFingerprint` override only if the ALIS trust identity changes
- `-TempGpgHome` optional explicit temporary verification keyring directory
- `-RequiredAsset` verifies only named downloaded assets after authenticating
  the complete checksum manifest
- `-SummaryPath` optional verification receipt outside the download directory
- `-KeepTempKeyring` keeps the temporary verification keyring for debugging

### `verify_release.bat`

Windows wrapper for `verify_release.ps1`.

Example:

```bat
scripts\ue\package\verify_release.bat -ReleaseDir <build-dir>
```

## Notes

- For the current ALIS target, source-engine packaging is the verified path.
- source Shipping enables logging with a unique build environment and is a
  deliberate cold public-release gate, not a daily iteration path
- Public release packaging defaults to `-skipencryption`; encrypted startup
  containers currently fail with `Failed to find requested encryption key
  00000000000000000000000000000000`.
- Current ALIS zip headroom under GitHub's 2 GiB limit is only about `256 MiB`, so split archives are the default safe GitHub transport path.
- Each newly signed release is locally verifiable from its mirrored files. The site trust page remains the authoritative out-of-band fingerprint confirmation.
- Normal users do not need to run `verify_release.ps1`; it exists for advanced authenticity checks.
- For Windows advanced users, the preferred release-side entry point is `VERIFY_RELEASE.bat` inside the packaged release folder.
