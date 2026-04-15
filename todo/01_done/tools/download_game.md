# Download Game

Status: done
Last verified: 2026-03-19

## Canonical Docs

- `docs/build/packaging_guide.md` -> stable release packaging and GitHub Releases source of truth
- `scripts/ue/package/README.md` -> canonical script usage

Do not duplicate packaging policy here. Keep the durable process in the docs above.

## Resolved Outcome

- GitHub Releases is the current ALIS public release transport
- verified package script: `scripts/ue/package/package_release.ps1`
- verified signing script: `scripts/ue/package/sign_release.ps1`
- verified verification script: `scripts/ue/package/verify_release.ps1`
- release hash/sign helper exists and generates `SHA256SUMS.txt` plus detached signature
- current release flow is package -> sign -> optional verify
- current verified archive result:
  - release payload: `1.988 GiB`
  - zip artifact: `1.755 GiB`
  - largest packaged file: `0.793 GiB`

## Current Release Flow

1. Run `scripts/ue/package/package_release.ps1 -CreateReleaseArchive`
2. Run `scripts/ue/package/sign_release.ps1 -ReleaseDir <release_dir>`
3. Optionally run `scripts/ue/package/verify_release.ps1 -ReleaseDir <release_dir>`
4. Upload archive parts plus `SHA256SUMS.txt` and `SHA256SUMS.txt.asc` to GitHub Releases

## Follow-up Ops

- decide whether to publish a GitHub Pages download page or rely on Releases only
- decide when torrent mirror becomes mandatory instead of optional
- verify packaged build boots and updates cleanly on a separate Windows machine outside the dev box

## Temporary Reference

Latest temp packaging output created during verification:

- `<temp-dir>\ALIS_release_20260310_135345`
