# Contributing to ALIS

ALIS welcomes contributions under the legal model and component assignments
defined only in the root [LICENSE](LICENSE#5-contributions). This file owns the
submission process.

ALIS does not require DCO sign-off lines or cryptographic commit signatures.
The pull-request checklist records the contributor's rights and provenance
affirmation without adding personal certification data to every commit. It is
not a CLA, copyright assignment, or consent to future relicensing.

## License Integrity

Before submitting:

1. Identify whether the change is UE-facing, a separate process, neutral
   interoperability code, documentation, an asset, or data.
2. Split changes that cross file, process, data, or asset boundaries.
3. Declare copied material, generated code, templates, datasets, assets, and
   every new direct dependency with exact upstream terms.
4. Do not commit Epic, Fab, Marketplace, or other restricted source payloads.
5. Add required attribution, notices, source offers, and provenance.
6. Confirm that required package metadata agrees with the root assignment.

Unknown ownership or license compatibility blocks acceptance.

Pull requests must complete the repository template's license and provenance
checklist. The repository status check rejects missing or incomplete
affirmations. Public-branch protection must require that check for PR merges.
The project-owned filtered-mirror publisher is the only normal direct-update
exception because it publishes the reviewed canonical snapshot. Hosting
settings must scope that bypass to the publisher identity; repository workflow
code cannot enforce that external setting by itself.

Direct or emergency maintainer changes are not release-ready until the release
operator records the same rights and provenance decision in the private
evidence record required by the packaged-release gate.

## Engineering Requirements

- Follow the project architecture and naming rules.
- Keep changes focused and test production behavior.
- Add or update tests for changed functionality.
- Run the relevant validation and smoke checks before review.
- Update the owning stable documentation when a public contract changes.

Detailed dependency and release checks live in
[docs/legal/component_license_policy.md](docs/legal/component_license_policy.md).
World data and asset rules live in
[docs/legal/world_data_and_asset_policy.md](docs/legal/world_data_and_asset_policy.md).
