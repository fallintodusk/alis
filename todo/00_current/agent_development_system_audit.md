# Agent Development System Audit

**Type:** transient audit artifact. Stable docs must never link to this file.

**Revision 2 (operator review applied).** Since revision 1, three changes were
implemented by the operator and are marked DONE below: C1, C2, and the C6
contradiction removal. Everything else in this report remains a PROPOSAL - not
implemented. Two factual errors in revision 1 were corrected (C2 routing
target, and the Codex "truncation" claim in G2); several proposals were
narrowed on reviewer instruction. Corrections are called out inline so the
revision-1 reasoning stays auditable.

**Scope:** meta-process for the upcoming multi-month World-generation
implementation run on Claude Code + Codex. Not a World implementation review.

**Method:** read-only inspection of the repo, `.claude/**`, `.codex/**`,
`~/.claude/**`, `~/.codex/**`, plus current vendor primary docs. No builds, no
gates, no MCP, no L3/L4, no staging, no commits.

**Verified environment:** Claude Code `2.0.76`. Codex installed (active
`~/.codex/` with `config.toml`, `auth.json`, live logs, `[projects.'<project-root>'] trust_level = "trusted"`)
but not on the Git Bash PATH, so its version string was not read. `.agents/`
does NOT exist in this repo - confirmed. `CLAUDE.md` and `CODEX.md` are both
symlinks to `AGENTS.md`.

---

## 1. Current strengths - KEEP

Load-bearing and better than most projects have. Do not restructure these.

| Asset | Why it is strong |
|---|---|
| `docs/agents/scientific_debugging.md` (192 lines) | Six evidence rails plus a 14-step protocol, each rail traced to a real multi-hour failure. Two-strike reset and "creation defects and recovery defects are separate hypotheses" are genuinely rare. Best single process doc in the repo. |
| `canonical.md` sec 7 "Gate scope and missing acceptance dimensions" (:288-327) | Names the exact failure mode - a green gate proving a narrower invariant than reported - and gives four questions to ask before trusting a numeric gate. |
| `docs/testing/world_pipeline_layers.md` (74 lines) | L0-L4 with data scope, required proof, and explicit exclusions. "Never rerun a higher gate merely because another edit occurred" is the right default for a months-long run. |
| Slice plans `world_realize_kazan_territory_slice_3.md`, `..._slice_2.md` | Already ExecPlan-grade: Goal, Non-goals, Read first, Verified evidence split into Facts vs Inferences/proof gaps, Architecture boundary, Decision, Rejected alternatives, Required invariants, tasks, Test-first, Rollout/rollback, Completion criteria, dated Review record. The `SUPERSEDED / FALSIFIED` block at `slice_3.md:795-806` that retains a wrong hypothesis as history is exemplary. |
| World stable SOTs | `architecture_overview.md` (399) + `territory_generation.md` (40, pure router) + `territory_contract.md` (1150) + `world_partition.md` (544) + `pitfalls.md` (429, 14 entries). Zero broken links across all World docs, `tools/World/**`, `scripts/ue/world/**`. |
| `tools/World/**` component shape | Identical `app/ contracts/ fixtures/ profiles/ tests/ api.py bootstrap.py README.md` per stage, an explicit dependency-direction block, and a cross-component fitness test. |
| Script-layer refusal | `run_single.ps1` / `iterate.ps1` / `run_cpp_tests_safe.ps1` reject broad filter shapes and force `-Mode Gate` / `-AllowBroadFilter`. Rule plus mechanism, not rule alone. |
| `scripts/ue/world/` transaction rails | Lock, drift check, snapshot, journal/recovery, prospective validation, active-set-committed-last, interactive `yes` confirmation, named `-Reconstruct` / `-EnrollManifests` authorization flags. |
| `territory_contract.md:99-110` geodetic error budget | "One tolerance must never hide several different errors", then a per-term gate table. This is a working proof-traceability matrix, already proven in this repo. |
| `todo/README.md` one-way dependency rule | Todos link to docs; docs never link to todos. |
| Router/SOT split | `AGENTS.md` short rule plus `canonical.md` deep contract, each naming the other: AGENTS.md:133 / canonical:194 (dev loop), AGENTS.md:153 / canonical:230 (checkpoint), AGENTS.md:223 / canonical:782 (file size). This is good design, NOT duplication to remove. |
| `.claude/skills/*/agents/openai.yaml` | The repo already invented a cross-tool skill wrapper convention. It just lives in a directory Codex never scans (see G3). |

---

## 2. Confirmed gaps only

Ten gaps. Each has file:line or a reproducible command. Nothing speculative.

### G1 - `@` route markers in AGENTS.md are imports, not links (CRITICAL) - RESOLVED

**Status: FIXED by C1 + C2.** Verified after the fix:
`grep -c "@[A-Za-z0-9_./-]*\.md" AGENTS.md` = 0; all 35 `@` characters across
the 26 unique paths stripped; `AGENTS.md` is now 466 lines / 20,844 bytes with
zero imports. Evidence below is retained because it is the justification for
keeping the router import-free.

