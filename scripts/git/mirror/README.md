# Public Mirror

Utilities in this folder publish a filtered snapshot of the ALIS repository to a separate public mirror repository.

Files:
- `mirror_to_github.sh`: main mirror script. Battle-tested flow copied from the async mirror pattern and adapted for ALIS validation.
- `mirror_to_github.ps1`: Windows wrapper that runs the Bash script through WSL.
- `mirror_to_github.bat`: simple batch entrypoint for Windows shells.
- `compose_developer_payload.py`: builds the authority-driven binary complement
  for public developers.
- `install_developer_payload.ps1`: verifies and installs that complement into
  a matching ALIS source checkout.
- `mirror.exclude`: blacklist of files and folders removed from the public mirror snapshot.
- `forbidden_text_patterns.regex`: hard-fail content validation for text files that must never survive filtering.

## Why This Exists

Goal:
- publish source code and public docs to the GitHub branch;
- publish approved generated runtime assets as a separate release payload;
- avoid test outputs, unclassified third-party assets, and local workspace files;
- keep the working repository remotes untouched.

The script does not change local `origin` and does not push from your working repository.

## Public-First Policy

ALIS mirror policy is public-first and denylist-first: a tracked file is
published unless it (or a parent dir) matches a rule in `mirror.exclude`. Folder
name alone does not make content private.
- code is public by default;
- architecture and build docs are public by default;
- build, packaging, and verification scripts are public by default;
- plugin `Data/` JSON schemas and data-driven generator inputs (loot, dialogue,
  UI layouts, vitals) are public references; source in a folder named `Data` is
  code and stays public too.

What stays out of the Git branch:
- secrets and credentials;
- private keys and key material (`*.pem`, `*.key`, `*.p12`, `*.pfx`, `id_rsa*`);
- machine-local config;
- disposable generated outputs;
- licensed or non-redistributable asset payloads;
- UE asset/binary/media file TYPES anywhere - including inside published `Data/`
  dirs, where only text/JSON survives.

Approved persistent generated binaries are not lost. They are selected from
active world authority or an explicit plugin-owned public asset manifest and
published as the separate developer release described below. The Git branch
remains small and cloneable. Unclassified and restricted dependencies remain
outside that release until they are replaced or separately approved.

This matches the long-term direction:
- open-source development;
- public review;
- decentralized distribution and verification;
- no dependence on hidden architecture for security.

If a workflow requires secrets, keep them outside the repository and inject them separately at runtime or in CI.

## Safety Model

The flow is intentionally isolated:
1. Reads tracked file paths from `HEAD`.
2. Applies `mirror.exclude` before export, then checks out only surviving files from a temporary `HEAD` index.
3. Preserves the canonical `Alis.uproject` in the filtered snapshot so the public mirror shows the real plugin graph.
4. Runs hard validation against forbidden paths, binary file types, and forbidden text patterns.
5. Creates a temporary git repository.
6. Commits filtered snapshot in the temp repo.
7. Pushes to `--remote-url` only if `--push` is provided.
8. Deletes temp folder on exit.

This prevents accidental mutation of your working tree and avoids remote misconfiguration in local `.git/config`.

## Baseline Model

The normal mirror flow compares against the mirror remote branch, not against a persistent local clone.

Persistent state:
- the separate mirror repository and its target branch.

Disposable state:
- the temporary mirror repo created for each run.

Normal behavior:
1. fetch mirror branch tip into the temp repo;
2. copy filtered snapshot over it;
3. use `git add -A` + `git diff --cached --quiet` to decide whether anything changed.

Result:
- no SHA manifest is used for the normal git mirror flow;
- no persistent local mirror checkout is required;
- the remote mirror branch is the baseline.

`--ephemeral-preview` is the only mode that runs without a remote baseline. Use it only for one-off local inspection.

## Dirty Working Tree Policy

Default behavior is strict:
- fails if staged files exist;
- fails if unstaged tracked changes exist;
- fails if untracked files exist.

