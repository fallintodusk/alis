# Inventory UI — Decouple Nearby Container Panel

**Status:** REOPENED — design re-ratified 2026-04-21 (third revision, engine-source verified). Use Slate primitives where they exist; add only a thin domain layer above. Slices 16-20 supersede earlier "overlay widget + no-widget-drop-override" rule, which reinvented engine wheels.
**Priority:** Major
**Date started:** 2026-04-20
**Date completed:** 2026-04-20 (initial decouple); reopened 2026-04-21 (engine-aligned rewrite)
**Related:** [inventory_ui_bugs.md](inventory_ui_bugs.md) | `Plugins/Features/ProjectInventory/docs/design_vision.md:356-387`

---

## TL;DR — authoritative design lives in ONE place

**Read "SOLID Golden Design — Engine-Aligned (2026-04-21, FINAL)" below. That section is the current contract. Everything else in this file is either (a) supporting investigation (Engine-Source Investigation, Reviewer divergence — all ratified and current), (b) per-slice landed history (a dated log of what shipped), or (c) explicitly-superseded plan material under the "History" banner near the bottom.**

Quick map of what's authoritative vs historical:

| Section | Status |
|---|---|
| TL;DR + this navigation block | Current |
| Engine-Source Investigation | Current (why we built what we built) |
| Reviewer divergence + ratification | Current |
| SOLID Golden Design — Engine-Aligned (FINAL) | **Authoritative design** |
| Execution plan (Slices 16-20) inside FINAL | Current |
| Orchestration & Execution Speed — Investigation + Fixes | Current (test infra) |
| Everything under "History" banner | Superseded; kept for provenance |
| Slice history log (6a-15, 16-20 entries) | Dated landed-work records; still useful |

---

## Engine-Source Investigation (2026-04-21)

Before committing to a structure I dug through UE 5.7 source at `<ue-path>` and Epic's current docs. Two prior "golden design" rewrites in this file were partly correct but each reinvented pieces UE already ships. Recording the evidence here so the next revisit doesn't repeat the research.

### What UE already gives us (do not reinvent)

| Engine-provided | Source | What it gives | Implication for Alis |
|---|---|---|---|
| `FDragDropOperation::CreateCursorDecoratorWindow` | `Engine/Source/Runtime/SlateCore/Public/Input/DragAndDrop.h` | Transparent top-level `SWindow` that Slate opens during drag, follows cursor, invisible to hit-test. | We do NOT need `SInventoryDragOverlay`. The drag visual layer already exists. |
| `UDragDropOperation::DefaultDragVisual` + `Pivot` + `Offset` | `Engine/Source/Runtime/UMG/Public/Blueprint/DragDropOperation.h:78` | UMG adapter that plugs into the Slate decorator window. Set a widget here; it follows the cursor for free. | Our drag visual is a one-line assignment on the DragOp. Zero overlay widget. |
| `FEventRouter::FBubblePolicy` for `OnDragOver` / `OnDrop` | `Engine/Source/Runtime/Slate/Private/Framework/Application/SlateApplication.cpp:5523, 5827` | Slate dispatches drag events to every widget under the cursor, leaf-to-root, stopping at first `Handled`. | Cells SHOULD override `NativeOnDrop`/`NativeOnDragOver`. The bubble router IS the "drop on whichever target the cursor is over" behavior. |
| `FSlateApplication::GetDragDroppingContent()` / `IsDragDropping()` | `Engine/Source/Runtime/Slate/Public/Framework/Application/SlateApplication.h:877-883` | Global live query: "is a drag happening? what is it?" | Useful as a diagnostic probe. NOT the architectural SOT — it has no domain knowledge (source tag, preview validity, reject reason). |
| `UWidgetBlueprintLibrary::DetectDragIfPressed / CreateDragDropOperation / CancelDragDrop` | `Engine/Source/Runtime/UMG/Public/Blueprint/WidgetBlueprintLibrary.h` | Standard drag lifecycle helpers. | Source cells already use `DetectDragIfPressed`. Keep. |
| `ULocalPlayerSubsystem::PlayerControllerChanged` | `Engine/Source/Runtime/Engine/Public/Subsystems/LocalPlayerSubsystem.h` | Per-local-player lifetime; survives widget churn; reacts to controller swap. | Correct lifetime bucket for the drag session. Already used. |
| `FSlateDebugging` input/focus/cursor/navigation broadcasts | `Engine/Source/Runtime/SlateCore/Public/Debugging/SlateDebugging.h` | Engine-side debug hooks for Slate event flow. | Use for live diagnosis before adding our own logs. |
| Widget Reflector + Slate Insights (Unreal Insights plugin) | Epic docs | Pick-hit-testable-widgets, widget-event stream, per-frame paint/invalidation. | Hit-test regressions (our biggest class) are already debuggable by engine tooling. |
| `FAutomationSpecBase` + `FUntilDoneLatentCommand` | `Engine/Source/Runtime/Core/Public/Misc/AutomationTest.h:2885` | BDD-style test shape + frame-pumping latent commands. | Matches drag-test needs (tick between mouse events). |
| `FSlateApplication::ProcessMouseButtonDownEvent / ProcessMouseMoveEvent / ProcessMouseButtonUpEvent` | `SlateApplication.cpp:5193, 6269, 6009` | Synthetic input entry points with full pointer-event semantics. | Drive E2E tests through these. Must tick between moves (cached geometry) and cross `GetDragTriggerDistance()` to trigger drag. |

### What UE does NOT give us (thin domain layer above)

| Missing primitive | Why we need it | Proposed shape |
|---|---|---|
| Typed drag-session domain state (source tag, preview snapshot, reject reason) | `FSlateApplication` only knows THAT a drag exists, not WHAT it means for inventory. | `FInventoryDragSession` owned by `UInventoryUIDragHostSubsystem`. |
| Structured event stream for domain decisions (DropResolved, DropRejected, Routed, VMInvoked) | Slate debug broadcasts are at input-event level; tests can't subscribe to "dispatcher rejected with reason=SelfOverlap". GAS events are ability-shaped; Lyra GMR isn't in stock UE 5.7; `FAsyncGameplayMessageSystem` is experimental + async (breaks deterministic tests). | `DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryDragEvent, const FInventoryDragEvent&)` on the subsystem. |
| Test-side sequence assertion over domain events | UE Automation has `TestEqual` etc. but no `TestSequence`. | `FInventoryDragEventRecorder` test helper, subscribes in `BeforeEach`, asserts ordered match. |
| Tag-keyed validation-policy provider (replaces widget-closure checkers that closed over global Slate state — this was the equip-backpack bug class) | Today surfaces register with `OccupantAllowedChecker` lambdas that reach into global Slate state; when that state is stale the lambda rejects every cell. | Dispatcher asks VM or a policy interface keyed by surface tag. Lambdas in registration path are banned. |

### What the engine CANNOT fix for us (the actual live bug class)

Our repeating bug pattern is NOT "missing overlay" — it is **UMG `Visibility` misconfiguration breaking Slate's bubble router**:

- Root widget set to `SelfHitTestInvisible` — bubble enters the child path but when no child accepts, the bubble can't re-enter the root; drop is dropped.
- Sibling layout (second widget with `Fill` size policy) sits on top of the intended drop target — bubble delivers to the wrong widget.
- Decorative overlays left as `Visible` — swallow events they don't handle.

Engine debugging tools surface these (Widget Reflector "Pick Hit-Testable Widgets"), but engine can't enforce the rule. We enforce with:

1. Explicit `Visibility` contract per widget (documented in each `W_*.h` header).
2. Framework fitness test: dump tree on open + assert every user-widget root and every drop-target child has the contracted visibility.
3. Event-bus sequence assertion: if `DropResolved` never fires in the recorded stream, a specific named test fails with the mismatched step, pointing at the regression.

### Reviewer divergence — where we disagreed, which direction we chose, and final ratification

An external reviewer independently proposed a parallel design. After circulating the engine-source evidence, reviewer **ratified** this direction on 2026-04-21 with one refinement adopted below ("smallest semantic drop target", not strictly cells).

