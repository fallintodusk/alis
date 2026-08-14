# Prepare Public Developer Asset Release

Status: Done - composition and installation boundary
Completed: 2026-08-13

## Goal

Ship every currently approved ALIS runtime asset outside the source mirror as a
verified developer payload. A developer downloads the release parts, runs one
installer, and receives the exact persistent files at their project-relative
paths.

## Standing contracts

- Asset and data licensing: `@[docs/legal/world_data_and_asset_policy.md]`
- Source mirror and developer payload: `@[scripts/git/mirror/README.md]`
- Data generation ownership: `@[docs/data/README.md]`
- Definition generator contract:
  `@[Plugins/Editor/ProjectDefinitionGenerator/README.md]`

## Delivered

- [x] Explicit public asset-selection contract and schema.
- [x] Live Unreal Asset Registry export and plugin-owned ProjectObject
      generated definition authority.
- [x] Approved JSON sources and persistent generated DataAssets included
      without copying referenced third-party dependency bytes.
- [x] Active ProjectWorldData world and canonical authority included.
- [x] ProjectWorldTestData, HLOD, and unapproved third-party payloads rejected.
- [x] Deterministic logical ZIP split below the GitHub Releases 2 GiB per-file
      limit, with hashes and standard release-signing route.
- [x] One-command installer verifies every part, archive, entry, and target;
      conflict and rollback behavior are covered.
- [x] Stable SOT docs updated. Broader asset classification and replacement
      remains active in `todo/00_current/publish_project_assets_mirror_policy.md`.
- [x] Exact public revision/tag binding and trusted checkout-side installation.

Public draft upload, remote verification, and cohesive source/tag/release
publication are deliberately not claimed here. They remain active in
`todo/00_current/publish_public_developer_release_transaction.md`.

## Evidence

- Unreal authority refresh: 82 generated definition assets.
- Real audit payload: 210 entries, 10,437,487 archive bytes, zero forbidden
  TestData/ThirdParty/HLOD paths.
- Focused payload/installer suite: 11 of 11 passed, including multi-part
  install, hash sabotage, exact revision/tag refusal, trusted-verifier
  bootstrap, and direct-push refusal.
- Data cross-reference and inline schema validation passed for 82 files.
- Release contract and generated authority JSON schemas validated.
- Python compilation, PowerShell parsing, Bash syntax, ASCII docs, and
  `git diff --check` passed.