Use `--force` to intentionally publish the current tracked and non-ignored
untracked worktree instead of `HEAD`. This mode requires
`--source-revision HEAD`; it must not be used to label a dirty projection as
another revision. `--allow-dirty` is a backward-compatible alias.

## Usage

Dry run against real mirror baseline:
```bash
bash ./scripts/git/mirror/mirror_to_github.sh --remote-url git@github.com:org/repo.git --dry-run
```

One-off local preview without remote baseline:
```bash
bash ./scripts/git/mirror/mirror_to_github.sh --dry-run --ephemeral-preview
```

Real push:
```bash
bash ./scripts/git/mirror/mirror_to_github.sh --remote-url git@github.com:org/repo.git --push
```

Official ALIS release source uses a protected release branch. The additional
mode rejects any repository except `fallintodusk/alis`, rejects `main` as the
push target, requires the explicit public World manifest projection, and bases
a new release branch on the current remote `main` tip:

```bash
bash ./scripts/git/mirror/mirror_to_github.sh \
  --remote-url git@github.com:fallintodusk/alis.git \
  --branch release/v2.0.0-source \
  --public-world-manifest-root tmp/release/public-world/Manifests \
  --official-release --push
```

Remote execution still requires explicit operator authorization. The flag
validates the destination; it does not grant publication authority.

Windows wrapper:
```powershell
.\scripts\git\mirror\mirror_to_github.ps1 --remote-url git@github.com:org/repo.git --dry-run
.\scripts\git\mirror\mirror_to_github.ps1 --dry-run --ephemeral-preview
.\scripts\git\mirror\mirror_to_github.ps1 --remote-url git@github.com:org/repo.git --push
```

## Developer Project Release

After changing generated definitions, refresh the reviewed binary authority:

```powershell
.\scripts\git\mirror\refresh_developer_asset_authority.ps1
```

This reads the live Unreal Asset Registry, requires every declared source JSON
to have a generated asset with the same normalized source hash, and rewrites
the plugin-owned authority manifest. JSON source authentication normalizes line
endings so the same Git bytes remain valid in Windows and Unix checkouts.
Review and commit source, `.uasset`, and
manifest changes together. Release composition then requires a clean tree.

Compose the text-only mirror preview and its matching binary developer payload:

```powershell
.\scripts\git\mirror\mirror_to_github.ps1 `
  --dry-run --ephemeral-preview `
  --developer-release-dir ..\alis-developer-v1 `
  --developer-version 1.0.0
```

The output is one logical archive with a human-readable name. It is either one
file:

```text
ALIS_DeveloperProject_1.0.0_<identity>.zip
```

or, when large, numbered parts:

```text
ALIS_DeveloperProject_1.0.0_<identity>.zip.001
ALIS_DeveloperProject_1.0.0_<identity>.zip.002
```

An archive below the threshold remains a single `.zip`. Larger archives are
split into numbered raw parts, 1700 MiB by default. This stays below GitHub's
2 GiB per-release-asset limit without changing the logical ZIP. The release
also contains its identity manifest, extracted attribution notices, and
`INSTALL_ALIS_DEVELOPER_PROJECT.*`. Applicable ALIS license texts are copied
beside the release so they remain visible before installation.

The composer includes only:

- active manifest-owned production `.uasset` and `.umap` files;
- selected immutable canonical ZIP bundles;
- persistent generated `.uasset` files selected by
  `developer_asset_release.json` and each plugin-owned authority manifest.

Public source JSON, schemas, release contracts, active indexes, and owner
manifests remain in the exact Git tag. The composer reads and authenticates
those owners but does not duplicate their text inside the binary payload.

It rejects TestData, stale generations, HLOD artifacts, incomplete source
collections, untracked files, wrong hashes, and paths outside declared owners.
It does not scan every Content folder or guess whether an unrelated third-party
asset is public. A generated definition may retain a soft reference to a
separately obtained dependency; the referenced dependency bytes are not copied.

The payload is composed only after the filtered public candidate commit exists.
Its manifest records the exact full public commit, branch, and intended source
tag. World assets are selected through that candidate's projected public
manifest set, while exact binary bytes come from the clean internal owner tree.
The internal repository `HEAD` is not used as public source identity.
An ordinary public source-branch advance does not redefine an earlier release:
that payload remains installable from its recorded immutable tag. Only a new
tagged developer release creates a new source/payload pair.

For publication, pass this exact developer directory to the combined release
transaction in `scripts/ue/package/prepare_release.ps1`. The coordinator
re-authenticates the archive parts and rejects extra files before the release
owner approves and signs the complete player/developer release once. Do not
separately sign or add files to the developer payload directory.

Developer install from the exact public source tag recorded by the release:

```powershell
# Run the trusted installer from the clean source checkout.
.\scripts\git\mirror\install_developer_payload.ps1 `
  -ProjectRoot E:\path\to\Alis `
  -ReleaseDir C:\Downloads\ALIS_DeveloperProject_v1 `
  -RequireReleaseSignature
```

