# Package & Release Guide

Canonical source of truth for ALIS public release packaging.

Use this doc for:

- Windows and Linux x86-64 release packaging
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
The release operator uses `make release` and `make mirror` for GitHub. The
optional `make publish itch` command distributes the already signed game;
`RTAG` remains the explicit switch from generic mirroring to reviewed source
publication.

1. Start or resume the automated local release without signing:
   ```powershell
   make release 2.0.0 RELEASE_SIGN=0
   ```
   The command runs all machine-owned gates without a prompt and creates the
   complete unsigned review directory. A valid existing
   `tmp/release/v2.0.0/` is the candidate authority and is verified and reused
   without rebuilding, even when the workspace later changes. Remove that
   version directory before this command only when intentionally requesting a
   fresh candidate. If the exact default output contains a
   matching pending unsigned release from an obsolete layout, the command
   removes it and continues from fresh output.
2. Review `tmp/release/v2.0.0/game/` as the directly runnable game and
   `tmp/release/v2.0.0/github/` as the exact flat GitHub upload inventory.
   Machine reports remain outside `github/`.
3. From native Windows, run the explicit reviewed release publication command:
   ```powershell
   make mirror RTAG=v2.0.0
   ```
   It publishes the exact reviewed public-source commit and matching tag through
   the maintainer's SSH identity, then requires remote `main` and `v2.0.0` to
   resolve to that commit. An existing identical remote tag is an idempotent
   success; a conflicting tag fails closed and is never moved.
4. Finalize the same workspace with `make release 2.0.0`. That command is the
   explicit Product/terms/rights approval action. It refuses a changed public
   Git tree, adds signed verification material to `game/`, rebuilds only the
   affected Player archive, signs the flat `github/` inventory, verifies both
   projections, and passes the only interactive prompt directly to GPG.
5. Ensure GitHub release immutability is enabled for the repository. Create a
   draft release, upload every signed file from `tmp/release/v2.0.0/github/`, compare
   each uploaded asset digest covered by `SHA256SUMS.txt`, and then publish the
   draft. Neither command uploads or publishes release assets.
6. Publish the signed Windows game projection to the ALIS itch project:
   ```powershell
   make publish itch
   ```
   Add `PUBLISH_VERSION=2.0.0` only to select an older non-default workspace.
   `ITCH_TARGET=user/game` overrides the project default when intentionally
   publishing to another destination.
   This command does not build, sign, mirror, or modify the retained release.

For an itch-only release, the reviewed local candidate does not depend on the
GitHub source tag. After steps 1 and 2, sign only the retained game set:

```powershell
make release 2.0.0 TARGET=game
make publish itch
```

`TARGET=game` requires the existing reviewed workspace, never builds, and does
not read a GitHub remote or tag. It records approval for game distribution,
signs and verifies every required game platform projection, and does not
refresh Player archives or sign the GitHub projection. Its local release
manifest records that bounded approval; it cannot later become a full GitHub
release from the same workspace. A full release requires a newly prepared and
reviewed workspace.

Linux release assets are GitHub-only. `make publish itch` never selects or
creates a Linux channel.

`make package` remains the lower-level Shipping package/archive command. It
does not approve or sign a release.

For 2.1.0 and later, the same commands own one multi-platform workspace:

```text
tmp/release/vX.Y.Z/
|-- game/
|   |-- windows-x86_64/
|   `-- linux-x86_64/
|-- github/
`-- release-workspace.json
```

The Windows game remains the World-owned product/performance Candidate. The
Linux package is cross-compiled with the configured v26 toolchain. Linux release
eligibility remains parked until the exact unsigned TAR passes rendered runtime
acceptance on a native Ubuntu 22.04 x86-64 GPU host. Acceptance copies the exact
archive to that host's local Linux filesystem before verification, extraction,
and execution; shared storage is transport only. WSL may prove archive
transport, ext4 execute modes, shell-native verification, and non-rendering
diagnostics, but it is not the final Vulkan acceptance authority. The receipt
binds the normalized runtime payload tree. Signing may add only the declared
verification envelope; final refresh rejects any other payload change and the
GitHub signature binds the final downloadable archive parts. If the declared
Vulkan device or either required product route is unavailable, release
preparation fails without creating the workspace.

Multi-platform releases write workspace v2 and manifest v4. Historical 2.0.0
workspace v1 and manifest v3 remain readable and verifiable, and are never
migrated in place.

## Canonical Script

Primary script:

- `scripts/ue/package/release.ps1`
- `scripts/ue/package/package_release.ps1`
- `scripts/ue/package/sign_release.ps1`
- `scripts/ue/package/verify_release.ps1`
- `scripts/ue/package/verify_release.sh`
- `scripts/ue/package/publish_itch.ps1`

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
- writes `package_summary.txt`
- can create a release zip
- defaults to a `1900 MiB` split threshold, leaving `148 MiB` below GitHub's
  `2 GiB` per-asset limit while maximizing payload capacity per part; reduce it
  only for a demonstrated transport constraint
