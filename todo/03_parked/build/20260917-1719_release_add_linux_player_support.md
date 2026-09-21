# Add Linux Player Support for ALIS 2.1.0

**Status:** PARKED - blocked on a local native Ubuntu 22.04 x86-64 GPU
acceptance host is available and configured

**Scope:** Linux x86-64 player packaging, acceptance, release trust, and
qualified promotion handoff

**Stable documentation owners:** `docs/build/packaging_guide.md`,
`scripts/ue/package/README.md`, and `player/README.md`

## Contents

- [Goal and authority](#goal-and-authority)
- [Evidence and gap](#evidence-and-gap)
- [Accepted design](#accepted-design)
- [Required invariants](#required-invariants)
- [Implementation stages](#implementation-stages)
- [Acceptance matrix](#acceptance-matrix)
- [Documentation and rollout](#documentation-and-rollout)
- [Completion](#completion)

## Goal and authority

ALIS 2.1.0 ships one reviewed release identity with Windows and Linux x86-64
player artifacts on GitHub. Linux compatibility must be proven on an
operator-approved Ubuntu 22.04 x86-64 acceptance surface before promotion.

### Operator decisions

- **D1** ALIS 2.1.0 will support a Linux player build.
  - Effect: Linux player support is a release requirement, not an optional
    experiment after 2.1.0.
  - Reason: not stated
  - Date/source: operator statement supplied on 2026-09-17
- **D2** Update all relevant documentation for the Linux-supported release.
  - Effect: Windows-only player and release wording must not remain after the
    capability is accepted.
  - Reason: not stated
  - Date/source: operator statement supplied on 2026-09-17
- **D3** The ALIS 2.1.0 GitHub Release will include both Windows and Linux player
  packages.
  - Effect: both platform packages belong to one version/tag and one flat GitHub
    upload projection.
  - Reason: not stated
  - Date/source: operator statement supplied on 2026-09-17
- **D4** This work must include extending promotion automation/platform coverage
  toward suitable Linux communities.
  - Effect: the work includes a cross-repository promotion handoff after Linux
    support is real; it does not move connector or venue ownership into ALIS.
  - Reason: not stated
  - Date/source: operator statement supplied on 2026-09-17
- **D5 [SUPERSEDED by runtime evidence and D9]** Ubuntu 22.04 x86-64 under the current
  WSL2/WSLg environment is the available final Linux acceptance surface for
  ALIS 2.1.0.
  - Effect: WSL remains bounded to transport, filesystem-mode, verifier, and
    non-rendering diagnostics. It cannot issue final rendered acceptance.
  - Reason: this is the only Linux environment available on the current
    release workstation.
  - Date/source: operator statement supplied on 2026-09-17
- **D6** Linux delivery for ALIS 2.1.0 is GitHub-only; do not extend the itch
  publisher for Linux.
  - Effect: the existing Windows itch route remains unchanged and is outside
    the Linux completion gate.
  - Reason: the itch account/publication path is temporarily blocked.
  - Date/source: operator statement supplied on 2026-09-17
- **D7** Windows and Linux belong to one release workspace and one release
  lifecycle.
  - Effect: there is no parallel Linux release command, approval, signing
    transaction, or version authority.
  - Reason: not stated
  - Date/source: operator statement supplied on 2026-09-17
- **D8** Compatible Unreal Linux cross-toolchains may remain installed
  side-by-side; ALIS selects the exact project-relevant toolchain through the
  existing UE configuration source of truth.
  - Effect: adding v26 must not remove older toolchains, add a second config
    file, or depend on whichever global environment value happens to be set.
  - Reason: preserve older toolchains while selecting the exact one required
    by this project.
  - Date/source: operator statement supplied on 2026-09-17
- **D9** Future final rendered Linux acceptance uses a local native Ubuntu
  22.04 x86-64 GPU host, not WSLg or cloud acceptance.
  - Effect: resume only after that host provides GPU-backed Vulkan satisfying
    UE 5.8, SSH access from the release workstation, local Linux storage for
    runtime execution, and SCP or shared-storage artifact transport.
  - Reason: the current WSLg host provides GPU-backed OpenGL but only software
    Vulkan, so it cannot satisfy the unchanged release gate.
  - Date/source: operator approval supplied on 2026-09-18

### Gates and assumptions

- **Q1 [CLOSED by D9]:** A local native Ubuntu 22.04 x86-64 GPU host will run
  final rendered Linux acceptance after it is acquired and configured.
- **Q2 [CLOSED by D6]:** Must 2.1.0 also publish the Linux build to an itch
  `linux` channel, or is GitHub the only required Linux distribution for this
  release? - default while open: A4
- **A1 [ACTIVE]:** Windows remains the single build and release workstation;
  Linux x86-64 is cross-compiled with Epic's UE 5.8 Windows toolchain.
- **A2 [INVALIDATED by evidence]:** WSL2/WSLg cannot currently serve as final
  Linux runtime acceptance. It remains useful for transport, ext4 mode, shell
  verification, and non-rendering checks.
- **A3 [ACTIVE]:** D1 is Linux x86-64 player support only. Linux editor
  development, Linux-native building, and Linux dedicated-server delivery are
  outside this release.
- **A4 [RESOLVED by D6]:** GitHub delivery is required by D3. The itch
  publisher remains Windows-only and selects the verified Windows workspace
  projection; Linux itch publication is not a 2.1.0 completion gate.
- **A5 [ACTIVE]:** Promotion automation changes occur only for an eligible,
  selected Linux venue that the promotion owner has qualified; a platform
  theme alone does not justify a new connector.

### Non-goals

- Linux Editor/native build/CI, dedicated server, ARM64, macOS, Steam Deck
  certification, distro packages, or broad native-distro certification.
- Linux performance parity with the Windows RTX 4070/D3D12 product gate.
- Linux itch delivery, per-platform approval commands, a generic platform
  framework, or ALIS-local promotion connectors.

## Evidence and gap

- UE 5.8 requires `v26_clang-20.1.8-rockylinux8`; the host has v21-v23 and its
  ambient `LINUX_MULTIARCH_ROOT` selects v22.
- `scripts/config/ue_path.conf` is the strict config authority, but its shared
  PowerShell, batch, and Python readers do not accept that key. UE reads the key
  from process environment and identifies an SDK through
  `ToolchainVersion.txt`.
- `package_release.ps1` passes `-Platform` to UAT but post-processes only
  `Windows`, `Win64`, `.exe`, and `ALIS_Win64` output.
- The World-owned Candidate is the Windows reference product/performance
  authority. Its receipt and executable paths are explicitly Windows/D3D12.
- `release_workspace.py` and release manifests own one Windows `game/` and one
  player identity. `TARGET=game` already signs only game, skips GitHub, records
  game-scoped approval, and refuses later promotion to full GitHub approval.
- `publish_itch.ps1` verifies a signed Windows executable directly below
  `game/`; moving Windows to a platform child therefore requires a bounded
  consumer update even though D6 excludes Linux itch.
- Consumer verification is BAT/PowerShell-only. A Linux player must not need
  PowerShell to verify the same public key, detached signature, and checksums.
- Orchestrator's Authenticode helper searches `Binaries/Win64`, but no
  production caller invokes it. `StageColdUpdates()` contains only a TODO for
  `signature_thumbprint`; current Shipping boot does not enforce this helper.
  GPG release signing is therefore not replacing an active runtime check, and
  the release must not claim runtime module-signature enforcement.
- The D5 host exposes Ubuntu 22.04, WSL2/WSLg, `/dev/dxg`, and the RTX 4070
  through `nvidia-smi`, but its Vulkan loader exposes only `llvmpipe`. The
  NVIDIA ICD cannot provide the Vulkan entry point in this environment.
- The exact Linux Shipping package now builds, cooks, stages, archives, and
  passes byte-for-byte staged-file verification. The archive is copied to and
  extracted on the WSL ext4 filesystem with both launcher and ELF mode `0755`.
- The normal, unmodified UE launch then fails closed: none of the one reported
  Vulkan devices satisfies `VP_UE_Vulkan_SM5`. No Linux acceptance receipt is
  written. Diagnostic software-rendering bypasses are not release evidence.

The remaining gap is the unavailable D9 native acceptance host and the bounded
package/release adapter needed to run the exact Linux archive there. Toolchain,
package, schema, workspace, signing, and Windows-only itch changes are already
implemented and covered by focused tests.

## Accepted design

```text
World owner
  -> accepted Windows reference Candidate + product/performance receipt

Package/release owner
  -> Linux package + native Ubuntu platform-acceptance receipt

release workspace v2.1.0
  -> game/windows-x86_64/
  -> game/linux-x86_64/
  -> github/                    # one flat GitHub upload inventory
  -> release-workspace.json    # one version/approval/recovery authority
```

Both inputs bind the same source state and version. Linux platform acceptance
lives under package/release, not `scripts/ue/world`; opening Kazan/Manhattan
does not transfer platform QA ownership to World. The workspace map is closed
to the two proven keys.

The existing config owner gains `LINUX_MULTIARCH_ROOT`. Linux packaging
validates it against UE's `Linux_SDK.json` and `ToolchainVersion.txt`, scopes it
to the UAT child process, and restores the caller environment. Installed older
toolchains remain untouched; there is no release CLI selector.

The implemented WSL runtime runner remains useful only for transport, ext4
mode, shell verification, and non-rendering evidence. Final rendered acceptance
will use the D9 native Ubuntu host after it is available; no SSH, provisioning,
or shared-storage automation is added while this task is parked. Linux is
distributed on GitHub only. The Windows itch publisher resolves the Windows
entry from the verified workspace platform map; it does not infer a path or
gain a Linux channel.

Acceptance transfers the exact unsigned Linux archive through SCP or shared
storage, then copies it to the native host's local Linux filesystem before
verification, extraction, mode checks, and launch. Shared storage is transport
only and never the runtime surface. Its receipt binds that transport plus the
normalized runtime payload tree. Game signing adds only the declared verifier,
public key, checksum manifest, and detached signature. Final refresh rejects
any other runtime-payload change, archives the signed tree, and puts the final
parts under the GitHub signature. The final signed TAR is not claimed to have
been executed.

The multi-platform workspace and public manifest write new schema generations:
`alis-release-workspace-v2` and `alis-release-manifest-v4`. Readers retain
verification support for finalized v1/v3 releases. An explicitly requested
historical 2.0.0 operation retains its v1/v3 semantics; no path migrates,
reinterprets, or rewrites the reviewed 2.0.0 workspace in place.

The Linux game projection reuses the ALIS GPG trust material through a small
Linux-native verifier (or equivalent documented `gpg`/`sha256sum` commands).
This is an adapter to the existing trust model, not a second trust authority.

## Required invariants

1. One unsigned command prepares both required platforms; one signed command
   approves/signs/verifies both. Missing or stale platform evidence fails.
2. Windows World Candidate ownership and its fixed performance budget do not
   change. Linux acceptance is package/release-owned and functional only.
3. `TARGET=game` signs/verifies the complete Windows+Linux game set, performs no
   GitHub finalization, remains game-scoped/non-promotable, and gains no
   per-platform variant.
4. Workspace adoption/recovery remains fail-closed and binds source, version,
   platform package tree, executable, and acceptance receipt identities.
5. Linux transport preserves executable mode and rejects tampering, missing
   parts, unsafe paths, or basename collisions.
6. The GitHub projection is flat and uniquely named; Linux delivery does not
   change or invoke itch, while Windows itch follows the verified Windows map.
7. GPG alone requests the passphrase. Build, test, and signing perform no
   remote write.
8. WSL evidence remains diagnostic and is never reported as native Ubuntu
   rendered acceptance or broad native-Linux certification.
9. The Linux release claims only package authenticity. Any future runtime
   plugin/module trust contract remains Orchestrator-owned and requires its own
   implemented, tested cross-platform invariant.
10. Linux execute-mode acceptance is performed after copying the unsigned
    archive into a Linux filesystem; `/mnt/*` cannot satisfy it. Final refresh
    must prove the accepted runtime payload is unchanged after signing.
11. New multi-platform state writes workspace v2 and manifest v4. Historical
    workspace v1 and manifest v3 remain readable/verifiable but immutable.

## Implementation stages

- [x] **Toolchain red gate:** add failing resolver/preflight tests, extend the
  existing config readers with `LINUX_MULTIARCH_ROOT`, install v26 beside older
  SDKs with explicit host-install authorization, and prove wrong/missing SDKs
  fail before UAT. Prove child scoping and restoration on success/failure.
- [x] **Disposable feasibility package:** cross-compile/cook/package Linux under
  `tmp/`; record actual root, launcher/ELF, file modes, size, plugin failures,
  runtime-data staging, and Orchestrator boot behavior. Fix only demonstrated
  blockers. Do not create a Linux World Candidate.
- [ ] **Platform acceptance - PARKED FOR NATIVE HOST:** after the D9 host is
  available, adapt the package/release-owned environment boundary without
  adding a parallel release owner. Transfer the exact unsigned archive, copy it
  to local Linux storage before extraction, and prove mode preservation there;
  then prove Vulkan boot,
  menu/UI/input, Kazan and Manhattan load, runtime data, no missing
  module/plugin error, clean exit, and a hash-bound environment receipt.
- [x] Close Q1 through D9. Do not use
  `-SkipVulkanProfileCheck`, `-AllowSoftwareRendering`, a Linux NVIDIA display
  driver inside WSL, or an unstable Mesa PPA as release evidence.
- [x] **Workspace migration:** replace the single game/player state with the
  closed platform map in workspace v2 / manifest v4 while preserving v1/v3
  read/verify compatibility, atomic adoption, interruption recovery,
  stale-workspace refusal, exact inventory, and public manifest identity. Do
  not start this stage until the disposable Linux package compiles and cooks.
- [x] **Transport and trust:** create Linux archives that preserve executable
  mode; extend signing/verification for each game projection and the flat
  GitHub inventory; provide Linux-native consumer verification using the same
  public key/signature/checksum authority.
- [x] **Existing consumers:** preserve full and `TARGET=game` semantics; update
  Windows itch to select only `windows-x86_64` from the verified workspace;
  add no Linux itch route.
- [ ] **Docs and promotion:** update stable owners from realized artifact names
  and commands, then hand the proven GitHub Linux fact to promotion. Promotion
  qualifies venues; automations changes connectors only for selected gaps.
- [ ] Run the matrix below, inspect the complete diff against D1-D9, and stop
  at the unsigned 2.1.0 workspace for operator review.

## Acceptance matrix

| Evidence | Required proof |
|---|---|
| Config/toolchain | All config readers agree; v22/missing/fake marker reject; v26 accepts; caller env is unchanged. |
| Linux package | Shipping compile/cook/package and runtime-data audit pass through the source engine. |
| World boundary | Existing Windows Candidate and RTX 4070/D3D12 controls remain green; no Linux owner is added under World. |
| Linux acceptance | Unsigned Linux archive is transferred to the native Ubuntu host, copied to its local Linux filesystem, then verifies/extracts/launches with executable mode preserved; Vulkan, both maps, UI/input, data, and clean exit pass with a runtime-payload-bound receipt. Shared mounts cannot prove runtime filesystem behavior. |
| Workspace | Two-platform identity, missing/swapped platform, tamper, collision, and interrupted adoption/recovery controls pass. |
| Schema evolution | New state writes workspace v2 / manifest v4; finalized v1/v3 artifacts still verify; no command rewrites 2.0.0 in place. |
| Trust | Windows and Linux standalone projections plus flat GitHub inventory verify; changed byte and removed execute bit reject. |
| Final Linux archive | Signed-tree refresh accepts only the runtime payload proven by Linux acceptance plus declared signer-owned files; final TAR parts are bound by the GitHub signature. |
| `TARGET=game` | Both game platforms sign/verify; GitHub remains untouched; full promotion is refused. |
| Windows itch | Verified Windows platform path is selected; Linux cannot be selected or published. No remote test write is required. |
| Linux user route | Verification and launch work on Ubuntu without BAT or PowerShell. |
| Release | Fresh unsigned `make release 2.1.0 RELEASE_SIGN=0` produces one complete unsigned workspace and performs no remote write. |

## Documentation and rollout

- `docs/build/packaging_guide.md` owns the operator flow, Linux support wording,
  GitHub upload, `TARGET=game`, and Windows-only itch compatibility.
- `scripts/ue/package/README.md` owns the platform workspace, toolchain input,
  recovery, signing, and verification behavior.
- `player/README.md` owns concise Windows/Linux extraction, verification, and
  run instructions. Root README only routes and names release platforms.
- Generated `README.txt` derives platform instructions from the manifest.
  Developer workflow remains Windows-hosted; promotion/connector details remain
  in their sibling owners.
- Feasibility output stays in `tmp/`. Do not mutate 2.0.0. Until 2.1.0 is
  signed, rollback removes only the disposable 2.1.0 implementation/workspace.
  GitHub and promotion writes retain explicit operator authorization.

## Completion

- A real Linux package passes the full D9 artifact route with bounded public
  wording, and Windows behavior/performance is unchanged.
- One verified 2.1.0 workspace owns both game projections and one flat GitHub
  projection; normal and game-only approval paths pass their refusal controls.
- Toolchain selection has one config authority and supports side-by-side SDKs.
- Linux consumer verification is shell-native, Linux itch does not exist,
  Windows itch still consumes the signed Windows game, and no runtime trust
  claim exceeds code.
- Stable docs contain the durable result, promotion receives only proven facts,
  and no credentials, private paths, unrelated changes, or remote mutations
  enter the diff.
- This todo returns to the active queue only after the D9 host prerequisites
  are available.

## Review record

- 2026-09-17: initial investigation established the UE/toolchain and
  Windows-shaped release gaps; D1-D4 recorded.
- 2026-09-17: D5-D8 closed environment, distribution, workspace, and toolchain
  choices. Architecture review separated Linux platform acceptance from World,
  preserved `TARGET=game` and Windows itch, added Linux-native consumer
  verification, and recorded the actual dormant Orchestrator signature state.
- 2026-09-17: final architecture review required Linux-filesystem acceptance
  rather than DrvFS evidence and explicit workspace v2 / manifest v4 evolution.
  D5-D8 were the active decisions at implementation start because each was
  backed by a direct operator statement in the approval thread. Implementation
  began at the toolchain and disposable-package feasibility boundary.
- 2026-09-18: v26 selection, Linux compilation/cook/stage/archive, runtime-data
  staging, exact archive transport, ext4 extraction, ELF identity, and execute
  modes passed. The production acceptance runner rejected the normal launch
  because WSLg exposed only `llvmpipe`, which does not satisfy UE 5.8's
  `VP_UE_Vulkan_SM5`. No acceptance receipt or 2.1.0 workspace was created;
  the runner removed its ephemeral WSL filesystem checkout.
- 2026-09-18: workspace v2 / manifest v4, dual-platform signing and refresh,
  Linux TAR mode preservation, shell-native consumer verification, historical
  v1/v3 compatibility, source-identity binding, and Windows-only itch routing
  passed focused tests. The final unsigned workspace and public player docs
  remain blocked on the unchanged Vulkan acceptance gate.
- 2026-09-18: host diagnostics proved WSLg OpenGL uses accelerated D3D12 on the
  RTX 4070 while the UE Vulkan route still has no accepted device. Q1 reopened;
  native Ubuntu remains only a proposal. Release verification now makes 2.1+
  schema generation version-authoritative, enforces identical platform source
  identities, and distinguishes accepted runtime payload identity from the
  final signed TAR transport.
- 2026-09-18: supported WSL servicing and distro Vulkan diagnostics confirmed
  WSLg exposes only software Vulkan despite accelerated RTX OpenGL. D9 selects
  a future local native Ubuntu GPU host, Q1 is closed, and the task is parked
  until that host and its access/transport prerequisites exist.
