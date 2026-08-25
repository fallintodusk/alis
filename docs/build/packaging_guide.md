# Package & Release Guide

Canonical source of truth for ALIS public release packaging.

Use this doc for:

- Win64 release packaging
- mirror transport decisions
- script usage
- size-limit validation
- post-package trust artifacts
- normal user install flow
- advanced verification flow

## Quick Start

Packaging success is not public-distribution approval. Before uploading a
packaged Product, satisfy the
[Packaged Product Legal Compliance](../legal/release_compliance.md) gate.

1. Run `make prepare-tests` followed by `make test-unit-smart BASE=origin/main`.
2. Package and sign through the project Make target:
   ```powershell
   make package
   ```
3. Review `debug/package_summary.txt` and `debug/sign_release_summary.txt` in the output folder.
4. Upload the same custom ALIS release-root files to each approved distribution mirror; do not upload `debug/`.
5. Point users to the ALIS trust page for authoritative fingerprint confirmation.

## Canonical Script

Primary script:

- `scripts/ue/package/package_release.ps1`
- `scripts/ue/package/sign_release.ps1`
- `scripts/ue/package/verify_release.ps1`

Windows wrapper:

- `scripts/ue/package/package_release.bat`
- `scripts/ue/package/sign_release.bat`
- `scripts/ue/package/verify_release.bat`

Script behavior:

- reads `UE_PATH` from `scripts/config/ue_path.conf`
- routes source-engine packaging through `package_release_source.bat`, which
  supplies the explicit source-release admission flag
- uses `-nodebuginfo` by default so staged `.pdb` files do not bloat the distributable payload
- uses `-skipencryption` by default for public release packaging
- writes `package_summary.txt` and, for signed releases, moves it under `debug/`
- can create a release zip
- defaults to `1700 MiB` split threshold for GitHub-safe archive transport
- signing script writes `SHA256SUMS.txt`, `SHA256SUMS.txt.asc`, and `sign_release_summary.txt`
- signed package flow moves `Windows/` and summary files under `debug/` so the release root is the upload set
- signing script exports the selected signing key's public half as `ALIS_PUBLIC_KEY.asc` before hashing
- signing script also writes `INSTALL.txt` so the release folder contains a fast install and verify guide
- signing script also copies `VERIFY_RELEASE.ps1` and `VERIFY_RELEASE.bat` into the release folder so advanced users do not need the repo docs
- manifest hashes the payload, bundled key, install guide, and verification helpers; it deliberately excludes itself and its detached signature
- signing script reuses the ALIS site trust key fingerprint by default
- verification script prefers the bundled public key, verifies its expected fingerprint, builds an explicitly isolated temporary verification keyring, checks the detached signature, and validates all archive hashes without contacting a mirror or site
- verification falls back to the site key URL only for older release sets without `ALIS_PUBLIC_KEY.asc`

Important flags:

- `package_release_source.bat` for the configured `%UE_SOURCE_PATH%`
- `-OutputDir <path>`
- `-CreateReleaseArchive`
- `-SignRelease`
- `-GpgHome <isolated-keyring-path>` for throwaway signing tests
- `-SplitSizeMB 1700`
- `-SkipBuild`
- `-IncludeStagedDebugFiles`
- `-EncryptContent`

## Verified ALIS Baseline

Verified on 2026-03-10 with the project packaging script and source engine path `%UE_SOURCE_PATH%`.

Current verified output:

- release payload: `2,134,597,110` bytes (`1.988 GiB`)
- generated archive part 1: `1,782,579,200` bytes (`1.660 GiB`)
- generated archive part 2: `96,143,799` bytes (`0.090 GiB`)
- largest packaged file: `pakchunk10-Windows.ucas` = `851,966,304` bytes (`0.793 GiB`)
- files `>= 2 GiB`: `0`
- public release package was generated with `-skipencryption`
- equivalent single-zip headroom under GitHub's `2 GiB` limit: `268,760,662` bytes (`256.31 MiB`, `12.52%`)

Meaning:

- current ALIS public release technically fits as one zip asset
- that margin is too small to treat single-zip transport as stable
- split archives should be the default GitHub release transport for ALIS

## GitHub Release Constraints

Verified constraints:

- each release asset must be under 2 GiB
- up to 1000 assets per release
- GitHub docs state no total release size limit and no bandwidth limit for release assets
- usage is still subject to GitHub Acceptable Use controls

Practical rule for ALIS:

- prefer split archives as the default GitHub transport
- only use one zip when the generated archive remains comfortably below the split threshold
- do not upload hundreds of loose packaged files unless there is a strong reason

## ALIS Packaging Requirements

Current project requirements for GitHub-compatible content containers:

- `Config/DefaultGame.ini`
  - `BuildConfiguration=PPBC_Shipping`
  - `ForDistribution=True`
  - `IncludeDebugFiles=False`
  - `bUseIoStore=True`
  - `bGenerateChunks=True`
  - no `MaxChunkSize` override
- boot map references must point to `L_OrchestratorBoot`
- Asset Manager chunk rules must keep content out of one oversized `pakchunk0`
- GitHub asset-size handling is done after packaging via split 7-Zip archives, not UE chunk-size limits

Important implementation note:

- `Source/Alis.Target.cs` uses UBT's `bIsEngineInstalled` as the engine-kind authority
- installed-engine game targets and all Shipping targets are monolithic because installed engines lack modular `UnrealGame` import libraries
- source-engine Development/DebugGame targets stay modular for CDN hot-loading iteration
- public release packaging must currently stay unencrypted because encrypted startup containers fail before the game module registers the key

## Recommended Public Release Flow

1. Package and sign with `make package`.
2. Review `debug/package_summary.txt` and `debug/sign_release_summary.txt`.
3. Upload the same custom ALIS release-root files to every approved mirror; do not upload `debug/`.
4. Only override the split threshold if you have a specific transport reason.
5. Upload optional torrent file.
6. Point users to the canonical ALIS trust page for fingerprint confirmation.

## User Experience

There are two valid user flows. Do not force advanced verification on every user.

### Normal Install

This is the default public-user path:

1. Download all archive parts for one release from one approved mirror.
2. Put all parts in one folder.
3. Install 7-Zip if needed.
4. Extract the first archive part.
5. Run `Alis.exe`.

For current ALIS transport, normal users do not need to import keys, run GPG, or
manually compare hashes unless they want authenticity guarantees.

### Advanced Verify

This is the security-conscious path:

1. Download all archive parts.
2. Download `SHA256SUMS.txt`, `SHA256SUMS.txt.asc`, and `ALIS_PUBLIC_KEY.asc` from the same mirror.
3. Confirm the bundled key fingerprint against the site trust page or another trusted record.
4. Verify the detached signature on `SHA256SUMS.txt`.
5. Verify all archive hashes against `SHA256SUMS.txt`.
6. Extract the first archive part only after verification succeeds.

Recommended command:

```powershell
.\scripts\ue\package\verify_release.ps1 -ReleaseDir <release_dir>
```

Preferred release-side Windows command:

```powershell
.\VERIFY_RELEASE.bat
```

### Why Verification Exists

Verification is optional for convenience, but useful for:

- proving the release really came from ALIS
- detecting accidental re-uploads or corrupted mirrors
- detecting tampered GitHub or mirror assets
- providing transparent public trust for advanced users, journalists, and testers

Practical rule:

- normal users -> install
- advanced users -> verify, then install

Recommended release asset set:

- `ALIS_Win64_<version>.zip` or split zip parts
- `INSTALL.txt`
- `ALIS_PUBLIC_KEY.asc`
- `VERIFY_RELEASE.ps1`
- `VERIFY_RELEASE.bat`
- `SHA256SUMS.txt`
- `SHA256SUMS.txt.asc`
- optional `.torrent`
- optional `release_notes.md` or extraction instructions
- optional `HOW_TO_INSTALL.txt`

Local-only release debug set:

- `debug/Windows/`
- `debug/package_summary.txt`
- `debug/sign_release_summary.txt`
- `debug/verify_release_summary.txt`

Manifest protocol invariant:

- `SHA256SUMS.txt` covers every uploaded payload/helper asset, including `ALIS_PUBLIC_KEY.asc`
- `SHA256SUMS.txt` never contains an entry for itself or `SHA256SUMS.txt.asc`
- `SHA256SUMS.txt.asc` is the detached signature over the exact bytes of `SHA256SUMS.txt`

## Published Release Immutability

A published release tag and its asset set are historical records. Do not silently replace an
archive, manifest, signature, verifier, install guide, or key asset under an already published tag.
This section governs a correction after the release owner decides one is needed; it does not by
itself require republishing an otherwise supported historical release.

If release verification or distribution metadata needs correction:

1. Create a new corrective release identity through the release owner.
2. Leave the previous tag and assets available as historical artifacts.
3. State whether the game archive bytes are unchanged or the game was rebuilt.
4. If archive bytes are reused, confirm their hashes are byte-for-byte identical to the prior
   release; otherwise regenerate all archive parts and the release allowlist.
5. Generate a new manifest and detached signature for the new release set.
6. Publish that exact new custom-asset set to each approved mirror.

Do not describe re-signing as a replacement of assets on a published release. A new manifest or
signature changes the authenticated release record even when the game archive bytes are unchanged.