- signing writes recursive verification under each required game platform and
  flat GitHub verification under `github/`
- signing script exports the selected signing key's public half as `ALIS_PUBLIC_KEY.asc` before hashing
- unsigned preparation writes one self-contained `README.txt` for both roles
  before owner review
- signing copies BAT/PowerShell and shell verifiers into the flat release so
  advanced Windows and Linux users do not need repository scripts
- unsigned manifest hashes every flat public asset; the signed checksum later
  adds the bundled key and verification helpers while excluding itself and its
  detached signature
- signing script reuses the ALIS site trust key fingerprint by default
- verification script prefers the bundled public key, verifies its expected fingerprint, builds an explicitly isolated temporary verification keyring, checks the detached signature, and validates all archive hashes without contacting a mirror or site
- verification falls back to the site key URL only for older release sets without `ALIS_PUBLIC_KEY.asc`

Important flags:

- `package_release_source.bat` for the configured `%UE_SOURCE_PATH%`
- `-OutputDir <path>`
- `-CreateReleaseArchive`
- `-SplitSizeMB 1900`
- `-SkipBuild`
- `-IncludeStagedDebugFiles`
- `-EncryptContent`

## Verified Win64 Shipping Package Baseline

The documented installed-engine Win64 Shipping route, a full cook, and the
packaged GrandPa fixed-view gate establish the current package baseline:

- total packaged payload: `2,038,005,138` bytes (`1.898 GiB`)
- largest packaged file: `pakchunk10-Windows.ucas` = `1,248,397,392`
  bytes (`1.163 GiB`)
- listed IoStore entries: `10,402`
- ProjectWorld entries: `2,425`
- ProjectWorld listed bytes: `285,928,100` (`272.68 MiB`)
- ProjectWorldTestData entries: `0`
- generated ObjectDefinitions present: `69` of `69`
- generated capability cook references present: `8` of `8`
- MetaHuman authoring entries: `0`
- Mutable sample entries: `0`
- package generated with `-skipencryption`

This is `3,011,476,101` bytes (`2.805 GiB`, `59.64%`) smaller than the
`5,049,481,239` byte pre-cleanup baseline. The reduction comes from cook
ownership rather than compression tuning:

- ProjectObject is no longer an unconditional whole-plugin cook root
- generated ObjectDefinitions carry their validated soft-reference closure
- the Hero uses fixed project-owned body/head assets instead of Mutable sample content
- MetaHuman authoring model roots are explicitly excluded from Shipping

MetaHuman Core Tech remains available because current GrandPa runtime content
retains its transitive classes and assets. The cook exclusion is limited to
authoring roots; it does not remove the runtime MetaHuman owner.

`FObjectCapabilityPropertyResolver` derives capability cook references from
reflected soft-object and soft-class properties. The packaging route exports
that projection from the generated assets, and `inspect_iostore.ps1` proves the
projected paths are present without reinterpreting arbitrary JSON strings. The
inspection also rejects MetaHuman authoring or Mutable sample entries and proves
every generated ObjectDefinition is present.

The internal IoStore container size is not a GitHub asset-size failure. Public
transport remains the split release archive owned by `make release`.

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
- the accepted release and reviewed-mirror route runs through native Windows
  PowerShell/Make; WSL mirror parity is not part of the 2.0.0 acceptance path
- during release-only public World projection, close Unreal Editor and do not
  run another World/generated-content operation; successful preparation must
  restore the private generated tree byte-for-byte

## Recommended Public Release Flow

1. Run `make release 2.0.0 RELEASE_SIGN=0`; it creates all machine-owned inputs.
2. Run and inspect each exact unsigned platform below `game/`, then review the
   exact flat `github/` projection including its Developer payload, README, and
   terms.
3. Commit/freeze the reviewed tracked source without changing the reviewed
   public tree. Any public content drift returns to step 1.
4. From native Windows, run `make mirror RTAG=v2.0.0` after the explicit
   remote-write decision. It publishes and reads back the already-reviewed
   source commit on `main` and the matching immutable tag.
5. Run `make release 2.0.0`. The command invocation approves the exact prepared
   Product, terms, and rights, proves the final public tree is unchanged, signs
   both projections in one finalization pass, and consumer-verifies both. Only
   GPG prompts.
6. Upload only the files under `github/` after explicit remote authorization.

## User Experience

There are two valid user flows. Do not force advanced verification on every user.

### Player

This is the default public-user path:

