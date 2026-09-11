# ALIS 2.0.0 Release Queue

Status: CURRENT - one-command release entrypoint before final source freeze
Active action: 1 - complete and run automated local release preparation

Execution details and evidence:
[developer release transaction](20260813-1511_publish_public_developer_release_transaction.md).

Stable procedures:
[package and signing](../../scripts/ue/package/README.md),
[public source and developer payload](../../scripts/git/mirror/README.md), and
[release packaging](../../docs/build/packaging_guide.md).

## Remaining actions

1. [ ] Run `make release 2.0.0 RELEASE_SIGN=0`. The script freezes the effective
   source state and runs all machine-owned local gates, producing the replacement
   machine-accepted Candidate, public source, developer payload, clean-clone
   proof, reports, and exact Product terms ready for operator review. The KISS
   entrypoint creates and resumes those exact inputs itself,
   derives tag `v2.0.0`, composes terms/guides and player/developer artifacts,
   supports `RELEASE_SIGN=0` for a private-key-free rehearsal, enforces approval
   before private-key access, signs once, consumer-verifies the exact folder,
   and performs no GitHub write. `make package` remains archive-only.
2. [ ] Review the complete unsigned directory: launch the packaged Product,
   inspect the developer payload/reports, and approve the exact
   [Product terms](../../PRODUCT_TERMS.txt). Then authorize the named GitHub
   ruleset, branch, PR/merge, tag, release, and asset writes.
3. [ ] Agent: execute the protected GitHub source transaction, bind all release
   artifacts to the merged `main` commit, require zero unresolved items, and
   prepare the release notes. Run the approved `make release 2.0.0` transaction
   for the exact final folder. The operator enters the passphrase directly into
   its one GPG prompt. Then complete draft creation, upload/read-back, remote-download
   verification, publication, final audit, bounded cleanup, evidence retention,
   and todo archival.
