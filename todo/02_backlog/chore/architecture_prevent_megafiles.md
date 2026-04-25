# Architecture: Prevent Mega-Files + ProjectInventoryComponent Split

**Status:** Open, plan-first (no blind refactor)
**Priority:** Major (architecture hygiene; agent discipline)
**Date opened:** 2026-04-23
**Trigger:** user flagged `ProjectInventoryComponent.cpp` at 4083 lines while reviewing
a bug fix that added more to it. "Is this 4000 lines actually good?" (paraphrased).

---

## Goal

1. **Short-term (this slice):** make the "NO MEGA FILES" rule in AGENTS.md actually
   binding. Today the rule exists (`<=1000 lines hard limit`) but there is no
   enforcement and agents add lines to files that are already 4x over.
2. **Targeted refactor:** split `UProjectInventoryComponent` (4083 LOC) along
   its interface seams. It implements 4 interfaces and owns unrelated state
   (player inventory + equipment + world-container bridge + action receiver).
   Each is a natural extraction candidate.
3. **Systemic prevention:** a pre-commit check that fails when a first-party
   source file grows past the hard limit, and a clear agent escalation
   protocol when a file is already over.

Not in scope:
- Third-party / Local plugins (`Plugins/Local/BlueprintMCP/`, `Plugins/InstanceArrayTool/`).
  Large by virtue of being vendor drops.
- Test files over 1000 lines. Tests accumulate cases; a different threshold
  and splitting strategy applies (by test category, not by SRP), and we
  already do this (multiple Inventory*Tests.cpp files exist).

---

## ALIS baseline (first-party source over 1000 LOC, measured 2026-04-23)

Real source files (not `Intermediate/`, not `.gen.cpp`, not test files):

| File | LOC | Owning plugin | Notes |
|---|---:|---|---|
| `Plugins/Features/ProjectInventory/.../ProjectInventoryComponent.cpp` | **4083** -> **1663** | ProjectInventory | Slices 2+3+4a landed 2026-04-23 (-59%). Remaining over 1000 due to Internal_Use/Equip/Unequip, save/load, queries, logging, weight-tag mgmt. Slices 4b/4c/4d will bring it below 1000. |
| `Plugins/Features/ProjectInventory/.../ProjectInventoryComponent_WorldContainer.cpp` | **1162** | ProjectInventory | Slice 2 extraction. Over limit pending further split (Authority vs Resolved vs Orchestration) once the authority-subsystem refactor lands. |
| (none else over 1000 in first-party plugins) | | | |

Third-party / Local plugins (out of scope):
- `ActorArrayBase.cpp` 6231, `InstanceArraysBase.cpp` 5813 - InstanceArrayTool vendor
- `BlueprintMCPHandlers_Mutation.cpp` 2610 - Local BlueprintMCP plugin
- `InstanceArraysObject.cpp` 2214, `ActorArrayObject.cpp` 2133 - vendor
- `BlueprintMCPServer.cpp` 2096, `BlueprintMCPHandlers_MaterialMutation.cpp` 2094 - Local

Test files with many tests (accepted by test-file convention):
- `InventoryLootPlacesIntegrationTest.cpp` 5092 (76+ tests; splitting means test-category split, not SRP)
- `ProjectUIInventoryDumpTreeTest.cpp` 2569
- `ObjectParentGeneralizationIntegrationTest.cpp` 2457
- `CharacterCleanPathIsolationTest.cpp` 1952

**First-party top offender count: 1 file.** The rule is nearly enforced; one
legacy hotspot remains.

---

## Slice landing log (2026-04-23)

### Slice 1: FWorldContainerMoveOp pure op
- **Landed.** Extracted the session-scoped consume+store+rollback body from
  `UProjectContainerSessionSubsystem::MoveWithinWorldContainerSession` into
  `Operations/WorldContainerMoveOp.h/.cpp` (pure static, no UObject
  ownership). Subsystem is now a thin caller (-91 LOC).
- **Tests:** integration test
  `ProjectIntegrationTests.InventoryLootPlaces.UI.MoveWithinNearbyContainerMutatesGridPos`
  passes post-extraction (11.7s warm, 63s cold).
- **Files added:**
  - `Plugins/Features/ProjectInventory/Source/ProjectInventory/Public/Operations/WorldContainerMoveOp.h` (51 LOC)
  - `Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Operations/WorldContainerMoveOp.cpp` (138 LOC)