1. Keep every numbered Player ZIP part together.
2. With 7-Zip, extract only the `.zip.001` file.
3. Run `Alis.exe` from the extracted folder.

The optional `INSTALL_ALIS_PLAYER.bat` convenience path embeds the accepted
part sizes and SHA-256 hashes, joins split parts, and extracts the package with
Windows components. It performs no network, administrator, or registry action.
Publisher authenticity is established separately by `VERIFY_RELEASE.bat`.

### Developer or Contributor

1. Clone the exact release tag.
2. Extract the matching Developer payload ZIP into that checkout with 7-Zip.
3. Configure the supported launcher Unreal Engine and build the project.

The optional `INSTALL_ALIS_DEVELOPER.bat` convenience path clones the exact
public tag and delegates payload installation to the trusted checkout-local
installer. It verifies only the downloaded Developer payload subset against the
signed release checksum manifest, so Player archives are not required. It does
not install Unreal Engine, Visual Studio, Git, or other machine-wide
prerequisites.

For current ALIS transport, normal users do not need to import keys, run GPG, or
manually compare hashes unless they want authenticity guarantees.

### Advanced Verify

This is the security-conscious path:

1. Install GnuPG or Git for Windows. Verification requires `gpg` and `gpgv`.
2. Download `VERIFY_RELEASE.bat`, `VERIFY_RELEASE.ps1`, `SHA256SUMS.txt`,
   `SHA256SUMS.txt.asc`, and `ALIS_PUBLIC_KEY.asc` from one release.
