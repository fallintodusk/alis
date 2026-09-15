# ALIS 2.0.0 Release Queue

Status: CURRENT - flat unsigned release verification
Active action: 3 - produce the exact unsigned review folder

Execution details and evidence:
[developer release transaction](20260813-1511_publish_public_developer_release_transaction.md).

Stable procedures:
[package and signing](../../scripts/ue/package/README.md),
[public source and developer payload](../../scripts/git/mirror/README.md), and
[release packaging](../../docs/build/packaging_guide.md).

## Authority register

### Operator decisions

- **D1** GitHub is the primary ALIS 2.0.0 release path and must publish the
  complete Player and Developer release.
  - Effect: itch.io does not replace or reduce the GitHub release.
  - Reason: not stated.
  - Date/source: operator, 2026-09-15.
- **D2** Publish the ALIS 2.0.0 Player build to itch.io as a secondary release
  path.
  - Effect: the release remains incomplete until the secondary publication is
    verified.
  - Reason: not stated.
  - Date/source: operator, 2026-09-15.
- **D3** The final local release directory must be flat and mirror the exact
  GitHub Release asset set.
  - Effect: local-only reports and nested role folders cannot remain in the
    upload directory.
  - Reason: the operator must review the same surface that users receive.
  - Date/source: operator, 2026-09-15.
- **D4** Minimize public files and link exact tagged repository documentation
  instead of duplicating it in release assets.
  - Effect: every remaining public artifact must have a release-time consumer
    or compliance responsibility. The required Product terms and non-personal
    rights result remain signed release assets.
  - Reason: reduce noise for Players and Developers.
  - Date/source: operator, 2026-09-15.
- **D5** Manual 7-Zip setup is the primary Player and Developer path. The small
  BAT/PowerShell installers remain optional conveniences.
  - Effect: neither role must run a downloaded script. The optional Developer
    installer authenticates only its downloaded subset and never requires the
    Player archives.
  - Reason: keep the visible release path familiar and transparent.
  - Date/source: operator, 2026-09-15.

### Operator gates

- **Q1 [CLOSED]:** The operator accepted the minimal flat asset inventory with
  manual setup first and optional installers.

### Working assumptions

- **A1 [ACCEPTED BY D5]:** Keep tiny BAT launchers beside maintainable PowerShell
  implementations because Windows does not reliably execute `.ps1` files by
  double-click; embed only the Player archive-part inventory in its generated
  PowerShell installer.

## Current state

- The release performance gate keeps a fixed `16.670 ms` p95 budget. It records
  host CPU and maximum NVIDIA GPU load immediately before capture for diagnosis,
  but host load never changes product acceptance. Missing or invalid load
  evidence fails closed; the gate does not retry or select a best run.
- The existing `tmp/release/v2.0.0/` was prepared before the fixed-budget and
  notices-guide corrections. Its flat shape is correct, but its generated
  `README.txt` and Developer source payload do not contain those corrections.
  It remains immutable for comparison and is not a publication candidate. The
  operator must remove that version directory before the next full unsigned
  preparation.
- A full unsigned transaction passed packaging, the three-run Development
  performance gate, Shipping runtime checks, public World realization, public
  source filtering, developer installation/build/map load, archive verification,
  and release verification. Its output is now superseded as a publication
  candidate by the source corrections above. No mirror or signing action is
  automatic.
- The flat composer and focused Player installer, Developer subset verifier,
  signing, release-entrypoint, finalization, and mirror tests pass. The exact
  full unsigned replacement remains action 3.

## Remaining actions

1. [x] Close Q1 by approving or revising the minimal flat public asset
   inventory. Require a concrete consumer or compliance owner for every file.
2. [x] Update the release composer, installers, verifier, tests, and stable
   packaging documentation so `tmp/release/v2.0.0/` is the exact flat GitHub
   asset set. Keep local gate evidence in its existing validation/input owner,
   outside the upload directory. Link exact `v2.0.0` repository guides, Product
   terms, and license texts instead of copying role READMEs and licenses.
3. [ ] Run `make release 2.0.0 RELEASE_SIGN=0` from native Windows. Require zero
   prompts/GPG access, green Player and Developer gates, exact flat output, no
   public local-machine paths, and a second unsigned invocation that changes no
   byte.
4. [ ] Operator: inspect/play the exact unsigned Player payload, manually
   extract the Developer payload into an exact-tag checkout, and review the
   generated release notes and bundled
   [Product terms](../../PRODUCT_TERMS.txt).
5. [ ] Operator: run `make mirror RTAG=v2.0.0` to publish
   the already-reviewed public-source commit and matching immutable tag through
   SSH. Do not silently create a second projection; require remote `main` and
   the tag to read back as the reviewed commit.
6. [ ] Run `make release 2.0.0`. The invocation approves the exact prepared
   Product, terms, and rights. Require identical public Git tree, metadata-only
   rebind if only the revision changed, one direct GPG prompt, and green consumer
   verification. Exercise the optional signed Developer bootstrap without
   downloading Player archives. Any content drift returns to action 1 without
   signing.
7. [ ] With explicit remote authorization, upload every file from the exact
   signed flat folder to GitHub Release, then read back and verify the complete
   asset set.
8. [ ] Publish the same accepted ALIS 2.0.0 Windows Player build to itch.io as
   the secondary channel. Verify the public page, Windows install/download
   route, version label, and packaged-game identity against the accepted Player
   receipt. Do not substitute a separately rebuilt package.
9. [ ] Archive the release todos only after both GitHub and itch.io publication
   checks pass.

## Review record

### 2026-09-15 - Public asset boundary and secondary Player channel

- **Trigger:** the nested local bundle did not mirror GitHub's flat asset
  surface, and internal evidence produced unnecessary public noise.
- **Root cause:** one directory was serving both operator review evidence and
  public distribution.
- **Fix:** D1-D5 constrain the flat release design and the itch.io secondary
  publication is an explicit remaining action.
- **Verification:** current `tmp/release/v2.0.0/` was inspected and confirmed to
  contain nested role folders and release-internal reports.
- **Authority:** D1-D5; Q1 closed.
