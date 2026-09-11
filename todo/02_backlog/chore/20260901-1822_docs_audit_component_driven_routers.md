# Docs: Audit Repo for Component-Driven Ownership (Routers -> SOT Components)

**Status:** PROTOTYPE -- not investigated against the working tree yet.
**Priority:** HIGH
**Created:** 2026-09-01
**Source:** external PATCH review of the public mirror (research pasted by
operator, 2026-09-01). All findings below are UNVERIFIED against the current
private tree until the audit pass runs.

---

## Verdict from the review (accepted direction)

ALIS stays on the current component-driven line. No permanent SDD + ADR layer.
The target model is **ownership-driven architecture**, not
documentation-driven architecture:

```text
ROOT
  global invariants + router
        |
        v
DOMAIN / TIER
  domain boundary + router
        |
        v
COMPONENT
  responsibility, boundary, contracts, invariants, WHY
        |
        +-- code
        +-- tests
        +-- schemas/data

TODO = current change delta
Git/MR = history
C4/index = derived views (generated, never hand-authoritative)
```

Rules the audit enforces:

- Truth lives at the **narrowest real owner** (component -> domain -> root).
- Common/shared docs are **routers only**: they combine references to
  component SOTs, they do not restate or own component truth.
- Component docs hold **semantic truth only** (responsibility, boundary,
  invariants, WHY). Machine-provable facts (dependencies, module lists,
  API surface) stay in code / `.uplugin` / `Build.cs` / schemas / tests;
  any whole-map view is generated from those, not hand-maintained.
- CI guards boundaries (dependency direction, schemas, naming), not
  synchronization between several hand-written descriptions of the same
  boundary.

## Known drift candidates (from review; verify each)

1. **`docs/architecture/source_of_truth.md`** -- claims
   "THIS FILE is authoritative for the whole map and flows" and prescribes
   central-file-first update order. This contradicts the router model and
   has already drifted (misses newer Systems/Features and the Editor /
   Resources / Test tiers). Expected fix direction: demote to a derived
   view or remove; `docs/architecture/README.md` stays the router.
2. **`Plugins/World/ProjectWorld/README.md`** -- ~944 lines mixing
   implemented architecture with planned/proposal material. Expected fix
   direction: README answers "what I am / own / don't own / contracts /
   invariants / WHY / verification routes"; future architecture moves to
   an active todo until real. (Finding may apply only to the public
   mirror -- check the private tree first.)

## Task

### Phase 1 -- Audit (careful, repo-wide, read-only)

- [ ] Inventory all common/shared docs (`docs/**`, root `README.md`,
      `CLAUDE.md`/`AGENTS.md` routes) and classify each: router | owner of
      genuinely cross-component truth | duplicated/central restatement of
      component truth (drift risk).
- [ ] Verify drift candidate 1: current state of
      `docs/architecture/source_of_truth.md` vs actual plugin tiers and
      component docs.
- [ ] Verify drift candidate 2: `ProjectWorld/README.md` size and
      implemented-vs-planned mix in the private tree.
- [ ] Sweep component docs (plugin READMEs / `docs/`) for machine-provable
      facts maintained by hand (dependency lists, module inventories,
      mirrored API tables) that belong to `.uplugin`/`Build.cs`/schemas.
- [ ] Sweep for the same rule/contract stated in two sibling owners
      (duplicated semantic authority).
- [ ] Check existing CI/validators: which boundary checks exist
      (dependency direction, schema, naming, no-Alis-prefix) vs which
      checks merely compare hand-written docs.

### Phase 2 -- Corrections (per audit evidence, smallest safe changes)

- [ ] Demote/remove central authoritative docs found in Phase 1;
      convert common docs to pure routers combining refs to component SOTs.
- [ ] Move misplaced truth to its narrowest owner; delete duplicated
      restatements at parent level.
- [ ] Strip hand-maintained machine-provable facts from component docs or
      mark them as generated views with their generator.
- [ ] Where a whole-system map is genuinely needed, make it a derived
      view (generated from `.uproject`/`.uplugin`/`Build.cs`/contracts).

## Acceptance

- Every common doc is either a router or an explicit owner of a real
  cross-component invariant; none restates component-owned truth.
- No doc claims whole-map authority over facts owned by components or by
  build descriptors.
- Audit report lists: verified drift, applied fixes, and anything left
  UNVERIFIED with reason.

## Non-goals

- No new documentation framework, ADR log, or SDD artifact chain.
- No CI whose job is proving that multiple hand-written docs agree.
- No repo-wide rewrite of healthy component docs; touch only what the
  audit proves drifted or misplaced.