3. Download every release asset named by `SHA256SUMS.txt` into that same folder.
4. Confirm the bundled key fingerprint against the
   [ALIS trust page](https://fall.is/trust/) or another trusted record.
5. Run the verifier from that folder. It authenticates the checksum manifest
   and then verifies every file named by it.

Windows:

```powershell
.\VERIFY_RELEASE.bat
```

Repository operators may run the same implementation directly:

```powershell
.\scripts\ue\package\verify_release.ps1 -ReleaseDir <release_dir>
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
- `ALIS_Linux_x86_64_<version>.tar` or split TAR parts for a multi-platform release
- `INSTALL_ALIS_PLAYER.bat` and `INSTALL_ALIS_PLAYER.ps1`
- `INSTALL_ALIS_DEVELOPER.bat` and `INSTALL_ALIS_DEVELOPER.ps1`
- `README.txt`, `PRODUCT_TERMS.txt`, and `release_manifest.json`
- Developer payload ZIP, payload manifest, notices, and effective component
  manifest
- `ALIS_PUBLIC_KEY.asc`
- `VERIFY_RELEASE.ps1`
- `VERIFY_RELEASE.bat`
- `VERIFY_RELEASE.sh`
- `SHA256SUMS.txt`
- `SHA256SUMS.txt.asc`

Local-only release debug set:

- `debug/Windows/`
- `debug/package_summary.txt`
- optional operator-requested signing and verification summaries outside the
  upload directory

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
  - run `INSTALL_ALIS_PLAYER.bat`
- short install note for normal users:
  - verification is optional
  - `verify_release.ps1` is available for advanced users

## Command Reference

The normal operator transaction is the Quick Start above. The commands below
are lower-level packaging and diagnostic entry points, not extra release steps.

Lower-level package/archive:

```powershell
make package
```

Prepare and verify the complete release without private-key access:

```powershell
make release 2.0.0 RELEASE_SIGN=0
```

Approve, sign, and consumer-verify that exact prepared folder:

```powershell
make release 2.0.0
```

Publish the highest local signed game workspace to the ALIS itch project:

```powershell
make publish itch
```

Verify release artifacts:

```powershell
.\scripts\ue\package\verify_release.ps1 `
  -ReleaseDir tmp\release\v2.0.0\github
```

Package into an explicit directory:

```powershell
.\scripts\ue\package\package_release_source.bat `
  -OutputDir Saved\PackageRelease\MyBuild
```

Package the game, then prepare the combined player/developer release:

```powershell
.\scripts\ue\package\package_release_source.bat `
  -OutputDir Saved\PackageRelease\MyBuild `
  -CreateReleaseArchive
```

The exact accepted Candidate and developer-source reports then enter the single
release transaction documented in
[Package Scripts](../../scripts/ue/package/README.md#release-transaction).

Force split archives:

```powershell
.\scripts\ue\package\package_release_source.bat `
  -SplitSizeMB 1900
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
Quick Start:
- Player: extract the first numbered ZIP part with 7-Zip.
- Developer or contributor: clone the exact tag and extract the Developer ZIP
  into it with 7-Zip.
- The Player and Developer installers are optional conveniences.

Advanced:
Use the published ALIS public key and verify SHA256SUMS.txt.asc before installation.
```

Generated release helper:

- `prepare_release.py` writes one Quick Start into the flat `github/` review
  projection
- `README.txt` owns the release overview, version highlights, and complete
  minimal instructions for both roles
- `ALIS_PUBLIC_KEY.asc`, `VERIFY_RELEASE.ps1`, `VERIFY_RELEASE.bat`, and
  `VERIFY_RELEASE.sh` are written into `github/`; each runnable game keeps its
  platform-native verifier plus signed material under `Verification/`
- the key, README, optional installers, and verification helpers are included
  in `SHA256SUMS.txt` and covered by the detached signature
- each projection's checksum manifest and detached signature are protocol
  envelopes and do not hash themselves

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
- any requested signing or verification summary is outside the upload directory
- internal package-container sizes are recorded for diagnosis
- release archive parts were generated and each is below the GitHub limit
- no staged `.pdb` files are being shipped unless explicitly intended
- package boots on a clean Windows machine
- hashes and signature verify correctly
- verification succeeds from the downloaded release directory with network access disabled
- bundled `ALIS_PUBLIC_KEY.asc` matches the expected fingerprint
- manifest contains no self-hash or detached-signature entry
- published tags and their existing assets were not mutated; corrections use a new release identity
- release notes point to `https://fall.is/trust/` and `https://fall.is/assets/security/public-key.asc`
- itch publication reads back the exact channel/version after Butler accepts
  the upload

## Troubleshooting

| Symptom | Cause | Fix |
| --- | --- | --- |
| Packaging fails with `AutomationTool exiting with ExitCode=5` | Stale staging or cook state | Delete `Saved/StagedBuilds/` and rerun the packaging script. |
| Packaging fails because project modules cannot load in cook | engine/editor binaries were built against a different UE install | Build/package with the same engine root, or rebuild `AlisEditor` with the chosen engine first. |
| Zen is alive but IoStore staging reports it unavailable | Common Zen inherited the cook lifetime, or .NET routed `[::1]` through a proxy | use the canonical packaging wrapper; it scopes the supported lifetime and proxy-bypass overrides to UAT. |
| One internal content container exceeds 2 GiB | chunking rules assigned substantial content to one chunk | use the recorded size as package-layout information; GitHub transport limits are enforced against the split release assets, not internal containers or `MaxChunkSize`. |
| Release folder is huge because of debug files | staged `.pdb` files were included | keep `-nodebuginfo` enabled. |
| Missing DLC chunks | incorrect Asset Manager chunk rules or Primary Asset Labels | verify `Config/DefaultGame.ini` chunk rules or project label assets assign expected chunk IDs. |
| Packaged game crashes with `Failed to find requested encryption key 00000000000000000000000000000000` | encrypted startup containers cannot resolve the current runtime key | use the release script default `-skipencryption`, or only enable `-EncryptContent` after implementing and validating a runtime key-loading path. |
| Player installer reports a missing archive part | Player assets were downloaded into different folders or one part is absent | place every Player file beside `INSTALL_ALIS_PLAYER.bat` and rerun it. |
| `verify_release.ps1` fails before signature check | `gpg.exe` is missing, the bundled key is absent, or an old release cannot reach its fallback key URL | install GnuPG or Git for Windows; for old releases pass `-PublicKeyPath` explicitly. |
| `verify_release.ps1` reports fingerprint mismatch | wrong or tampered public key file was used | compare `ALIS_PUBLIC_KEY.asc` against the fingerprint on the site trust page. |

## References

- GitHub Releases limits and asset model: <https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases>
- GitHub immutable release workflow: <https://docs.github.com/en/code-security/concepts/supply-chain-security/immutable-releases>
- GitHub release asset digests: <https://docs.github.com/en/rest/releases/assets>
- GitHub large files guidance: <https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-large-files-on-github>
- GitHub Acceptable Use: <https://docs.github.com/en/site-policy/acceptable-use-policies/github-acceptable-use-policies>
- GitHub Pages limits: <https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits>
- Git LFS and Pages note: <https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-git-large-file-storage>
- Unreal chunking overview: <https://dev.epicgames.com/documentation/en-us/unreal-engine/cooking-content-and-creating-chunks-in-unreal-engine>
- Unreal preparing assets for chunking: <https://dev.epicgames.com/documentation/en-us/unreal-engine/preparing-assets-for-chunking-in-unreal-engine>
- Butler installation: <https://itch.io/docs/butler/installing.html>
- Butler push command: <https://itch.io/docs/butler/pushing.html>
- Unreal packaging settings API: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Developer/DeveloperToolSettings/UProjectPackagingSettings>
- Unreal packaging settings page: <https://dev.epicgames.com/documentation/en-us/unreal-engine/project-section-of-the-unreal-engine-project-settings>