| Topic | Reviewer position | Our decision | Rationale (engine-source citations) |
|---|---|---|---|
| **"One overlay surface owns drag-over/drop/cancel during active drag"** | Initially proposed a single UMG overlay widget that becomes hit-testable during a drag and owns all drag-over/drop callbacks. **On second round reviewer withdrew this** and agreed: "do not add a custom UMG drag overlay ... adding a custom overlay creates another hit-test surface you then have to debug." | **REJECTED.** Smallest-semantic-drop-target widgets own `NativeOnDragOver`/`NativeOnDrop`; trivial forwarders to the subsystem. | `FDragDropOperation::CreateCursorDecoratorWindow` (`SlateCore/Input/DragAndDrop.h`) already opens a transparent top-level `SWindow` during drag — it is the engine's drag-active overlay, hit-test-invisible by design. Slate's `FEventRouter::FBubblePolicy` (`SlateApplication.cpp:5523, 5827`) already delivers `OnDrop` to every widget under the cursor leaf-to-root, stopping at first `Handled`. A UMG overlay above the viewport adds a **second** hit-test layer that fights the first — the same class of bug that broke us in the first place. Idiomatic UE: let the Slate bubble route; put the drop target at the bottom. |
| **"Smallest semantic drop target" generalization** (reviewer's refinement on ratification) | Don't phrase the rule as "cells own drop" — phrase it as "the smallest semantic drop target under the cursor owns drop". For inventory grids that is a cell; for equip it may be an equip-slot widget; future non-grid targets (chest slots, vendor slots, crafting ingredient slots) fit without another rewrite. | **ACCEPTED.** Invariant #1 reworded to "smallest-semantic-drop-target widgets". Slice 17 introduces an `IInventoryDropTarget` marker interface so the fitness test (Slice 20) is role-based, not class-name-based. | Prevents the rule from rotting the first time equip or a non-grid surface joins. Role > concrete class. |
| **"Widgets should stop owning routing"** | Split concerns: cell = source click, subsystem = session, controller = geometry, dispatcher = validity, router = command. | **ACCEPTED.** Same split adopted. | Reviewer's SOC matches ours. No divergence on this. |
| **Visual-layer strategy** | Reviewer does not specify; implies overlay widget also provides the preview visual. | Use `UDragDropOperation::DefaultDragVisual` + `Pivot` + `Offset`. | `UMG/Blueprint/DragDropOperation.h:78` — assigning `DefaultDragVisual` causes `SObjectWidget::OnDragDetected` (`SObjectWidget.cpp:371`) to wrap the widget in the Slate decorator window automatically. Zero overlay code, zero hit-test concerns, free cursor-follow. |
| **Observability on top of engine tooling** | Use Widget Reflector + Slate Insights + `FSlateDebugging` first; add one domain event stream on top. | **ACCEPTED — reviewer was ahead of me here.** Added Widget Reflector / Slate Insights / `FSlateDebugging` as the engine-side diag path in the investigation table; our `FOnInventoryDragEvent` is explicitly positioned as "thin domain layer above". | No divergence. Reviewer's phrasing is sharper than my original draft — adopted verbatim. |
| **`GetDragDroppingContent()` as contract** | "fine as a live query, but weak as your architectural source of truth". | **ACCEPTED** and promoted to invariant #4 (no closures capturing this). | This is exactly the equip-backpack bug class documented in `Plugins/UI/ProjectInventoryUI/docs/pitfalls.md`. Reviewer diagnosed it cleanly; we enforce with CI grep. |
| **Validators as widget closures** | Implied-but-not-explicit that this is the old mess to avoid. | **STRENGTHENED.** Made it invariant #4 + `IInventorySurfacePolicyProvider` type + grep-based fitness test. | Concrete shape beats implication; prevents regression. |
| **Custom hit-test debugger** | "Do not build your own — use Widget Reflector." | **ACCEPTED.** Not in scope. | No divergence. |
| **Testing primitives** | `FAutomationTestFramework::EnqueueLatentCommand` + CQTest `FRunSequence` / `FExecute`. | **ACCEPTED** (variant): `FAutomationSpecBase` + `FUntilDoneLatentCommand`. | Same engine mechanism, different helper class. `FAutomationSpecBase` (`AutomationTest.h:2885`) already wraps BDD-style specs + latent pumps and is what existing Alis tests use. CQTest helpers are compatible; we don't need both. |
| **ULocalPlayerSubsystem as session lifetime** | Correct bucket. | **ACCEPTED.** Already present. | No divergence. |
| **"Do not reinvent hit-test debugging first"** | Use Widget Reflector before custom tooling. | **ACCEPTED** explicitly. | No divergence. |

### Why we chose this direction (single-line rationale)

**Let Slate do what Slate already does (bubble to the smallest-semantic-drop-target, decorator-window visual, session-pointer query); add exactly the thin domain layer the engine is missing (typed session state, structured event stream, tag-keyed policy provider); enforce the `Visibility` contract with tests because the engine can't.**

Both sides considered the overlay approach. It is defensible — centralized drag-over/drop on one widget IS easier to reason about in isolation — but it costs an extra hit-test layer, diverges from Slate's designed event flow, and doesn't actually prevent the real bug class (`Visibility` misconfiguration still breaks routing under an overlay, just in subtler ways). Smallest-semantic-drop-target-owns-drop + CI-enforced visibility contract + event-bus observability is smaller, more idiomatic, and catches the bug class at the rule that actually causes it. Reviewer concurred on second pass.

### Two cautions from reviewer (locked into invariants)

1. **Don't turn "cells own drop" into a religion.** Rule is "smallest semantic drop target owns drop" — for grids that is a cell, for equip it is a slot widget. Slice 17's `IInventoryDropTarget` marker interface and Slice 20's role-based fitness test enforce the principle, not the class.
2. **Don't depend on `GetDragDroppingContent()` for validation or routing.** Keep it for debug/sanity probes only. The subsystem's `FInventoryDragSession` stays authoritative — enforced by invariant #4 (no closures capturing global Slate state in validation paths) and a grep-based fitness test.

---

### Reconciliation with prior "golden design" attempts in this file

| Prior proposal | Engine verdict | Action |
|---|---|---|
| `SInventoryDragOverlay` (full-screen hit-testable-during-drag widget) | REDUNDANT — Slate's cursor decorator window already is the drag-active overlay. A UMG overlay adds a second hit-test layer that fights the first. | REMOVED from plan. |
| Invariant: "Widgets must not override `NativeOnDragOver` / `NativeOnDrop`" | WRONG — inverts the Slate bubble contract. Epic's own drag-drop docs explicitly show drop targets overriding these. | REMOVED. Replaced with: "Only `UProjectGridCell` overrides drag handlers; user-widget roots must not." |
| Child drop-zone widget per grid host (`UW_InventoryDropZone` on `NearbyGridHost` etc.) | REDUNDANT with cell-level `OnDrop`. Cells already are the smallest hit target; wrapping them in a drop-zone adds a layer without adding information. | REMOVED. Cells are the drop targets. |
| `ULocalPlayerSubsystem` as drag-session owner | CORRECT. Matches engine subsystem lifetime. | KEEP. |
| Event bus (`FOnInventoryDragEvent` multicast) | CORRECT. Engine has no equivalent; GMR isn't in stock UE 5.7; Trace is wrong tool (out-of-process, async, not test-queryable). | KEEP. |
| Tag-pair-to-VM-command router | CORRECT. Single SOT for command mapping; parent-tag matching via `MatchesTag` supports adding surfaces without table churn. | KEEP. |
| Dispatcher (validation) between controller and router | CORRECT. Separates geometry resolve (controller), inventory validity (dispatcher), command mapping (router). | KEEP. |
| Widget closures as validation checkers | REJECTED — caused equip-backpack bug by reaching to global Slate state at lambda-capture time. | BANNED. Dispatcher asks VM or surface-policy provider by tag. |

---

## SOLID Golden Design — Engine-Aligned (2026-04-21, FINAL)

### One-sentence target

**The smallest semantic drop target under the cursor owns `NativeOnDrop` (Slate bubble contract) and forwards to the subsystem via a minimal context struct; subsystem owns session + dispatches through dispatcher + router + emits structured events; widgets paint preview by pull from a read-only snapshot.**

"Smallest semantic drop target" = the widget that knows exactly which domain slot the pointer is over. For inventory grids that is `UProjectGridCell`. For equip it is an equip-slot target widget (`UW_EquipSlot` or similar), not a grid cell. The principle generalizes without rewrite; cells are the current concrete case, not the rule.

### Data-flow

```
 Slate (engine)
   decorator window = UDragDropOperation::DefaultDragVisual (auto)
   bubble policy delivers OnDragOver / OnDrop leaf-to-root

 Input layer (widget-bound, trivial)
   UProjectGridCell::NativeOnMouseButtonDown -> DetectDragIfPressed
   UProjectGridCell::NativeOnDragDetected    -> Subsystem::BeginCellDrag(CellContext) + return DragOp
   UProjectGridCell::NativeOnDragOver(Geom)  -> Subsystem::UpdatePreview(CellContext, Geom.AbsolutePosition)
   UProjectGridCell::NativeOnDrop(Geom)      -> Subsystem::CompleteDrop(CellContext, Geom.AbsolutePosition) => FReply::Handled|Unhandled

 Pipeline (subsystem-owned, widget-free)
   UInventoryUIDragHostSubsystem  (ULocalPlayerSubsystem)
     |-- FInventoryDragSession           (PoD state)
     |-- FProjectUIGridDragDropController (geometry -> tag+cell)
     |-- FInventoryDragDispatcher        (tag+cell+payload -> allow/reject + reason)
     |-- FInventoryDropRouter            (source tag + target tag -> VM method)
     |-- FOnInventoryDragEvent           (multicast: Started/PreviewUpdated/DropResolved/DropRejected/Routed/VMInvoked/Completed/Cancelled)
     \-- IInventorySurfacePolicyProvider (tag-keyed validation; no widget closures)

 Presentation (pull, no subscriptions from widgets)
   Widgets Tick/Invalidate paint from Subsystem::GetPreviewSnapshot()
```

### Rules (invariants, CI-enforced where possible)

1. **Drop handlers live only on smallest-semantic-drop-target widgets** (`UProjectGridCell` for grids; `UW_EquipSlot` or equivalent for equip; future non-grid targets register themselves against this role). User-widget roots (`UW_InventoryPanel`, `UW_NearbyContainerPanel`) must NEVER override `NativeOnDragOver` / `NativeOnDrop`. Enforced by reflection scan in Slice 20 via an allow-list of drop-target classes, not a single hard-coded class name.
2. **No widget `#include "MVVM/InventoryDropRouter.h"` or `"MVVM/InventoryDragDispatcher.h"`.** Subsystem-private. Enforced by grep in Slice 20.
3. **No widget builds `FProjectUIGridDragPayload` / `FInventoryDragContext` / `FInventoryDropTarget`.** Subsystem owns those types.
4. **No closures capture global Slate state for validation.** All checkers are either stateless or ask a policy provider by tag. Enforced by code review; surface-registration signature no longer accepts `OccupantAllowedChecker` as a closure — it takes an `FGameplayTag PolicyTag` and the subsystem looks up the policy implementation.
5. **Every subsystem state transition emits exactly one `FInventoryDragEvent`.** No silent rejects; `DropRejected` carries a named reason.
6. **User-widget roots' `Visibility` contract is declared in the header and asserted by a framework test.** Root is `Visible` unless a strict reason demands otherwise, and if `SelfHitTestInvisible` is used, the test lists which named child must still be hit-testable.
7. **Equip drops go through the same pipeline.** Equip slots register with a distinct surface tag; router maps the pair to `RequestEquipItem`. No widget-level inline branch.

### Components

#### `UInventoryUIDragHostSubsystem` (extended)

```cpp
// Drag lifecycle — called from UProjectGridCell's engine handlers
TOptional<UInventoryDragDropOperation*> BeginCellDrag(const FCellContext& Src);
void UpdatePreview(const FCellContext& Candidate, const FVector2D& ScreenPos);
FReply CompleteDrop(const FCellContext& Candidate, const FVector2D& ScreenPos);
void CancelDrag();

// Pull for presentation
FInventoryPreviewSnapshot GetPreviewSnapshot() const;

// Observability (tests + diagnostic logger subscribe here)
FOnInventoryDragEvent OnDragEvent;

// Surface registration (no closures)
void RegisterSurface(const FInventorySurfaceRegistration& Reg); // {SurfaceTag, Grid, Dims, PolicyTag}
void UnregisterSurface(FGameplayTag SurfaceTag);
```

Widgets talk only to this API (plus the visibility/layout concerns they already own).

#### `FInventoryDragEvent` (observability SOT)

```cpp
UENUM() enum class EInventoryDragEventKind : uint8 {
    Started, PreviewUpdated, PreviewCleared,
    DropResolved, DropRejected,
    Routed, VMInvoked,
    Completed, Cancelled,
};

struct FInventoryDragEvent {
    EInventoryDragEventKind Kind;
    FGameplayTag SourceTag;
    FGameplayTag TargetTag;
    FIntPoint SourceCell = FIntPoint(-1, -1);
    FIntPoint TargetCell = FIntPoint(-1, -1);
    int32 InstanceId = INDEX_NONE;
    int32 Quantity = 0;
    FName RejectReason;          // DropRejected only
    FName VMMethod;              // VMInvoked only
    double TimestampSeconds = 0.0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryDragEvent, const FInventoryDragEvent&);
```

Production ships a `FInventoryDragLogger` (compiled out in Shipping) that subscribes and writes a one-line-per-event summary to `LogInventoryDragHost` Verbose. Turn-on with `inv.drag.log 1` for bug reports.

### Execution plan (slices)

Each slice ships with its own action-level test, sabotage-verified.

- **Slice 16 — session state + event bus + logger.** `FInventoryDragSession`, `FInventoryDragEvent`, `FOnInventoryDragEvent`, `FInventoryPreviewSnapshot`. Subsystem grows the new API. Today's cell paths fire the new methods internally — behavior unchanged, observability added. Ship `FInventoryDragEventRecorder` test helper + `FInventoryDragLogger`. Tests: event emission shape (Started on BeginCellDrag, Cancelled on timeout, etc.), 5 cases.
- **Slice 17 — smallest-semantic-drop-targets own drag handlers; strip from user widgets.** Move `NativeOnDragOver` / `NativeOnDrop` from `UW_InventoryPanel` and `UW_NearbyContainerPanel` down to `UProjectGridCell` (inventory grids) AND equip-slot target widgets (when equip is migrated — may land in Slice 19). Introduce an `IInventoryDropTarget` marker/interface the allow-list fitness test scans for, so the rule is role-based, not class-name-based. User-widget roots get a contracted `Visibility` (`Visible` unless justified). First E2E test: `DragFromNearbyCellDropOnMainBackpackEmitsExpectedEventSequence` via synthetic `FSlateApplication::ProcessMouse*` + latent tick pumps + `FInventoryDragEventRecorder`. Sabotage: force user-widget root to `SelfHitTestInvisible` without child fallback -> test fails with "expected PreviewUpdated, got nothing". Second E2E: main->nearby. Third: within-main (split-self-overlap).
- **Slice 18 — ban widget closures in surface registration.** Replace `OccupantAllowedChecker` lambdas with `PolicyTag` + `IInventorySurfacePolicyProvider` resolved by subsystem. Dispatcher calls the provider. Delete the helper that built lambdas from widget state. Existing `BackpackEmptyCellDropAccepts` + `PayloadCarriesFullSourceContext` + `PlayerGridTagReRegistersOnTabChange` stay green. Add `NoSurfaceRegistrationUsesWidgetClosures` fitness test (grep for `[this`/`[=`/`[&` in `*Surface*Registration*` code paths; fail if found in `Plugins/UI/**/Widgets/*.cpp`).
- **Slice 19 — equip slots through the pipeline.** Equip slots register with `Item.Equip.*` surface tags; router gets equip rows. Delete inline equip branch in `W_InventoryPanel::NativeOnDrop` (which by Slice 17 is already relocated to the equip slot cell). E2E test: drag backpack->equip emits correct event sequence ending with `VMInvoked{VMMethod="RequestEquipItem"}`.
- **Slice 20 — architecture fitness tests (CI-enforced invariants).**
  1. `NoWidgetIncludesRouterOrDispatcher` — greps `Plugins/UI/**/Widgets/*.cpp`, fails on include of router/dispatcher headers.
  2. `OnlyDropTargetWidgetsOverrideDragHandlers` — UClass reflection scan: no widget class in `ProjectInventoryUI` may override `NativeOnDragOver` / `NativeOnDrop` unless it implements the `IInventoryDropTarget` marker interface. Role-based, so adding a new smallest-semantic-drop-target (equip slot, future crafting slot, chest slot) does not require the fitness test to change — registering the interface is enough.
  3. `UserWidgetVisibilityContracted` — opens each registered inventory widget via layer host; for each asserts the root's `Visibility` equals the contract declared in the header (parsed from a `VISIBILITY_CONTRACT(...)` macro or a virtual `GetRootVisibilityContract()` accessor — settle in the slice).
  4. `EveryDragSessionEmitsCompletedOrCancelled` — deterministic fuzz over 100 randomized sequences of Begin/Update/Complete/Cancel; every run ends with exactly one terminal event.

### Deferred cleanup / naming follow-ups (non-blocking)

Tracked here so they don't get rediscovered. Each is explicitly NOT being acted on now.

- **`UInventoryUIDragHostSubsystem::SetPolicyProvider(...)` is now underspecified.** After the `IInventoryDropCommandTarget` extraction (2026-04-22) the bound object satisfies two roles: `IInventorySurfacePolicyProvider` (preview-time policy) AND `IInventoryDropCommandTarget` (dispatch). One setter configures both. Reviewer call (2026-04-22): "rename later OR split only if a second implementation ever appears." No action now; revisit when either (a) a distinct command-target implementation exists, or (b) the next nearby refactor naturally touches this setter.
- **`AddExpectedError` quarantine in `InventoryLootPlaces.Session.InteractionHoldOpensWorldContainerSession`.** Kept as tracked temporary containment; real timing bug captured in [interaction_hold_world_container_timing.md](interaction_hold_world_container_timing.md). Must stay an exception, not become a standard testing style. If a second quarantine appears elsewhere, treat that as a design smell and investigate root cause instead of repeating the pattern.

### Non-goals for this rewrite

- Animation polish. Out of scope.
- VM split. `UInventoryViewModel` stays whole.
- ProjectUI drag controller rewrite. Stays — just stops being touched by widgets directly.
- Replacing the multicast delegate with Lyra GMR. Not in stock UE 5.7; revisit only if 3+ other plugins want the same event surface.
- UE Insights / Trace integration. Out-of-process, async. `FSlateDebugging` + `LogInventoryDragHost` is the live-diag path; Insights stays an optional developer tool.

### Explicit supersession

This section supersedes both prior "SOLID Golden Design" blocks dated 2026-04-21 earlier in this history. The `SInventoryDragOverlay` widget is REMOVED from the plan. The rule "widgets must not override `NativeOnDragOver`/`NativeOnDrop`" is REPLACED with "only `UProjectGridCell` does". Other invariants (no widget constructs payloads, no widget includes router/dispatcher, no closure-based validation, event bus on the subsystem, tag-pair router as command SOT) are retained.

---

---

# History — pre-FINAL plan + per-slice landed log

Everything from here until the "Orchestration & Execution Speed" section is **historical**. It describes the original plan framing (Problem / Goal / Principles / Implementation slices 1-15) that pre-dated the engine-source investigation and the FINAL design. The prescriptive content here has been superseded by the FINAL section; the slice history log (6a-15, 16-20 entries) is still useful as a dated record of what actually shipped. If you're here to understand the CURRENT system, scroll back up to "SOLID Golden Design — Engine-Aligned (FINAL)".

---

## Problem (historical framing)

`W_InventoryPanel` hosts both the main inventory and the nearby-container surface inside one auto-sized `Background` (`NearbySection` sits inside `ContentRow`). Toggling nearby loot reflows the single container, so the main inventory visibly jumps when a loot session opens or closes.

Design vision already mandates a **distinct right-side nearby-container panel** as a separate visual surface. Current coupling is a shortcut, not the SOT.

## Goal

Split the nearby container surface into its own widget, independently anchored.
- Main inventory stays stable and center-anchored, does not reflow when nearby toggles.
- Nearby widget toggles visibility on its own, right-anchored.
- Both widgets bind the **same** `UInventoryViewModel`.
- Cell size SOT is **single** and lives outside any one panel JSON.
- Drag/drop across widgets works through a player-scoped subsystem, routed by **gameplay tag** (not boolean primary/secondary).

## Review response (what the revised plan addresses)

Review (prior assistant turn) flagged eight items. This plan addresses each:

1. **Surface routing** — boolean `bSecondary` insufficient (three+ surfaces now). **Resolved** using existing global tag taxonomy in `ProjectGameplayTags.h:163-172` (`Item.Container.Hands`, `LeftHand`, `RightHand`, `Pockets` + `Pockets1..4`, `Backpack`, `WorldStorage`). No new enum.
2. **No next-tick widget coupling** — both widgets register through a player-scoped subsystem. No widget-to-widget direct reference.
3. **Shared `cellSize` SOT** — moved to `Data/InventoryUISettings.json` + `FInventoryUISettings` loader. `InventoryPanel.json` no longer carries it.
4. **"Slides in" dropped** — visibility toggles only. Animation out of scope.
5. **Shared presentation helper** — `FNearbyContainerPresentation` avoids duplication.
6. **Explicit drop command routing** — `FInventoryDropRouter` converts `{SourceTag, TargetTag}` into the correct VM command. Single SOT.
7. **Multi-surface drag test** — new focused test registers three surfaces and asserts correct tag resolution.
8. **Strong positives accepted** — one VM, no layer-host rewrite, separate nearby widget, shared grid sizing helper, `Collapsed` on hide.

## Principles

1. Layer-host registers both widgets via `ui_definitions.json` entries.
2. Both widgets bind the **same** `UInventoryViewModel` (`vm_creation: Global`, `scope: PerPlayer`).
3. `Data/InventoryUISettings.json` is the SOT for cell + padding geometry.
4. Visibility: main observes `bPanelVisible`; nearby observes `bPanelVisible && bHasNearbyContainer`. `Collapsed` when hidden (no reserved layout).
5. Surface identity is `FGameplayTag`. ProjectUI stays domain-agnostic; inventory supplies `Item.Container.*` tags.
6. Cross-widget coordination goes through `UInventoryUIDragHostSubsystem` (`ULocalPlayerSubsystem`). No direct widget references, no tick-deferred registration.
7. Drop routing is explicit and tag-driven in `FInventoryDropRouter`.

## Implementation slices

### Slice 1 — shared settings SOT (DONE)

- `Plugins/UI/ProjectInventoryUI/Data/InventoryUISettings.json`
- `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Public/Settings/InventoryUISettings.h`
- `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Private/Settings/InventoryUISettings.cpp`

`FInventoryUISettings::Get()` lazy-loads once per process. Falls back to in-code defaults if JSON missing or malformed (logs warning). Exposes `CellSize`, `GridSlotLineWidth`, `CellInnerPadding`, `HostOuterPadding`.

### Slice 2 — tag-indexed surface API (DONE: API surface + impl)

File: `Plugins/UI/ProjectUI/Source/ProjectUI/Public/Interaction/ProjectUIGridDragDropController.h`

Added alongside existing dual-grid API (kept during migration):
- `FProjectUIGridSurface { SurfaceTag, Grid, Dims, EnabledChecker, OccupantChecker, OccupantAllowedChecker }`
- `FProjectUIGridDragPreviewResult::TargetSurfaceTag` + `PreviewCellsBySurface` as SOT; boolean/primary/secondary mirrors retained for legacy callers.
- `FProjectUIGridDragDropController::RegisterSurface / UnregisterSurface / ClearSurfaces / HasSurface / GetSurfaceCount`.
- `UpdatePreviewOverSurfaces(ScreenPos, Payload)` — iterates registered surfaces, first hit wins.
- `ResolveDropTargetOverSurfaces(ScreenPos, Payload, OutSurfaceTag, OutCol, OutRow) const` — same iteration + footprint validation.

File: `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Interaction/ProjectUIGridDragDropController.cpp`

Added anonymous-namespace helpers `ResolveHitOnSurface`, `ValidateSurfaceFootprint`, then the public methods. Uses cached geometry directly (no dependency on `HitDetector` for the per-surface path, so the subsystem owns the controller cleanly without the hit detector needing to be shared).

### Slice 3 — subsystem bridge (DONE)

Created:
- `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Public/Subsystems/InventoryUIDragHostSubsystem.h`
- `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Private/Subsystems/InventoryUIDragHostSubsystem.cpp`

`ULocalPlayerSubsystem` owning one `FProjectUIGridDragDropController`. Thin passthroughs for `RegisterSurface`/`UnregisterSurface`/`ClearSurfaces`/`HasSurface`/`GetSurfaceCount`. `Deinitialize` clears all surfaces defensively. Build-verified.

Lifecycle (as implemented):
- Widgets call `GetLocalPlayer()->GetSubsystem<UInventoryUIDragHostSubsystem>()->RegisterSurface(...)` on build.
- Call `UnregisterSurface(Tag)` on destruct.
- Subsystem outlives widgets, so build-order doesn't matter.

### Slice 4 — drop router (DONE)

New files:
- `Public/MVVM/InventoryDropRouter.h` / `Private/MVVM/InventoryDropRouter.cpp`

```cpp
struct FInventoryDragContext
{
    int32 InstanceId = INDEX_NONE;
    FGameplayTag SourceSurfaceTag;
    FIntPoint SourcePos = FIntPoint(-1, -1);
    bool bSourceRotated = false;
    int32 Quantity = 0;
};

struct FInventoryDropTarget
{
    FGameplayTag TargetSurfaceTag;
    FIntPoint TargetPos = FIntPoint(-1, -1);
    bool bTargetRotated = false;
};

struct FInventoryDropRouter
{
    static void Route(UInventoryViewModel& VM,
        const FInventoryDragContext& Ctx,
        const FInventoryDropTarget& Target);
};
```

Routing (tag-match, so parent matches like `Item.Container.Hands` catch both hands):
- `Target.MatchesTag(Item.Container.WorldStorage)` -> `VM.RequestStoreItemInNearbyContainerAt(...)`.
- `Source.MatchesTag(Item.Container.WorldStorage)` -> `VM.RequestTakeNearbyItemToContainer(...)`.
- Both player-side -> `VM.RequestMoveItem(...)`.

Widgets never pick VM commands inline after this.

### Slice 5 — shared presentation helper (DONE)

New files:
- `Public/Presentation/NearbyContainerPresentation.h` / `.cpp`

```cpp
struct FNearbyContainerPresentation
{
    static FText BuildTitle(const UInventoryViewModel& VM);
    static FText BuildStats(const UInventoryViewModel& VM);
    static bool  ShouldShowTakeAll(const UInventoryViewModel& VM);
    static bool  ShouldShowHint(const UInventoryViewModel& VM);
};
```

### Slice 6a — dormant new widget + generalised builder (DONE)

Landed in this pass:
- `Plugins/UI/ProjectInventoryUI/Data/NearbyContainerPanel.json` - right-center anchored layout extracted from the old NearbySection subtree.
- `Public/Widgets/W_NearbyContainerPanel.h` / `Private/Widgets/W_NearbyContainerPanel.cpp` - new widget. Binds `UInventoryViewModel`, observes `bPanelVisible && bHasNearbyContainer` for visibility, builds its grid via the shared `FInventoryPanelGridBuilder`, reads cell size from `FInventoryUISettings::Get()`, registers its grid with `UInventoryUIDragHostSubsystem` under `Item.Container.WorldStorage` at priority `InventoryUISurfacePriority::NearbyWorldStorage` when visible, routes `TakeAllButton` to `VM.RequestTakeAllNearbyContainer()`. Drag event wiring (mouse-down handler + drop dispatch) intentionally deferred to Slice 6b.
- `Plugins/UI/ProjectInventoryUI/Public/Interaction/InventoryUISurfacePriority.h` - central surface-priority table (PlayerStorage=0, NearbyWorldStorage=10, ModalOverlay=100 reserved). No ad-hoc integers in widget code.
- `FInventoryPanelGridBuilder::Initialize` generalised from `UW_InventoryPanel*` to `UUserWidget*`. New `SetCellMouseDownHandler(...)` setter lets each widget bind its own cell click handler instead of the builder hard-coding `&UW_InventoryPanel::HandleCellMouseDown`.
- `W_InventoryPanel::NativeConstruct` now binds its own cell mouse-down handler via the builder setter.
- Priority regression test `ProjectIntegrationTests.UI.Framework.GridDragDrop.SurfacePriorityOrdering` asserts higher priority wins on overlap + ties preserve registration order + unregister works.

**Not yet done in 6a:**
- `ui_definitions.json` does NOT yet carry a `ProjectInventoryUI.NearbyContainerPanel` entry, so the widget is dormant (no instance constructed at runtime).
- The old `NearbySection` subtree is STILL in `InventoryPanel.json`; main panel still renders nearby today.

### Slice 6b - live-path migration (PENDING)

This is the flip. All changes ship together to avoid a double-render or broken-drag intermediate state. Steps, in order:

1. **Main panel drag migration to the tag API**. In `W_InventoryPanel.cpp`:
   - Remove the local `FProjectUIGridDragDropController DragDropHandler` instance (or keep it but route through the subsystem's controller via `GetLocalPlayer()->GetSubsystem<UInventoryUIDragHostSubsystem>()`).
   - On `NativeConstruct`: register primary storage grid, secondary player-storage grid, both hand grids, and any pocket grids with the subsystem using appropriate `Item.Container.*` tags and `InventoryUISurfacePriority::PlayerStorage`.
   - On `NativeDestruct`: unregister the same tags.
   - Replace `HitDetector.ResolveDualGridHit` and `DragDropHandler.UpdatePreview(primary, secondary, ...)` calls with the subsystem controller's `UpdatePreviewOverSurfaces` / `ResolveDropTargetOverSurfaces`.
   - On drop: build `FInventoryDragContext` + `FInventoryDropTarget` and call `FInventoryDropRouter::Route(VM, Ctx, Target)` instead of picking `RequestMoveItem` inline.
   - Remove the legacy `bSecondary`/`PrimaryCells`/`SecondaryCells` mirrors on `FProjectUIGridDragPreviewResult` once no callers remain.

2. **Strip nearby from main panel**:
   - Remove the `NearbySection` subtree from `InventoryPanel.json` (including any `settings.cellSize` if still present there; settings live in `InventoryUISettings.json` now).
   - Remove `NearbySection`, `NearbyGridHost`, `NearbyGridSizeBox`, `NearbyTitleText`, `NearbyStatsText`, `TakeAllButton` fields from `W_InventoryPanel.h`.
   - Delete `bRenderSecondaryAsNearby` branching in `RebuildGrids` - secondary grid always mounts in `GridHostSecondary`.
   - Drop `InventoryPanelBaseWidth` / `InventoryPanelNearbyWidth` constants and `UpdateResponsiveLayout` special-casing for nearby.
   - Remove `HandleTakeAllClicked` and its delegate binding.
   - Switch `CachedCellSize` source from `InventoryPanel.json -> settings.cellSize` to `FInventoryUISettings::Get().CellSize`.
   - Strip `NearbyTitleText`/`NearbyStatsText`/`TakeAllButton` refs + `UpdateNearbyContainerInfo` from `InventoryPanelTextUpdater.h/.cpp`.

3. **Wire nearby widget drag**:
   - Add `NativeOnMouseButtonDown` handling in `W_NearbyContainerPanel` OR use the same `UProjectGridCell::FOnGridCellMouseDown` pattern by setting `GridBuilder.SetCellMouseDownHandler(...)` to a method that initiates drag via the subsystem.
   - On drag update: `Subsystem->GetController().UpdatePreviewOverSurfaces(ScreenPos, Payload)`.
   - On drop: `ResolveDropTargetOverSurfaces` -> build router context -> `FInventoryDropRouter::Route`.

4. **Register nearby widget in ui_definitions.json**:
   ```json
   {
     "id": "ProjectInventoryUI.NearbyContainerPanel",
     "layer": "UI.Layer.Menu",
     "widget_class": "/Script/ProjectInventoryUI.W_NearbyContainerPanel",
     "viewmodel_class": "/Script/ProjectInventoryUI.InventoryViewModel",
     "layout_json": "NearbyContainerPanel.json",
     "load": "OnDemand",
     "spawn": "Persistent",
     "scope": "PerPlayer",
     "size_policy": "Fill",
     "input": "Menu",
     "priority": 15,
     "vm_creation": "Global"
   }
   ```

5. **Build + full regression sweep**:
   - `ProjectIntegrationTests.UI.Framework.Inventory.*`
   - `ProjectIntegrationTests.InventoryLootPlaces.UI.*`
   - `ProjectIntegrationTests.UI.Framework.GridDragDrop.*`
   - `ProjectIntegrationTests.UI.Layout.Inventory*.DumpTree`
   - `ProjectInventory.View.*`, `ProjectInventory.Depth.*`

6. **New focused tests** (Slice 10 body, folded into this flip):
   - `NearbyPanelIsIndependentlyAnchored` - main panel width unchanged with/without nearby.
   - `MultiSurfaceDragResolvesCorrectTag` - three surfaces registered, drag over each, assert tag.
   - `DropRouterDispatchesByTag` - already half-covered by the router unit path; add explicit VM-double dispatch check.

7. **Docs** (Slice 9):
   - `architecture.md` - widget decomposition, drag host subsystem, drop router, surface priority.

### Slice 6 — new nearby widget (DONE as Slice 6a; see above)

### Slice 6b — live-path strip (DONE)

Landed:

- `Data/ui_definitions.json` - nearby widget registered.
- `Data/InventoryPanel.json` - `NearbySection` subtree removed.
- `W_InventoryPanel.h/.cpp` - nearby fields + wiring stripped:
  - Removed `NearbySection`, `NearbyGridHost`, `NearbyGridSizeBox`, `NearbyTitleText`, `NearbyStatsText`, `TakeAllButton`.
  - Removed `HandleTakeAllClicked` and its binding.
  - `RebuildGrids`: no more `bRenderSecondaryAsNearby` branching. Secondary grid always mounts in `GridHostSecondary`. When `bHasNearbyContainer` is true, the secondary VM data is owned by `UW_NearbyContainerPanel` and main panel does not render or size it.
  - `UpdateResponsiveLayout`: dropped the nearby-width constant. Main panel always uses its base width.
  - `RebuildGridsDirty` check updated to drop the `NearbyGridHost` branch.
  - `NativeConstruct`: binds cell mouse-down handler via the generalised `FInventoryPanelGridBuilder::SetCellMouseDownHandler`.
- `InventoryPanelTextUpdater.h/.cpp` - `NearbyTitleText`/`NearbyStatsText`/`TakeAllButton` refs removed, `UpdateNearbyContainerInfo` removed. Nearby presentation now lives in `FNearbyContainerPresentation`.
- `W_NearbyContainerPanel.h` - delegate binding uses a dedicated UFUNCTION `HandleInventoryVMPropertyChanged` (distinct name) so dynamic-delegate dispatch through the UClass reflection table unambiguously hits the derived implementation. The earlier concern about inherited-method metadata hiding is eliminated by construction.
- `ProjectUIInventoryDumpTreeTest.cpp` - `InventoryNearbyLoot.DumpTree` updated to:
  1. Assert `NearbySection` / `NearbyGridHost` are NOT in the main panel tree.
  2. Locate `UW_NearbyContainerPanel` via `UWidgetBlueprintLibrary::GetAllWidgetsOfClass`.
  3. Verify the nearby widget is visible with a populated grid during an active session.
  4. Also spawns the nearby widget through the layer host (`ShowDefinition("ProjectInventoryUI.NearbyContainerPanel")`) and binds the same VM.

Cross-widget drag: starts in main panel via its existing dual-grid controller (unchanged), UMG dispatches `NativeOnDragOver`/`NativeOnDrop` to whichever widget sits under the cursor. The nearby widget's UMG drag handling is **not yet wired** - see follow-up below.

### Slice 6c — drag-INTO-nearby wiring (DONE)

`W_NearbyContainerPanel::NativeOnDragOver` and `NativeOnDrop` are now wired. `NativeOnDragOver` resolves a preview via the subsystem controller; `NativeOnDrop` resolves the target, fail-closes if the resolved tag is not `Item.Container.WorldStorage`, then dispatches via `FInventoryDropRouter::Route`.

### Slice 6d — drag-FROM-nearby wiring (DONE)

`W_NearbyContainerPanel::NativeOnMouseButtonDown` now returns
`DetectDragIfPressed` so UMG fires `NativeOnDragDetected`. That override
resolves the pressed cell via the subsystem controller, reads the entry
through `VM.TryGetSecondaryEntryByCellIndex`, and constructs a
`UInventoryDragDropOperation` with `FromContainer = Item.Container.WorldStorage`.
Dropping lands on whichever widget ends up under the cursor - main panel
handles its own `NativeOnDrop` as today, nearby widget handles via the
router when the user drops back onto the world grid.

### Review-response changes (DONE)

Follow-up items raised by reviewer about this slice:

1. **Architecture doc honesty**: `Plugins/UI/ProjectInventoryUI/docs/architecture.md` Drop Command Routing section now clearly states that only the nearby widget routes through `FInventoryDropRouter` today; `W_InventoryPanel::NativeOnDrop` still uses its local inline path. Main-panel migration is tracked as follow-up, not claimed as live.

2. **Dump tree test strengthened**: `ProjectIntegrationTests.UI.Layout.InventoryNearbyLoot.DumpTree` no longer falls back to `CreateWidget + AddToViewport` when `LayerHost->ShowDefinition("ProjectInventoryUI.NearbyContainerPanel")` returns null - it now hard-fails (`TestNotNull`). That proves the `ui_definitions.json` registration is live. It also no longer manually sets `Visibility = Visible` on the nearby panel; visibility must derive from `VM.bPanelVisible && VM.bHasNearbyContainer` via `RefreshFromViewModel`. The test asserts `GetVisibility() != Collapsed` after binding.

3. **Router test proves dispatch**: `UInventoryViewModel::RequestMoveItem` / `RequestTakeNearbyItemToContainer` / `RequestStoreItemInNearbyContainerAt` are now `virtual`. A test-double `UInventoryViewModelSpy` (under `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Support/`) overrides those three and records last-call + arguments. `DropRouterDispatchesByTag` now asserts **which** VM method fires for each tag pair plus that the args (source tag, target tag, pos, quantity) are carried through correctly. A bad router refactor returning true while calling the wrong method is now caught.

### Dynamic refresh test (DONE)

`ProjectIntegrationTests.UI.Framework.Inventory.NearbyPanelRefreshesOnViewModelChange`
proves the `UInventoryViewModel::OnPropertyChanged` -> `HandleInventoryVMPropertyChanged`
-> `RefreshFromViewModel` binding is live on real session mutations:

1. No session attached -> panel `Collapsed`.
2. `SetNearbyContainerSource(...)` -> panel non-`Collapsed`, title non-empty.
3. `ClearNearbyContainerSource()` -> panel back to `Collapsed`.
4. Re-attach -> panel visible again.

No forced `SetVisibility`, no `CreateWidget` fallback. The test fails if
the dynamic-delegate path is broken.

The label-mutation step was cut because `UInventoryViewModel::UpdateNearbyContainerLabel`
is protected - the label is driven internally from the source's
`GetContainerDisplayLabel`. Attach/clear/re-attach covers the important
property-notification path without poking protected internals.

### Slice 6e (DONE)

`W_InventoryPanel` migrated to the subsystem + tag + router path:

- Removed local `FProjectUIGridDragDropController DragDropHandler` member; the widget now shares the subsystem controller owned by `UInventoryUIDragHostSubsystem`.
- `NativeConstruct`/`NativeDestruct`: cell mouse-down binding via `FInventoryPanelGridBuilder::SetCellMouseDownHandler`; `UnregisterAllOwnedSurfaces()` on destruct unwinds hands, pockets, primary/secondary tabs.
- `RebuildHandGrids` registers LeftHand/RightHand surfaces on first build; `RebuildPocketGrids` unregisters stale tags then re-registers the current pocket set; `RebuildGrids` calls `RegisterPlayerGridSurfaces` which diffs the cached primary/secondary tag against the VM-selected tag and re-registers when a tab switch changes the active container.
- `NativeOnDragOver` calls `UpdateDragPreviewFromSubsystem` which runs `UpdatePreviewOverSurfaces` on the shared controller.
- `NativeOnDrop` routes all grid drops through `FInventoryDropRouter::Route`. Hand drops keep the VM-side hand-slot gating (`ResolveHandDropTargetAtScreenPos`) then dispatch through the router. Pocket drops keep their specific error-toast validation inline and dispatch through the router. Equip-slot drops stay inline (router rejects non-container target tags by design).
- `UpdateAllVisuals` reads the preview from `Subsystem->GetController().GetPreviewResult().PreviewCellsBySurface.Find(CachedPrimarySurfaceTag / CachedSecondarySurfaceTag)`. No legacy boolean branching.
- Split-self-overlap cancel still lives at the widget level (inline in `NativeOnDrop`) after resolve, so it can compare source pos against target pos before the VM is called.

Legacy API removed (Slice 6e cleanup):

- `FProjectUIGridDragPreviewResult::bSecondary / PrimaryCells / SecondaryCells` fields deleted.
- Dual-grid overloads of `UpdatePreview` and `ResolveDropGridTarget` deleted from `FProjectUIGridDragDropController`.
- `FProjectUIGridDragDropController::Initialize` + internal `FProjectUIGridHitDetector*` pointer deleted; the controller's hit-test path is tag-indexed only.
- `FProjectUIGridHitDetector` is still used by `W_InventoryPanel` directly (hover, hand/pocket inline hit-test, equip-slot hit-test) - it is a ProjectUI helper, independent of the drag controller.
- Controller header no longer includes `ProjectUIGridHitDetector.h`.

New tests:

- `ProjectIntegrationTests.UI.Framework.Inventory.MultiSurfaceDragResolvesCorrectTag` - three surfaces registered with main-panel-shape tags; asserts priority-ordered resolution + fail-closed on no cached geometry.
- `ProjectIntegrationTests.UI.Framework.GridDragDrop.PlayerGridTagReRegistersOnTabChange` - mirrors the live production path for tab-switch tag churn; asserts count stability and that same-tag re-register replaces (not duplicates).
- `ProjectIntegrationTests.UI.Framework.Inventory.NearbyPanelIsIndependentlyAnchored` - opens a main panel, measures `BackgroundWidthSizer->GetWidthOverride()`, toggles nearby session on/off, asserts the width is stable. Proves the decouple invariant at widget level.
- `ProjectIntegrationTests.UI.Layout.InventoryHands.DumpTree` - extended: assert absence of `NearbySection` / `NearbyGridHost` inside main panel even in the hands-only scenario.

New files:
- `Public/Widgets/W_NearbyContainerPanel.h` / `Private/Widgets/W_NearbyContainerPanel.cpp`
- `Data/NearbyContainerPanel.json` — right-center anchored root CanvasPanel.

Widget binds same VM, runs a private `FInventoryPanelGridBuilder` seeded with `FInventoryUISettings::Get().CellSize`, registers its grid with the subsystem under `Item.Container.WorldStorage`, routes drops via `FInventoryDropRouter::Route`.

### Slice 7 — layer-host registration (DONE — landed in 6b)

Add to `Data/ui_definitions.json`:

```json
{
  "id": "ProjectInventoryUI.NearbyContainerPanel",
  "layer": "UI.Layer.Menu",
  "widget_class": "/Script/ProjectInventoryUI.W_NearbyContainerPanel",
  "viewmodel_class": "/Script/ProjectInventoryUI.InventoryViewModel",
  "layout_json": "NearbyContainerPanel.json",
  "load": "OnDemand",
  "spawn": "Persistent",
  "scope": "PerPlayer",
  "size_policy": "Fill",
  "input": "Menu",
  "priority": 15,
  "vm_creation": "Global"
}
```

### Slice 8 — strip nearby from main panel (DONE — landed in 6b; final dual-grid API removal in 6e)

- `Data/InventoryPanel.json` — remove `NearbySection` subtree and `settings.cellSize` (moved to shared settings).
- `Public/Widgets/W_InventoryPanel.h` — drop `NearbySection`, `NearbyGridHost`, `NearbyGridSizeBox`, `NearbyTitleText`, `NearbyStatsText`, `TakeAllButton` fields.
- `Private/Widgets/W_InventoryPanel.cpp` — drop nearby bindings; `RebuildGrids` always mounts secondary in `GridHostSecondary`; drop responsive-width constants; register hand + primary + secondary grids with subsystem; route drops through `FInventoryDropRouter`.
- `Widgets/InventoryPanelTextUpdater.h/.cpp` — drop nearby refs + `UpdateNearbyContainerInfo`.

### Slice 9 — docs (DONE)

`Plugins/UI/ProjectInventoryUI/docs/architecture.md` — add sections:
- Widget decomposition (two widgets under `UI.Layer.Menu`, shared VM, independently anchored).
- Surface identity (tag-based `Item.Container.*`).
- Drag host subsystem (per-player).
- Drop routing (`FInventoryDropRouter`).
- Cell size contract (`InventoryUISettings.json` SOT).

### Slice 10 — tests (DONE)

- `ProjectIntegrationTests.UI.Layout.InventoryHands.DumpTree` — extend: no `NearbySection` in dump.
- `ProjectIntegrationTests.UI.Layout.InventoryNearbyLoot.DumpTree` — extend: two sibling widgets, main width unchanged vs Hands scenario.
- `ProjectIntegrationTests.UI.Framework.Inventory.NearbyPanelIsIndependentlyAnchored` — main panel width equal with/without nearby.
- `ProjectIntegrationTests.UI.Framework.Inventory.MultiSurfaceDragResolvesCorrectTag` — three surfaces, assert correct tag per drag position.
- `ProjectIntegrationTests.UI.Framework.Inventory.DropRouterDispatchesByTag` — player->player, player->world, world->player; assert correct VM command.

### Slice 12 — split source-hit from drop-validation API (DONE, 2026-04-21)

Reviewer flagged that `UW_NearbyContainerPanel::NativeOnDragDetected`
used `ResolveDropTargetOverSurfaces(...)` with a probe payload
(`InstanceId=INDEX_NONE`, `ItemSize=(1,1)`) to identify the pressed
cell. That API runs footprint validation including occupancy rules, so
an occupied world cell gets REJECTED during drag start - meaning the
user cannot pick up items. Drag source is a source-side question ("which
cell is under the cursor?"), drop is a target-side question ("does this
footprint validate here?"), and the two APIs had been conflated.

Fix:
- Added `FProjectUIGridDragDropController::ResolveSurfaceCellAtScreenPos(ScreenPos, OutTag, OutCol, OutRow)` - pure hit test; no payload; no occupancy rule.
- Rewrote `UW_NearbyContainerPanel::NativeOnDragDetected` to use the new API.
- Added `ProjectIntegrationTests.UI.Framework.GridDragDrop.ResolveSourceIgnoresOccupancy` - unit-level two-API contract test (shape + miss behavior).
- Added `ProjectIntegrationTests.UI.Framework.Inventory.NearbyDragStartWorksOnOccupiedCell` - real-geometry integration test. Creates the nearby widget with the loot fixture (Cigarette@(0,0), InstanceId=1001), waits 5 frames for Slate to paint, then asserts on the actual cached geometry at the center of cell (0,0):
  - source-hit API returns TRUE, tag=WorldStorage, (col,row)=(0,0);
  - drop-validation API with probe `InstanceId=INDEX_NONE` returns FALSE (the bug the fix prevents);
  - drop-validation API with `InstanceId=1001` returns TRUE (self-drop sanity).
  This is the test that would have failed before the fix.
- `architecture.md` documents both APIs and why they must not be mixed.

### Slice 13 — equip-backpack drop regression (DONE, 2026-04-21)

User-reported: after equipping a backpack, dropping items into the new
backpack grid showed "unavailable cell" on every empty cell.

Root cause: `RegisterPlayerGridSurfaces` set the surface
`OccupantAllowedChecker` lambda to reach to
`UWidgetBlueprintLibrary::GetDragDroppingContent()` for the active
DragOp, then call `IsDropOccupantAllowed(DragOp, ...)`. Two sins in one:
- the lambda depended on global Slate state; `GetDragDroppingContent()`
  returned null in lifecycle edge cases (some preview frames after
  re-registration on tab/surface churn);
- `IsDropOccupantAllowed` checked `if (!DragOp) return false` BEFORE the
  empty-cell short-circuit, so a single null DragOp instantly rejected
  every empty cell on the grid.

Fix:
- `FProjectUIGridDragPayload` extended with `Quantity` and `SourceSurfaceTag`. The payload now carries everything a validation rule needs.
- `IsDropOccupantAllowed` reordered: empty cell short-circuit FIRST. Defensive even if a future caller passes null.
- New self-contained anonymous-namespace helper `IsPayloadAllowedOnOccupant(WeakVM, Payload, OccupantId, bSecondary, [Cell])` replaces the global-state lookup. Uses Payload directly. Stack-merge policy uses `Payload.Quantity`. Source-side lookup picks Nearby vs main inventory based on `Payload.SourceSurfaceTag.MatchesTag(Item.Container.WorldStorage)`.
- Both primary and secondary tabbed-grid lambdas in `RegisterPlayerGridSurfaces` now call the helper; no `GetDragDroppingContent()` left in the registration path.
- Both `NativeOnDragOver` and `NativeOnDrop` in W_InventoryPanel now populate the full payload (Quantity + SourceSurfaceTag); same in W_NearbyContainerPanel.

New tests (the elementary checks that were missing):
- `ProjectIntegrationTests.UI.Framework.GridDragDrop.PayloadCarriesFullSourceContext` - asserts the OccupantAllowedChecker contract is empty-first, payload-only, and that Payload carries Quantity + SourceSurfaceTag through.
- `ProjectIntegrationTests.UI.Framework.Inventory.BackpackEmptyCellDropAccepts` - real-geometry integration test. Spawns W_InventoryPanel with the Hands+Pockets+Backpack fixture (empty 6x8 backpack), waits for Slate to paint, then calls `Subsystem->GetController().ResolveDropTargetOverSurfaces` at the center of an EMPTY backpack cell. Asserts:
  - drop with full payload resolves with tag=Backpack, col=1, row=1;
  - drop with degenerate payload (no source) STILL resolves on empty cell (defensive empty-first check);
  - the surface is in fact registered under the Backpack tag.
  This is the test that would have failed before the fix.

### Slice 14 — nearby panel never spawned in production (DONE, 2026-04-21)

User-reported: opened a real cardboard-box loot session and the nearby
container panel was completely absent on screen. Inventory main panel
showed up; nearby panel did not exist.

Root cause: After the decouple, the nearby panel was registered in
`ui_definitions.json` but only the dump-tree test ever called
`LayerHost->ShowDefinition("ProjectInventoryUI.NearbyContainerPanel")`.
Production code path (`ASinglePlayController::HandleInventoryViewModelPropertyChanged`)
only spawned the main panel - the nearby sibling was never asked to
exist. The dump-tree test masked the bug by spawning both widgets
explicitly, which made the test pass while production was broken.

Fix:
- New `InventoryUIVisibilityCoordinator` (free function in `ProjectInventoryUI/Public/UI/`):
  - `SetInventoryUIVisible(LayerHost*, bool)` toggles BOTH panels as a unit.
  - `GetMainPanelDefinitionId()` / `GetNearbyPanelDefinitionId()` expose ids so callers and tests can't drift.
- `ASinglePlayController::HandleInventoryViewModelPropertyChanged` rerouted through the coordinator. Inline `ShowDefinition`/`HideDefinition` for InventoryPanel only is gone.

New regression test (verified to fail without the fix):
- `ProjectIntegrationTests.UI.Framework.Inventory.VisibilityCoordinatorSpawnsBothPanels` - uses `SetInventoryUIVisible(LayerHost, true)` (the production-shaped trigger) and asserts BOTH widgets appear in the viewport via `GetAllWidgetsOfClass`. Sabotage-verified: temporarily commenting out the second `ShowDefinition` in the coordinator made the test fail with the exact "nearby container panel MUST also exist" message before this fix was committed; restoring the line made it pass.

Future contract: any new caller that opens inventory UI MUST go through `InventoryUIVisibilityCoordinator::SetInventoryUIVisible`. Header comment makes the rule explicit. Direct `LayerHost->ShowDefinition("...InventoryPanel")` is the gap that hid loot containers.

### Slice 15 — nearby panel anchored to TopLeft instead of CenterRight (DONE, 2026-04-21)

User-reported: even after Slice 14 (coordinator spawning both panels), the
nearby loot panel was constructed but visually absent. The widget tree
log showed `NearbyBackground` at `Anchor=(0.00,0.00)-(0.00,0.00)` with a
preceding warning: `LogLayoutRegistry: Warning: Unknown anchor preset:
RightCenter, using TopLeft`.

Root cause: `NearbyContainerPanel.json` declared `"anchor": "RightCenter"`
but the registry exposes `CenterRight`. The lookup silently fell back to
TopLeft, parking the panel at (0,0) where the editor toolbar covered it.
The Slice-14 coordinator test only proved widget existence, not screen
position - the panel was technically present, just invisible.

Fix:
- `NearbyContainerPanel.json`: `RightCenter` -> `CenterRight`.
- `LayoutWidgetRegistry::GetAnchorPreset` upgraded from `Warning` to `Error` and now lists every valid preset key in the message. Future typos surface loudly with the canonical list right next to the bad value.

New regression test (sabotage-verified):
- `ProjectIntegrationTests.UI.Framework.Inventory.NearbyPanelAnchoredCenterRight` - constructs the nearby widget from JSON, finds `NearbyBackground`, asserts its `CanvasPanelSlot.GetAnchors()` lands at `(1.0, 0.5)` (right edge, vertical center). Verified to fail with the typo restored: it tripped on the new error log AND on the anchor mismatch (`anchor X to be 1.0, but it was 0.0`).

This pattern - JSON typo silently degrading to a fallback - is exactly the failure class my earlier tests didn't catch. Now an anchor typo fails loud at framework load time AND fails the data-shape regression test.

### Slice 11 — build + regression sweep (DONE)

Green results:
- `ProjectIntegrationTests.UI.Framework.*` — 22/22 pass (includes new `MultiSurfaceDragResolvesCorrectTag`, `PlayerGridTagReRegistersOnTabChange`, `NearbyPanelIsIndependentlyAnchored`, `NearbyPanelRefreshesOnViewModelChange`).
- `ProjectIntegrationTests.UI.HUD.MindThought.*` — 19/19 pass after the related `mindthought_test_failures.md` fix landed (`RecordResolutionIntegration` + `Scan.ToastStaysToast`).
- `ProjectIntegrationTests.UI.Layout.Inventory*` — 5/5 pass.
- `ProjectIntegrationTests.InventoryLootPlaces.UI.*` — 14/14 pass.
- `ProjectInventory.View.*` — all pass.
- Build `AlisEditor Development` — clean.

- `scripts/ue/standalone/build.ps1`
- `python scripts/ue/check/data/validate_all.py`
- `ProjectIntegrationTests.UI.Framework.Inventory.*`
- `ProjectIntegrationTests.InventoryLootPlaces.UI.*`
- `ProjectIntegrationTests.UI.Framework.GridDragDrop.*`
- `ProjectInventory.View.*`, `ProjectInventory.Depth.*`
- Manual visual check: open without nearby, approach loot, drag each direction, TakeAll, close loot.
- Log scan: no new warnings from InventoryPanel / GridBuilder / drag controller.

---

## Orchestration & Execution Speed — Investigation + Fixes (2026-04-21)

Recorded while executing Slices 16-19 via sub-agent orchestration. Raw numbers from this session; fixes ranked by ROI.

### Step 1 — Measure where the time actually goes

Wall-clock budget for one slice end-to-end (observed):

| Phase | Cost | % of slice |
|---|---|---|
| Sub-agent cold read (todo + invariants + prior-slice context + grep) | ~5-10 min | ~15% |
| Implementation + local compile loops | ~10-20 min | ~30% |
| Full `build.ps1` (link stage dominates even incremental) | ~5-10 min | ~15% |
| Test run: 60s editor boot + map load PER `run_cpp_tests_safe.ps1` invocation | ~1-3 min per run × N runs | ~25% |
| Sabotage verification (3× run per new test: pass → mute → fail → restore → pass) | N × 3 runs | ~10% |
| Parent (me) verification + re-run on different filter | 1-2 extra editor boots | ~5% |

Slice 17 took ~90 min wall-clock. Slice 19 took ~95 min. Of that, roughly 60-70% is structural infra cost (editor boot × N, cold context × 1), only 30-40% is actual architectural work.

### Step 2 — Rank fixes by ROI (signal per hour invested)

1. **Persistent test editor with RPC-style command inject.** Biggest win. `UnrealEditor-Cmd.exe` spends ~60s booting + loading MainMenuWorld for every test-filter invocation. Real test execution is <5s. Running N filters costs ~N × 60s; if we keep one editor warm and feed `Automation RunTests <filter>; ...` over stdin or a local HTTP endpoint, each subsequent filter drops to ~5-10s. 5-10x speedup on test iteration. Prototype: launch `UnrealEditor-Cmd -run=Automation -unattended` without the `-testexit` sentinel, send commands via `-execcmds` pipe or attach a small `FConsoleCommand` endpoint, reap reports from `Saved/Automation/Reports/`. ETA: half-day infra setup, pays back within ~3 slices.

2. **Batch sabotage verification.** Current pattern per new test: run green → edit source to mute one emit → rebuild → run red → restore → rebuild → run green. Three editor boots per test. Instead: mute ALL planned sabotages in one pass (wrap each behind a compile-time `#if SABOTAGE_N`), enable them with one preprocessor define, run red once, restore with another compile, run green once. Drops sabotage from 3N runs to 2 runs total, independent of N. ETA: per-slice prompt refactor — cheap.

3. **Union-filter verification runs.** I caught myself running `Inventory.*` + `GridDragDrop.*` + `LootPlaces.UI.*` as three separate `run_cpp_tests_safe.ps1` invocations (3× editor boot). A filter like `ProjectIntegrationTests.(UI.Framework.Inventory|UI.Framework.GridDragDrop|InventoryLootPlaces.UI)` runs in one boot. Already applied mid-session; codify as the default in sub-agent prompts. ETA: immediate.

4. **Sub-agent continuation via SendMessage instead of fresh agent per slice.** Slice 17 used SendMessage after its pause-to-ask; it kept ~30k tokens of research warm. Slices 18 + 19 started fresh → each cost ~10-15k tokens re-reading the todo + grepping for prior-slice types + understanding invariants. Downside: a warm agent carries prior-slice decisions as "how we did it" context which can leak into later-slice scope. Mitigation: explicit "Slice N+1 is different scope, here's the new contract" in the continuation message. ETA: cheap, apply from Slice 20 onward.

5. **Parent-side verification budget cap.** I re-ran Inventory batch after Slice 17 agent reported "all green" because I wanted to verify the "pre-existing failures" claim — that was right (caught 2 real regressions). But I also ran GridDragDrop and LootPlaces separately when the agent had already run them. Rule: parent re-runs ONLY the subset the sub-agent didn't verify, or the one suite the agent's own changes most likely regressed. ETA: immediate, prompt-side discipline.

6. **Shared test fixture helpers to shrink per-test boilerplate.** Slice 17's E2E tests each re-build a painted grid from scratch (30-frame wait, viewport add). A reusable `FSlice17GridFixture` that sets up + tears down in a scope object would cut boilerplate AND shorten per-test latent pumps. Not a speed-of-orchestration fix, but a speed-of-test-iteration fix that compounds over Slices 19-20. ETA: half-day; defer unless Slice 20 needs many new tests.

7. **Build incremental hygiene.** Most slice edits compile in 5-15s. Occasional 5-10 min rebuilds happened when a subsystem header changed (ripples across .cpp files that include it). Prevent by splitting `UInventoryUIDragHostSubsystem.h` public API from private impl headers so header churn doesn't invalidate widget TUs. ETA: already partly done by Slice 16 creating `InventoryDragEvent.h` separately; revisit if link time grows.

### Step 3 — What NOT to optimize

- **Do not remove sabotage verification.** It's what catches test theater. The cost is real but the signal is load-bearing. Batch it, don't skip it.
- **Do not parallelize slices.** They depend on each other's landed types; parallel would produce merge hell and cross-slice regressions. Sequential with tight gates is right.
- **Do not move to "fewer, bigger slices".** The gates BETWEEN slices are where we caught Slice 17's contract-vs-E2E scope drift and Slice 19's grid-drop-VMInvoked leftover. Fewer gates = later discovery.

### Step 4 — Apply to Slice 20

Concrete prompt deltas for the Slice 20 sub-agent:
- Use `SendMessage` to continue the Slice 19 agent rather than spawn fresh (items 4 + 6 above).
- Prompt includes: "Do not re-verify Slices 16-19 state; assume landed. Run final regression only as union filter."
- Sabotage pattern: request ALL sabotages guarded by a single `#define SLICE20_SABOTAGE 1` toggle, runs 2 test passes total.

Expected saving: Slice 20 should land in ~25-40 min vs ~90 min baseline.

### Step 5 — Follow-up to capture as its own todo

Persistent-test-editor infra (item 1) deserves its own todo file under `todo/00_current/` — it will pay back on every iteration of every future feature, not just this one. Flag for the next planning pass; don't inline into this slice.

### Step 6 — First-run timeout rough edge (2026-04-22, follow-up)

Reported during the speedup session: first warm dispatch on `UI.Framework.Inventory`
hit the 120s default timeout, while later `GridDragDrop` (10.8s) and `LootPlaces`
(14.8s) dispatches were fine. Suspected first-run cost (JIT + first Slate paint)
tipping the caller's 2-min cap.

Reproduction attempt (clean artifacts, cold start):
- Editor boot: ~67s (as expected)
- First-run `UI.Framework.Inventory` (36 tests): **32.6s** — well under 120s
- `UI.Framework.GridDragDrop` (5 tests): 10.7s
- `InventoryLootPlaces` (38 tests, 1 pre-existing fail): 14.8s
- **Total warm (3 filters): 58s** vs 182s cold = **3.1x speedup**

Could NOT reproduce the 120s overrun. Likely a one-time artifact of the earlier
session's disk / shader-cache / concurrent-process state. But kept the fix
defensive rather than leaving the rough edge open:

Chosen approach: **(a) + (c) combined — extend default + reset-on-progress.**

1. `persistent_editor_run.ps1` default `-TimeoutSeconds` bumped from `180` to
   `300` (hard wall-clock cap).
2. New `-IdleTimeoutSeconds` param (default `120`) with a progress-reset loop:
   count `Test Completed.` lines in the slice; reset the idle clock whenever
   the count grows. A slow-but-progressing run keeps running; only a real
   hang (no new `Test Completed.` for 120s) aborts early.
3. `run_cpp_tests_safe.ps1` forwards `[Math]::Max($TimeoutSeconds, 300)` when
   dispatching via the persistent path, so the cold-path default of 120s does
   not silently leak into persistent runs. Cold-path wait logic untouched.

A warmup-tick option (b) was considered but rejected — if the issue is
first-Slate-paint for the Inventory filter specifically, a trivial
`GridDragDrop.SurfacePriorityOrdering` dispatch on startup wouldn't pay that
cost (it's a pure-unit test that doesn't touch Slate). The timeout-plus-
idle-guard fix is cheaper and catches the symptom class generically.

Files modified:
- `scripts/ue/test/unit/persistent_editor_run.ps1`
- `scripts/ue/test/unit/run_cpp_tests_safe.ps1`

Measured warm wall-clock AFTER fix (second pass, everything truly warm):
`Inventory=18s + GridDragDrop=11s + LootPlaces=16s = 45s` (vs 182s cold).
**Net speedup: ~4x.**

---

## Non-goals

- Animation. No "slide in" this pass.
- VM split. Nearby state stays on `UInventoryViewModel`.
- Drag coordinator rewrite. Subsystem owns a single controller instance; no framework-wide refactor.
- Pre-existing failures (`LiveLootContainerDefinitionsCanonicalStorage` content-data issue, `InteractionHoldOpensWorldContainerSession` panel-visibility timing) remain out of scope.

## Risks

- **Tag-API migration in ProjectUI**: dual API (boolean + tag) coexists during migration. Mitigated by retaining boolean mirrors on `FProjectUIGridDragPreviewResult`; flip inventory-side callers one at a time.
- **Subsystem lifetime vs widget lifetime**: `ULocalPlayerSubsystem` outlives any single widget, so build order doesn't matter. Widgets register on construct, unregister on destruct.
- **Right-anchor overlap on tiny viewports**: out of scope. Same policy as the cell-size plan.

## Handoff state

- **Slices 1-5** complete and build-verified clean. Pure additions, no live code paths touched yet:
  - `InventoryUISettings.json` + loader.
  - Tag-indexed `RegisterSurface` API on `FProjectUIGridDragDropController` (additive; dual-API coexists with the dual-grid boolean API).
  - `UInventoryUIDragHostSubsystem` (`ULocalPlayerSubsystem`) owning one shared controller per local player.
  - `FInventoryDropRouter` - `FGameplayTag`-driven `{Source,Target}` -> VM command mapping.
  - `FNearbyContainerPresentation` - single source for title/stats/take-all visibility.
- **Slices 6-11** pending. Slices 6-9 are the live-path migration (new widget, layer-host entry, strip from main panel, docs). Slice 10 is regression tests, Slice 11 is full sweep.
- All changes **uncommitted** on branch `6r0m` per user policy.

**Recommended pause point**: after Slice 5 (current state). The next slice adds a brand-new widget and a layer-host entry - still low risk, but the migration in Slice 8 touches code paths covered by 30+ existing tests. Review in-between is worthwhile.

## Architectural audit (after Slice 5)

Passed with no violations. De-risking items applied before Slice 6:

- `FProjectUIGridDragDropController::ResolveHitOnSurface` now logs at Verbose when a registered surface has a destroyed grid (catches widget-lifecycle bugs without noise).
- `UInventoryUIDragHostSubsystem::UnregisterSurface` logs at Warning when the tag is not registered (catches double-unregister / typo). **Watch item for Slice 6**: if widgets rely on a central `ClearSurfaces()` before destruct, the warning will spam on teardown - revisit if that pattern appears in practice (likely: demote to Verbose, or short-circuit the warning when `RegisteredSurfaces.IsEmpty()`).
- Deprecated boolean mirrors on `FProjectUIGridDragPreviewResult` carry an explicit removal deadline (Slice 8) and the comment now states the two APIs are not mixable.
- `FProjectUIGridSurface` gained an explicit `int32 Priority` field; surfaces are stable-sorted on `RegisterSurface` so overlap routing is deterministic (higher priority first, ties preserve registration order).
- `FInventoryUISettings::Get()` comment corrected: first call must run on the game thread (file I/O); subsequent reads are safe on any thread.
- `FInventoryDropRouter` fails closed: both surface tags must `MatchesTag(Item.Container)` or the router returns false without calling the VM. A future non-inventory surface wired into this path will not silently fall through to `RequestMoveItem`.
- `Plugins/UI/ProjectInventoryUI/docs/architecture.md` "Cell Size Contract" section updated to reference `InventoryUISettings.json` and `FInventoryUISettings::Get()`.

Confirmed architecturally by the audit (no action needed):

- `ProjectUI` stays domain-agnostic. `FProjectUIGridSurface` uses `FGameplayTag` only as a pivot id; no inventory semantics leak.
- `ULocalPlayerSubsystem` is the correct scope for the drag host (matches `UProjectContainerSessionSubsystem`, supports split-screen correctly, outlives widgets).
- `FInventoryDropRouter` belongs in `ProjectInventoryUI`, not `ProjectCore` - it is feature-internal orchestration, not a cross-plugin contract.
- Router routing table is correct (parent-tag matches via `MatchesTag(Item.Container.WorldStorage)`, rejects world->world, defaults to `RequestMoveItem` for player-side).
- Settings loader fallback/warning path is sound.
