# Multiplayer Architecture Overview

This document outlines how the upcoming multiplayer stack fits into the existing modular architecture. Phase 8 work builds on proven Unreal Engine systems rather than re-implementing core networking.

## Core Entities

- **`ProjectOnline` module (Plugins/Online/ProjectOnline)**  
  Wraps Unreal's OnlineSubsystem/Online Services (OSSv1/OSSv2) inside `UProjectOnlineSubsystem`. Provides async APIs for sessions, matchmaking, invites, presence, and party state while translating provider errors into project enums.

- **`ProjectLoading` integration**  
  `FLoadRequest` gains optional session descriptors and travel intent. `UProjectLoadingSubsystem` emits an `OnTravelRequested` signal so Phase 5 (Travel) can coordinate session creation, matchmaking responses, content-pack readiness, and host migration.

- **`ProjectMenuMain` lobby layer**  
  MVVM view models (e.g., `UProjectLobbyViewModel`) surface roster, readiness, and matchmaking status. Menu screens consume ProjectOnline delegates to drive party UI, status toasts, and host controls without duplicating networking logic.

- **`ProjectSettings` dependency**  
  Phase 2 ensured optional content packs mount before the Travel phase. Multiplayer flows reuse this to guarantee every client has required chunks before session travel.

- **Engine services leveraged**  
  Uses Unreal Engine 5.5 features: Online Services (EOS/Null), Party & Voice plugins, Zen Server content streaming, and existing NetDriver/Travel mechanics.

## Deliverable Summary (Phase 8)

1. **ProjectOnline module** - subsystem wrapper, config loader, Blueprint exposure.  
2. **Loading subsystem hooks** - `OnTravelRequested`, matchmaking gating, host migration helpers.  
3. **Lobby UI** - roster/ready panels, invite & matchmaking UI, null-backend fallbacks.  
4. **Validation** - unit tests (mock OSS), functional Null backend runs, GAuntlet scenario, telemetry capture.

See `../../../todo/create_multiplayer.md` for the actionable checklist.
