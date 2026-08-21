# Publish Full Runnable Uproject

Status: Active long-term coverage goal
Priority: High

## Goal

A public developer can obtain the source mirror plus signed developer payload,
install into the project root, open the editor, and run without private or
missing asset dependencies.

## Standing contracts

- Distribution and provenance: `@[docs/legal/world_data_and_asset_policy.md]`
- Source mirror and developer payload: `@[scripts/git/mirror/README.md]`
- Data ownership: `@[docs/data/README.md]`

## Current boundary

- [x] GitHub Releases selected for large public developer payloads.
- [x] Deterministic split archive, hashes, signature route, installer, rollback.
- [x] Active ProjectWorldData generated authority is included.
- [x] Explicit plugin-owned generated definition authority is supported.
- [ ] Classify remaining UE packages as first-party public, approved upstream,
      private, replace, or regenerate.
- [ ] Add a public authority manifest for each newly approved first-party class.
- [ ] Replace every restricted dependency needed by the public runtime path.
- [ ] Prove a fresh public source checkout plus developer payload opens and runs
      with no missing references.
- [ ] Clean reconstruction agent-environment proof: on that same clean
      checkout, run the normal ALIS setup and prove it recreates
      `.agents/skills`, that `scripts/agents/link_codex_skills.ps1 -Verify`
      passes, and that Claude and Codex discover the same tracked project skill
      set. `.agents/skills` is a gitignored local junction, so it exists only
      after setup runs - a clean clone is the only honest way to authenticate
      that bootstrap. Deferred here deliberately: it belongs to mirror and
      reconstruction acceptance, not to any World or process gate.

## Rules

- Never publish by blindly scanning `Content/**` or every `.uasset`.
- Generated runtime assets are persistent release products, not cache.
- ProjectWorldTestData generated packages remain disposable test output.
- Referencing a separately licensed asset does not authorize copying its bytes.
- Transport work is complete only for assets selected by current authority;
  full public runtime coverage remains open until the final missing-dependency
  proof passes.
