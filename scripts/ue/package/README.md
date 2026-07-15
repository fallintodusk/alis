# Package Scripts

Canonical release packaging entry points for ALIS.

## Scripts

### `package_release.ps1`

Packages a Win64 release build through `RunUAT BuildCookRun`.

Defaults:

- reads `UE_PATH` from `scripts/config/ue_path.conf`
- uses `Shipping`
- uses `-nodebuginfo` so staged `.pdb` files do not bloat the distributable package
- uses `-skipencryption` for public release packaging
- uses `1700 MiB` split threshold for GitHub-safe archive transport
- writes a `package_summary.txt` into the output directory
- can sign the exact output directory after archive creation with `-SignRelease`
- with `-SignRelease`, moves `Windows/` and summary files under `debug/` so the release root is upload-ready

Examples:

```powershell
.\scripts\ue\package\package_release.ps1 -EngineRoot <ue-path>
```

```powershell
.\scripts\ue\package\package_release.ps1 `
  -EngineRoot <ue-path> `
  -CreateReleaseArchive `
  -SignRelease `
  -SplitSizeMB 1700
```

Key parameters:

- `-EngineRoot` optional override for packaging with a source engine without changing `scripts/config/ue_path.conf`
- `-OutputDir` explicit archive directory
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

Example:

```bat
scripts\ue\package\package_release.bat -EngineRoot <ue-path> -CreateReleaseArchive
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
- `Source/Alis.Target.cs` forces modular Shipping, so the packaged build contains many DLLs.
- Public release packaging defaults to `-skipencryption`; encrypted startup containers currently fail in modular Shipping with `Failed to find requested encryption key 00000000000000000000000000000000`.
- Current ALIS zip headroom under GitHub's 2 GiB limit is only about `256 MiB`, so split archives are the default safe GitHub transport path.
- Each newly signed release is locally verifiable from its mirrored files. The site trust page remains the authoritative out-of-band fingerprint confirmation.
- Normal users do not need to run `verify_release.ps1`; it exists for advanced authenticity checks.
- For Windows advanced users, the preferred release-side entry point is `VERIFY_RELEASE.bat` inside the packaged release folder.
