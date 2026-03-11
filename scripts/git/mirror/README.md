# Public Mirror

Utilities in this folder publish a filtered snapshot of the ALIS repository to a separate public mirror repository.

Files:
- `mirror_to_github.sh`: main mirror script. Battle-tested flow copied from the async mirror pattern and adapted for ALIS validation.
- `mirror_to_github.ps1`: Windows wrapper that runs the Bash script through WSL.
- `mirror_to_github.bat`: simple batch entrypoint for Windows shells.
- `mirror.exclude`: blacklist of files and folders removed from the public mirror snapshot.
- `forbidden_text_patterns.regex`: hard-fail content validation for text files that must never survive filtering.

## Why This Exists

Goal:
- publish source code and public docs to GitHub;
- avoid UE assets, generated artifacts, local workspace files, and internal-only material;
- keep the working repository remotes untouched.

The script does not change local `origin` and does not push from your working repository.

## Public-First Policy

ALIS mirror policy is public-first:
- code is public by default;
- architecture and build docs are public by default;
- build, packaging, and verification scripts are public by default.

What stays out of the mirror:
- secrets and credentials;
- private keys;
- machine-local config;
- generated outputs;
- licensed or non-redistributable asset payloads.

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
3. Rewrites `Alis_sanitized.uproject` to `Alis.uproject` in the filtered snapshot.
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

Use `--force` to bypass this check intentionally.
`--allow-dirty` is a backward-compatible alias.

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

Windows wrapper:
```powershell
.\scripts\git\mirror\mirror_to_github.ps1 --remote-url git@github.com:org/repo.git --dry-run
.\scripts\git\mirror\mirror_to_github.ps1 --dry-run --ephemeral-preview
.\scripts\git\mirror\mirror_to_github.ps1 --remote-url git@github.com:org/repo.git --push
```

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
make mirror MIRROR_REMOTE_URL=git@github.com:org/repo.git
```

Dry run:
```bash
make mirror MIRROR_DRY_RUN=1 MIRROR_REMOTE_URL=git@github.com:org/repo.git
```

Ephemeral local preview:
```bash
make mirror MIRROR_DRY_RUN=1 MIRROR_EPHEMERAL_PREVIEW=1
```

Allow dirty tree:
```bash
make mirror MIRROR_DRY_RUN=1 MIRROR_REMOTE_URL=git@github.com:org/repo.git MIRROR_FORCE=1
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
- hard-fail if global UE asset extensions survive filtering;
- hard-fail if forbidden UE asset and binary file types survive;
- hard-fail if forbidden paths survive;
- hard-fail if text files still contain patterns from `forbidden_text_patterns.regex`.

This keeps the low-maintenance blacklist model while still stopping unsafe publication.

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