`AGENTS.md` used `-> @docs/build/workflow.md` as routing notation, 26 times.
Claude Code treats `@path` as an import: "Imported files are expanded and
loaded into context at launch alongside the CLAUDE.md that references them"
([memory docs](https://code.claude.com/docs/en/memory)).

Measured this session:

```text
26 imported files      9,897 lines    340,279 chars
AGENTS.md                449 lines     20,227 chars
user-global AGENTS.md    288 lines     13,576 chars
TOTAL always-loaded   10,634 lines    374,082 chars   ~104k tokens
```

Reproduce the import set:
`grep -o "@[A-Za-z0-9_./-]*\.md" AGENTS.md | sed "s/^@//" | sort -u`
returns exactly the 26 files whose full text appears in this session's system
context. Import depth is 1; no imported file itself uses `@`.

Consequences:

- Violates the project's own budget. `~/.agents/AGENTS.md:167` "Never inject
  >1,500 lines total per turn"; `:165` "Root files <=200 lines". Actual is
  about 7x the line budget, driven from a 449-line root.
- The payload is re-injected after every compaction, so it directly attacks the
  continuity property this audit was asked to protect.
- Content is largely irrelevant to World work: `tools/Launcher/docs/user_guide.md`
  (279 lines of end-user FAQ, including "How do I uninstall the game?"),
  `tools/BuildService/docs/rust_setup_windows.md` (184 lines of rustup install
  steps), `docs/build/packaging_guide.md` (459), `tools/Launcher/docs/architecture.md`
  (704), `scripts/docs/architecture.md` (975).
- `territory_generation.md:3-5` says "do not load the deep contract as a default
  session bootstrap" - correct hygiene, already defeated, because the budget is
  spent before the agent reaches any World doc.

### G2 - Claude and Codex read radically different instruction sets from the same file

**Correction to revision 1.** Revision 1 said Codex reads the file "truncated".
That was wrong and is withdrawn. `AGENTS.md` was 20,227 bytes (now 20,844)
against Codex's `project_doc_max_bytes` default of 32,768, so Codex ingests it
IN FULL with about 36% headroom. The finding is the ASYMMETRY, not truncation.

Codex `project_doc_max_bytes` defaults to 32 KiB, and Codex "stops adding files
once the combined size reaches the limit"
([AGENTS.md guide](https://developers.openai.com/codex/guides/agents-md)).
Codex does not expand `@` imports.

Same repo, same file, before the C1 fix:

```text
Claude Code : ~374,082 chars  (router + 26 expanded imports)
Codex       :   20,227 chars  (router only; full, not truncated)
```

The two tools were reading instruction sets that differed by a factor of ~18.
C1 collapses the gap: both now read the same ~20.8 KB router, and every deeper
doc is opened on demand by whichever tool needs it. The headroom matters going
forward - the router must stay well under 32 KiB or Codex will start dropping
content that Claude still sees.

The rest of this gap is NOT resolved by C1:

Also: `grep -rn "\.codex\|CODEX\.md"` over tracked `.md` / `.ps1` / `.py`
returns nothing. The `CODEX.md` symlink is undocumented, and
`scripts/setup/setup_ue_env.ps1` + `scripts/ue/update/update_engine.ps1`
contain no `codex` reference - so the engine-update orchestrator syncs
`.mcp.json` and `.claude/settings.local.json` but never `~/.codex/config.toml`,
which currently pins `UE_EDITOR_CMD` to `UE_5.7` while the project engine is
`UE_5.8`.

### G3 - Skills live in three homes; one is untracked, one is invisible to Codex

Primary-source discovery paths:

- Claude Code: `.claude/skills/<name>/SKILL.md` (project), `~/.claude/skills/`
  (personal) ([skills docs](https://code.claude.com/docs/en/skills)).
- Codex: "Codex scans `.agents/skills` in every directory from your current
  working directory up to the repository root. Personal skills are stored in
  `<home>/.agents/skills` ... skills in `~/.codex/skills` are available from any
  repo" ([Codex skills](https://developers.openai.com/codex/skills)).

Current state:

| Location | Contents | Tracked | Claude sees | Codex sees |
|---|---|---|---|---|
| `.claude/skills/` | 5 ALIS skills | yes (mirror-excluded) | yes | NO |
| `.agents/skills/` | does not exist | - | no | would, if it existed |
| `~/.codex/skills/alis-character-animation-debug/` | 1 ALIS skill | NO | no | yes |

`~/.codex/skills/alis-character-animation-debug/SKILL.md` (dated 2026-04-06) and
`.claude/skills/character-animation-dev/skill.md` are two divergent copies of
the same ALIS workflow. One is not in version control and not reviewable.

Both tools require `SKILL.md` with `name` + `description` frontmatter - the
same format. So the divergence is avoidable without maintaining two documents.

Latent defect: `git ls-files` shows `character-animation-dev/skill.md` lowercase
while the other four are `SKILL.md`. Invisible on this case-insensitive Windows
checkout; would fail discovery on a case-sensitive checkout (WSL, Linux CI).

### G4 - An allowlisted skill instructs the agent to break a CRITICAL rule, with zero mechanical prevention - PARTIALLY RESOLVED

**Status: contradiction removed (C6 DONE); mechanical half still OPEN (C7).**
Verified: `.claude/skills/` now lists four skills, `ue-gamefeature-ci` is gone,
and `grep -n "ue-gamefeature-ci" .claude/settings.local.json` returns nothing.
The instruction to commit no longer exists. What is still true is that nothing
mechanically PREVENTS a commit - the deny list still lacks `git commit`, and
there are still zero hooks. Evidence below is retained as the justification
for C7.

`.claude/skills/ue-gamefeature-ci/SKILL.md`, as it existed at audit time:

```text
:3    description: "... rebuilds after edits, commits progress."
:227  "Commit after every green build+test cycle"
:349  git add -A
:350  git commit -m "feat(menuexperience): ..."
:505  "ALWAYS commit progress - visible incremental improvement"
```

No `bot/*` branch guard, no written-pre-approval check. It contradicts:

- `AGENTS.md:377-418` AUTONOMOUS COMMIT POLICY - "Autonomous commits by agents
  are forbidden by design"; `bot/*` is necessary but not sufficient;
- `docs/agents/overnight_mode.md:23-24` "Commits do not happen in autonomous
  mode" and `:137` "No `git commit`. No `git push`."

Enforcement state:

- `.claude/settings.local.json` allowlists `Skill(ue-gamefeature-ci)`.
- `.claude/settings.json` deny covers `git push`, `git reset --hard`,
  `git clean -f`, `git branch -D` - but NOT `git commit`, NOT `git add`.
- There are zero Claude Code hooks and zero Codex hooks configured in this repo.

The skill description sits in the always-visible skill list every session.
Current branch is `6r0m` - a user branch, not `bot/*`.

Secondary: that file carries 25 non-ASCII characters (emoji), against
`AGENTS.md:302` ASCII-ONLY.

### G5 - The L3 durable-authority entrypoint is pre-approved with an unbounded argument wildcard

`.claude/settings.local.json` permissions.allow contains:

```text
Bash(MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL=* powershell.exe -NoProfile
     -ExecutionPolicy Bypass -File scripts/ue/world/realize_canonical_world.ps1:*)
```

`:*` "is an equivalent way to write a trailing wildcard"
([permissions docs](https://code.claude.com/docs/en/permissions)), so every
argument set is pre-approved - including
`-Mode Apply -NonInteractive -EnrollManifests -WorldDataPlugin ProjectWorldData`.

`scripts/ue/world/README.md:105-109`: the operator confirmation is an
interactive `yes`, and "automation must opt in explicitly with
`-NonInteractive`". That waiver is exactly what the allow rule pre-approves,
with no prompt.

Remaining barriers are judgment-only: `AGENTS.md:153-170` Checkpoint Scope Rule
and the L3 row of `world_pipeline_layers.md`. Slice 3 is parked precisely at a
pre-L3 operator approval checkpoint (`slice_3.md:807-816`), so this is the live
risk surface, not a hypothetical.

### G6 - Plans carry invariants and proofs as two unlinked lists, and never state the execution envelope

In `world_realize_kazan_territory_slice_3.md`:

- `:354-375` - 14 "Required invariants";
- `:529-556` - "Mandatory proof classes";
- no mapping between them: no column, no ID, no cross-reference.

Two facts from the incident record in the same file:

1. The invariant set was count-shaped. Invariant 4 is "Exactly one Landscape
   component and initial proxy maps to each of 210 cells". There was no
   terrain-relief invariant, and correspondingly no relief proof - the missing
   acceptance dimension analysed at `:909-940` and in
   `tools/World/VisualVerification/README.md:9-22`.
2. The confirmed root cause was an execution-envelope mismatch, not a data
   defect. `slice_3.md:795-802`: "`-NullRHI` prevented Landscape RDG
   composition" plus a missing completion barrier.
   `scientific_debugging.md` rail 6 says "The execution envelope is part of the
   input" - but it is written as a DEBUGGING rule, invoked only after something
   has already broken. No plan section requires stating the envelope per proof
   up front.

This is not an argument for a new framework. The repo already runs the exact
pattern one level down: `territory_contract.md:99-110` refuses to let one
tolerance hide several errors and tabulates each term against its own gate. The
gap is that the pattern was never lifted to plan level, where the invariant
list lives.

Honest limit: a matrix would NOT have invented the relief invariant. What it
does provide is (a) a reviewer-checkable "which invariant has no proof, which
proof serves no invariant", (b) a place where naming a gate narrowly
(`canonical.md:319-322`) is structurally forced, and (c) an envelope column that
turns rail 6 into a planning obligation.

### G7 - R1/R2 review is the spine of the process and has no stable definition

`world_compile_kazan_territory_slice_2.md:37` "R1 - Pre-code design review" and
`:305` "R2 - Integrated exit review" define the gates inline, per plan.
`slice_3.md` reuses the names and carries about 20 dated reviewer entries
(PATCH accepted / PASS / external review).

`docs/testing/world_pipeline_layers.md:48-56` "Review Cadence" describes two
review points but never names them R1/R2 and never says who performs them.
`grep -rn "reviewer"` over `world_pipeline_layers.md` + `territory_contract.md`
returns exactly one line, `world_pipeline_layers.md:54`.

So the most load-bearing process element in a two-tool, multi-month effort has
no written answer to: is the reviewer the same agent, a fresh Claude session,
Codex, a subagent, or the human? What context does it get? What distinguishes
PATCH from PASS procedurally? Every new slice re-invents it.

### G8 - Two durable World decisions exist only inside a todo scheduled for archival

All five candidates checked:

| Decision | Recorded today | Fresh-agent discoverable |
|---|---|---|
| Canonical cells != runtime WP cells | `world_partition.md:109-111`, `CanonicalCompilation/README.md:142-144`, asserted in `ProjectWorldCoordinateMapping.cpp:67` | YES |
| HLOD exclusion | `world_partition.md:91-92`, `:298-301`, `territory_contract.md:1084-1087` | YES |
| Generated vs runtime-state authority split | `territory_contract.md:287-292`, `:344-345`, `architecture_overview.md:36-39` | YES |
| Native Landscape realization - rejection of a second tiled importer and of any ProceduralMesh fallback | `slice_3.md:123-124` and `:138` ONLY | NO |
| UE Water plugin rejected - rationale and citations | decision IS in `world_partition.md:192-197`; rationale/alternatives only in `slice_3.md:104`, `:133-135`, `:347` | PARTIAL |

`todo/README.md` moves a finished plan to `01_done/`, and `AGENTS.md:324-357`
forbids stable docs from linking to todos. So when Slice 3 closes, the "why not
a second tiled importer, why not ProceduralMesh" record becomes unreachable by
design. That is exactly the class the project's own "lift it INTO the doc" rule
targets, and it was missed for these two.

### G9 - Dead and misleading agent-facing files, including inside always-loaded context

| File | Evidence |
|---|---|
| `agent_policy.yaml` (81 lines, repo root) | All 8 referenced `scripts/win/*.cmd` are MISSING (`project_validate.cmd`, `full_compile.cmd`, `bp_compile.cmd`, `data_validate.cmd`, `unit_tests.cmd`, `build_editor.cmd`, `run_tests.cmd`, `first_error.bat`). `grep -rn agent_policy` over tracked md/ps1/py/json/toml returns nothing. Fully dead. |
| `.clinerules` (4 lines) | Cline-only; project uses Claude Code + Codex. |
| `docs/architecture/source_of_truth.md` (153 lines) | `:8` claims "THIS FILE is authoritative for the whole map and flows". `:15` World tier omits `ProjectWorldData`, `ProjectWorldTestData`, and all of `tools/World`. `:5-7` describe a Backstage catalog (`find . -name catalog-info.yaml` = 0 hits) and an OpenTelemetry service graph (only mention in the repo). It is routed to as "Source of Truth" from `plugin_rules.md:5`, one of the 26 always-imported files. |
| Doc link rot | 46 broken relative `.md` links across 17 files. 23 sit inside always-imported files: `conventions.md` 6, `principles.md` 5, `troubleshooting.md` 4, `smoke_tests.md` 3, `crash_investigation.md` 2, `unit_tests.md` 2, `workflow.md` 1. Zero broken links in World docs. |
| Todo-reference rule has a hole | `docs/testing/smoke_tests.md:689` and `docs/testing/automation.md:703` both link `../../todo/create_architecture.md` (deleted), violating `AGENTS.md:324`. The documented check at `AGENTS.md:352` is `grep -rE "todo/(00_current\|01_done)/"`, so it cannot match a bare `todo/<name>.md`. `smoke_tests.md` is always-imported, so every session reads a stable doc breaking a CRITICAL rule stated 350 lines earlier in the same window. |
| `docs/architecture/diagrams/README.md:3` | References `../GLOSSARY.md` and `../RESPONSIBILITIES.md`; neither exists. |
| `canonical.md:6` | "Under 400 lines on purpose" - the file is 884 lines. |
| `territory_generation.md:30-37` owner map | Omits `tools/World/VisualVerification/`, which owns the surface-quality gate `canonical.md:325-327` points at. |

### G10 - `scientific_debugging.md` survives only because of the force-import - RESOLVED

**Status: FIXED by C2.** `AGENTS.md:133` now carries a
"READ BEFORE CHANGING BEHAVIOR (CRITICAL!)" block that names the trigger and
the path directly, so the mandatory read no longer depends on an import.

Its single inbound link was `canonical.md:285`. `AGENTS.md` never mentioned it.
It is reachable today only because `@docs/agents/canonical.md` drags canonical
into every session. Fixing G1 naively would delete the mandatory-read trigger
for the best process doc in the repo. Any G1 fix MUST preserve an
always-visible pointer to the debugging triggers.

### Sandbox note - verify before relying on it

`.claude/settings.local.json` sets `"sandbox": {"enabled": true,
"autoAllowBashIfSandboxed": true, ...}`. Primary docs: "Claude Code's sandboxed
Bash tool supports macOS, Linux, and WSL2, but WSL1 and native Windows are not
supported" ([sandboxing](https://code.claude.com/docs/en/sandboxing)). This
machine is native Windows 10, so the block is not providing the containment its
presence implies. Whether `autoAllowBashIfSandboxed` is inert or auto-allowing
on an unsupported platform was NOT determined this pass. Do not count it as a
control, and check it before any further permission loosening.

---

## 3. Rejected additions / overengineering

Considered and rejected. Each has the reason it fails the "demonstrated
failure or gap" bar.

| Considered | Rejected because |
|---|---|
| New C4 diagrams for World ownership / canonical compilation / realization / streaming / transaction / active authority | The C4 workspace covers 0 of 6 (1 partial, name-only). But `architecture_overview.md` covers all six in Mermaid, and `territory_generation.md:11` routes to it as row 1 of its task table. `architecture_overview.md:6` states the intent explicitly: "World work does not require the project-wide Structurizr workspace." A fresh agent reaches the right diagram in two hops. No missing relationship remains that cannot be understood. Adding C4 world views creates a second world SOT to keep in sync - the exact bureaucracy to avoid. ONE exception survives into section 5 as a two-line disambiguation, not a diagram. |
| A parallel ExecPlan / PLANS.md system | Slice 2 and Slice 3 already carry every ExecPlan section. Codex's guidance is that an ExecPlan must allow restart "from _only_ the ExecPlan and no other work" ([codex_exec_plans](https://developers.openai.com/cookbook/articles/codex_exec_plans)). ALIS deliberately inverts this: `slice_3.md:50-51` "Stable docs own architecture. This todo owns only review state, execution order, evidence, and blockers." For a multi-month effort with durable SOTs and a one-way todo->docs rule, the ALIS split is BETTER than a self-contained plan that duplicates architecture and rots. Adopt the ExecPlan resumability TEST, not the ExecPlan FORMAT. |
| A plan template file in `todo/` | Two proven exemplars exist. A template is a maintained artifact that will drift, and the observed low-quality plans (`fix_wpo_foliage.md` is a knowledge dump, not a plan) are old and in `01_done/`, not a current-practice regression. Point at slice_2/slice_3 as exemplars instead. |
| Swarm / multi-agent orchestration, agent teams, `.claude/agents/` role definitions | No evidence in this repo of work that failed for lack of parallel agents. The observed failures were evidence-quality and envelope failures, which more agents make worse. `overnight_mode.md:39-58` already covers subagent discipline for the autonomous path. |
| Class diagrams | No communication failure found that a class diagram would solve. Explicitly out per the brief. |
| Mechanically encoding architecture / naming / style judgment | Already correctly split: `validate_no_alis_prefix.py`, `validate_text_format.py`, `validate_plugin_data_staging.py` encode the mechanical half; the judgment half stays prose. Do not extend automation into SRP/boundary calls. |
| A docs link checker as a NOW item | 46 broken links sound alarming, but 23 are inside files that should not be always-loaded at all (G1), and World docs have zero. Fixing G1 removes most of the harm. A link checker is cheap and worth having, but LATER. |
| ADRs for canonical-cells-vs-WP-cells, HLOD exclusion, generated-vs-runtime authority | All three are already in stable docs with multiple corroborating locations, and two are asserted in code. An ADR would duplicate a live SOT - the anti-pattern the brief warns about. |
| Rewriting `canonical.md` down to its claimed <400 lines | An 884-line SOT is not itself a defect; it is a reference, read on demand. It only hurts because it is force-imported. Fix the import, then fix the stale claim at `:6` with a one-word edit. |
| Migrating skills into a Claude Code plugin | Plugins add a distribution layer for a 5-skill, single-repo, two-tool setup. `.agents/skills` + `.claude/skills` is smaller and both vendors document it. |
| A new "reviewer agent" definition file / `.claude/agents/reviewer.md` | The gap (G7) is that the reviewer ROLE is undefined, not that a new agent type is missing. A 12-line table in an existing doc closes it. |
| Encoding the Checkpoint Scope Rule mechanically | It is genuinely subjective ("is this the approved candidate?"). Only its most durable consequence - unattended L3 - is mechanizable, and that is proposed in section 5. |

---

## 4. Target KISS operating model

One page. Nothing here is new machinery; it is the existing system with the
context leak closed.

**Context budget.** `AGENTS.md` is a ROUTER: it is read in full every session
and must stay under ~450 lines with ZERO `@` imports. Everything else is opened
on demand, by path, when the task needs it. The only content that earns
always-loaded status is (a) the routing table, (b) the CRITICAL operational
rules, and (c) skill descriptions - which both tools show without loading
bodies.

**Two tools, one source.** Instructions: `AGENTS.md` at the root, under
32 KiB so Codex reads all of it, symlinked as `CLAUDE.md` / `CODEX.md`
(already true). Skills: one tracked directory, discoverable by both.
Enforcement: one guard script, registered as a PreToolUse hook in both tools.

**Plan shape.** Keep the slice-plan format exactly as it is. Add one table
that binds what the plan already contains:

```text
invariant -> acceptance surface -> execution envelope -> cheapest proof -> final proof -> stop condition
```

One row per invariant. The reviewer's job at R1 becomes mechanical: find an
invariant with no final proof, a proof with no invariant, or a proof whose
envelope differs from the shipping envelope.

**Layers.** L0-L4 unchanged. Judgment rules stay prose; the two durable/
irreversible actions (commit, unattended L3) become impossible rather than
discouraged.

**Topology.** Smallest rule that covers observed practice:

| Situation | Use |
|---|---|
| Continuing the same slice, context healthy | same agent |
| Context compacted, or resuming a paused slice | fresh agent, bootstrapped from the plan's Status + Read first + unchecked items only |
| Bounded read-only fan-out (inventory, grep sweep, doc survey) | subagent, returns conclusions not file dumps |
| R1 / R2 review | fresh agent in the OTHER tool, given only the plan + its Read-first links + the diff. Never the agent that wrote the code. |
| Speculative or destructive experiment on tracked files | worktree |
| Anything durable (commit, push, L3, publish) | human |

**Knowledge flow.** Todo -> stable doc is one-way and must happen BEFORE the
todo is archived. A decision whose reversal would reopen multiple slices gets
lifted into the owning SOT at the moment it is accepted, not at archival.

---

## 5. Exact minimal repo changes proposed

Ten changes. Each shows the smallest diff shape. C1, C2, and C6 are DONE; the
rest are proposals.

### C1 - Strip `@` from the 26 route markers in AGENTS.md - DONE

```text
problem prevented   ~104k tokens of irrelevant instruction loaded every
                    session and re-loaded after every compaction
evidence            G1; 26 imports = 9,897 lines / 340,279 chars measured
current owner       AGENTS.md (26 lines)
proposed owner      AGENTS.md (unchanged file, 26 characters removed)
smallest change     - Build entire project? -> @docs/build/README.md
                    + Build entire project? -> docs/build/README.md
                    (mechanical; the path is still there, still followable)
maintenance cost    zero
```

**Implemented.** All 35 `@` characters across the 26 unique paths removed from
`AGENTS.md` (the real file; `CLAUDE.md` and `CODEX.md` are symlinks to it).
Verified: 0 `@` route markers remain; file is 466 lines / 20,844 bytes; every
path still present and still resolvable. Shipped together with C2, which was
mandatory - stripping the imports alone would have deleted the
`scientific_debugging.md` trigger (G10).

### C2 - Add a "Read before changing behavior" block to AGENTS.md - DONE

```text
problem prevented   losing the mandatory-read triggers that only survive today
                    because canonical.md is force-imported
evidence            G10 - single inbound link at canonical.md:285
current owner       canonical.md:285 (reachable only via import)
proposed owner      AGENTS.md, one short block naming trigger + path
maintenance cost    zero
```

**Correction to revision 1.** Revision 1 proposed routing multi-week slice
planning to "canonical.md sections 7 and 10". That was wrong: section 10 is the
mega-file / file-size guardrail and is not a planning contract. The implemented
block routes correctly.

**Implemented** at `AGENTS.md:133`, three routes:

```text
### READ BEFORE CHANGING BEHAVIOR (CRITICAL!)

Routes above are paths to open on demand, NOT auto-loaded context.

Reality contradicts a green gate, an acceptance surface proved wrong, or two
hypotheses were falsified?
-> docs/agents/scientific_debugging.md is MANDATORY before changing behavior.

Planning or reviewing a multi-week slice (layer choice, review cadence,
ownership boundary)?
-> docs/testing/world_pipeline_layers.md, plus the current slice plan and
   the owning SOT.

Deciding checkpoint scope, gate escalation, or expensive-gate budget?
-> docs/agents/canonical.md section 7.
```

The first line is doing quiet but important work: it tells a fresh agent that
the route table is paths, not preloaded context - which is the behavior C1
just changed.

### C3 - Add the proof-traceability table to the slice-plan shape

```text
problem prevented   an invariant with no proof, or a proof run in an envelope
                    that is not the shipping envelope, going unnoticed until
                    the artifact is visibly wrong
evidence            G6 - slice_3.md:354-375 vs :529-556 unlinked; confirmed
                    root cause at :795-802 was an envelope mismatch
current owner       nothing; two unlinked lists per plan
proposed owner      docs/testing/world_pipeline_layers.md, new section
                    "Proof traceability" (~14 lines), referenced from the
                    Review Cadence section
smallest change     one 6-column table spec + one worked row, plus the R1
                    checklist rule below
maintenance cost    per-plan authoring cost only; no new file, no automation
```

**Directionality rule (narrowed on reviewer instruction).** The mapping is
one-way, not bijective:

- Every ACCEPTANCE proof MUST map to an invariant. A proof that gates the slice
  and answers to no stated invariant is either an undeclared requirement or
  wasted expense.
- An invariant with no final proof is an R1 blocker.
- Diagnostic evidence MAY exist with NO invariant, provided it is explicitly
  labelled diagnostic / non-authoritative. It never gates anything.

Revision 1 stated this as a symmetric "no proof without an invariant", which is
wrong in practice: it would push agents to manufacture fake invariants to
justify useful probes. `scientific_debugging.md` rail 10 explicitly says to KEEP
independently useful diagnostics after a hypothesis is falsified - the H1
final-surface reader in that document is exactly such an instrument, and it
existed before any invariant named it. The label is what prevents a diagnostic
from being quietly reported as acceptance, which is the actual failure mode at
`canonical.md:319-322`.

R1 checklist line: "R1 blocks a plan with an invariant that has no final proof,
an acceptance proof that maps to no invariant, or a proof whose execution
envelope differs from the shipping envelope without being marked diagnostic."

Apply it to Slice 3 on resume by restructuring `:354-375` and `:529-556` into
one table. Nothing new is written - the columns are filled from content the
plan already has, except `execution envelope`, which is the point.

### C4 - Define the independent R1/R2 reviewer contract (NOW)

```text
problem prevented   R1/R2 re-invented per slice; a slice reviewed by the agent
                    that authored it, which cannot catch an authoring blind
                    spot
evidence            G7 - defined inline at slice_2.md:37 and :305; one
                    "reviewer" hit in world_pipeline_layers.md:54
current owner       each slice plan, ad hoc
proposed owner      docs/testing/world_pipeline_layers.md "Review Cadence"
smallest change     ~12 lines fixing the invariant, the two gates, and the
                    shared verdict vocabulary
maintenance cost    zero
```

**The invariant is INDEPENDENCE, not tooling.** The reviewer must be fresh and
must not have authored the implementation under review. A different tool or
model is PREFERRED, because it decorrelates blind spots - but it is not
mandatory, and the operator chooses the reviewer. Revision 1 over-specified
this as "fresh agent in the other tool"; that is a preference, not a rule.

Contract to record:

```text
R1  fresh pre-code architecture review    (before production code)
R2  fresh integrated exit review          (after focused L0/L1 pass)
verdicts  PASS / PATCH / BLOCKER          (shared vocabulary, both gates)
inputs    the slice plan + its Read-first links + the diff
bar       reviewer did not author the implementation
```

Promoted from LATER to NOW on reviewer instruction: the next thing Slice 3 does
on resume is a review boundary, so the contract has to exist before it, not
after.

### C5 - One tracked skill body both tools can discover - DECISION REQUIRED / LATER

```text
problem prevented   two divergent copies of the same ALIS workflow, one of
                    them outside version control
evidence            G3 - ~/.codex/skills/alis-character-animation-debug/
                    (untracked, 2026-04-06) vs .claude/skills/
                    character-animation-dev/ (tracked)
current owner       .claude/skills/ (Claude only) + ~/.codex/skills/ (untracked)
proposed owner      DECISION REQUIRED - topology deliberately not preselected
maintenance cost    depends on chosen topology
```

Revision 1 preselected a directory symlink. Withdrawn - that is an operator
decision, and `AGENTS.md` forbids creating links without asking anyway. This
report records only the REQUIREMENTS any solution must satisfy:

1. Exactly one tracked canonical skill body per skill. No second prose copy.
2. Discoverable by Claude Code AND by Codex from a clean checkout.
3. No divergent personal/untracked copy anywhere
   (`~/.codex/skills/alis-character-animation-debug/` must go, after merging
   anything newer in it).
4. Safe on a case-sensitive checkout: `SKILL.md`, exact casing, verified via
   `git ls-files` rather than the local filesystem. Rename
   `character-animation-dev/skill.md` -> `SKILL.md` with plain `mv`, not
   `git mv`, per the operator branch rule; do not stage.
5. Whatever keeps the copies identical must FAIL LOUDLY on divergence rather
   than drift silently.

Note on `agents/openai.yaml`: it is optional Codex interface metadata
(display name, short description, default prompt). It is NOT the discovery
contract - discovery is the directory location plus `SKILL.md` frontmatter.
Do not design the topology around it.

### C6 - Remove the live commit contradiction - DONE

```text
problem prevented   an allowlisted skill telling the agent to run
                    `git add -A; git commit` on a user branch
evidence            G4 - SKILL.md:227, :349-350, :505 vs AGENTS.md:377-418
                    and overnight_mode.md:23-24, :137
current owner       .claude/skills/ue-gamefeature-ci/SKILL.md
proposed owner      same file (contradiction removed)
smallest change     replace every commit instruction with "leave work as
                    unstaged edits; the human commits after review" per
                    overnight_mode.md:23-24; drop git add / git commit from
                    the tool list at :450; strip 25 emoji
maintenance cost    zero
```

**Action taken by the operator:** `.claude/skills/ue-gamefeature-ci/` was
deleted outright and its `Skill(ue-gamefeature-ci)` allowlist entry removed
from `.claude/settings.local.json`. Verified: four skills remain, zero
allowlist hits. The contradiction is gone.

**Standing rule for future cases (reviewer correction to revision 1).**
Revision 1 recommended deleting the skill *because* `overnight_mode.md`
exists. That conflates two decisions and is the wrong default. The rule is:

1. Remove the contradiction IMMEDIATELY. A rule and an instruction to break it
   must never coexist, and the fix is bounded and reversible.
2. Retirement is a SEPARATE, later decision, valid only once the artifact's
   remaining functionality is proven redundant. "A newer doc covers the same
   topic" is a hypothesis about redundancy, not evidence of it.

Applied here, step 1 and step 2 happened to collapse into one action because
the operator judged the whole skill superseded. That does not make "delete on
suspicion of overlap" the general rule.

### C7 - Mechanically block agent commits in both tools

```text
problem prevented   an autonomous commit on the user's working branch
evidence            G4 - deny list has git push but not git commit; zero
                    hooks configured; a skill actively instructs it
current owner       prose only (AGENTS.md:377, overnight_mode.md:137)
proposed owner      one guard script + two hook registrations
smallest change     scripts/agents/guard_tool_use.py (~50 lines): read the
                    hook JSON on stdin, deny when a Bash subcommand is
                    `git commit` or `git push`. No exceptions, no env
                    override. Register as PreToolUse(Bash) in
                    .claude/settings.json and in Codex hooks config.
maintenance cost    one script, no per-rule edits
```

**Scope narrowed on reviewer instruction, two ways:**

- **`git add` is NOT blocked.** Revision 1 included it. The evidence supports
  prohibiting COMMITS - `AGENTS.md:377`, `overnight_mode.md:23-24` and `:137`
  all target the commit, and `overnight_mode.md:33-34` explicitly defines the
  deliverable as "the ready-to-commit state (the files, unstaged)". There is no
  equivalent evidence that staging must be impossible, and blocking `git add`
  breaks legitimate index-based inspection (`git add -N` for intent-to-add
  diffs, `git diff --cached` review). The operator's own memory note about not
  staging on user branches is a behavioral rule, correctly left to judgment.
- **No escape hatch.** Revision 1 proposed `ALIS_AGENT_COMMIT_OK` plus a
  `bot/*` branch check. Withdrawn. ALIS agents never commit - full stop. An
  env-var bypass is a hole that any agent can reason itself into opening, and
  it re-introduces exactly the judgment call the guard exists to remove. A
  differently-governed repo would need its own explicit policy; it must not
  inherit a backdoor from this one.

Rationale for a hook rather than a deny rule alone: a PreToolUse hook exiting 2
blocks BEFORE permission rules are evaluated, and the same script registers in
both tools, so the prohibition cannot drift between Claude and Codex. Add
`permissions.deny: Bash(git commit *)` and `Bash(git push *)` as a cheap second
layer - per current docs those deny rules block in every mode including
bypassPermissions, so the two layers are complementary rather than redundant.
See section 7.

### C8 - Close the unattended-L3 pre-approval

```text
problem prevented   an unattended durable enrollment into ProjectWorldData
                    while a plan is parked at a pre-L3 operator checkpoint
evidence            G5 - settings.local.json allow ends in `:*`;
                    scripts/ue/world/README.md:105-109 lets -NonInteractive
                    waive the yes prompt; slice_3.md:807-816 is parked pre-L3
current owner       .claude/settings.local.json (one allow entry)
proposed owner      same entry, narrowed; plus the C7 guard script
smallest change     deny agent-executed `-EnrollManifests`. One flag, one
                    rule. Narrow the `:*` allow entry so it no longer
                    pre-approves every argument set.
maintenance cost    ~5 lines inside the script that already exists after C7
```

**Simplified on reviewer instruction.** Revision 1 proposed parsing
combinations of `-Mode Apply` + `-NonInteractive` + `-WorldDataPlugin`, guarded
by an `ALIS_L3_APPROVED` env var. Both are withdrawn:

- **Match the real irreversible boundary, not a combination.**
  `scripts/ue/world/README.md:227-230` already names the single authorizing
  flag: "`-EnrollManifests` is a different route - it waives only the 'no
  accepted manifest yet' refusal for a brand-new scope". Enrollment is the
  point of no return; `-Mode Apply` on its own is reversible and is the normal
  L1/L2 loop. A multi-flag predicate is more code, more false positives, and
  more ways to be wrong than the one flag that actually marks the boundary.
- **Policy: production enrollment is HUMAN-executed.** After operator approval,
  the human runs it. There is no `ALIS_L3_APPROVED` session variable, because
  an env var that an agent can observe is an env var an agent can talk itself
  into setting - and this is precisely the checkpoint `slice_3.md:807-816` is
  parked at.
- **Do not slow the inner loop.** TestData work, reversible `-Mode Apply`
  diagnostics, Validate, audit, and history are untouched. A guard that makes
  L1/L2 iteration slower will be worked around, which is worse than no guard.

If agent-executed approved L3 is ever genuinely needed, design a one-shot
operator-issued capability at that point - a single-use token bound to a
specific run, not an ambient environment flag. Do not build it speculatively.

### C9 - Widen the todo-reference check and add the two missing routes

```text
problem prevented   stable docs pointing at deleted todos; a documented check
                    that cannot catch its own rule's violations
evidence            G9 - smoke_tests.md:689 and automation.md:703 link
                    todo/create_architecture.md; AGENTS.md:352 regex matches
                    only todo/(00_current|01_done)/
current owner       AGENTS.md:352 (documented grep, not automated)
proposed owner      same grep, widened; optionally into validate_all
smallest change     - grep -rE "todo/(00_current|01_done)/"
                    + grep -rE "\]\([^)]*todo/|todo/(00_current|01_done)/"
                    then delete the two dead links.
                    Separately: add tools/World/VisualVerification to
                    territory_generation.md:30-37 owner map (1 row), and add
                    one line to docs/architecture/diagrams/README.md stating
                    that "promote"/"rollback" in the C4 model refer to CDN
                    bundle delivery, NOT world realization - the world
                    transaction is architecture_overview.md:251-298.
maintenance cost    zero
```

That last line is the ONLY diagram-side change proposed. It exists because the
C4 model contains an active false friend: `model_cdn_components.dsl:12` and
`model.dsl:43` use promote/rollback for plugin-bundle delivery, so an agent
searching the architecture diagrams for the world transaction finds the wrong
mechanism rather than nothing.

### C10 - Codex/Claude engine-environment parity (NOW)

```text
problem prevented   two agents doing World/UE work against different engine
                    roots - the execution-envelope failure class this repo
                    has already paid for once
evidence            ~/.codex/config.toml pins UE_EDITOR_CMD to
                    <ue-path>/...; .claude/settings.local.json
                    sets UE_PATH=<ue-path>; the project engine
                    is UE 5.8. Neither setup_ue_env.ps1 nor
                    scripts/ue/update/update_engine.ps1 contains any `codex`
                    reference, so the engine-update orchestrator syncs the
                    Claude side and .mcp.json but never the Codex side.
current owner       ~/.codex/config.toml, hand-maintained, drifted
proposed owner      the existing project-owned resolver
                    (scripts/config/ue_path.conf via setup_ue_env.ps1)
smallest change     eliminate the duplicated engine truth: either remove the
                    hardcoded engine paths from the Codex config so it
                    resolves from the same env the rest of ALIS uses, or
                    extend setup_ue_env.ps1 to rewrite the Codex MCP env
                    block the same way it already rewrites .mcp.json.
                    Prefer removal over a second sync target.
maintenance cost    zero if removed; one sync branch if synced
```

This is not config tidiness. `scientific_debugging.md` rail 6 is "The execution
envelope is part of the input", and the confirmed root cause of the flat-terrain
incident (`slice_3.md:795-802`) was a proof run in the wrong envelope. Two
agents about to spend months on the same World pipeline, pointed at two
different engine roots, is the same defect class waiting upstream of the work
rather than downstream of it.

`docs/ue_engine/version_update.md` states that ONE command owns every
deterministic engine mutation and that engine paths are never hand-edited.
A hand-maintained Codex config pinning a stale engine is a direct exception to
that contract, and it is invisible to `validate_engine_env.py` because that
validator scans tracked repo text, not `~/.codex/`.

---

## 6. Text that should MOVE or be DELETED, not duplicated

| Item | Action | Destination / reason |
|---|---|---|
| `agent_policy.yaml` (81 lines, root) | DELETE | All 8 referenced scripts missing; zero inbound references. Dead config at the repo root that a fresh agent will read as live. |
| `.clinerules` (4 lines, root) | DELETE | Cline is not used; Claude + Codex are. |
| `docs/architecture/source_of_truth.md` (153 lines) | DELETE or demote | Describes a Backstage + OpenTelemetry architecture that does not exist (0 `catalog-info.yaml`, no other OTel reference), and its World tier omits `ProjectWorldData`, `ProjectWorldTestData`, and `tools/World`. If kept, retitle to "Aspirational architecture notes (not authoritative)" and remove the "Source of Truth" pointer at `plugin_rules.md:5`, which is always-imported. |
| `docs/testing/smoke_tests.md:689`, `docs/testing/automation.md:703` | DELETE the two links | Point at deleted `todo/create_architecture.md`, violating `AGENTS.md:324`. |
| `docs/architecture/diagrams/README.md:3` | DELETE the two refs | `../GLOSSARY.md` and `../RESPONSIBILITIES.md` do not exist. |
| `slice_3.md:123-124` and `:138` (native Landscape / no second tiled importer / no ProceduralMesh fallback) | MOVE | Into `world_partition.md` next to `:206-210`, as a rejected-alternatives note. This is the one true ADR-class decision that is currently todo-only (G8). Reversing it reopens realization, dirty scope, and identity. |
| `slice_3.md:104`, `:133-135`, `:347` (Water plugin rationale) | MOVE the rationale | The decision already lives at `world_partition.md:192-197`; append the two-sentence rationale (experimental lifecycle, Nanite/SingleLayerWater incompatibility) so the archived todo is not the only record. |
| `~/.codex/skills/alis-character-animation-debug/` | DELETE after merge | Untracked duplicate of `.claude/skills/character-animation-dev/`. Merge anything newer in it first, then remove. |
| `.claude/settings.local.json.backup`, `.claude/settings.local.json.engine-sync.bak`, `.mcp.json.engine-sync.bak` | DELETE | Backup files tracked next to live config invite editing the wrong one. |
| `canonical.md:6` "Under 400 lines on purpose" | EDIT | The file is 884 lines. Either fix the number or drop the clause; a false self-description undermines the rest. |
| `.claude/skills/ue-gamefeature-ci/SKILL.md` | DELETE (preferred) | Superseded by `docs/agents/overnight_mode.md`, which states the opposite commit policy. See C6. |

Explicitly NOT duplication to remove: the AGENTS.md / canonical.md rule pairs
(dev loop, checkpoint scope, file size, migration policy). Each names the other
and one is mechanically enforced. Their only cost is that both are always
loaded, which C1 fixes.

---

## 7. Tool enforcement feasibility: Claude vs Codex

Sources are current vendor docs, cited inline.

### What each tool can do

| Capability | Claude Code 2.0.76 | Codex |
|---|---|---|
| Instruction file | `CLAUDE.md`, plus `@path` imports expanded at launch, depth 5 ([memory](https://code.claude.com/docs/en/memory)) | `AGENTS.md` / `AGENTS.override.md`, global `~/.codex` then repo root down to cwd, concatenated, capped at `project_doc_max_bytes` = 32 KiB default ([agents-md](https://developers.openai.com/codex/guides/agents-md)). No `@` expansion. |
| Path-scoped rules | subdirectory `CLAUDE.md` | `<repo>/.codex/rules/*.rules`, loaded only when the project is trusted ([rules](https://developers.openai.com/codex/rules)). ALIS IS trusted. |
| Skills | `.claude/skills/<n>/SKILL.md`, `~/.claude/skills/` ([skills](https://code.claude.com/docs/en/skills)) | DOCUMENTED: `.agents/skills` scanned cwd-to-repo-root, and `<home>/.agents/skills` ([skills](https://developers.openai.com/codex/skills)) |
| Skill format | `SKILL.md` + frontmatter `name`, `description`, `allowed-tools`, `metadata`, `license`, `compatibility` | `SKILL.md` + frontmatter `name`, `description` |
| Hooks | PreToolUse/PostToolUse and others, in `settings.json` ([hooks](https://code.claude.com/docs/en/hooks)) | 11 hook events incl. a blocking PreToolUse; `features.codex_hooks`, `hooks.json` or inline `[hooks]` ([hooks](https://developers.openai.com/codex/hooks)) |
| Hook can block a tool | YES. Exit 2, or exit 0 with `hookSpecificOutput.permissionDecision: "deny"`. Exit 2 blocks BEFORE permission rules are evaluated | YES. PreToolUse "can stop the tool before it runs"; `"decision": "block"`; any deny wins |
| Declarative command deny | `permissions.deny`, e.g. `Bash(git commit *)` - blocks in every mode, including bypassPermissions | Codex Rules / execpolicy, incl. `prefix_rule(decision = "forbidden")` ([rules](https://developers.openai.com/codex/rules)); plus sandbox mode + approval policy |
| Sandbox | macOS / Linux / WSL2 only; native Windows NOT supported ([sandboxing](https://code.claude.com/docs/en/sandboxing)) | `sandbox_mode` = `read-only` / `workspace-write` / `danger-full-access` with `approval_policy` ([sandboxing](https://developers.openai.com/codex/concepts/sandboxing)) |

### Does prefix matching or shell chaining defeat a naive rule?

The operator memory note ("Bash rules are prefix-only; chained commands can
never be allowlisted") is STALE for 2.0.76. Current docs
([permissions](https://code.claude.com/docs/en/permissions)):

- matching is glob/wildcard based, and a wildcard may appear at ANY position -
  not prefix-only. A trailing space carries a word-boundary rule;
- `:*` is equivalent to a trailing ` *`;
- Claude Code is shell-operator aware - "a rule like `Bash(safe-cmd *)` won't
  give it permission to run the command `safe-cmd && other-cmd`". Recognized
  separators: `&&`, `||`, `;`, `|`, `|&`, `&`, newlines. Compound commands are
  SPLIT and a rule must match EACH subcommand independently;
- a rule shaped `Bash(command:rm *)` is IGNORED with a startup warning,
  precisely because it would be bypassable by a compound command;
- deny rules block in EVERY mode, including bypassPermissions.

**Correction to revision 1.** Revision 1 justified the hook by claiming a
settings deny rule does not survive `--dangerously-skip-permissions`. That was
wrong; deny rules block in every mode. The hook is still the right primary
mechanism, but for different reasons: a PreToolUse hook exiting 2 blocks
*before* permission rules are evaluated, it carries logic a glob cannot express
(argument shape, branch state), and one script registers in both tools so the
prohibition cannot drift between Claude and Codex. Deny rules are the cheap
second layer, not a weaker substitute.

So chaining does NOT defeat a well-formed Claude rule. What still does:

1. **Indirection** - `sh -c "git commit ..."`, a `.ps1`/`.py` wrapper, or a
   Makefile target that runs git internally. Neither a rule nor a hook matching
   literal `git commit` sees these. Real residual; state it, do not paper over
   it.
2. **The allow side** - a trailing `:*` on a script path (G5) pre-approves every
   argument set, which is how the L3 confirmation prompt becomes waivable. This
   is an allow-list defect, not a matching defect.
3. **Codex-side opaque invocations** - Codex documents that redirection,
   `$(...)` command substitution, environment assignments, wildcards, and
   control flow are NOT split, and evaluate as one opaque `bash -lc`
   invocation. That is a materially weaker guarantee than Claude's per-
   subcommand split, and it hits this repo directly: the G5 allow entry begins
   with the env assignments `MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL=*`, exactly
   the shape Codex treats as one opaque string. Any Codex-side rule must
   therefore be written against the opaque form, and the blocking PreToolUse
   hook - which sees the whole command text - is the more reliable control
   there.

**Codex skills-path divergence (flag).** The documented Codex skill locations
are `.agents/skills` (scanned cwd-to-repo-root) and `<home>/.agents/skills`.
`~/.codex/skills` is NOT a documented discovery path, yet this machine has a
populated `~/.codex/skills/` containing `.system/` skills and the untracked
`alis-character-animation-debug`. The local layout therefore diverges from the
documented contract - possibly a junction, an installer artifact, or an
undocumented path. C5 must design against the DOCUMENTED paths
(`.agents/skills`), not against the observed local layout, or the solution
will not reproduce on a clean machine.

### Verdict per prohibition

| Prohibition | Claude | Codex | Mechanism |
|---|---|---|---|
| `git commit` by agent | FEASIBLE, strong | FEASIBLE | PreToolUse hook, deny, no exceptions. Second layer: `permissions.deny: Bash(git commit *)` / Codex `prefix_rule(decision = "forbidden")`. Residual: indirection via a wrapper script. |
| `git push` by agent | ALREADY DENIED, add hook | FEASIBLE | `settings.json` deny already covers it; add to the same hook so one script owns the whole prohibition in both tools. |
| `git add` by agent | FEASIBLE but NOT PROPOSED | same | No evidence supports it, and it breaks index-based inspection. See C7. |
| Production `-EnrollManifests` (L3) | FEASIBLE, one flag | FEASIBLE | Same hook, single-flag rule (C8). Narrowing the `:*` allow entry alone is NOT sufficient - it removes pre-approval but does not deny. |
| Architecture / naming / SRP judgment | NOT feasible, DO NOT ATTEMPT | same | Stays prose plus the existing narrow validators. |

Recommended shape: ONE `scripts/agents/guard_tool_use.py`, registered as
PreToolUse in both tools. Same script, two registration snippets - no divergent
logic between Claude and Codex, it runs before permission rules are evaluated,
and it is the only layer that can see the whole command string on the Codex
side where compound splitting is not guaranteed.

---

## 8. Priority: NOW / LATER / REJECT

### NOW - before resuming Slice 3

Order revised on reviewer instruction. C6 moves ahead of C7 (remove the
instruction before building the guard that would fight it). C10 is new. C4
moves up from LATER because it gates the next implementation, not the run.

| # | Change | Status | Why it gates resumption |
|---|---|---|---|
| C1 + C2 | strip `@`, add the read-on-demand block | DONE | Reclaims ~104k tokens/session and every post-compaction reload. Every other continuity improvement is downstream of this. Shipped as a pair. |
| C6 | remove the commit instruction from `ue-gamefeature-ci` | DONE (skill deleted, commit `0a72a1074`) | Highest-severity live contradiction: an allowlisted skill instructed `git add -A; git commit` on a user branch. Standing rule going forward: fix the contradiction first; prove redundancy before deleting a workflow. |
| C7 | commit/push guard hook | TODO | Makes `git commit` and `git push` mechanically impossible. No `git add` block and no env-var escape hatch. |
| C8 | human-only production L3 enrollment | TODO | The plan is parked at a pre-L3 operator checkpoint while the L3 entrypoint is pre-approved with an unbounded wildcard. Guard the one flag that marks the boundary, `-EnrollManifests`. |
| C10 | Codex/Claude UE environment parity | TODO | `~/.codex/config.toml` pins UE 5.7 while ALIS runs UE 5.8. This is the execution-envelope failure class the repo already paid for. Both tools must resolve the same engine before either runs World/UE work. |
| C3 | proof-traceability table | TODO | Cheapest correctness lever for the whole slice sequence, and Slice 3's resume list is where it gets exercised first. Acceptance proofs map to invariants; diagnostic evidence is allowed when labelled non-authoritative. |
| C4 | independent R1/R2 reviewer contract | TODO | Fresh non-author reviewer at R1 and R2. Different tool/model preferred, not mandatory; the operator chooses the reviewer. |

### LATER - during the run, not blocking

| # | Change | Trigger |
|---|---|---|
| C4 | define R1/R2 reviewer role | Before the next R1, i.e. before 3B. |
| C5 | unify skills into a both-tools-visible tracked dir | When the first World-specific skill is proposed, or at the next skill edit. Needs a yes for the symlink. |
| C9 | widen todo-ref check, add the 2 routes, add the C4 disambiguation line | Next docs pass. |
| sec 6 deletions | `agent_policy.yaml`, `.clinerules`, `source_of_truth.md`, backups, dead links | Next chore pass. Low risk, low urgency once C1 lands. |
| G8 moves | lift the Landscape and Water rationale into `world_partition.md` | Before Slice 3 is archived - this is a hard deadline, not a preference. |
| sandbox verification | determine `autoAllowBashIfSandboxed` behavior on native Windows | Before any further permission loosening. |
| docs link checker | mechanize the 46-broken-link scan | Only after C1; most of the harm evaporates. |

### REJECT

New C4 World views; a parallel ExecPlan/PLANS.md system; a plan template file;
swarm/agent-team topology; class diagrams; mechanically encoding architecture
or style judgment; ADRs for the three decisions already in stable docs;
rewriting `canonical.md` to fit its stale <400-line claim; migrating skills
into a plugin; a dedicated reviewer-agent definition file.

Only TWO reusable skills were evaluated, per the brief:

- `scientific-debugging` - REJECT as a skill, ACCEPT as C2. The content already
  exists at `docs/agents/scientific_debugging.md` and is excellent. What it
  lacks is an always-visible trigger, which C2's six lines supply for both
  tools at zero maintenance. A skill would add a second copy or a stub whose
  only job is to point at the doc. If C1 later proves the trigger is still
  being missed, promote it to a skill whose body is one line: read that file.
- `large-slice` - REJECT for now. There is no observed failure of slice
  decomposition; the failures were evidence-quality and envelope failures. The
  slice format is already strong and lives in two exemplars. C3 + C4 close the
  two real weaknesses without a skill. Revisit only if a slice plan is
  authored that materially departs from the slice_2/slice_3 shape.

Canonical-content design, for when a skill IS justified: author the body ONCE
under `.claude/skills/<name>/SKILL.md`, expose it to Codex via `.agents/skills`
(symlink preferred, hash-equality validator as fallback), and keep
`agents/openai.yaml` as the Codex interface wrapper - the convention this repo
already uses. Never maintain two prose copies.

---

## 9. Acceptance criteria for any future implementation

Each proposed change must be provable, cheaply, without a UE build.

**C1 + C2 (context)**
- `grep -c "@[A-Za-z0-9_./-]*\.md" AGENTS.md` returns 0.
- A fresh session's loaded-context total is under 1,500 lines, matching
  `~/.agents/AGENTS.md:167`.
- Every one of the 26 paths is still present in `AGENTS.md` and still resolves
  (`test -f` on each).
- A fresh agent asked "reality contradicts a green gate, what do you read
  first" answers `docs/agents/scientific_debugging.md` without being told.
- Observed RED first: confirm the 26 files ARE in context before the edit, and
  are NOT after. Per `scientific_debugging.md` rail 5, a gate nobody has watched
  fail is not a gate.

**C3 (proof traceability)**
- Slice 3's resume section carries one table with one row per invariant, all six
  columns filled.
- At least one row's `execution envelope` names the shipping envelope
  explicitly, and any proof running in a different envelope is marked as
  diagnostic, not acceptance.
- A reviewer can state, from the table alone, that no invariant lacks a final
  proof and no proof lacks an invariant.
- Sabotage check: delete one invariant's proof cell; R1 must reject.

**C4 (reviewer role)**
- The next R1 names its performer, its inputs, and its PASS/PATCH criterion by
  citing the stable doc, not by restating it inline.

**C5 (skills)**
- Set equality holds from a clean shell: canonical skill directory set
  == Claude-discovered project skill set == Codex-discovered project skill
  set. Do not hardcode a count; C6 already changed it.
- `.agents/skills/<n>/SKILL.md` and `.claude/skills/<n>/SKILL.md` are identical
  by content hash (or are the same inode).
- `git ls-files` shows `SKILL.md` (uppercase) for every tracked skill.
- `~/.codex/skills/alis-character-animation-debug/` no longer exists.

**C6 (ci skill)**
- `grep -rn "git commit" .claude/skills/` returns nothing, or returns only
  text that forbids it.
- `grep -rlP "[^\x00-\x7F]" .claude/skills/` returns nothing.

**C7 + C8 (guards)**
- Observed RED: with the hook installed, an agent-issued `git commit -m x` is
  denied and the denial reason is visible. Repeat under
  `--dangerously-skip-permissions` and confirm it is still denied.
- Repeat for `git push`, and for `git commit` reached via `&&` and via `;`.
- Repeat for `realize_canonical_world.ps1 -Mode Apply -NonInteractive
  -EnrollManifests -WorldDataPlugin ProjectWorldData`.
- Confirm NOT denied: the same script against `ProjectWorldTestData`, and every
  read-only git command. A guard that slows L0/L1 iteration is a failed guard.
- Same four checks pass under Codex.
- Known residual is documented in the guard script header: indirection through
  a wrapper script or `sh -c` is not caught by command-text matching.

**C9 + section 6 (docs)**
- The widened grep returns 0 hits outside `todo/`.
- Broken relative `.md` links in `docs/**` drop from 46; World docs stay at 0.
- No deleted file is still referenced anywhere
  (`grep -rn` for each removed filename returns nothing).

**G8 (decision migration)**
- Before Slice 3 moves to `01_done/`: the native-Landscape rejected-alternatives
  note and the Water-plugin rationale are readable from
  `world_partition.md` alone, and a fresh agent asked "why not ProceduralMesh
  water" answers from stable docs without opening `todo/`.

**Global**
- ASCII-only across every changed file
  (`grep -P "[^\x00-\x7F]"` returns nothing).
- No stable doc references this audit file or any other todo.
- No production code, World code, or World data changed by any of these.


---

## 10. Operator decisions (FROZEN - implementation may not reinterpret)

Recorded after the reviewer PASS on the factual phase. C1, C2, and C6 are
already implemented and are NOT reopened.

### D-C5 Skills topology - `.agents/skills` is canonical

- `.agents/skills/<skill>/SKILL.md` is the ONE canonical tracked body.
- `.claude/skills/<skill>` becomes a PER-SKILL symlink to
  `../../.agents/skills/<skill>`. Not one directory-level link - per-skill
  links keep ownership obvious and let skills move independently.
- Conditional on a clean-checkout proof: fresh Windows checkout AND fresh
  WSL/case-sensitive checkout, with both tools discovering the full set.
- If Git symlink materialization proves unreliable on the real workstation,
  fall back to a GENERATED MIRROR plus an equality validator. Never maintain
  two authored copies.
- Fix any lowercase `skill.md` to `SKILL.md`.
- Compare the untracked personal Codex skill once, merge anything uniquely
  useful, then delete the personal duplicate.
- No `scientific-debugging` skill and no `large-slice` skill. Rejection stands.

Acceptance: set equality of canonical / Claude-discovered / Codex-discovered
skill sets. No hardcoded count.

### D-C7 Enforcement - one guard, two registrations, no exceptions

```text
agent git commit -> DENY
agent git push   -> DENY
git add          -> ALLOWED
read-only git    -> ALLOWED
```

- One tracked guard: `scripts/agents/guard_tool_use.py`, registered as
  `PreToolUse(Bash)` in BOTH Claude and Codex. The Python guard owns the
  semantic policy.
- Declarative deny rules stay as defense-in-depth, but are not the policy.
- No `ALIS_AGENT_COMMIT_OK`, no `bot/*` exception, no temporary commit grant.
  Only the operator commits in this repo.
- OPERATOR RULE: agents must NOT be told to bypass the `.claude/**` write deny
  via Bash. Either the operator authorizes the exact reviewed config files, or
  the operator applies the config diff. Teaching an agent to evade its own
  guard destroys the guard.

### D-C8 L3 - production enrollment is human-only, NARROWLY scoped

Evidence gathered before deciding (reviewer required this grep):
`-EnrollManifests` IS legitimately used by automation -
`tools/World/EndToEndValidation/app/enrollment.py:234`,
`app/execution.py:659`, and
`scripts/ue/world/test/integration/realization_layer_lifecycle.ps1:422`.
Those paths are parameterized by `-WorldDataPlugin` and exercise
`ProjectWorldTestData`.

Therefore a universal flag block is WRONG - it would break E2E validation and
the lifecycle integration test. The rule is the narrow predicate the reviewer
tolerated:

```text
DENY  iff  -EnrollManifests  AND  -WorldDataPlugin ProjectWorldData
```

- TestData enrollment stays allowed. Ordinary `-Mode Apply`, Validate,
  Reconstruct, and TestData iteration stay allowed.
- This mirrors a boundary the script already encodes:
  `realization_layer_lifecycle.ps1:187` sets
  `$isProductionIsolation = $worldDataPlugin -ceq 'ProjectWorldData'`.
- Narrow or remove the unbounded `:*` pre-approval on the realization script so
  the allowlist stops making every argument shape look routine. The hook is the
  hard stop.

### D-C10 Engine parity - one SOT, pin everything to 5.8

Operator instruction: every engine reference resolves to 5.8.

Measured drift:

```text
scripts/config/ue_path.conf:32   UE_PATH=<ue-path>
scripts/config/ue_path.conf:33   UE_SOURCE_PATH=<ue-path>
~/.codex/config.toml:13          UE_EDITOR_CMD=<ue-path>/...  [STALE]
```

- `scripts/config/ue_path.conf` (and the existing resolver that derives
  `UE_PATH` from it) REMAINS the single source of truth.
- Do NOT make `~/.codex/config.toml` a second generated copy of a version
  literal. Prefer inherited environment: project resolver -> `UE_PATH` /
  `UE_EDITOR_CMD` -> inherited by Claude and Codex -> forwarded to MCP via
  `mcp_servers.<id>.env_vars`.
- Implementation must FIRST check whether `scripts/setup/setup_ue_env.ps1`
  already establishes values Codex inherits. If yes, delete the stale Codex
  literals. If no, extend the EXISTING resolver once.
- Do not build a separate "sync Codex engine config" subsystem unless
  environment inheritance genuinely cannot work.

Acceptance: Claude-resolved UE root == Codex-resolved UE root == project
resolver root, all 5.8; zero `UE_5.7` literal remains in active ALIS Codex
configuration.

### D-C3 Proof traceability - APPROVED

Stable owner: `docs/testing/world_pipeline_layers.md`. Columns:

```text
Invariant | Acceptance surface | Execution envelope | Cheapest proof |
Final proof | Stop condition
```

One-way rule: every ACCEPTANCE proof maps to an invariant. Diagnostic probes
need no invariant; label them `DIAGNOSTIC / NON-AUTHORITATIVE`. Apply the table
to Slice 3 before World resumes.

### D-C4 Reviewer contract - APPROVED with one nuance

```text
R1 = fresh pre-code architecture review
R2 = fresh integrated exit review
Verdicts: PASS | PATCH | BLOCKER
```

Reviewer did not author the production change; receives plan + read-first SOTs
+ diff/evidence; a different model/tool is PREFERRED, not mandatory; the
operator chooses the reviewer.

Nuance: on PATCH, the SAME independent reviewer may recheck the fix. A brand
new reviewer per correction adds ceremony without adding independence.

### Final NOW implementation list (one bounded pass, after authorization)

```text
C7   commit/push guard, two registrations
C8   narrow L3 enrollment denial + allowlist narrowing
C10  engine parity, pin all to 5.8 via existing resolver
C3   proof-traceability table + apply to Slice 3
C4   R1/R2 reviewer contract
C5   skills topology + clean-checkout proof
```

C1, C2, C6: DONE. World stays paused until C3 is applied to Slice 3.


---

## 11. Session decisions (2026-08-18) - state at handoff

Recorded at end of session. Nothing staged, nothing committed.

### D-11.1 C7/C8 enforcement: simple deny rules NOW, Python guard PARKED

REVISES D-C7. The operator challenged the proportionality of a Python guard
plus two hook registrations plus a session restart, and was right to.

Applied instead, immediate effect, no restart:

```text
.claude/settings.json        deny   + Bash(git commit:*)
                                    + Bash(git commit)
                                    (git push already denied)
.claude/settings.local.json  allow  - both realize_canonical_world.ps1:* entries
```

Removing the blanket L3 allow restores the human gate: any realization run now
prompts with its exact flags visible, and the operator is the approver. That
was the actual risk - production enrollment running silently as a one-liner.

`scripts/agents/guard_tool_use.py` (45 tests passing) is KEPT but NOT
registered. It costs nothing parked.

Register it only on evidence of one of:
- an agent reaching `git -C <path> commit`, which a string rule cannot match;
- a need to block under `--dangerously-skip-permissions`, which a settings deny
  does not cover;
- wanting the `-EnrollManifests AND ProjectWorldData` predicate automated
  rather than operator-judged at the prompt.

Known residual, accepted: `Bash(git commit:*)` does not match `git -C x commit`.

### D-11.2 C10: repoint, do NOT delete - the frozen decision was unsafe as written

D-C10 said to delete the Codex `UE_EDITOR_CMD` literal so it would inherit from
the project resolver. Verification before implementing showed inheritance does
NOT exist:

```text
GetEnvironmentVariable('UE_EDITOR_CMD','User')  -> empty
$env:UE_EDITOR_CMD                              -> empty
GetEnvironmentVariable('UE_PATH','User')        -> <ue-path>
```

Deleting the line would have broken the Codex MCP server outright. The
`UEEnvSync.psm1` change that derives `UE_EDITOR_CMD` is uncommitted and has
never been run.

Applied: `~/.codex/config.toml` UE_5.7 -> UE_5.8, target verified present.
The duplicate literal remains, deliberately.

Follow-up before the literal can be deleted: verify the UEEnvSync change, run
`setup_ue_env.ps1`, confirm `UE_EDITOR_CMD` is persisted, THEN delete.

### D-11.3 C5: canonical home reversed to `.claude/skills`, junction not symlink

REVISES D-C5 on operator instruction ("1 original and 1 symlink from codex").

```text
.claude/skills/     tracked, the ONE authored body    -> Claude
.agents/skills      local junction to it, gitignored  -> Codex
```

Reversed direction so Claude, the primary tool, never depends on link
materialization; only Codex degrades if the link is missing.

The link is local and gitignored because `core.symlinks=false` here would
materialize a committed symlink as a text stub, and Codex would silently
discover zero skills. WSL portability was considered and DROPPED - ALIS is a
Windows-only UE project and is not checked out under WSL.

Junction, not symlink: Windows PowerShell 5.1 `New-Item -ItemType SymbolicLink`
fails with "Administrator privilege required" even with Developer Mode on.
Created by `scripts/agents/link_codex_skills.ps1` (junction -> symlink -> copy
fallback, matching the `install_hooks.sh` convention).

Durable Windows link knowledge, including the junction/rmtree hazard, now lives
in `scripts/setup/README.md`. Do not duplicate it here.

Also fixed: three skills carried lowercase `skill.md`; all four are `SKILL.md`.

### Remaining as of section 11 (SUPERSEDED by section 12)

```text
7.1  migrate 2 decisions to world_partition.md
     (ProceduralMesh fallback rejection, tiled-importer rejection;
      Landscape + Water already present)
7.2  canonical.md still self-describes as "400 lines", actual 884
7.3  agent_policy.yaml and .clinerules still exist - reverify dead, then remove
8.   closure states: only 3 RESOLVED recorded, 0 PARKED, 0 REJECTED
C7   optional guard registration, only on the triggers in D-11.1
C10  delete the duplicate literal once UEEnvSync is verified and run
```

World remains PAUSED. Slice 3 already carries the C3 proof table.

---

## 12. Final closure (2026-08-18, closure pass)

Every G and C item carries exactly one state. Nothing is "later maybe".

### Reviewer corrections applied

| Reviewer point | Outcome |
|---|---|
| Delete parked `guard_tool_use.py` + tests | ACCEPTED - 542 lines removed. Recoverable from commit `6dc4e0357`. |
| `link_codex_skills.ps1` recursive-deletes a junction | ACCEPTED as a defect, REFUTED as a data-loss BLOCKER (see below). Fixed with explicit unlink. |
| Remove copy fallback | ACCEPTED - a copy is a second authored body that drifts. |
| Reviewer "recheck its own fix" wording | ACCEPTED - the reviewer never authors the fix. |
| Finish or revert C10 | ACCEPTED as "finish"; the "revert UEEnvSync" half REFUTED (see below). |
| Classify the six todo refs before changing | ACCEPTED - and the scan was too narrow; see G9. |

### Refutations, with evidence

1. **"Recursive-deleting a junction destroys the linked target" - NOT reproducible.**
   Measured on PowerShell 5.1.19041 with a canary file: `Remove-Item -Recurse
   -Force` on a junction removed the link and left the target and its contents
   intact. Python's `shutil.rmtree` refuses outright. The claim describes
   PowerShell 5.0 and earlier. The script was still fixed, for the correct
   reason: depending on version-specific reparse-point handling is fragile.
   `scripts/setup/README.md` was corrected - it had repeated the same overclaim
   - and now carries the measured table. The real trap is that
   `os.path.islink()` returns False for a junction, so code branching on it
   treats a junction as an ordinary directory.

2. **"Revert the UEEnvSync addition" - REFUTED.**
   Deriving `UE_EDITOR_CMD` from the same root as `UE_PATH` is correct on its
   own merits and is covered by 3 passing tests (21/21 in
   `UEEnvSync.Tests.ps1`). Reverting it would leave `UE_EDITOR_CMD` underived
   AND keep the literal - strictly worse than either complete design.

3. **C10 completeness required a third option.** Codex env inheritance cannot
   be proven without running Codex, which is outside the verification budget,
   so neither "delete the literal" nor "revert" was honest. The conf remains
   the single authority; the Codex literal is an acknowledged derived cache;
   and drift is now caught by a new job D in
   `scripts/ue/check/governance/validate_engine_env.py`, proven RED/GREEN/SKIP
   and wired into the existing self-tests. `setup_ue_env.ps1` cannot cover this
   file - its machine-local planner is JSON-only and this config is TOML.

### Gap closure

| Gap | State |
|---|---|
| G1 always-loaded context leak | RESOLVED - 26 imports stripped |
| G2 Claude/Codex instruction asymmetry | RESOLVED - both now read the same ~20KB router |
| G3 skills in three homes | RESOLVED - one canonical body + junction; 1 operator step below |
| G4 skill instructed agent commits | RESOLVED - skill deleted (`0a72a1074`) |
| G5 unbounded L3 pre-approval | RESOLVED - both allowlist entries removed |
| G6 invariants and proofs unlinked | RESOLVED - proof-traceability table |
| G7 R1/R2 undefined | RESOLVED - reviewer contract |
| G8 durable decisions only in a todo | RESOLVED - migrated to `world_partition.md` |
| G9 dead/misleading agent files | PARTIAL - see split below |
| G10 `scientific_debugging.md` orphaned | RESOLVED - always-visible trigger |

G9 splits:

- RESOLVED: `agent_policy.yaml` and `.clinerules` deleted, plus their now-stale
  `mirror.exclude` entries; the `canonical.md` "under 400 lines" claim removed
  (actual 884); `VisualVerification` added to the World owner map; ALL
  forbidden todo references eliminated.
- The todo-reference scan in earlier revisions was too narrow - `docs/` and
  `scripts/` only. Widening it to `Plugins/`, `tools/`, and `Source/` found
  THREE dead references the audit never reported, two of them in production
  C++: `ProjectSaveSubsystem.cpp`, `ProjectSettingsService.cpp`, and
  `tools/agentic/inventory/dump_report.py` - all pointing at todos that no
  longer exist. Removed; the rationale was already inline in the comments, so
  nothing durable was lost.
- PARKED - `docs/architecture/source_of_truth.md` stale architecture claims.
  Trigger: the next change to World ownership or tooling topology, or an
  architecture-owner decision. C1 already removed its always-loaded harm.
- PARKED - broad repair of the remaining non-agent doc links.
  Trigger: only if a link check is added to validation.

### Change closure

| Change | State |
|---|---|
| C1 strip `@` imports | RESOLVED |
| C2 read-on-demand block | RESOLVED |
| C3 proof traceability | RESOLVED - contract plus applied to Slice 3 |
| C4 reviewer contract | RESOLVED - wording corrected |
| C5 skills topology | RESOLVED pending 1 operator step and post-commit clean-clone |
| C6 retire contradicting skill | RESOLVED |
| C7 prohibit agent commit/push | RESOLVED by deny rules. Python hook REJECTED - deleted as speculative. Residual: `git -C <path> commit` is not matched by a string rule. |
| C8 human-only production L3 | RESOLVED by removing the blanket allow; the operator is the gate. Automated predicate REJECTED with the guard. |
| C9 bounded hygiene | RESOLVED for the bounded items above; broad maintenance PARKED |
| C10 engine parity | RESOLVED - repoint, derivation, and drift check |

### PARKED items with triggers

```text
source_of_truth.md staleness   -> next World ownership/topology change
broad doc-link repair          -> only if a link checker is added
guard_tool_use.py reinstate    -> evidence of "git -C" evasion, or a need to
                                  block under bypass-permissions mode, or
                                  wanting the L3 predicate automated
Codex personal skill decision  -> operator step 2 below
delete Codex UE literal        -> once Codex is proven to inherit UE_EDITOR_CMD
```

### Operator steps still required

1. **Fix the tracked filename case.** Disk is now `SKILL.md`, but git still
   records `skill.md` for `character-animation-dev` (Windows `core.ignorecase`
   hides case-only renames). Staging is operator-owned:

   ```
   git mv .claude/skills/character-animation-dev/skill.md .claude/skills/character-animation-dev/SKILL.md
   ```

2. **Decide on `~/.codex/skills/alis-character-animation-debug`.** It is NOT a
   duplicate. It carries different reference docs and a unique 7.7KB
   `scripts/run_character_debug.ps1` that the canonical skill does not have.
   Deleting it blind would lose that script. Either merge the script into
   `.claude/skills/character-animation-dev/` (a `.claude/**` write, operator
   owned) or keep it deliberately and record why.

3. **Post-commit clean-clone check** for C5, which cannot honestly be run
   before the tree is committed:

   ```
   powershell -NoProfile -ExecutionPolicy Bypass -File scripts/agents/link_codex_skills.ps1 -Verify
   ```

### Verification run in this pass

```text
UEEnvSync.Tests.ps1                     21/21 pass (incl. 3 new C10 tests)
test_validate_engine_env.py             ALL PASS (incl. 4 new job-D assertions)
validate_engine_env.py --skip-identity  OK
link_codex_skills.ps1 and -Verify       junction created, set equality holds
junction canary probe                   target survives; reviewer claim refuted
forbidden todo references               0
```

No UE build, Editor, Check, Matrix, L3, or World execution.

### R2 readiness

READY for independent R2 review, with the three operator steps above listed as
known-open rather than hidden. The reviewer must be fresh and must not have
authored this change, per the contract in
`docs/testing/world_pipeline_layers.md`.

World remains PAUSED. Slice 3 carries the C3 proof table.