The installer requires clean Git source state and exact commit/tag identity, then uses
the trusted source checkout's `verify_release.ps1` to authenticate the download.
It never executes the downloaded installer or verifier. It next verifies every
part, archive, safe path, inventory entry, and existing target before copying.
A conflicting existing payload target aborts the whole install. Public-source
ignore rules keep installed payload files out of Git status, so reinstalling
the same authenticated payload is a verified no-op. The receipt is ignored under
`Saved/DeveloperPayload`.

Release copies of the installer are convenience material only and are not a
trust root. Do not execute downloaded scripts before authenticating them.

The installer restores all currently approved ALIS payloads. It does not make
an unclassified or restricted third-party dependency redistributable. As those
dependencies are replaced by ALIS-generated assets, add their plugin-owned
authority and the same release/install route picks them up.

### Publication boundary

Composition and installation are implemented. Asset publication itself stays
fail-closed: the mirror refuses `--push` when developer release arguments are
present; use `--dry-run` to compose candidates, then sign and upload the
release manually. `mirror --push` does not publish assets.

Routine source pushes are intentionally NOT blocked when generated public
authority (canonical data, public manifests, release selection) changed. The
model is:

- the public source branch is the development tip and advances freely;
- public developers obtain matching binary assets from a tagged developer
  release, whose installer pins the exact recorded commit/tag;
- an advanced source tip therefore never breaks an already published release,
  it only means assets for the newest tip are not installable until the next
  tagged developer release;
- source/asset coherence is enforced at release time by the release flow
  (compose + sign + verify against the exact filtered commit/tag), not by the
  mirror push.

When a push advances authority data past the latest release, the mirror prints
a `[WARN]` so the operator knows a new developer release is eventually due.

### Future direction: continuous asset distribution

Tagged developer releases remain the channel for stable, signed drops tied to
a stable game release. Open research item: a smoother channel that gives
developers the current tip's generated assets without cutting a full release
per commit - candidates include a dedicated asset repository/submodule or a
decentralized distribution channel not subject to GitHub commercial size and
bandwidth restrictions, with the same hash/signature verification model so
developers can fetch and append exactly the data their checkout needs.

Batch wrapper:
```bat
scripts\git\mirror\mirror_to_github.bat --remote-url git@github.com:org/repo.git --dry-run
scripts\git\mirror\mirror_to_github.bat --dry-run --ephemeral-preview
scripts\git\mirror\mirror_to_github.bat --remote-url git@github.com:org/repo.git --push
```

## Make Wrapper

The root `Makefile` exposes a mirror wrapper:

Push (default):
```bash
make mirror
```

Default mirror remote:
```text
git@github.com:fallintodusk/alis.git
```

Dry run:
```bash
make mirror MIRROR_DRY_RUN=1
```

