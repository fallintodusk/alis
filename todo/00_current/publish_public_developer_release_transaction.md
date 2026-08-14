# Publish Public Developer Release Transaction

Status: Active
Priority: High before the next public generated-authority update

## Goal

Publish one exact filtered public source tag and its signed developer payload
without a window where public authority points at unavailable release bytes.

## Standing contracts

- Mirror and payload workflow: `@[scripts/git/mirror/README.md]`
- Release compliance: `@[docs/legal/release_compliance.md]`
- Asset distribution: `@[docs/legal/world_data_and_asset_policy.md]`
- Signing and verification: `@[scripts/ue/package/README.md]`

## Current safe boundary

- [x] Payload records the exact filtered public commit, branch, and source tag.
- [x] Trusted clean checkout verifies commit/tag before installing.
- [x] Direct push with payload arguments is rejected.
- [x] Direct push of changed generated public authority is rejected.
- [ ] GitHub draft release staging and upload are not implemented.

## Work

- [ ] Build the filtered public source candidate and immutable developer payload.
- [ ] Create and authenticate the exact public source tag.
- [ ] Sign the complete developer release directory.
- [ ] Create a GitHub draft release and upload the exact signed inventory.
- [ ] Read the draft release back and verify names, sizes, and hashes.
- [ ] Publish source commit/tag and release through a recoverable ordered flow.
- [ ] Verify the public tag, release inventory, signature, and clean-checkout
      installation after publication.
- [ ] Preserve prior public pointers until the new pair is verified; document
      rollback for every partial failure point.

## Done criteria

- [ ] No command can advance generated public authority without a remotely
      verified matching payload.
- [ ] A release cannot publish for a different source revision or tag.
- [ ] Failure injection at every remote step preserves a usable prior release.
- [ ] A fresh clone at the published tag installs using only the trusted
      checkout-side script and opens with the approved payload present.
