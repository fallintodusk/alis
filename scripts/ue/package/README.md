# Package Scripts

Canonical release packaging entry points for ALIS.

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

### `package_release.ps1`

Packages a Win64 release build through `RunUAT BuildCookRun`.

Defaults:

- reads `UE_PATH` from `scripts/config/ue_path.conf`
- uses `Shipping`
- uses `-nodebuginfo` so staged `.pdb` files do not bloat the distributable package
- uses `-skipencryption` for public release packaging
- disables Asset Registry cache reads for the cook so replaced World Partition
  external actors are discovered from the current content tree
- uses `1700 MiB` split threshold for GitHub-safe archive transport
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
- can sign the exact output directory after archive creation with `-SignRelease`
- with `-SignRelease`, moves `Windows/` and summary files under `debug/` so the release root is upload-ready

Examples:

```powershell
.\scripts\ue\package\package_release.ps1 `
  -OutputDir Saved\PackageRelease\Candidate `
  -RequiredCookMap /ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory
```

```bat
scripts\ue\package\package_release_source.bat -SignRelease -SplitSizeMB 1700
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
- `-SplitSizeMB` archive split size in MiB, default `1700`
- `-SignRelease` runs `sign_release.ps1` against the exact output directory after archive creation
- `-GpgPath`, `-GpgHome`, `-SigningKeyFingerprint`, and `-SkipSignVerify` are forwarded to `sign_release.ps1` when `-SignRelease` is set

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

Generates `SHA256SUMS.txt` and `SHA256SUMS.txt.asc` for a packaged release directory.

Defaults:

- discovers `gpg.exe` from PATH or common Windows install locations
- reuses the ALIS site trust fingerprint `3B9885F0C2D8D927C27FAB58F61A530034CFB5E7`
- signs the root-level release assets in a packaged output directory
- exports the public half of the selected signing key as `ALIS_PUBLIC_KEY.asc`
- includes the exported key in `SHA256SUMS.txt` so every distribution mirror carries the same key asset
- excludes `SHA256SUMS.txt` and `SHA256SUMS.txt.asc` from the manifest; the signature signs the manifest, and the manifest never hashes itself
- writes `INSTALL.txt` into the release directory before hashing so the helper file is covered by the signed manifest
- copies `VERIFY_RELEASE.ps1` and `VERIFY_RELEASE.bat` into the release directory before hashing so advanced users have a self-contained verifier next to the archives
- verifies the detached signature after signing
- writes `sign_release_summary.txt` into the release directory

Examples:

```powershell
.\scripts\ue\package\sign_release.ps1 `
  -ReleaseDir <temp-dir>\ALIS_release_20260310_154307
```

```powershell
.\scripts\ue\package\sign_release.ps1 `
  -ReleaseDir <build-dir> `
  -GpgPath "gpg"
```

Key parameters:

- `-ReleaseDir` packaged release output directory that contains the archive parts
- `-GpgPath` optional explicit path to `gpg.exe`
- `-GpgHome` optional explicit signing keyring directory; it is mandatory when `-SigningKeyFingerprint` differs from the canonical ALIS key
- an explicit GPG home must already exist, must be owned by the calling operation, and is rejected if it resolves to the default user GPG directory or user profile
- the signing script never deletes an explicit GPG home because it contains caller-owned secret-key material; a throwaway test harness must clean only the disposable home it created
- `-SigningKeyFingerprint` override only if the ALIS public trust identity changes
- `-SkipVerify` skips the post-sign `gpg --verify` step

### `sign_release.bat`

Windows wrapper for `sign_release.ps1`.

Example:

```bat
scripts\ue\package\sign_release.bat -ReleaseDir <build-dir>
```

### `verify_release.ps1`

Verifies `SHA256SUMS.txt.asc` and all archive hashes using the ALIS public key
bundled with the release.

Defaults:

- uses explicit `-PublicKeyPath` first, then bundled `ALIS_PUBLIC_KEY.asc`
- falls back to `https://fall.is/assets/security/public-key.asc` only for older releases without a bundled key
- checks fingerprint `3B9885F0C2D8D927C27FAB58F61A530034CFB5E7`
- passes a temporary GPG home explicitly to all public-key operations, so it does not initialize or modify the user's main keyring
- verifies both the detached signature and every asset hash listed in `SHA256SUMS.txt`
- writes `verify_release_summary.txt` into the release directory
- when copied into a release directory, it can infer that directory automatically without `-ReleaseDir`

Examples:

```powershell
.\scripts\ue\package\verify_release.ps1 `
  -ReleaseDir <temp-dir>\ALIS_release_20260310_154307
```

```powershell
.\scripts\ue\package\verify_release.ps1 `
  -ReleaseDir <build-dir> `
  -PublicKeyPath <site-root>\assets\security\public-key.asc `
  -GpgPath "gpg"
```

Key parameters:

- `-ReleaseDir` packaged release output directory that contains archive parts and the hash/signature files
- `-PublicKeyPath` optional local ALIS public key file
- `-BundledPublicKeyName` override only for a legacy/nonstandard release asset name
- `-PublicKeyUrl` override only if the site public key URL changes
- `-ExpectedFingerprint` override only if the ALIS trust identity changes
- `-TempGpgHome` optional explicit temporary verification keyring directory
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