### Slice 2: _WorldContainer TU extraction
- **Landed.** All world-container bridge methods (Client RPCs, resolve
  helpers, authority open/close/take/store/takeAll, snapshot/restore,
  session-aware resolved helpers, local open/close handlers, bridge
  interface impls, server-RPC impls for the bridge) moved from the
  monolithic component.cpp into `ProjectInventoryComponent_WorldContainer.cpp`.
  Class declaration unchanged; definitions split across TUs per
  canonical.md's "Cross-TU method definitions" pattern.
- **LOC:** main.cpp 4083 -> 2936 (-1060 LOC).
- **Tests:** gate suite
  `ProjectIntegrationTests.InventoryLootPlaces` 40/40 pass.
- **Files added:**
  - `ProjectInventoryComponent_WorldContainer.cpp` (1162 LOC)
  - `ProjectInventoryComponentInternals.h` (121 LOC shared helpers +
    `ProjectInventoryInternal::` namespace)

### Slice 3: _Containers TU extraction
- **Landed.** Container resolution, grid math, weight/volume-per-container
  helpers extracted: GetEffectiveContainers, GetContainerConfig,
  GetDefaultContainerId, GetContainerIndex/SlotOffset/CellCount,
  ComputeSlotIndex, TryGetGridPosFromSlot, SanitizeGridSize,
  GetItemGridSize, GetContainerCellDepthUnits,
  GetEffectiveMaxStackForContainer, BuildContainerConfigFromGrant,
  UpsertContainerConfig, IsRectWithinContainer, DoesRectOverlap,
  FindFreeGridPos, GetEffectiveEntryPlacement,
  GetContainerCurrentWeight/Volume, ContainerAllowsItem,
  GetContainerOrder, GetEquipSlotContainerGrants, IsContainerEmpty.
- **LOC:** main.cpp 2936 -> 2497 (-439 LOC), file now 501 LOC.
- **Tests:** LootPlaces gate still 40/40 post-Slice-3.

### Slice 4a: _Mutation TU extraction
- **Landed.** Add/remove/move mutation surface extracted:
  TryAddItemAtPosition, TryAddItem/Detailed/WithOverrides,
  Internal_AddItem, Internal_RemoveItem, Internal_MoveItem,
  Server_AddItem/_RemoveItem/_MoveItem/_DropItem RPC impls.
- **LOC:** main.cpp 2497 -> 1663 (-834 LOC), new file 896 LOC.
- **Cumulative:** main.cpp 4083 -> 1663 (-59%).
- **Tests:** MoveWithinNearbyContainer 1/1 pass post-Slice-4a.

### Pending slices (next session)

- **Slice 4a: `_Mutation.cpp`** — move TryAddItem*, Internal_Add/Remove/Move,
  TryAddItemAtPosition, and Server_AddItem/_Remove/_MoveItem/_Drop RPC impls
  (~840 LOC). Target: main.cpp under 1700 LOC.
- **Slice 4b: `_Equipment.cpp`** — move Internal_Use/Equip/Unequip and
  Server_Use/Equip/Unequip/SwapHands RPC impls (~360 LOC). Target:
  main.cpp under 1400 LOC.
- **Slice 4c: `_Save.cpp`** — move save/load helpers (~150 LOC).
- **Slice 4d: `_Queries.cpp`** — move FindEntry, view helpers, query
  helpers (~300 LOC).
- **Slice 4e: `_Logging.cpp`** — move LogItemDataResolveState,
  LogMoveReject, RejectMove (~180 LOC).

If Slices 4a-4e all land, main.cpp drops to ~300 LOC (lifecycle + Request
wrappers + OnRep + HandleAction only). Component becomes a thin orchestrator;
behavior unchanged.

### Longer-term (pre-existing bug + callspace axis)

Slices 2-5 of the original reviewer plan (server-gated UWorldSubsystem
authority subsystem, NetEndpoint RPC split, `IWorldContainerAuthority`
storage-side primitive) remain open. Those are the REAL architectural
work; Slices 1-4 above are just file hygiene and do not fix the
`ResolveLocalPlayerSessionSubsystem`-from-authority bug.

---

## Target 1: Split `UProjectInventoryComponent` - the right axis