Ephemeral local preview:
```bash
make mirror MIRROR_DRY_RUN=1 MIRROR_EPHEMERAL_PREVIEW=1
```

Allow dirty tree:
```bash
make mirror MIRROR_DRY_RUN=1 MIRROR_FORCE=1
```

Compose the matching developer release:

```bash
make mirror MIRROR_DRY_RUN=1 MIRROR_EPHEMERAL_PREVIEW=1 \
  MIRROR_DEVELOPER_RELEASE_DIR=../alis-developer-v1 \
  MIRROR_DEVELOPER_VERSION=1.0.0
```

Override branch or exclude file:
```bash
make mirror MIRROR_REMOTE_URL=git@github.com:org/repo.git MIRROR_BRANCH=main
make mirror MIRROR_DRY_RUN=1 MIRROR_EXCLUDE_FILE=scripts/git/mirror/mirror.exclude
```

Pass raw extra args:
```bash
make mirror MIRROR_ARGS='--help'
```

## Validation Notes

Blacklist filtering is the first line of defense.

ALIS adds a second line of defense:
- hard-fail if global UE asset extensions survive filtering
  (`uasset`, `umap`, `ubulk`, `uexp`, `uptnl`, `ushaderbytecode`, `utoc`, `ucas`, `pak`);
- hard-fail if forbidden binary/media/key file types survive
  (executables, `png`/`jpg`/`exr`/`blend`/`fbx`/`wav`/`mp4`..., `pem`/`key`/`p12`/`pfx`);
- hard-fail if forbidden paths survive;
- hard-fail if text files still contain patterns from `forbidden_text_patterns.regex`;
- hard-fail if published text contains disallowed foreign-script blocks checked
  by `validate_text_format.py` (Cyrillic and CJK by default), or if surviving
  paths contain any non-ASCII character;
- hard-fail if a Git LFS pointer file survives;
- hard-fail on any non-empty binary-content file (NUL byte) regardless of
  extension - this is what makes "text data only" true, not just the extension
  denylist, so an unknown-extension binary cannot sneak through.

Because the model is denylist-first, plugin `Data/` dirs are validated by file
TYPE, not by path: JSON/text survives, any binary/asset inside is rejected. This
keeps the low-maintenance blacklist model while still stopping unsafe publication.

In other words:
- ALIS docs/scripts are not excluded just because they describe architecture;
- exclusions target concrete risk categories such as secrets, licensed assets, generated binaries, and local machine state.

Secret hardening rules:
- docs and examples must use placeholders such as `<api-key>`, `<password>`, `<user>`, and `<host>`;
- do not publish credential-shaped env assignments such as `API_KEY=<api-key>`;
- do not publish URLs with embedded credentials such as `postgres://<user>:<password>@<host>/db`;
- keep real secrets only in local env files, CI secret stores, or runtime-injected configuration.

## Anonymity Model

The mirror normalizes obvious personal metadata before commit:
- mirror commits always use `mirror-bot <mirror-bot@localhost>`;
- `.uplugin` creator/support metadata is rewritten to project-safe values;
- maintainer-local repo roots, UE/tool install paths, and Windows/WSL home paths are rewritten to neutral placeholders;
- sibling local repos referenced from docs are rewritten to neutral placeholders as well;
- explicit local usernames are rewritten when they appear in public docs/examples.

This is intended to remove maintainer identity leakage and machine-local traces.

What it does not do:
- it does not rewrite project-domain identifiers such as public map, region, or lore names;
- it does not hide the ALIS project brand.

If full world-setting anonymity is required, that is a separate content policy decision and cannot be solved safely by generic mirror sanitization alone.

## Recommended Rollout

Phase 1:
- run manually from a trusted maintainer machine;
- validate dry-run output against the real mirror remote;
- push to a separate GitHub mirror repository.

Phase 2:
- move the same script into Azure pipeline once mirror credentials and ownership are settled.
