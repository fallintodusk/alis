# Build an agent cinematic release workflow

**Status:** RELEASE 2.0.0 CURRENT SCOPE - REVIEW/RESEARCH NOT STARTED
**Created:** 2026-09-02 12:18 Europe/Moscow
**Owner:** ProjectCinematic Editor authoring and deterministic release-capture tooling

## Goal

Let an agent turn a short shot brief into reviewable Unreal camera/sequence work,
then hand the accepted authored sequence to the existing deterministic
ProjectCinematic/MRQ release owner. Improve agent cinematography, not rendering
infrastructure.

## Operator decisions

- Do not render video now.
- Kazan stabilization and its product PASS are complete and must not be reopened.
- Reuse ProjectCinematic; do not build a second capture or render pipeline.
- Research the smallest practical authoring workflow before implementation.
- Keep implementation tiny, local/open where practical, and reproducible through
  repo-owned instructions.
- MCP/editor control is authoring convenience. Deterministic scripts, receipts, and
  exact Candidate binding remain release authority.
- Create a narrow repo-local cinematic skill only after a successful authoring smoke
  proves the workflow and the active agent runtime's skill convention is verified.
- A future capture may bind to the deliberately selected accepted Candidate for that
  release. Never weaken exact package, executable, composite, source-state, or source-
  revision checks to reuse an older Candidate from a different source state.

## Verified baseline

- [ProjectCinematic](../../Plugins/Editor/ProjectCinematic/README.md) is Editor-only;
  capture dependencies must remain absent from Shipping.
- `ACinematicGameMode` owns Record/Render adaptation, clean render UI/pawn behavior,
  and active-camera World Partition streaming.
- The existing authored sequence is
  `/Game/Cinematics/Kazan/LS_KazanRelease_v1`; MRQ presets already exist.
- [run_release_capture.ps1](../../scripts/ue/cinematic/run_release_capture.ps1)
  owns schema validation, Editor/MRQ execution, evidence, promotion, and cleanup.
- [release_binding.ps1](../../scripts/ue/cinematic/release_binding.ps1) independently
  authenticates Candidate tree, Shipping executable, source revision/state, release
  operation, and composite identity before render.
- The accepted Kazan product decision is recorded at
  `Saved/Validation/WorldRealization/playable-tour/Candidate/operator-acceptance.json`.
- The frozen Kazan Candidate source revision is no longer current HEAD. This is an
  expected fail-closed condition, not permission to bypass release binding.
- ProjectWorld exposes no cinematic API and must continue exposing none.
- ProjectCinematic already has technically accepted Take Recorder source injection
  and interaction-event stamping. Treat those as reusable owner capabilities, not as
  a selected release-shot design; reopen them only if an accepted shot actually needs
  recorded gameplay interaction.

## Ownership boundary

### May change after future approval

- ProjectCinematic Editor-only authoring support;
- owner-local test assets or scratch sequence used for the smoke;
- native Unreal Python/Sequencer scripts when selected by evidence;
- a very small ProjectCinematic-local MCP toolset only for a proven recurring gap;
- repo-local cinematic skill/instructions after the workflow is proven;
- the deliberately selected release sequence and deterministic capture request.

### Must remain untouched

- ProjectWorld generation/realization and Kazan geography;
- packaged gameplay, ProjectCharacter, and ProjectSinglePlay behavior;
- Shipping performance policy;
- public runtime dependencies and package contents;
- accepted Candidate bytes and today's operator receipt;
- promotion/publication until separately authorized.

## Research order when activated

### V0 - capability census

Audit current HEAD and the installed UE version. Based on the supplied UE 5.8
documentation, investigate Epic's Experimental Unreal MCP first, but treat its local
availability and actual toolsets as unverified until live discovery proves them.

Discover, rather than guess, whether installed tools can inspect/create/edit:

- Level Sequences and Sequencer state;
- CineCameraActors, camera cuts, transforms, focal length, and focus tracks;
- playback ranges;
- MRQ/MRG jobs and configurations.

Keep any MCP endpoint loopback/local, Editor-only, and outside final acceptance.

If official MCP lacks a required operation, compare in this order:

1. native Unreal Python plus Sequencer/MRQ scripting inside the existing owner;
2. one tiny ProjectCinematic-local MCP extension for a repeated proven gap;
3. an external MCP only after a live UE 5.8 smoke proves a specific advantage.

The currently known third-party Unreal MCP is a research reference/fallback, not a
new hard dependency. Recheck compatibility when this todo is activated.

Research input: [Epic Unreal MCP documentation](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor).

### V1 - one throwaway authoring smoke

From a short natural-language brief, inspect, mutate, and re-inspect one test shot in
owner-local scratch or a deliberately selected test asset. Prove camera, cut, and
track structure deterministically. Produce cheap stills/preview for visual judgment;
do not render a release master.

### V2 - freeze the minimal workflow

Select official MCP, native Python, or the smallest measured combination. Only after
V1 passes, create/update one narrow agent skill whose job is:

`author/review an ALIS cinematic -> hand it to existing deterministic release capture`

Do not duplicate Unreal documentation, guessed MCP names, videos, or engine files.

### V3 - improve a real release sequence

Use the proven workflow for a small meaningful shot set. Automation proves sequence
structure; reviewer/human judgment owns composition, motion, readability, and story.

### V4 - deterministic release render

Use the existing ProjectCinematic/MRQ owner. Bind to the deliberately selected accepted
Candidate and matching source state. Do not hide a quality/scalability uplift relative
to product truth. Prove capture-only Editor/MRQ/Takes/compiler payload stays outside the
Shipping product through the existing staged-file and IoStore census.

### V5 - optional post-production

Only if delivery needs joining, trimming, transcoding, titles, or audio, reuse existing
open local tools first. Do not build a video editor inside ALIS.

## Acceptance target

A brief such as:

`Create a 6-second aerial establishing shot from Kremlin over the river, then a lower oblique city shot; keep motion slow and readable.`

should let an agent inspect state, author/propose cameras, show cheap previews for
review, and hand the accepted sequence to the existing release-capture owner.

## Stop conditions

- Stop if the installed official toolset cannot be authenticated; compare native
  Python before adding a dependency.
- Stop if authoring requires ProjectWorld or Shipping gameplay APIs; the boundary is
  wrong.
- Stop before real sequence mutation until V1's throwaway smoke and reviewer decision.
- Stop before render/promotion for the exact Candidate and operator authority required
  by that future release.

## Not authorized now

Do not execute V0-V5, enable plugins, create skills/code/assets, modify sequences, run
MRQ, or mutate/publish any Candidate in this task-creation turn.