Signing tests with a non-canonical fingerprint must pass `-GpgHome` and use a disposable keyring
created by the test operation. The signing script rejects a non-canonical key when no explicit GPG
home is provided, rejects an explicit home that resolves to the normal user GPG directory or user
profile, and does not create or delete caller-owned secret-key directories. The calling test cleans
only the disposable home it created. Verification always passes a temporary GPG home to public-key
operations and removes its owned temporary directory on success or failure.

Mirror parity covers maintainer-uploaded custom ALIS assets. GitHub-generated source-code ZIP and TAR
archives are platform conveniences, not ALIS release assets, and are outside the mirror-equivalence
check.

## Trust Source Of Truth

Use the same ALIS public signing identity already published on the site.

Canonical public trust endpoints:

- trust page: `https://fall.is/trust/`
- public key: `https://fall.is/assets/security/public-key.asc`
- fingerprint: `3B98 85F0 C2D8 D927 C27F AB58 F61A 5300 34CF B5E7`

Verified against the site repo on 2026-03-10:

- `site/_pages/about.md`
- `site/assets/security/public-key.asc`

Release signing rule:

- sign `SHA256SUMS.txt` with this same key
- export that signing key's public half as `ALIS_PUBLIC_KEY.asc` into every release set
- do not introduce a separate packaging-only trust identity
- in release notes, point users to the site trust page for out-of-band fingerprint confirmation

Release notes should include:

- trust page: `https://fall.is/trust/`
- public key: `https://fall.is/assets/security/public-key.asc`
- fingerprint: `3B98 85F0 C2D8 D927 C27F AB58 F61A 5300 34CF B5E7`
- short extraction note when split archives are used:
  - download all parts to one folder
  - extract the first part with 7-Zip
- short install note for normal users:
  - verification is optional
  - `verify_release.ps1` is available for advanced users

## Commands

Package release:

```powershell
make package
```

Sign release artifacts:

```powershell
.\scripts\ue\package\sign_release.ps1 `
  -ReleaseDir <temp-dir>\ALIS_release_20260310_154307
```

Verify release artifacts:

```powershell
.\scripts\ue\package\verify_release.ps1 `
  -ReleaseDir <temp-dir>\ALIS_release_20260310_154307
```

Package into an explicit directory:

```powershell
.\scripts\ue\package\package_release_source.bat `
  -OutputDir Saved\PackageRelease\MyBuild
```

Package and sign in one flow:

```powershell
.\scripts\ue\package\package_release_source.bat `
  -OutputDir Saved\PackageRelease\MyBuild `
  -SignRelease
```

Package, sign, and verify in one flow:

```powershell
.\scripts\ue\package\package_release_source.bat `
  -OutputDir Saved\PackageRelease\MyBuild `
  -SignRelease

.\scripts\ue\package\verify_release.ps1 `
  -ReleaseDir Saved\PackageRelease\MyBuild
```

Force split archives:

```powershell
.\scripts\ue\package\package_release_source.bat `
  -SplitSizeMB 1700