**Adopted from external architecture review 2026-04-23.** The review
argued convincingly that my initial split-by-interface plan was on the
wrong axis. UE's model wants a split by **callspace** (local-client,
owner-bound RPC edge, server authority, server domain, storage
authority), not by "N sibling components on the same pawn all implementing
an interface each." The review's recommendation is adopted as the final
target; the earlier 4-component plan below is kept only as a historical
record of the wrong take.

### Pre-existing architecture bug (surfaced by the review)

`ResolveLocalPlayerSessionSubsystem(this)` is called from **authority-side**
code paths inside `StoreInventoryEntryInWorldContainer_Implementation`,
my new `MoveWithinWorldContainer_Implementation`, and likely other world-
container methods on `UProjectInventoryComponent`. `ULocalPlayerSubsystem`
is client/local-player lifetime - it is the wrong source of truth for
authoritative session validation. On a pure dedicated server there is no
local player, so this path is broken; on listen-server / PIE it happens
to work only because the local player exists for the same connection.

This is a real bug but it is NOT introduced by the current world-to-world
fix - the pattern pre-dates it. The fix for this bug is **target-1 of
this refactor**, not a separate hotfix, because the correct fix requires
a server-side authority subsystem that does not exist yet.

### UE model reality

- UE is server-authoritative. Owning connection flows PC -> pawn ->
  components; that ownership is what makes Server RPCs legal on a
  component. [Epic docs on Actor Owner + Owning Connection](https://dev.epicgames.com/documentation/ar-ar/unreal-engine/actor-owner-and-owning-connection-in-unreal-engine).
- Component RPCs work but have more overhead than actor RPCs; Epic
  recommends routing through the actor when practical.
  [Epic docs on Replicating Actor Components](https://dev.epicgames.com/documentation/ar-ar/unreal-engine/replicating-actor-components-in-unreal-engine).
- `ULocalPlayerSubsystem` is client/local-player only.
  `UWorldSubsystem::ShouldCreateSubsystem` can gate a subsystem to
  server only. That is the correct home for authoritative session
  validation.
- `GameMode` is server-only but too rules-oriented; `GameState` is
  replicated-global. Neither is the right home for per-session
  world-container transport.

### Target split (callspace-aligned)

#### Layer 1. Local intent (client only)

- `UInventoryUIDragHostSubsystem` (local-player subsystem, as today)
- `UInventoryViewModel`
- `FInventoryDropRouter`
- preview / hover / cell resolution

Responsibility: build a command DTO. No authority branching. No
rollback. No `HasAuthority()` checks outside optional UX guards.

#### Layer 2. Network edge (thin, owner-bound, single RPC hop)

One of:
- `APlayerController`-owned RPC endpoint (preferred long-term; matches
  Epic's lower-overhead actor RPC recommendation), OR
- one dedicated replicated component on the owning pawn/PC:
  `UProjectInventoryNetEndpointComponent`

Responsibility:
- own the `Server_Request*` RPCs
- caller-ownership + obvious input sanity
- forward to the server service layer
- return failures via one client error RPC path

The edge MUST NOT own Consume/Store/rollback rules.

#### Layer 3. Authority / session validation (server only)

`UProjectWorldContainerSessionAuthoritySubsystem : UWorldSubsystem`

Responsibility:
- validate `FContainerSessionHandle`
- verify the caller is allowed to act on that session
- resolve target actor / container authority object
- choose + call the correct domain operation
- anti-cheat / range / stale-session checks

Why a `UWorldSubsystem` and not a `ULocalPlayerSubsystem`:
- world lifetime
- can be created server-only via `ShouldCreateSubsystem` returning
  `IsRunningDedicatedServer() || IsListenServer()` (or similar; audit
  against Epic docs at implementation time)
- no fake dependency on a client-scoped subsystem

#### Layer 4. Domain mutation (server only, no RPC)

Small operation objects or one service class with small methods. Example:

```cpp
struct FWorldContainerMoveOp {
    static bool Execute(UObject* ContainerAuthority,
                        const FContainerSessionHandle& Session,
                        int32 EntryInstanceId,
                        int32 Quantity,
                        FIntPoint TargetGridPos,
                        bool bTargetRotated,
                        FText& OutError);
};
```

Or `UProjectInventoryTransferService` with `MoveWithinWorldContainer`,
`TakeFromWorldContainer`, `StoreToWorldContainer` as free-standing
server-side domain methods.

Responsibility: pure domain mutation, rollback, overlap rules, quantity
clamping. NO ownership / network code. NO UI / toast code.

**This is where the current consume+store+rollback body should land.**
It lives today in `UProjectContainerSessionSubsystem::MoveWithinWorldContainerSession`
as a 2026-04-23 first iteration; target 1 moves it again, off the
local-player subsystem and into the server-side domain layer.

#### Layer 5. Storage authority (container-side, not player-side)

World-to-world rearrangement is a **container storage** concern, not
a player inventory concern. Long-term the atomic move primitive should
live on the world container authority:

```cpp
IWorldContainerAuthority::TryMoveEntryWithinSession(...)
```

For player <-> world transfers a coordinator is legitimate because two
aggregates are involved - the coordinator can live in Layer 4.

### Target data flow

```
UI / VM / DragHost (client)
  -> build FWorldContainerMoveCmd
  -> PlayerController / NetEndpoint Server RPC
  -> WorldContainerSessionAuthoritySubsystem (server, UWorldSubsystem)
       validates session + ownership + range
  -> FWorldContainerMoveOp::Execute on container authority
       consume + store + rollback
  -> replication updates clients
  -> optional client-side error RPC on failure
```

### Slice plan (planning-first; do not start execution blind)

- **Slice 0 (now, this todo):** document the target above + freeze
  current growth pattern (no more Server RPCs on `UProjectInventoryComponent`
  beyond what already shipped).
- **Slice 1:** extract the consume+store+rollback body from
  `UProjectContainerSessionSubsystem::MoveWithinWorldContainerSession`
  into `FWorldContainerMoveOp::Execute` (pure static helper, no UObject).
  Subsystem becomes a thin caller. Measure: LOC moves; no behavior change.
- **Slice 2:** create `UProjectWorldContainerSessionAuthoritySubsystem`
  as `UWorldSubsystem` with server-only creation gate. Move session
  validation (`IsSessionActive`, caller-ownership checks) there.
  `UProjectContainerSessionSubsystem` (local-player) stops hosting
  authority validation; keeps only client-side session-handle cache.
- **Slice 3:** create `UProjectInventoryNetEndpointComponent` OR a PC
  RPC endpoint (decide at implementation time; prefer PC per Epic's
  overhead note). Move `Server_Request*` RPCs off `UProjectInventoryComponent`.
- **Slice 4:** `UProjectInventoryComponent` is now only player inventory
  aggregate (list + replication + local add/remove/move + equip). Confirm
  it drops under 1500 LOC.
- **Slice 5:** long-term - world-to-world move primitive migrates to
  `IWorldContainerAuthority`. Container becomes the mutation owner.

Each slice is reversible; each passes the 121+ inventory suite green.

### Exit criteria for Target 1

- `UProjectInventoryComponent.cpp` under 1500 LOC.
- No Server RPCs on `UProjectInventoryComponent` after slice 3.
- No `ULocalPlayerSubsystem` call from an authority-only path anywhere
  in the world-container flow.
- `UWorldSubsystem`-scoped session authority service exists and is used.
- A pure-logic `FWorldContainerMoveOp` (or equivalent service) exists.
- Full 121+ inventory test suite green at each slice.

### Historical (what NOT to do)

The earlier draft of this todo proposed splitting `UProjectInventoryComponent`
into 4 sibling components each implementing one interface
(`UProjectEquipmentComponent`, `UProjectWorldContainerBridgeComponent`,
`UProjectInventoryActionReceiverComponent`). That split is on the wrong
axis: all four components would still sit on the same owning pawn, all
four would still be mixing client intent + authority + domain inside
one class each. It would have reduced individual file size without
fixing the architectural category error. The callspace-aligned split
above is the correct target.

---

## Target 2: Mandatory agent rule (AGENTS.md update)

Today's rule in `AGENTS.md` / user `CLAUDE.md`:

```
Code Architecture - NO MEGA FILES (CRITICAL!)
- MAX 1000 lines per file (hard limit)
- If approaching or exceeding limit: suggest a better approach and
  discuss with user (don't auto-split)
- Prefer <300 lines
```

That rule is correct but has no enforcement + no escalation protocol.
What actually happens: agent adds 170 lines to a 4000-line file without
noticing.

### Proposed rule upgrade (copy into AGENTS.md under CRITICAL Rules)

> ### FILE SIZE GUARDRAIL (CRITICAL!)
>
> **Before editing any `.cpp` / `.h` / `.ps1` / `.py` file, check its
> current line count.**
>
> ```bash
> wc -l <target-file>
> ```
>
> **Hard rule:**
> - **< 700 lines:** edit freely.
> - **700-1000 lines:** ok to add but write a one-line "why this grew"
>   note in the commit; consider the next SRP seam.
> - **>= 1000 lines AND your edit adds more than ~30 lines:** STOP.
>   The file is already over. Do NOT silently add more. Either:
>   1. Land the change in a new sibling file / helper / subsystem
>      (preferred - respects SRP).
>   2. Open a planning exchange with the user: "File X is Y LOC; my
>      change adds Z; extraction options are A, B, C; proceed as-is
>      or split first?"
> - **Pre-existing mega-files** (listed in the baseline above) are a
>   known debt. Adding to them requires either a clear SRP-consistent
>   reason (e.g. the new code is on the same responsibility as the
>   file's existing one) OR an explicit user greenlight.
>
> **Forbidden:**
> - Silently adding > 30 lines to a file already over 1000 LOC without
>   one of the above options.
> - "It'll be one more feature then we refactor" without a dedicated
>   refactor todo opened at the same time.
>
> **Check before commit:**
> ```bash
> # Lists all first-party source files over 1000 LOC (excludes
> # test files, Intermediate/, and third-party Local plugins).
> find Plugins/Foundation Plugins/Systems Plugins/Features \
>      Plugins/Gameplay Plugins/UI Plugins/Resources Plugins/World \
>      Plugins/Boot Source -type f \( -name "*.cpp" -o -name "*.h" \) \
>   | grep -v Intermediate/ | grep -v "\.gen\." \
>   | xargs wc -l 2>/dev/null \
>   | awk '$1 >= 1000 && $2 != "total" {print}' \
>   | sort -rn
> # The set MUST not grow. New entries are a commit blocker.
> ```

### Why this phrasing

- **The "stop and ask" threshold is NOT zero edits on a mega-file** -
  that would block legitimate bug fixes. It's "adds more than ~30 lines."
- **The check is a list, not a count.** Regressions show up as a new
  entry, which is visible and actionable.
- **Baseline is explicit** so the rule has a reference point.

---

## Target 3: Pre-commit hook (optional, deferred)

A CI check that runs the find+wc command above against the committed
tree and fails if a new first-party source file crosses 1000 LOC, using
a snapshot of the baseline as the allow-list.

Not in this slice; implement after Target 1 + Target 2 land and we see
whether manual discipline holds.

---

## Deliverables

### Must land

1. AGENTS.md "FILE SIZE GUARDRAIL" rule added under CRITICAL Rules, with
   the baseline list and the check command above.
2. ALIS baseline documented in the rule itself so future agents have a
   concrete reference.
3. A dedicated planning pass (separate todo or inline here) before the
   component split starts. Do NOT start the split in this slice.

### Should land (next slice)

4. Slice A of the component split (`UProjectWorldContainerBridgeComponent`
   extraction). Smallest, validates the pattern.

### Nice to have

5. Pre-commit hook that enforces Target 2's check.
6. Slice B, C, D execution.
7. Audit pass for UI-side + other plugins at the 700-1000 "warning" range
   so we spot the next offender early.

---

## Cross-refs

- AGENTS.md CRITICAL Rules section (where the new rule lands).
- docs/agents/canonical.md `Pimpl + internal-helper pattern` subsection -
  existing guidance on splitting big UCLASS `.cpp` files. Complementary
  to this todo: pimpl handles WITHIN-file split; this todo handles
  BETWEEN-component split.
- AGENTS.md `~/.agents/AGENTS.md` user-level rule (original source of
  "NO MEGA FILES") - this is the project-level upgrade with enforcement.

---

## Non-goals / forbidden shortcuts

- Do NOT mass-rename / bulk-delete. Each split slice is a reversible commit.
- Do NOT "just delete the rule" when it feels inconvenient. The rule
  exists because 4000-line components really do cost weeks of mental
  load on every subsequent edit.
- Do NOT apply this rule to generated `.gen.cpp` under `Intermediate/`,
  to vendor code under `Plugins/Local/`, or to `InstanceArrayTool` (not
  our code).
- Do NOT try to make the rule "only count non-comment non-blank lines."
  LOC is the honest proxy. A 4000-line file is hard regardless of what
  the tokens are.
