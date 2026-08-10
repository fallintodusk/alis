# Publish Full Runnable Uproject — Asset Publication and Mirror Policy

**Status:** Active — decision + audit are the next steps (no code yet)
**Priority:** High — core open-source goal: public clone must become fully runnable
**Date:** 2026-08-06
**Owner:** unassigned

---

## Main goal

A public clone must become a FULLY RUNNABLE uproject: clone → open in editor →
play, with all materials and content included. Over time every asset in the
project is either regenerated as project-owned content or replaced with a
free-licensed asset compatible with the project license, until no third-party
commercial payload remains in use. At that point the entire content layer is
publishable.

## Current state (verified 2026-08-06)

Already public (live on the GitHub mirror, verified via API):
- all JSON definition sources, schemas, and generator inputs in plugin `Data/`
  dirs (17 plugins: loot, dialogue, vitals, UI layouts, object definitions, ...)
- all C++ source, docs, build/test/packaging/mirror scripts

Not yet published (all project content):
- generated DataAssets under `Content/` — these are PERSISTENT BAKED CONTENT,
  the SOT of the UE asset layer. Levels and placed actors reference these
  assets directly; a fresh regeneration from JSON produces new objects that
  existing references do not resolve to (crashes, broken placements). They
  must be published, not regenerated on clone. Regeneration is the authoring/
  update path, not the bootstrap path.
- hand-authored binary content: maps, meshes, materials, textures, audio
  (`Content/**`, `Plugins/**/Content/**`), and `Config/**`

Mirror policy correction needed: the `mirror.exclude` header groups "generated
outputs" as never-public. That is correct for disposable build outputs
(`Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`) but WRONG for
generated DataAssets in `Content/` — those are persistent content, not cache.
Update the policy comment when the exclusion is narrowed.

## Path to the goal

1. **License audit** — classify every asset in `Content/**` and
   `Plugins/**/Content/**`: project-owned / free-licensed compatible /
   third-party commercial.
2. **Replace or regenerate** — for each third-party commercial asset, either
   regenerate a project-owned equivalent or substitute a license-compatible
   free asset. Track remaining count toward zero.
3. **Channel decision** — binary content cannot go through the current
   text-only mirror (extension denylist + NUL-byte validator). Decide:
   Git LFS on the mirror vs separate public asset repo vs release payloads.
   Record in `docs/build/` or `docs/legal/`.
4. **Open the gates** — narrow `mirror.exclude` (`Content/**`,
   `Plugins/**/Content/**`, `Config/**`, relevant extension rules), update the
   validator binary-guard policy to match, fix the policy header comment.

## Done criteria

- [ ] license audit complete; third-party-commercial in-use count is zero
- [ ] channel decision recorded in `docs/build/` or `docs/legal/`
- [ ] validator + `mirror.exclude` updated to publish the content layer
- [ ] fresh public clone opens in editor and plays with no missing references
- [ ] README: remove the temporary bullet
      `- project asset payloads that are not yet published`
      (single removal point; Download section wording stays valid unchanged)