```

Manual hash manifest:

```powershell
Get-ChildItem .\ALIS_Win64_* |
  Get-FileHash -Algorithm SHA256 |
  ForEach-Object { "{0} *{1}" -f $_.Hash.ToLower(), $_.Path.Substring($_.Path.LastIndexOf('\') + 1) } |
  Set-Content -Encoding Ascii .\SHA256SUMS.txt
```

Manual detached signature:

```powershell
gpg --armor --detach-sign --local-user 3B9885F0C2D8D927C27FAB58F61A530034CFB5E7 .\SHA256SUMS.txt
```

Manual verify signature:

```powershell
gpg --verify .\SHA256SUMS.txt.asc .\SHA256SUMS.txt
```

Import the published ALIS key first if needed:

```powershell
Invoke-WebRequest https://fall.is/assets/security/public-key.asc -OutFile .\public-key.asc
gpg --import .\public-key.asc
```

Release page verification text:

```text
Verify the ALIS release signature with the public key published at:
https://fall.is/trust/
https://fall.is/assets/security/public-key.asc

Fingerprint:
3B98 85F0 C2D8 D927 C27F AB58 F61A 5300 34CF B5E7
```

Normal install release-page text:

```text
Normal install:
1. Download all release parts to one folder.
2. Extract the first part with 7-Zip.
3. Run Alis.exe.

Advanced:
Use the published ALIS public key and verify SHA256SUMS.txt.asc before extraction.
```

Generated release helper:

- `INSTALL.txt` is written into the release directory by `sign_release.ps1`
- it is included in `SHA256SUMS.txt` and covered by the detached signature
- `ALIS_PUBLIC_KEY.asc` is exported from the selected signing key into the release directory
- `VERIFY_RELEASE.ps1` and `VERIFY_RELEASE.bat` are also written into the release directory by `sign_release.ps1`
- the key, install guide, and both helper scripts are included in `SHA256SUMS.txt` and covered by the detached signature
- `SHA256SUMS.txt` and `SHA256SUMS.txt.asc` are present in the release root but are not entries in the manifest

## Validation Checklist

- the canonical packaging wrapper scopes Common Zen lifetime and bracketed
  IPv6 proxy bypass to UAT, then restores the caller environment; no Windows
  service is installed
- pre-package guards passed (run automatically by the canonical script):
  - `scripts/ue/check/config/validate_shipping_ini.py`
  - `scripts/ue/check/data/validate_all.py`
  - `scripts/ue/check/governance/validate_plugin_data_staging.py` (catches plugins that read JSON via `FProjectPaths::GetPluginDataDir` but forgot to declare `RuntimeDependencies` for `Data/` in `.Build.cs` -- silent regression class, see [docs/agents/canonical.md section 8.5](../agents/canonical.md))
- post-package smoke check passed (`validate_plugin_data_staging.py --archive-root <output>`) -- confirms cook actually copied runtime-read JSONs into the staged build
- package build completed through the project script
- `debug/package_summary.txt` exists
- `debug/sign_release_summary.txt` exists
- `debug/verify_release_summary.txt` exists when advanced verification was run
- largest file is below 2 GiB
- release archive parts were generated and stay below the GitHub limit
- no staged `.pdb` files are being shipped unless explicitly intended
- package boots on a clean Windows machine
- hashes and signature verify correctly
- verification succeeds from the downloaded release directory with network access disabled
- bundled `ALIS_PUBLIC_KEY.asc` matches the expected fingerprint
- manifest contains no self-hash or detached-signature entry
- published tags and their existing assets were not mutated; corrections use a new release identity
- release notes point to `https://fall.is/trust/` and `https://fall.is/assets/security/public-key.asc`

## Troubleshooting

| Symptom | Cause | Fix |
| --- | --- | --- |
| Packaging fails with `AutomationTool exiting with ExitCode=5` | Stale staging or cook state | Delete `Saved/StagedBuilds/` and rerun the packaging script. |
| Packaging fails because project modules cannot load in cook | engine/editor binaries were built against a different UE install | Build/package with the same engine root, or rebuild `AlisEditor` with the chosen engine first. |
| Zen is alive but IoStore staging reports it unavailable | Common Zen inherited the cook lifetime, or .NET routed `[::1]` through a proxy | use the canonical packaging wrapper; it scopes the supported lifetime and proxy-bypass overrides to UAT. |
| One content container exceeds 2 GiB | chunking rules collapsed too much content into one chunk | verify Asset Manager rules and Primary Asset Label ownership; GitHub transport limits are handled by split release archives, not `MaxChunkSize`. |
| Release folder is huge because of debug files | staged `.pdb` files were included | keep `-nodebuginfo` enabled. |
| Missing DLC chunks | incorrect Asset Manager chunk rules or Primary Asset Labels | verify `Config/DefaultGame.ini` chunk rules or project label assets assign expected chunk IDs. |
| Packaged game crashes with `Failed to find requested encryption key 00000000000000000000000000000000` | encrypted startup containers cannot resolve the current runtime key | use the release script default `-skipencryption`, or only enable `-EncryptContent` after implementing and validating a runtime key-loading path. |
| User cannot extract split release parts | archive parts were downloaded into different folders or Windows Explorer was used directly | place all parts in one folder and extract the first part with 7-Zip. |
| `verify_release.ps1` fails before signature check | `gpg.exe` is missing, the bundled key is absent, or an old release cannot reach its fallback key URL | install GnuPG or Git for Windows; for old releases pass `-PublicKeyPath` explicitly. |
| `verify_release.ps1` reports fingerprint mismatch | wrong or tampered public key file was used | compare `ALIS_PUBLIC_KEY.asc` against the fingerprint on the site trust page. |

## References

- GitHub Releases limits and asset model: <https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases>
- GitHub large files guidance: <https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-large-files-on-github>
- GitHub Acceptable Use: <https://docs.github.com/en/site-policy/acceptable-use-policies/github-acceptable-use-policies>
- GitHub Pages limits: <https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits>
- Git LFS and Pages note: <https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-git-large-file-storage>
- Unreal chunking overview: <https://dev.epicgames.com/documentation/en-us/unreal-engine/cooking-content-and-creating-chunks-in-unreal-engine>
- Unreal preparing assets for chunking: <https://dev.epicgames.com/documentation/en-us/unreal-engine/preparing-assets-for-chunking-in-unreal-engine>
- Unreal packaging settings API: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Developer/DeveloperToolSettings/UProjectPackagingSettings>
- Unreal packaging settings page: <https://dev.epicgames.com/documentation/en-us/unreal-engine/project-section-of-the-unreal-engine-project-settings>
